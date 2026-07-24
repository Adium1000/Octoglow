import time
import asyncio
import requests
import threading
import queue

from winrt.windows.media.control import (
    GlobalSystemMediaTransportControlsSessionManager as MediaManager
)

#  CONFIGURATION
ESP32_IP       = "192.168.0.170"
ESP32_PORT     = 80
SC_USER        = ""        # <-- your SmartClock username
SC_PASS        = ""    # <-- your SmartClock password

POLL_INTERVAL  = 3       # Now Playing polling interval (seconds)
SEND_INTERVAL  = 10      # resend NP if it's the same (seconds)
NOTIF_POLL_MS  = 0.35    # notification polling interval (seconds)
NOTIF_MAX_CHARS = 110    # max characters per notification (rest → ...)

ETS2_POLL_INTERVAL = 0.5   # ETS2 telemetry read interval (seconds)


BASE_URL = f"http://{ESP32_IP}:{ESP32_PORT}"


_session = requests.Session()

def _login(retries: int = 5, delay: float = 5.0) -> bool:
    """
    Logs in to SmartClock and saves the session cookie.
    Retries up to `retries` times if the ESP32 is not yet available
    (useful at Windows startup, when the network may still be unstable).
    Returns True if login succeeded.
    """
    for attempt in range(1, retries + 1):
        try:
            r = _session.post(
                f"{BASE_URL}/login",
                data={"user": SC_USER, "pass": SC_PASS},
                timeout=5,
            )
            if r.status_code == 200:
                print(f"[AUTH] Login successful as '{SC_USER}'.")
                return True
            elif r.status_code == 401:
                print(f"[AUTH] ❌ Incorrect username or password. Check SC_USER/SC_PASS.")
                return False  
            else:
                print(f"[AUTH] Unexpected response: {r.status_code} — retry {attempt}/{retries}")
        except requests.exceptions.ConnectionError:
            print(f"[AUTH] Cannot connect to {ESP32_IP} (attempt {attempt}/{retries}) — retrying in {delay}s")
        except requests.exceptions.Timeout:
            print(f"[AUTH] Login timeout (attempt {attempt}/{retries})")
        except Exception as e:
            print(f"[AUTH] Error: {e}")
        time.sleep(delay)
    print("[AUTH] ❌ Login failed after all attempts. The script will continue but requests will be rejected.")
    return False

def _ensure_session(r: requests.Response) -> bool:
    """
    Checks whether the session has expired (401 from the ESP32).
    If so, automatically re-logs in.
    Returns True if the session is valid (or re-login succeeded).
    """
    if r.status_code == 401:
        print("[AUTH] Session expired — re-logging in...")
        return _login(retries=3, delay=2.0)
    return True

_notif_queue: queue.Queue[str] = queue.Queue()

def _notif_sender_thread():
    """
    Dedicated thread that consumes the queue and sends HTTP requests to the ESP32.
    Runs separately — never blocks the listener's async loop.
    """
    while True:
        text = _notif_queue.get()   # blocks until something appears
        url = f"{BASE_URL}/notification"
        try:
            r = _session.post(url, data={"text": text}, timeout=3)
            if r.status_code == 200:
                print(f"[NOTIF 🔔] {text}")
            elif r.status_code == 401:
                if _ensure_session(r):
                    r2 = _session.post(url, data={"text": text}, timeout=3)
                    if r2.status_code == 200:
                        print(f"[NOTIF 🔔] {text}")
                    else:
                        print(f"[NOTIF WARN] Status {r2.status_code} after re-login")
            else:
                print(f"[NOTIF WARN] Status {r.status_code}: {r.text}")
        except requests.exceptions.ConnectionError:
            print(f"[NOTIF ERR] Cannot connect to {ESP32_IP}")
        except requests.exceptions.Timeout:
            print("[NOTIF ERR] Timeout")
        except Exception as e:
            print(f"[NOTIF ERR] {e}")
        finally:
            _notif_queue.task_done()


_sender_t = threading.Thread(target=_notif_sender_thread, daemon=True, name="NotifSender")
_sender_t.start()

#NOW PLAYING 

async def _fetch_np() -> str | None:
    mgr = await MediaManager.request_async()
    session = mgr.get_current_session()
    if session is None:
        return None
    props = await session.try_get_media_properties_async()
    if props is None:
        return None
    title  = (props.title  or "").strip()
    artist = (props.artist or "").strip()
    if not title:
        return None
    return f"{artist} - {title}" if artist else title

def get_now_playing() -> str | None:
    try:
        return asyncio.run(_fetch_np())
    except Exception as e:
        print(f"[WARN] NP: {e}")
        return None

def send_nowplaying(text: str):
    url = f"{BASE_URL}/nowplaying"
    try:
        r = _session.post(url, data={"text": text}, timeout=3)
        if r.status_code == 200:
            print(f"[NP ▶]  {text}")
        elif r.status_code == 401:
            if _ensure_session(r):
                r2 = _session.post(url, data={"text": text}, timeout=3)
                if r2.status_code == 200:
                    print(f"[NP ▶]  {text}")
                else:
                    print(f"[NP WARN] Status {r2.status_code} after re-login")
        else:
            print(f"[NP WARN] Status {r.status_code}: {r.text}")
    except requests.exceptions.ConnectionError:
        print(f"[NP ERR] Cannot connect to {ESP32_IP}")
    except requests.exceptions.Timeout:
        print("[NP ERR] Timeout")

#NOTIFICATIONS 
_DIACRITICS_MAP = {
    "ă": "a", "â": "a", "à": "a", "á": "a", "ä": "a", "å": "a", "ã": "a",
    "Ă": "A", "Â": "A", "À": "A", "Á": "A", "Ä": "A", "Å": "A", "Ã": "A",
    "ș": "s", "ş": "s",
    "Ș": "S", "Ş": "S",
    "ț": "t", "ţ": "t",
    "Ț": "T", "Ţ": "T",
    "î": "i", "ì": "i", "í": "i", "ï": "i",
    "Î": "I", "Ì": "I", "Í": "I", "Ï": "I",
    "è": "e", "é": "e", "ê": "e", "ë": "e",
    "È": "E", "É": "E", "Ê": "E", "Ë": "E",
    "ò": "o", "ó": "o", "ô": "o", "õ": "o", "ö": "o", "ø": "o",
    "Ò": "O", "Ó": "O", "Ô": "O", "Õ": "O", "Ö": "O", "Ø": "O",
    "ù": "u", "ú": "u", "û": "u", "ü": "u",
    "Ù": "U", "Ú": "U", "Û": "U", "Ü": "U",
    "ñ": "n", "Ñ": "N",
    "ç": "c", "Ç": "C",
}

def _sanitize_for_display(text: str) -> str:
    out = []
    for ch in text:
        if ch in _DIACRITICS_MAP:
            out.append(_DIACRITICS_MAP[ch])
            continue
        cp = ord(ch)
        if 0x20 <= cp < 0x7F:
            out.append(ch)
        elif ch == "°":
            out.append(ch)
    return "".join(out)

def _truncate(text: str, maxlen: int = NOTIF_MAX_CHARS) -> str:
    if len(text) <= maxlen:
        return text
    return text[:maxlen - 3].rstrip() + "..."

def _build_notif_text(app: str, title: str, body: str) -> str:
    parts = []
    if app:
        parts.append(app)
    if title:
        parts.append(title)
    if body:
        parts.append(body)
    text = " — ".join(parts) if parts else "(notification)"
    text = _sanitize_for_display(text)
    return _truncate(text)

def enqueue_notification(app: str, title: str, body: str):
    text = _build_notif_text(app, title, body)
    _notif_queue.put(text)

def _parse_notif(n):
    app_name  = ""
    title_txt = ""
    body_txt  = ""
    try:
        app_info = n.app_info
        if app_info:
            try:
                app_name = app_info.display_info.display_name or ""
            except Exception:
                pass
        binding = n.notification.visual.get_binding("ToastGeneric")
        if binding:
            texts = list(binding.get_text_elements())
            if len(texts) > 0:
                title_txt = texts[0].text or ""
            if len(texts) > 1:
                body_txt  = texts[1].text or ""
    except Exception as e:
        print(f"[NOTIF WARN] Parse error: {e}")
    return app_name, title_txt, body_txt


def start_notification_listener():
    try:
        from winrt.windows.ui.notifications.management import (
            UserNotificationListener,
            UserNotificationListenerAccessStatus,
        )
        from winrt.windows.ui.notifications import NotificationKinds

        async def _listen():
            listener = UserNotificationListener.current
            status = await listener.request_access_async()
            if status != UserNotificationListenerAccessStatus.ALLOWED:
                print("[NOTIF] Access DENIED by Windows.")
                print("[NOTIF] Go to Settings > System > Notifications and allow access.")
                return

            print(f"[NOTIF] Fast polling active ({int(NOTIF_POLL_MS*1000)}ms interval).")

            seen_ids: set[int] = set()
            try:
                existing = list(await listener.get_notifications_async(NotificationKinds.TOAST))
                for n in existing:
                    seen_ids.add(n.id)
                if existing:
                    print(f"[NOTIF] {len(existing)} existing notifications ignored at startup.")
            except Exception as e:
                print(f"[NOTIF WARN] Init snapshot: {e}")

            while True:
                try:
                    notifs = list(await listener.get_notifications_async(NotificationKinds.TOAST))
                    for n in notifs:
                        if n.id not in seen_ids:
                            seen_ids.add(n.id)
                            app, title, body = _parse_notif(n)
                            if title or body:
                                enqueue_notification(app, title, body)
                    current_ids = {n.id for n in notifs}
                    gone = seen_ids - current_ids
                    if gone:
                        seen_ids -= gone
                except Exception as e:
                    print(f"[NOTIF ERR] Poll error: {e}")
                await asyncio.sleep(NOTIF_POLL_MS)

        def _thread_run():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                loop.run_until_complete(_listen())
            except Exception as e:
                print(f"[NOTIF] Thread stopped with error: {e}")

        t = threading.Thread(target=_thread_run, daemon=True, name="NotifListener")
        t.start()
        return True

    except ImportError as e:
        print(f"[NOTIF] Missing WinRT module: {e}")
        print("[NOTIF] Install with: pip install winrt-Windows.UI.Notifications.Management")
        return False
    except Exception as e:
        print(f"[NOTIF] Error starting listener: {e}")
        return False


# ── ETS2 (Euro Truck Simulator 2) ──────────────────────────────────────────────

def send_ets2_speed(speed_kmh: int):
    url = f"{BASE_URL}/ets2speed"
    try:
        r = _session.post(url, data={"speed": speed_kmh}, timeout=2)
        if r.status_code == 200:
            print(f"[ETS2 🚚] {speed_kmh} km/h")
        elif r.status_code == 401:
            if _ensure_session(r):
                _session.post(url, data={"speed": speed_kmh}, timeout=2)
        elif r.text not in ("DISABLED", "OK"):
            print(f"[ETS2 WARN] Status {r.status_code}")
    except requests.exceptions.ConnectionError:
        print(f"[ETS2 ERR] Cannot connect to {ESP32_IP}")
    except requests.exceptions.Timeout:
        print("[ETS2 ERR] Timeout")
    except Exception as e:
        print(f"[ETS2 ERR] {e}")


def _is_ets2_running() -> bool:
    try:
        import psutil
        for p in psutil.process_iter(["name"]):
            name = (p.info.get("name") or "").lower()
            if name == "eurotrucks2.exe":
                return True
        return False
    except ImportError:
        try:
            import subprocess
            out = subprocess.check_output(
                ["tasklist", "/FI", "IMAGENAME eq eurotrucks2.exe"],
                creationflags=subprocess.CREATE_NO_WINDOW,
            ).decode(errors="ignore").lower()
            return "eurotrucks2.exe" in out
        except Exception:
            return False


def _ets2_thread():
    try:
        import truck_telemetry
    except ImportError:
        print("[ETS2] The 'truck_telemetry' module is not installed — ETS2 disabled.")
        print("[ETS2] Install with: pip install truck-telemetry")
        return

    connected = False
    last_speed_sent = None

    while True:
        if not _is_ets2_running():
            if connected:
                print("[ETS2] eurotrucks2.exe is no longer running — disconnected.")
            connected = False
            last_speed_sent = None
            time.sleep(2)
            continue

        if not connected:
            try:
                truck_telemetry.init()
                connected = True
                last_speed_sent = None
                print("[ETS2] eurotrucks2.exe detected — connected to telemetry.")
            except Exception:
                time.sleep(2)
                continue

        try:
            data = truck_telemetry.get_data()
            speed_ms = data.get("speed", 0.0)
            speed_kmh = int(round(abs(speed_ms) * 3.6))
            if speed_kmh != last_speed_sent:
                send_ets2_speed(speed_kmh)
                last_speed_sent = speed_kmh
            else:
                send_ets2_speed(speed_kmh)
        except Exception as e:
            print(f"[ETS2] Telemetry unavailable ({e}). ETS2 has likely closed.")
            connected = False
            last_speed_sent = None

        time.sleep(ETS2_POLL_INTERVAL)


def start_ets2_listener():
    t = threading.Thread(target=_ets2_thread, daemon=True, name="Ets2Telemetry")
    t.start()
    return True




def main():
    print("=" * 55)
    print("  SmartClock — Now Playing + Notifications + ETS2 Sender")
    print(f"  Target: {BASE_URL}")
    print("=" * 55)
    _login(retries=5, delay=5.0)

    notif_ok = start_notification_listener()
    if not notif_ok:
        print("[NOTIF] Notifications will NOT be sent.")

    start_ets2_listener()
    print("[ETS2] Waiting for ETS2 in the background (start the game whenever you want)...")
    print()

    print("[TEST] Checking what's currently playing...")
    test = get_now_playing()
    if test:
        print(f"[TEST] Found: {test}\n")
    else:
        print("[TEST] Nothing detected at the moment.\n")

    last_sent_text = None
    last_send_time = 0

    while True:
        now_playing = get_now_playing()
        now = time.time()

        if now_playing:
            if now_playing != last_sent_text or (now - last_send_time) >= SEND_INTERVAL:
                send_nowplaying(now_playing)
                last_sent_text = now_playing
                last_send_time = now
            else:
                print(f"[NP --]  {now_playing}")
        else:
            if last_sent_text is not None:
                print("[INFO] Nothing is playing anymore")
            last_sent_text = None

        time.sleep(POLL_INTERVAL)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopping.")
