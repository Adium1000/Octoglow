"""
Octoglow Sender 
Requirements (Windows, Python 3.9+):
    pip install requests customtkinter
    pip install winrt-Windows.Media.Control winrt-Windows.UI.Notifications.Management
    pip install pystray pillow
    pip install pywinstyles
    (optional, for ETS2 telemetry) pip install truck-telemetry psutil
"""

import os
import sys
import json
import time
import math
import queue
import asyncio
import threading
import tkinter as tk

import requests
import customtkinter as ctk
from tkinter import messagebox
from PIL import Image, ImageDraw, ImageTk  # Pillow is already a hard dependency of customtkinter

try:
    import winreg
except ImportError:
    winreg = None

try:
    import pywinstyles
    HAVE_PYWINSTYLES = True
except ImportError:
    HAVE_PYWINSTYLES = False

try:
    from cryptography.fernet import Fernet
    HAVE_CRYPTO = True
except ImportError:
    HAVE_CRYPTO = False

try:
    import pystray
    HAVE_TRAY = True
except ImportError:
    HAVE_TRAY = False

try:
    from winrt.windows.media.control import (
        GlobalSystemMediaTransportControlsSessionManager as MediaManager,
    )
    HAVE_WINRT_MEDIA = True
except ImportError:
    HAVE_WINRT_MEDIA = False

MD3_LIGHT = {
    "primary":            "#6750A4",
    "on_primary":         "#FFFFFF",
    "primary_container":  "#EADDFF",
    "on_primary_container": "#21005D",
    "secondary":          "#625B71",
    "secondary_container": "#E8DEF8",
    "surface":            "#FFFBFE",
    "surface_dim":        "#DED8E1",
    "surface_container":  "#F3EDF7",
    "surface_container_high": "#ECE6F0",
    "surface_variant":    "#E7E0EC",
    "on_surface":         "#1C1B1F",
    "on_surface_variant": "#49454F",
    "outline":            "#79747E",
    "outline_variant":    "#CAC4D0",
    "error":              "#B3261E",
    "on_error":           "#FFFFFF",
    "error_container":    "#F9DEDC",
}

MD3_DARK = {
    "primary":            "#D0BCFF",
    "on_primary":         "#381E72",
    "primary_container":  "#4F378B",
    "on_primary_container": "#EADDFF",
    "secondary":          "#CCC2DC",
    "secondary_container": "#4A4458",
    "surface":            "#141218",
    "surface_dim":        "#141218",
    "surface_container":  "#211F26",
    "surface_container_high": "#2B2930",
    "surface_variant":    "#49454F",
    "on_surface":         "#E6E0E9",
    "on_surface_variant": "#CAC4D0",
    "outline":            "#938F99",
    "outline_variant":    "#49454F",
    "error":              "#F2B8B5",
    "on_error":           "#601410",
    "error_container":    "#8C1D18",
}


MD3 = dict(MD3_LIGHT)
THEME_MODE = "light"


def _set_theme(mode: str):
    """Swap the global color palette in place."""
    global THEME_MODE
    THEME_MODE = mode
    MD3.clear()
    MD3.update(MD3_DARK if mode == "dark" else MD3_LIGHT)
    ctk.set_appearance_mode("dark" if mode == "dark" else "light")


ctk.set_appearance_mode("light")


#  Language strings
LANG = {
    "ro": {
        "app_title": "Octoglow Sender",
        "section_connection": "CONEXIUNE",
        "label_ip": "Adresă IP",
        "label_port": "Port",
        "label_user": "Utilizator",
        "label_pass": "Parolă",
        "show_pass": "arată",
        "section_options": "OPȚIUNI",
        "opt_startup": "Pornește la Windows startup",
        "opt_tray": "Minimizează în tray",
        "opt_tray_unavailable": "(instalează 'pystray' și 'pillow' pentru tray)",
        "btn_save_start": "Rulează & Salvează",
        "btn_stop": "Oprește",
        "btn_minimize": "Minimizează acum",
        "section_log": "JURNAL",
        "log_starting": "Pornire — țintă",
        "log_stopped": "Oprit.",
        "tray_show": "Arată",
        "tray_exit": "Ieșire",
        "msgbox_tray_title": "Tray indisponibil",
        "msgbox_tray_body": "Instalează 'pystray' și 'pillow' (pip install pystray pillow) pentru a activa asta.",
        "lang_popup_title": "Alege limba",
        "lang_popup_ro": "Română",
        "lang_popup_en": "Engleză",
    },
    "en": {
        "app_title": "Octoglow Sender",
        "section_connection": "CONNECTION",
        "label_ip": "IP Address",
        "label_port": "Port",
        "label_user": "Username",
        "label_pass": "Password",
        "show_pass": "show",
        "section_options": "OPTIONS",
        "opt_startup": "Run at Windows startup",
        "opt_tray": "Minimize to tray",
        "opt_tray_unavailable": "(install 'pystray' and 'pillow' for tray support)",
        "btn_save_start": "Run & Save",
        "btn_stop": "Stop",
        "btn_minimize": "Minimize now",
        "section_log": "LOG",
        "log_starting": "Starting — target",
        "log_stopped": "Stopped.",
        "tray_show": "Show",
        "tray_exit": "Exit",
        "msgbox_tray_title": "Tray unavailable",
        "msgbox_tray_body": "Install 'pystray' and 'pillow' (pip install pystray pillow) to enable this.",
        "lang_popup_title": "Choose language",
        "lang_popup_ro": "Romanian",
        "lang_popup_en": "English",
    },
}


# Config (persisted to disk) 

APP_NAME = "OctoglowSender"

def _config_dir() -> str:
    base = os.environ.get("APPDATA") or os.path.expanduser("~")
    path = os.path.join(base, APP_NAME)
    os.makedirs(path, exist_ok=True)
    return path

CONFIG_PATH = os.path.join(_config_dir(), "config.json")

DEFAULT_CONFIG = {
    "esp32_ip": "192.168.0.170",
    "esp32_port": 80,
    "sc_user": "",
    "sc_pass": "",
    "run_at_startup": False,
    "minimize_to_tray": False,
    "language": "ro",
    "dark_mode": False,
    "poll_interval": 3,
    "send_interval": 10,
    "notif_max_chars": 110,
    "ets2_poll_interval": 0.5,
}


def _get_or_create_key() -> bytes:
    key_path = os.path.join(_config_dir(), "key.bin")
    if os.path.exists(key_path):
        with open(key_path, "rb") as f:
            return f.read()
    key = Fernet.generate_key()
    with open(key_path, "wb") as f:
        f.write(key)
    return key


_fernet = Fernet(_get_or_create_key()) if HAVE_CRYPTO else None


def _encrypt_pass(plain: str) -> str:
    if not plain:
        return ""
    if not HAVE_CRYPTO:
        return plain  # no 'cryptography' installed, store as plain text
    return _fernet.encrypt(plain.encode("utf-8")).decode("ascii")


def _decrypt_pass(stored: str) -> str:
    if not stored:
        return ""
    if not HAVE_CRYPTO:
        return stored
    try:
        return _fernet.decrypt(stored.encode("ascii")).decode("utf-8")
    except Exception:
        # saved before encryption was added, or the key changed
        return stored


def load_config() -> dict:
    cfg = dict(DEFAULT_CONFIG)
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                cfg.update(json.load(f))
            cfg["sc_pass"] = _decrypt_pass(cfg.get("sc_pass", ""))
        except Exception:
            pass
    return cfg


def save_config(cfg: dict):
    to_save = dict(cfg)
    to_save["sc_pass"] = _encrypt_pass(cfg.get("sc_pass", ""))
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(to_save, f, indent=2)


# Windows "run at startup" (registry, current user, no admin needed)

RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"


def _startup_command() -> str:
    script_path = os.path.abspath(__file__)
    py_dir = os.path.dirname(sys.executable)
    pythonw = os.path.join(py_dir, "pythonw.exe")
    interpreter = pythonw if os.path.exists(pythonw) else sys.executable
    return f'"{interpreter}" "{script_path}"'


def set_run_at_startup(enabled: bool) -> bool:
    if winreg is None:
        return False
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            if enabled:
                winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, _startup_command())
            else:
                try:
                    winreg.DeleteValue(key, APP_NAME)
                except FileNotFoundError:
                    pass
        return True
    except Exception as e:
        print(f"[STARTUP] Could not update registry: {e}")
        return False


# Backend: start/stop-able worker class 

_DIACRITICS_MAP = {
    "ă": "a", "â": "a", "à": "a", "á": "a", "ä": "a", "å": "a", "ã": "a",
    "Ă": "A", "Â": "A", "À": "A", "Á": "A", "Ä": "A", "Å": "A", "Ã": "A",
    "ș": "s", "ş": "s", "Ș": "S", "Ş": "S",
    "ț": "t", "ţ": "t", "Ț": "T", "Ţ": "T",
    "î": "i", "ì": "i", "í": "i", "ï": "i",
    "Î": "I", "Ì": "I", "Í": "I", "Ï": "I",
    "è": "e", "é": "e", "ê": "e", "ë": "e",
    "È": "E", "É": "E", "Ê": "E", "Ë": "E",
    "ò": "o", "ó": "o", "ô": "o", "õ": "o", "ö": "o", "ø": "o",
    "Ò": "O", "Ó": "O", "Ô": "O", "Õ": "O", "Ö": "O", "Ø": "O",
    "ù": "u", "ú": "u", "û": "u", "ü": "u",
    "Ù": "U", "Ú": "U", "Û": "U", "Ü": "U",
    "ñ": "n", "Ñ": "N", "ç": "c", "Ç": "C",
}


class SenderBackend:
    """Owns the HTTP session + all background threads. Start/stop-able."""

    def __init__(self, cfg: dict, log_fn):
        self.cfg = cfg
        self.log = log_fn
        self.session = requests.Session()
        self.stop_event = threading.Event()
        self.notif_queue: "queue.Queue[str]" = queue.Queue()
        self.threads: list[threading.Thread] = []

    @property
    def base_url(self) -> str:
        return f"http://{self.cfg['esp32_ip']}:{self.cfg['esp32_port']}"

    def _login(self, retries: int = 5, delay: float = 5.0) -> bool:
        for attempt in range(1, retries + 1):
            if self.stop_event.is_set():
                return False
            try:
                r = self.session.post(
                    f"{self.base_url}/login",
                    data={"user": self.cfg["sc_user"], "pass": self.cfg["sc_pass"]},
                    timeout=5,
                )
                if r.status_code == 200:
                    self.log(f"[AUTH] Login successful as '{self.cfg['sc_user']}'.")
                    return True
                elif r.status_code == 401:
                    self.log("[AUTH] Incorrect username or password.")
                    return False
                else:
                    self.log(f"[AUTH] Unexpected response: {r.status_code} — retry {attempt}/{retries}")
            except requests.exceptions.ConnectionError:
                self.log(f"[AUTH] Cannot connect to {self.cfg['esp32_ip']} (attempt {attempt}/{retries})")
            except requests.exceptions.Timeout:
                self.log(f"[AUTH] Login timeout (attempt {attempt}/{retries})")
            except Exception as e:
                self.log(f"[AUTH] Error: {e}")
            self.stop_event.wait(delay)
        self.log("[AUTH] Login failed after all attempts.")
        return False

    def _ensure_session(self, r: requests.Response) -> bool:
        if r.status_code == 401:
            self.log("[AUTH] Session expired — re-logging in...")
            return self._login(retries=3, delay=2.0)
        return True

    def _notif_sender_thread(self):
        while not self.stop_event.is_set():
            try:
                text = self.notif_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            url = f"{self.base_url}/notification"
            try:
                r = self.session.post(url, data={"text": text}, timeout=3)
                if r.status_code == 200:
                    self.log(f"[NOTIF] {text}")
                elif r.status_code == 401:
                    if self._ensure_session(r):
                        r2 = self.session.post(url, data={"text": text}, timeout=3)
                        if r2.status_code == 200:
                            self.log(f"[NOTIF] {text}")
                        else:
                            self.log(f"[NOTIF WARN] Status {r2.status_code} after re-login")
                else:
                    self.log(f"[NOTIF WARN] Status {r.status_code}")
            except requests.exceptions.ConnectionError:
                self.log(f"[NOTIF ERR] Cannot connect to {self.cfg['esp32_ip']}")
            except requests.exceptions.Timeout:
                self.log("[NOTIF ERR] Timeout")
            except Exception as e:
                self.log(f"[NOTIF ERR] {e}")
            finally:
                self.notif_queue.task_done()

    def _sanitize(self, text: str) -> str:
        out = []
        for ch in text:
            if ch in _DIACRITICS_MAP:
                out.append(_DIACRITICS_MAP[ch])
                continue
            cp = ord(ch)
            if 0x20 <= cp < 0x7F or ch == "°":
                out.append(ch)
        return "".join(out)

    def _truncate(self, text: str) -> str:
        maxlen = self.cfg["notif_max_chars"]
        if len(text) <= maxlen:
            return text
        return text[: maxlen - 3].rstrip() + "..."

    def _build_notif_text(self, app: str, title: str, body: str) -> str:
        parts = [p for p in (app, title, body) if p]
        text = " — ".join(parts) if parts else "(notification)"
        return self._truncate(self._sanitize(text))

    def enqueue_notification(self, app: str, title: str, body: str):
        self.notif_queue.put(self._build_notif_text(app, title, body))

    @staticmethod
    def _parse_notif(n):
        app_name, title_txt, body_txt = "", "", ""
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
                    body_txt = texts[1].text or ""
        except Exception:
            pass
        return app_name, title_txt, body_txt

    def _start_notification_listener(self) -> bool:
        try:
            from winrt.windows.ui.notifications.management import (
                UserNotificationListener,
                UserNotificationListenerAccessStatus,
            )
            from winrt.windows.ui.notifications import NotificationKinds
        except ImportError as e:
            self.log(f"[NOTIF] Missing WinRT module: {e}")
            return False

        async def _listen():
            listener = UserNotificationListener.current
            status = await listener.request_access_async()
            if status != UserNotificationListenerAccessStatus.ALLOWED:
                self.log("[NOTIF] Access DENIED by Windows. Enable it in Settings > Notifications.")
                return

            self.log("[NOTIF] Listener active.")
            seen_ids: set[int] = set()
            try:
                existing = list(await listener.get_notifications_async(NotificationKinds.TOAST))
                for n in existing:
                    seen_ids.add(n.id)
            except Exception as e:
                self.log(f"[NOTIF WARN] Init snapshot: {e}")

            while not self.stop_event.is_set():
                try:
                    notifs = list(await listener.get_notifications_async(NotificationKinds.TOAST))
                    for n in notifs:
                        if n.id not in seen_ids:
                            seen_ids.add(n.id)
                            app, title, body = self._parse_notif(n)
                            if title or body:
                                self.enqueue_notification(app, title, body)
                    current_ids = {n.id for n in notifs}
                    seen_ids -= (seen_ids - current_ids)
                except Exception as e:
                    self.log(f"[NOTIF ERR] Poll error: {e}")
                await asyncio.sleep(0.35)

        def _thread_run():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                loop.run_until_complete(_listen())
            except Exception as e:
                self.log(f"[NOTIF] Thread stopped: {e}")

        t = threading.Thread(target=_thread_run, daemon=True, name="NotifListener")
        t.start()
        self.threads.append(t)
        return True

    async def _fetch_np(self):
        mgr = await MediaManager.request_async()
        session = mgr.get_current_session()
        if session is None:
            return None
        props = await session.try_get_media_properties_async()
        if props is None:
            return None
        title = (props.title or "").strip()
        artist = (props.artist or "").strip()
        if not title:
            return None
        return f"{artist} - {title}" if artist else title

    def get_now_playing(self):
        if not HAVE_WINRT_MEDIA:
            return None
        try:
            return asyncio.run(self._fetch_np())
        except Exception as e:
            self.log(f"[WARN] NP: {e}")
            return None

    def send_nowplaying(self, text: str):
        url = f"{self.base_url}/nowplaying"
        try:
            r = self.session.post(url, data={"text": text}, timeout=3)
            if r.status_code == 200:
                self.log(f"[NP] {text}")
            elif r.status_code == 401:
                if self._ensure_session(r):
                    r2 = self.session.post(url, data={"text": text}, timeout=3)
                    if r2.status_code == 200:
                        self.log(f"[NP] {text}")
            else:
                self.log(f"[NP WARN] Status {r.status_code}")
        except requests.exceptions.ConnectionError:
            self.log(f"[NP ERR] Cannot connect to {self.cfg['esp32_ip']}")
        except requests.exceptions.Timeout:
            self.log("[NP ERR] Timeout")

    def _nowplaying_loop(self):
        last_sent_text = None
        last_send_time = 0
        while not self.stop_event.is_set():
            now_playing = self.get_now_playing()
            now = time.time()
            if now_playing:
                if now_playing != last_sent_text or (now - last_send_time) >= self.cfg["send_interval"]:
                    self.send_nowplaying(now_playing)
                    last_sent_text = now_playing
                    last_send_time = now
            else:
                last_sent_text = None
            self.stop_event.wait(self.cfg["poll_interval"])

    def send_ets2_speed(self, speed_kmh: int):
        url = f"{self.base_url}/ets2speed"
        try:
            r = self.session.post(url, data={"speed": speed_kmh}, timeout=2)
            if r.status_code == 401:
                if self._ensure_session(r):
                    self.session.post(url, data={"speed": speed_kmh}, timeout=2)
        except requests.exceptions.ConnectionError:
            self.log(f"[ETS2 ERR] Cannot connect to {self.cfg['esp32_ip']}")
        except requests.exceptions.Timeout:
            self.log("[ETS2 ERR] Timeout")
        except Exception as e:
            self.log(f"[ETS2 ERR] {e}")

    @staticmethod
    def _is_ets2_running() -> bool:
        try:
            import psutil
            for p in psutil.process_iter(["name"]):
                if (p.info.get("name") or "").lower() == "eurotrucks2.exe":
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

    def _ets2_loop(self):
        try:
            import truck_telemetry
        except ImportError:
            self.log("[ETS2] 'truck_telemetry' not installed — ETS2 disabled.")
            return

        connected = False
        last_speed_sent = None
        while not self.stop_event.is_set():
            if not self._is_ets2_running():
                if connected:
                    self.log("[ETS2] eurotrucks2.exe closed — disconnected.")
                connected = False
                last_speed_sent = None
                self.stop_event.wait(2)
                continue
            if not connected:
                try:
                    truck_telemetry.init()
                    connected = True
                    self.log("[ETS2] Connected to telemetry.")
                except Exception:
                    self.stop_event.wait(2)
                    continue
            try:
                data = truck_telemetry.get_data()
                speed_kmh = int(round(abs(data.get("speed", 0.0)) * 3.6))
                self.send_ets2_speed(speed_kmh)
                last_speed_sent = speed_kmh
            except Exception as e:
                self.log(f"[ETS2] Telemetry unavailable ({e}).")
                connected = False
                last_speed_sent = None
            self.stop_event.wait(self.cfg["ets2_poll_interval"])

    def start(self):
        self.stop_event.clear()

        t_notif_sender = threading.Thread(target=self._notif_sender_thread, daemon=True, name="NotifSender")
        t_notif_sender.start()
        self.threads.append(t_notif_sender)

        def _login_then_go():
            self._login(retries=5, delay=5.0)
            if not self._start_notification_listener():
                self.log("[NOTIF] Notifications will NOT be sent.")

            t_ets2 = threading.Thread(target=self._ets2_loop, daemon=True, name="Ets2Telemetry")
            t_ets2.start()
            self.threads.append(t_ets2)

            t_np = threading.Thread(target=self._nowplaying_loop, daemon=True, name="NowPlaying")
            t_np.start()
            self.threads.append(t_np)

        t_login = threading.Thread(target=_login_then_go, daemon=True, name="LoginBootstrap")
        t_login.start()
        self.threads.append(t_login)

    def stop(self):
        self.stop_event.set()


#Color-lerp helper (used by the animated switch) 
def _lerp_color(c1: str, c2: str, t: float) -> str:
    c1 = c1.lstrip("#")
    c2 = c2.lstrip("#")
    r1, g1, b1 = int(c1[0:2], 16), int(c1[2:4], 16), int(c1[4:6], 16)
    r2, g2, b2 = int(c2[0:2], 16), int(c2[2:4], 16), int(c2[4:6], 16)
    r = round(r1 + (r2 - r1) * t)
    g = round(g1 + (g2 - g1) * t)
    b = round(b1 + (b2 - b1) * t)
    return f"#{r:02x}{g:02x}{b:02x}"


def _ease_out(t: float) -> float:
    return 1 - (1 - t) ** 3


# Hand-drawn icons (PIL), no external image files 

def _draw_icon(kind: str, color, size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w = max(1, round(size * 0.09))

    if kind == "play":  # Run && Save
        d.polygon(
            [(size * 0.30, size * 0.18), (size * 0.30, size * 0.82), (size * 0.84, size * 0.5)],
            fill=color,
        )
    elif kind == "stop":  # Stop
        pad = size * 0.22
        d.rounded_rectangle([pad, pad, size - pad, size - pad], radius=size * 0.12, fill=color)
    elif kind == "minimize":  # Minimize now
        d.rounded_rectangle(
            [size * 0.18, size * 0.46, size * 0.82, size * 0.46 + w],
            radius=w / 2, fill=color,
        )
    elif kind == "globe":  # language selector
        pad = size * 0.12
        d.ellipse([pad, pad, size - pad, size - pad], outline=color, width=w)
        d.ellipse([size * 0.36, pad, size * 0.64, size - pad], outline=color, width=max(1, round(w * 0.75)))
        d.line([pad, size / 2, size - pad, size / 2], fill=color, width=max(1, round(w * 0.75)))
    elif kind == "moon":  # dark-mode toggle
        r = size * 0.36
        cx, cy = size * 0.54, size * 0.5
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=color)
        r2 = size * 0.30
        cx2, cy2 = cx - size * 0.20, cy - size * 0.14
        d.ellipse([cx2 - r2, cy2 - r2, cx2 + r2, cy2 + r2], fill=(0, 0, 0, 0))
    elif kind == "check":  # selection mark in the language list
        d.line(
            [(size * 0.20, size * 0.52), (size * 0.42, size * 0.76), (size * 0.82, size * 0.26)],
            fill=color, width=w, joint="curve",
        )
    return img


def _make_icon(kind: str, color, size: int = 22) -> ctk.CTkImage:
    # draw at 4x and downsample for antialiasing
    supersample = 4
    big = _draw_icon(kind, color, size * supersample)
    img = big.resize((size, size), Image.LANCZOS)
    return ctk.CTkImage(light_image=img, dark_image=img, size=(size, size))


def _hex_to_rgba(hex_color: str, alpha: int) -> tuple:
    h = hex_color.lstrip("#")
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    return (r, g, b, alpha)


def _draw_app_logo(size: int) -> Image.Image:
    """Glowing orb with 8 orbiting dots (nod to the 'Octo' in the name)."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx = cy = size / 2
    primary = MD3_LIGHT["primary"]
    glow = MD3_LIGHT["primary_container"]

    # soft outer glow: translucent rings, largest/faintest first
    for radius_f, alpha in ((0.50, 35), (0.42, 65), (0.36, 100)):
        r = size * radius_f
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=_hex_to_rgba(glow, alpha))

    # 8 small satellite dots orbiting the core
    orbit_r = size * 0.40
    dot_r = size * 0.045
    for i in range(8):
        angle = (2 * math.pi / 8) * i - math.pi / 2
        dx = cx + orbit_r * math.cos(angle)
        dy = cy + orbit_r * math.sin(angle)
        d.ellipse([dx - dot_r, dy - dot_r, dx + dot_r, dy + dot_r], fill=primary)

    # solid glowing core
    r_core = size * 0.28
    d.ellipse([cx - r_core, cy - r_core, cx + r_core, cy + r_core], fill=primary)

    # small bright highlight so the core reads as "glowing", not flat
    r_hl = size * 0.09
    hx, hy = cx - r_core * 0.4, cy - r_core * 0.4
    d.ellipse([hx - r_hl, hy - r_hl, hx + r_hl, hy + r_hl], fill="#FFFFFF")

    return img


def _make_app_logo(size: int) -> Image.Image:
    supersample = 4
    big = _draw_app_logo(size * supersample)
    return big.resize((size, size), Image.LANCZOS)


def _prepare_app_icon_file() -> "str | None":
    """Renders the app logo to a .ico (multi-size) in the config folder so
    it can be used as the native Windows title-bar/taskbar icon."""
    try:
        path = os.path.join(_config_dir(), "app_icon.ico")
        _make_app_logo(256).save(
            path, format="ICO",
            sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
        )
        return path
    except Exception as e:
        print(f"[UI] Could not create app icon file: {e}")
        return None



class MD3Switch(ctk.CTkLabel):
    """MD3-style switch rendered as a supersampled Pillow image for smooth edges."""
    WIDTH = 52
    HEIGHT = 32
    THUMB_R_OFF = 8
    THUMB_R_ON = 12
    PAD = 4
    BORDER_W = 2       # ring thickness shown only in the "off" state
    SS = 4             # supersampling factor for anti-aliasing

    def __init__(self, parent, variable: tk.BooleanVar, bg_color: str, command=None):
        super().__init__(parent, text="", image=None, fg_color=bg_color,
                          width=self.WIDTH, height=self.HEIGHT, cursor="hand2")
        self.variable = variable
        self.command = command
        self._anim_job = None
        self._img_ref = None  # keep a reference so it isn't garbage-collected

        self.bind("<Button-1>", self._on_click)
        t0 = 1.0 if self.variable.get() else 0.0
        self._render(t0, self.THUMB_R_ON if self.variable.get() else self.THUMB_R_OFF)

    def _thumb_x(self, t: float, r: float) -> float:
        left = self.PAD + r
        right = self.WIDTH - self.PAD - r
        return left + (right - left) * t

    def _render(self, t: float, thumb_r: float):
        s = self.SS
        W, H = self.WIDTH * s, self.HEIGHT * s
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)

        track_color = _lerp_color(MD3["surface_variant"], MD3["primary"], t)
        # at t=1 the border matches the track, so the ring disappears
        border_color = _lerp_color(MD3["outline"], MD3["primary"], t)
        thumb_color = _lerp_color(MD3["outline"], MD3["on_primary"], t)

        d.rounded_rectangle([0, 0, W - 1, H - 1], radius=H // 2, fill=border_color)
        b = self.BORDER_W * s
        d.rounded_rectangle([b, b, W - 1 - b, H - 1 - b], radius=(H - 2 * b) // 2, fill=track_color)

        # Keep the thumb fully inside the pill at all times, even mid-bulge.
        max_r = self.HEIGHT / 2 - self.BORDER_W - 1
        thumb_r = min(thumb_r, max_r)

        cx, cy, rr = self._thumb_x(t, thumb_r) * s, H / 2, thumb_r * s
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=thumb_color)

        img = img.resize((self.WIDTH, self.HEIGHT), Image.LANCZOS)
        ctk_img = ctk.CTkImage(light_image=img, size=(self.WIDTH, self.HEIGHT))
        self._img_ref = ctk_img
        self.configure(image=ctk_img)

    def _on_click(self, _event=None):
        new_state = not self.variable.get()
        self.variable.set(new_state)
        self._animate(new_state)
        if self.command:
            self.command()

    def _animate(self, turning_on: bool, steps: int = 10, delay_ms: int = 16):
        if self._anim_job is not None:
            self.after_cancel(self._anim_job)
            self._anim_job = None

        start_t = 0.0 if turning_on else 1.0
        end_t = 1.0 if turning_on else 0.0

        def step(i=0):
            progress = _ease_out(i / steps)
            t = start_t + (end_t - start_t) * progress
            bulge = 1.0 + 0.35 * (1 - abs(progress - 0.5) * 2) if 0 < progress < 1 else 1.0
            base_r = self.THUMB_R_OFF + (self.THUMB_R_ON - self.THUMB_R_OFF) * progress if turning_on \
                else self.THUMB_R_ON - (self.THUMB_R_ON - self.THUMB_R_OFF) * progress
            self._render(t, base_r * bulge)
            if i < steps:
                self._anim_job = self.after(delay_ms, lambda: step(i + 1))
            else:
                self._anim_job = None

        step(0)

    def set(self, value: bool, animate: bool = False):
        self.variable.set(value)
        if animate:
            self._animate(value)
        else:
            t = 1.0 if value else 0.0
            self._render(t, self.THUMB_R_ON if value else self.THUMB_R_OFF)

    def unbind(self, sequence=None, funcid=None):
        # used to disable interaction (e.g. tray switch when pystray/pillow missing)
        super().unbind(sequence, funcid)




class App:
    def __init__(self, root: ctk.CTk):
        self.root = root
        self.cfg = load_config()
        self.strings = LANG[self.cfg.get("language", "ro")]
        self.backend: SenderBackend | None = None
        self.log_queue: "queue.Queue[str]" = queue.Queue()
        self.tray_icon = None
        self.lang_popup = None
        self._log_line_n = 0

        _set_theme("dark" if self.cfg.get("dark_mode") else "light")

        root.title(self.strings["app_title"])
        root.geometry("520x640")
        root.minsize(420, 480)
        root.configure(fg_color=MD3["surface"])
        root.protocol("WM_DELETE_WINDOW", self.on_close)
        self._apply_app_icon(root)
        if HAVE_PYWINSTYLES:
            try:
                pywinstyles.change_header_color(root, MD3["primary"])
                pywinstyles.change_title_color(root, MD3["on_primary"])
            except Exception as e:
                print(f"[UI] pywinstyles could not set title bar color: {e}")

        self._build_ui()
        self.root.after(200, self._drain_log_queue)

        if self.cfg["sc_user"] and self.cfg["sc_pass"]:
            self.start_backend()

    # app icon 
    def _apply_app_icon(self, window):
        """Sets the Octoglow logo as the window/taskbar icon. Keeps a
        reference on the window itself so the PhotoImage isn't garbage-collected."""
        icon_path = _prepare_app_icon_file()
        if icon_path and sys.platform.startswith("win"):
            try:
                window.iconbitmap(default=icon_path)
            except Exception as e:
                print(f"[UI] Could not set .ico window icon: {e}")
        try:
            window._icon_photo_ref = ImageTk.PhotoImage(_make_app_logo(64))
            window.iconphoto(True, window._icon_photo_ref)
        except Exception as e:
            print(f"[UI] Could not set window icon: {e}")
        if icon_path and sys.platform.startswith("win"):
            window.after(250, lambda: self._reapply_icon(window, icon_path))

    def _reapply_icon(self, window, icon_path):
        try:
            if window.winfo_exists():
                window.iconbitmap(default=icon_path)
        except Exception:
            pass
    def _section_label(self, parent, text):
        return ctk.CTkLabel(
            parent, text=text, anchor="w",
            font=ctk.CTkFont(size=12, weight="bold"),
            text_color=MD3["primary"],
        )

    def _md3_entry(self, parent, textvariable, show=None, width=200):
        return ctk.CTkEntry(
            parent, textvariable=textvariable, show=show, width=width,
            corner_radius=14, height=38,
            fg_color=MD3["surface_variant"], border_width=0,
            text_color=MD3["on_surface"],
        )

    def _flash_then(self, button, base_color, flash_color, action):
        """Tiny press-flash animation before running the real action."""
        button.configure(fg_color=flash_color)
        def restore():
            button.configure(fg_color=base_color)
            if action:
                action()
        self.root.after(90, restore)

    def _md3_filled_button(self, parent, text, command, icon=None):
        btn = ctk.CTkButton(
            parent, text=text, command=None,
            corner_radius=20, height=40,
            fg_color=MD3["primary"], hover_color=MD3["on_primary_container"],
            text_color=MD3["on_primary"],
            font=ctk.CTkFont(size=13, weight="bold"),
            image=_make_icon(icon, MD3["on_primary"]) if icon else None,
            compound="left",
        )
        btn.configure(command=lambda: self._flash_then(btn, MD3["primary"], MD3["on_primary_container"], command))
        return btn

    def _md3_tonal_button(self, parent, text, command, icon=None):
        btn = ctk.CTkButton(
            parent, text=text, command=None,
            corner_radius=20, height=40,
            fg_color=MD3["secondary_container"], hover_color=MD3["outline_variant"],
            text_color=MD3["on_surface"],
            font=ctk.CTkFont(size=13, weight="bold"),
            image=_make_icon(icon, MD3["on_surface"]) if icon else None,
            compound="left",
        )
        btn.configure(command=lambda: self._flash_then(btn, MD3["secondary_container"], MD3["outline_variant"], command))
        return btn

    def _build_ui(self):
        s = self.strings

        # Top "app bar"
        self.appbar = ctk.CTkFrame(self.root, fg_color=MD3["primary"], corner_radius=0, height=64)
        self.appbar.pack(fill="x")
        self.appbar.pack_propagate(False)
        self.lbl_title = ctk.CTkLabel(
            self.appbar, text=s["app_title"], text_color=MD3["on_primary"],
            font=ctk.CTkFont(size=20, weight="bold"),
        )
        self.lbl_title.pack(side="left", padx=20)

        self.lang_button = ctk.CTkButton(
            self.appbar, text="", command=self.open_language_popup,
            image=_make_icon("globe", MD3["on_primary"], size=20),
            width=36, height=36, corner_radius=18,
            fg_color=MD3["on_primary_container"], hover_color=MD3["primary_container"],
        )
        self.lang_button.pack(side="right", padx=(0, 20))

        self.theme_button = ctk.CTkButton(
            self.appbar, text="", command=self.toggle_dark_mode,
            image=_make_icon("moon", MD3["on_primary"], size=20),
            width=36, height=36, corner_radius=18,
            fg_color=MD3["on_primary_container"], hover_color=MD3["primary_container"],
        )
        self.theme_button.pack(side="right", padx=(0, 8))

        body = ctk.CTkScrollableFrame(
            self.root, fg_color=MD3["surface"],
            corner_radius=0, border_width=0,
        )
        body.pack(fill="both", expand=True)

        # Card: connection settings
        card1 = ctk.CTkFrame(body, fg_color=MD3["surface_container"], corner_radius=20)
        card1.pack(fill="x", padx=(16, 8), pady=(16, 16))
        inner1 = ctk.CTkFrame(card1, fg_color="transparent")
        inner1.pack(fill="x", padx=20, pady=16)

        self.lbl_section_connection = self._section_label(inner1, s["section_connection"])
        self.lbl_section_connection.grid(row=0, column=0, columnspan=4, sticky="w", pady=(0, 10))

        self.lbl_ip = ctk.CTkLabel(inner1, text=s["label_ip"], text_color=MD3["on_surface_variant"])
        self.lbl_ip.grid(row=1, column=0, sticky="w")
        self.ip_var = ctk.StringVar(value=self.cfg["esp32_ip"])
        self._md3_entry(inner1, self.ip_var, width=180).grid(row=2, column=0, sticky="w", pady=(2, 12))

        self.lbl_port = ctk.CTkLabel(inner1, text=s["label_port"], text_color=MD3["on_surface_variant"])
        self.lbl_port.grid(row=1, column=1, sticky="w", padx=(12, 0))
        self.port_var = ctk.StringVar(value=str(self.cfg["esp32_port"]))
        self._md3_entry(inner1, self.port_var, width=80).grid(row=2, column=1, sticky="w", padx=(12, 0), pady=(2, 12))

        self.lbl_user = ctk.CTkLabel(inner1, text=s["label_user"], text_color=MD3["on_surface_variant"])
        self.lbl_user.grid(row=3, column=0, sticky="w")
        self.user_var = ctk.StringVar(value=self.cfg["sc_user"])
        self._md3_entry(inner1, self.user_var, width=280).grid(row=4, column=0, columnspan=2, sticky="w", pady=(2, 12))

        self.lbl_pass = ctk.CTkLabel(inner1, text=s["label_pass"], text_color=MD3["on_surface_variant"])
        self.lbl_pass.grid(row=5, column=0, sticky="w")
        self.pass_var = ctk.StringVar(value=self.cfg["sc_pass"])
        self.pass_entry = self._md3_entry(inner1, self.pass_var, show="*", width=220)
        self.pass_entry.grid(row=6, column=0, sticky="w", pady=(2, 0))

        pass_row = ctk.CTkFrame(inner1, fg_color="transparent")
        pass_row.grid(row=6, column=1, sticky="w", padx=(12, 0))
        self.show_pass_var = tk.BooleanVar(value=False)
        self.show_pass_switch = MD3Switch(
            pass_row, self.show_pass_var, bg_color=MD3["surface_container"],
            command=self._toggle_pass_visibility,
        )
        self.show_pass_switch.pack(side="left")
        self.lbl_show_pass = ctk.CTkLabel(pass_row, text=s["show_pass"], text_color=MD3["on_surface_variant"], font=ctk.CTkFont(size=11))
        self.lbl_show_pass.pack(side="left", padx=(6, 0))

        # Card: toggles
        card2 = ctk.CTkFrame(body, fg_color=MD3["surface_container"], corner_radius=20)
        card2.pack(fill="x", padx=(16, 8), pady=(0, 16))
        inner2 = ctk.CTkFrame(card2, fg_color="transparent")
        inner2.pack(fill="x", padx=20, pady=16)

        self.lbl_section_options = self._section_label(inner2, s["section_options"])
        self.lbl_section_options.pack(anchor="w", pady=(0, 10))

        row_a = ctk.CTkFrame(inner2, fg_color="transparent")
        row_a.pack(fill="x", pady=4)
        self.lbl_opt_startup = ctk.CTkLabel(row_a, text=s["opt_startup"], text_color=MD3["on_surface"])
        self.lbl_opt_startup.pack(side="left")
        self.startup_var = tk.BooleanVar(value=self.cfg["run_at_startup"])
        MD3Switch(row_a, self.startup_var, bg_color=MD3["surface_container"]).pack(side="right")

        row_b = ctk.CTkFrame(inner2, fg_color="transparent")
        row_b.pack(fill="x", pady=4)
        self.lbl_opt_tray = ctk.CTkLabel(row_b, text=s["opt_tray"], text_color=MD3["on_surface"])
        self.lbl_opt_tray.pack(side="left")
        self.tray_var = tk.BooleanVar(value=self.cfg["minimize_to_tray"])
        self.tray_switch = MD3Switch(row_b, self.tray_var, bg_color=MD3["surface_container"])
        self.tray_switch.pack(side="right")

        self.lbl_tray_unavailable = None
        if not HAVE_TRAY:
            self.tray_switch.unbind("<Button-1>")
            self.lbl_tray_unavailable = ctk.CTkLabel(
                inner2, text=s["opt_tray_unavailable"],
                text_color=MD3["error"], font=ctk.CTkFont(size=11),
            )
            self.lbl_tray_unavailable.pack(anchor="w", pady=(2, 0))

        # Buttons row
        btns = ctk.CTkFrame(body, fg_color="transparent")
        btns.pack(fill="x", padx=(16, 8), pady=(0, 16))
        self.btn_save_start = self._md3_filled_button(btns, s["btn_save_start"], self.on_save_start, icon="play")
        self.btn_save_start.pack(side="left")
        self.stop_btn = self._md3_tonal_button(btns, s["btn_stop"], self.on_stop, icon="stop")
        self.stop_btn.pack(side="left", padx=8)
        self.stop_btn.configure(state="normal" if self.backend is not None else "disabled")
        self.btn_minimize = self._md3_tonal_button(btns, s["btn_minimize"], self.hide_to_tray, icon="minimize")
        self.btn_minimize.pack(side="left")

        # Card: log
        card3 = ctk.CTkFrame(body, fg_color=MD3["surface_container_high"], corner_radius=20)
        card3.pack(fill="both", expand=True, padx=(16, 8), pady=(0, 16))
        inner3 = ctk.CTkFrame(card3, fg_color="transparent")
        inner3.pack(fill="both", expand=True, padx=20, pady=16)
        self.lbl_section_log = self._section_label(inner3, s["section_log"])
        self.lbl_section_log.pack(anchor="w", pady=(0, 10))
        self.log_text = ctk.CTkTextbox(
            inner3, height=200, corner_radius=14,
            fg_color=MD3["surface"], text_color=MD3["on_surface_variant"],
            font=ctk.CTkFont(family="Consolas", size=12),
            wrap="char",
        )
        self.log_text.pack(fill="both", expand=True)
        self.log_text.configure(state="disabled")
        self.log_text._textbox.tag_configure("flash", background=MD3["surface"])

    def _toggle_pass_visibility(self):
        self.pass_entry.configure(show="" if self.show_pass_var.get() else "*")

    # language
    def open_language_popup(self):
        if self.lang_popup is not None and self.lang_popup.winfo_exists():
            self.lang_popup.lift()
            self.lang_popup.focus_force()
            return

        s = self.strings
        current_lang = self.cfg.get("language", "ro")
        options = [("ro", s["lang_popup_ro"]), ("en", s["lang_popup_en"])]

        width = 260
        height = 88 + 48 * len(options)

        popup = ctk.CTkToplevel(self.root)
        popup.title(s["lang_popup_title"])
        popup.configure(fg_color=MD3["surface"])
        popup.resizable(False, False)
        popup.transient(self.root)
        popup.geometry(f"{width}x{height}")
        self._apply_app_icon(popup)

        # centre over the main window
        self.root.update_idletasks()
        x = self.root.winfo_x() + (self.root.winfo_width() - width) // 2
        y = self.root.winfo_y() + (self.root.winfo_height() - height) // 2
        popup.geometry(f"+{x}+{y}")

        title_lbl = ctk.CTkLabel(
            popup, text=s["lang_popup_title"], text_color=MD3["on_surface"],
            font=ctk.CTkFont(size=15, weight="bold"), anchor="w",
        )
        title_lbl.pack(fill="x", padx=20, pady=(18, 10))

        list_card = ctk.CTkFrame(popup, fg_color=MD3["surface_container"], corner_radius=16)
        list_card.pack(fill="both", expand=True, padx=16, pady=(0, 16))

        for code, label in options:
            self._add_list_row(list_card, label, code == current_lang, lambda c=code: self._select_language(c, popup))

        popup.after(10, popup.grab_set)
        popup.protocol("WM_DELETE_WINDOW", lambda: self._close_language_popup(popup))
        self.lang_popup = popup

    def _add_list_row(self, parent, text, is_selected, on_select):
        """One clickable Material-3-style list item (used by the language popup)."""
        row = ctk.CTkFrame(parent, fg_color="transparent", corner_radius=12, height=44)
        row.pack(fill="x", padx=6, pady=4)
        row.pack_propagate(False)

        lbl = ctk.CTkLabel(
            row, text=text, text_color=MD3["on_surface"], anchor="w",
            font=ctk.CTkFont(size=13, weight="bold" if is_selected else "normal"),
        )
        lbl.pack(side="left", fill="both", expand=True, padx=(14, 0))

        widgets = [row, lbl]
        if is_selected:
            check_lbl = ctk.CTkLabel(row, text="", image=_make_icon("check", MD3["primary"], size=16))
            check_lbl.pack(side="right", padx=(0, 14))
            widgets.append(check_lbl)

        def on_enter(_e=None):
            row.configure(fg_color=MD3["surface_container_high"])

        def on_leave(_e=None):
            row.configure(fg_color="transparent")

        for w in widgets:
            w.bind("<Enter>", on_enter)
            w.bind("<Leave>", on_leave)
            w.bind("<Button-1>", lambda _e=None: on_select())
            w.configure(cursor="hand2")

    def _close_language_popup(self, popup):
        if popup is not None and popup.winfo_exists():
            popup.grab_release()
            popup.destroy()
        self.lang_popup = None

    def _select_language(self, lang, popup):
        self.on_language_change(lang)
        self._close_language_popup(popup)

    def on_language_change(self, lang: str):
        self.cfg["language"] = lang
        save_config(self.cfg)
        self.strings = LANG[lang]
        self.retranslate()
    def toggle_dark_mode(self):
        self.cfg = self._collect_cfg_from_ui()
        self.cfg["dark_mode"] = not self.cfg.get("dark_mode", False)
        save_config(self.cfg)

        def rebuild_with_new_theme():
            _set_theme("dark" if self.cfg["dark_mode"] else "light")
            for child in self.root.winfo_children():
                child.destroy()
            self.root.configure(fg_color=MD3["surface"])
            if HAVE_PYWINSTYLES:
                try:
                    pywinstyles.change_header_color(self.root, MD3["primary"])
                    pywinstyles.change_title_color(self.root, MD3["on_primary"])
                except Exception as e:
                    print(f"[UI] pywinstyles could not set title bar color: {e}")
            self._build_ui()

        self._fade_theme_switch(rebuild_with_new_theme)

    def _fade_theme_switch(self, mid_action, steps=8, delay_ms=12, dip_to=0.35):
        """Quick dim-out -> swap theme -> dim-in, so the light/dark switch
        reads as a soft cross-fade instead of an abrupt flash."""
        def fade(i, start, end, on_done):
            t = i / steps
            try:
                self.root.attributes("-alpha", start + (end - start) * t)
            except tk.TclError:
                on_done()
                return
            if i < steps:
                self.root.after(delay_ms, lambda: fade(i + 1, start, end, on_done))
            else:
                on_done()

        def do_rebuild_then_fade_in():
            mid_action()
            fade(0, dip_to, 1.0, lambda: None)

        fade(0, 1.0, dip_to, do_rebuild_then_fade_in)

    def retranslate(self):
        s = self.strings
        self.root.title(s["app_title"])
        self.lbl_title.configure(text=s["app_title"])
        self.lbl_section_connection.configure(text=s["section_connection"])
        self.lbl_ip.configure(text=s["label_ip"])
        self.lbl_port.configure(text=s["label_port"])
        self.lbl_user.configure(text=s["label_user"])
        self.lbl_pass.configure(text=s["label_pass"])
        self.lbl_show_pass.configure(text=s["show_pass"])
        self.lbl_section_options.configure(text=s["section_options"])
        self.lbl_opt_startup.configure(text=s["opt_startup"])
        self.lbl_opt_tray.configure(text=s["opt_tray"])
        if self.lbl_tray_unavailable is not None:
            self.lbl_tray_unavailable.configure(text=s["opt_tray_unavailable"])
        self.btn_save_start.configure(text=s["btn_save_start"])
        self.stop_btn.configure(text=s["btn_stop"])
        self.btn_minimize.configure(text=s["btn_minimize"])
        self.lbl_section_log.configure(text=s["section_log"])
        if self.tray_icon is not None:
            self.tray_icon.stop()
            self.tray_icon = None
    def log(self, msg: str):
        self.log_queue.put(msg)

    def _drain_log_queue(self):
        try:
            while True:
                msg = self.log_queue.get_nowait()
                self._insert_log_line(msg)
        except queue.Empty:
            pass
        self.root.after(200, self._drain_log_queue)

    def _insert_log_line(self, msg: str):
        tb = self.log_text
        tb.configure(state="normal")
        tag = f"flash{self._log_line_n}"
        self._log_line_n += 1
        start = tb.index("end-1c")
        tb.insert("end", msg + "\n")
        end = tb.index("end-1c")
        tb._textbox.tag_add(tag, start, end)
        tb._textbox.tag_configure(tag, background=MD3["primary_container"])
        tb.see("end")
        tb.configure(state="disabled")
        self._fade_log_tag(tag, 0)

    def _fade_log_tag(self, tag, step, steps=10):
        t = step / steps
        color = _lerp_color(MD3["primary_container"], MD3["surface"], t)
        try:
            self.log_text._textbox.tag_configure(tag, background=color)
        except tk.TclError:
            return
        if step < steps:
            self.root.after(60, lambda: self._fade_log_tag(tag, step + 1, steps))
    def _collect_cfg_from_ui(self) -> dict:
        cfg = dict(self.cfg)
        cfg["esp32_ip"] = self.ip_var.get().strip()
        try:
            cfg["esp32_port"] = int(self.port_var.get().strip())
        except ValueError:
            cfg["esp32_port"] = DEFAULT_CONFIG["esp32_port"]
        cfg["sc_user"] = self.user_var.get().strip()
        cfg["sc_pass"] = self.pass_var.get()
        cfg["run_at_startup"] = self.startup_var.get()
        cfg["minimize_to_tray"] = self.tray_var.get()
        return cfg

    def on_save_start(self):
        self.cfg = self._collect_cfg_from_ui()
        save_config(self.cfg)
        set_run_at_startup(self.cfg["run_at_startup"])
        self.start_backend()

    def start_backend(self):
        if self.backend is not None:
            self.backend.stop()
        self.log(f"[APP] {self.strings['log_starting']} http://{self.cfg['esp32_ip']}:{self.cfg['esp32_port']}")
        self.backend = SenderBackend(self.cfg, self.log)
        self.backend.start()
        self.stop_btn.configure(state="normal")

    def on_stop(self):
        if self.backend is not None:
            self.backend.stop()
            self.log(f"[APP] {self.strings['log_stopped']}")
        self.stop_btn.configure(state="disabled")

    def _make_tray_image(self):
        return _make_app_logo(64)

    def hide_to_tray(self):
        if not HAVE_TRAY:
            messagebox.showinfo(self.strings["msgbox_tray_title"], self.strings["msgbox_tray_body"])
            return
        self.root.withdraw()
        if self.tray_icon is None:
            s = self.strings
            menu = pystray.Menu(
                pystray.MenuItem(s["tray_show"], self._tray_show),
                pystray.MenuItem(s["tray_exit"], self._tray_exit),
            )
            self.tray_icon = pystray.Icon("OctoglowSender", self._make_tray_image(), "Octoglow Sender", menu)
            threading.Thread(target=self.tray_icon.run, daemon=True).start()

    def _tray_show(self, icon=None, item=None):
        self.root.after(0, self.root.deiconify)

    def _tray_exit(self, icon=None, item=None):
        if self.backend is not None:
            self.backend.stop()
        if self.tray_icon is not None:
            self.tray_icon.stop()
        self.root.after(0, self.root.destroy)

    def on_close(self):
        if self.tray_var.get() and HAVE_TRAY:
            self.hide_to_tray()
        else:
            if self.backend is not None:
                self.backend.stop()
            self.root.destroy()


def main():
    root = ctk.CTk()
    app = App(root)
    try:
        root.mainloop()
    except KeyboardInterrupt:
        if app.backend is not None:
            app.backend.stop()
        if app.tray_icon is not None:
            app.tray_icon.stop()
        try:
            root.destroy()
        except Exception:
            pass


if __name__ == "__main__":
    main()
