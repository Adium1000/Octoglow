using System.Diagnostics;
using System.Net;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using Windows.Media.Control;
using Windows.UI.Notifications;
using Windows.UI.Notifications.Management;
namespace OctoglowSender;
public class SenderBackend : IDisposable
{
    private readonly AppConfig _cfg;
    private readonly Action<string> _log;
    private readonly HttpClient _http;
    private CancellationTokenSource? _cts;
    private readonly List<Task> _tasks = new();
    private string? _sessionCookie;
    private Ets2TelemetryService? _ets2;
    private static readonly Dictionary<char, char> DiacriticsMap = new()
    {
        ['ă'] = 'a', ['â'] = 'a', ['Ă'] = 'A', ['Â'] = 'A',
        ['ș'] = 's', ['ş'] = 's', ['Ș'] = 'S', ['Ş'] = 'S',
        ['ț'] = 't', ['ţ'] = 't', ['Ț'] = 'T', ['Ţ'] = 'T',
        ['î'] = 'i', ['Î'] = 'I',
        ['é'] = 'e', ['è'] = 'e', ['ê'] = 'e', ['É'] = 'E',
        ['ñ'] = 'n', ['Ñ'] = 'N', ['ç'] = 'c', ['Ç'] = 'C',
    };
    public SenderBackend(AppConfig cfg, Action<string> log)
    {
        _cfg = cfg;
        _log = log;
        _http = new HttpClient { Timeout = TimeSpan.FromSeconds(5) };
    }
    private string BaseUrl => $"http://{_cfg.Esp32Ip}:{_cfg.Esp32Port}";
    public void Start()
    {
        ScreenMirror.Reset();
        _cts = new CancellationTokenSource();
        var token = _cts.Token;

        _tasks.Add(Task.Run(() => LoginThenRunAsync(token), token));
    }
    public void Stop()
    {
        var cancellation = Interlocked.Exchange(ref _cts, null);
        cancellation?.Cancel();
        ScreenMirror.Reset();
    }
    public void Dispose() => Stop();
    private async Task LoginThenRunAsync(CancellationToken token)
    {
        var ok = await LoginAsync(token, retries: 5, delaySeconds: 5);
        if (!ok)
        {
            _log("[AUTH] Login eșuat după toate încercările.");
            return;
        }
        var notifTask = Task.Run(() => RunNotificationListenerAsync(token), token);
        var npTask = Task.Run(() => NowPlayingLoopAsync(token), token);
        var ets2Task = Task.Run(() => Ets2LoopAsync(token), token);
        var screenTask = Task.Run(() => ScreenLoopAsync(token), token);
        _tasks.Add(notifTask);
        _tasks.Add(npTask);
        _tasks.Add(ets2Task);
        _tasks.Add(screenTask);
        try { await Task.WhenAll(notifTask, npTask, ets2Task, screenTask); }
        catch (OperationCanceledException) { /* normal on Stop() */ }
    }
    private async Task<bool> LoginAsync(CancellationToken token, int retries, double delaySeconds)
    {
        for (int attempt = 1; attempt <= retries; attempt++)
        {
            if (token.IsCancellationRequested) return false;
            try
            {
                var content = new FormUrlEncodedContent(new Dictionary<string, string>
                {
                    ["user"] = _cfg.ScUser,
                    ["pass"] = _cfg.ScPass,
                });
                var resp = await _http.PostAsync($"{BaseUrl}/login", content, token);
                if (resp.IsSuccessStatusCode)
                {
                    if (resp.Headers.TryGetValues("Set-Cookie", out var cookies))
                        // Set-Cookie also contains attributes such as Path and HttpOnly.
                        // Only the name=value pair is valid in a subsequent Cookie header.
                        _sessionCookie = cookies.FirstOrDefault()?.Split(';', 2)[0];
                    _log($"[AUTH] Autentificat cu succes ca '{_cfg.ScUser}'.");
                    return true;
                }
                if ((int)resp.StatusCode == 401)
                {
                    _log("[AUTH] Utilizator sau parolă incorectă.");
                    return false;
                }
                _log($"[AUTH] Răspuns neașteptat: {(int)resp.StatusCode} - reîncerc {attempt}/{retries}");
            }
            catch (HttpRequestException)
            {
                _log($"[AUTH] Nu mă pot conecta la {_cfg.Esp32Ip} (încercarea {attempt}/{retries})");
            }
            catch (TaskCanceledException)
            {
                _log($"[AUTH] Timeout la login (încercarea {attempt}/{retries})");
            }
            try { await Task.Delay(TimeSpan.FromSeconds(delaySeconds), token); }
            catch (OperationCanceledException) { return false; }
        }
        return false;
    }

    // SCREEN MIRROR
    //
    // The clock streams its panel over UDP (firmware /screensub): the key comes
    // over the logged-in HTTP session, then we keep sending it to the clock from
    // our UDP socket about once a second and the clock pushes every new frame back
    // to that socket. The traffic starting on our side is what lets the frames
    // through Windows Firewall - they arrive as replies.
    //
    // Firmware without the stream (404), or a network that never delivers the UDP
    // frames, falls back to polling /getscreen, and the stream is tried again every
    // minute. A slow answer or a lost frame never clears the picture: the last frame
    // stays up, and the clock only counts as offline once nothing has arrived for
    // ScreenStaleAfter. The clock goes quiet for a moment whenever it is busy - a
    // weather fetch, a settings save - and that is not the clock going away.
    //
    // All of it runs only while a page showing the panel is open
    // (ScreenMirror.IsWatched). With nobody looking the clock is left alone, and
    // its WiFi, kept awake for the stream, goes back to sleep.
    private static readonly TimeSpan ScreenStaleAfter = TimeSpan.FromSeconds(3);
    private static readonly TimeSpan ScreenNoUdpAfter = TimeSpan.FromSeconds(4);
    private static readonly TimeSpan ScreenResubscribeAfter = TimeSpan.FromSeconds(10);
    private static readonly TimeSpan ScreenPunchEvery = TimeSpan.FromSeconds(1);
    private static readonly TimeSpan ScreenStreamRetryEvery = TimeSpan.FromMinutes(1);
    private DateTime _screenLastFrameUtc = DateTime.MinValue;
    private bool _screenUnsupportedLogged;
    private bool _screenFallbackLogged;

    private enum ScreenStreamEnd { Retry, NoStream, NoUdp }

    private async Task ScreenLoopAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            if (!ScreenMirror.IsWatched)
            {
                ScreenMirror.Reset();
                if (!await DelayAsync(TimeSpan.FromMilliseconds(250), token)) break;
                continue;
            }

            ScreenStreamEnd end;
            try
            {
                end = await RunScreenStreamAsync(token);
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                break;
            }

            if (end == ScreenStreamEnd.Retry)
            {
                if (!await DelayAsync(TimeSpan.FromSeconds(1), token)) break;
                continue;
            }

            if (end == ScreenStreamEnd.NoUdp && !_screenFallbackLogged)
            {
                _screenFallbackLogged = true;
                _log("[SCREEN] Cadrele UDP nu ajung (firewall?) - folosesc HTTP.");
            }

            await RunScreenPollingAsync(ScreenStreamRetryEvery, token);
        }
    }

    private async Task<ScreenStreamEnd> RunScreenStreamAsync(CancellationToken token)
    {
        int port;
        byte[] key;
        using (var timeout = CancellationTokenSource.CreateLinkedTokenSource(token))
        {
            timeout.CancelAfter(TimeSpan.FromSeconds(3));
            try
            {
                using var request = new HttpRequestMessage(HttpMethod.Get, $"{BaseUrl}/screensub");
                if (!string.IsNullOrWhiteSpace(_sessionCookie))
                    request.Headers.TryAddWithoutValidation("Cookie", _sessionCookie);
                using var response = await _http.SendAsync(request, timeout.Token);

                if (response.StatusCode == HttpStatusCode.Unauthorized)
                {
                    await LoginAsync(token, retries: 3, delaySeconds: 2);
                    return ScreenStreamEnd.Retry;
                }
                if (response.StatusCode == HttpStatusCode.NotFound)
                    return ScreenStreamEnd.NoStream;
                if (!response.IsSuccessStatusCode)
                {
                    MarkScreenMissed();
                    return ScreenStreamEnd.Retry;
                }

                var body = await response.Content.ReadAsStringAsync(timeout.Token);
                if (!TryParseScreenSubscription(body, out port, out key))
                    return ScreenStreamEnd.NoStream;
            }
            catch (OperationCanceledException) when (!token.IsCancellationRequested)
            {
                MarkScreenMissed();
                return ScreenStreamEnd.Retry;
            }
            catch (HttpRequestException)
            {
                MarkScreenMissed();
                return ScreenStreamEnd.Retry;
            }
        }

        var punch = new byte[8];
        "OGSK"u8.CopyTo(punch);
        key.CopyTo(punch, 4);

        using var udp = new System.Net.Sockets.UdpClient(System.Net.Sockets.AddressFamily.InterNetwork);
        try
        {
            // A connected UDP socket on Windows turns an ICMP "port unreachable"
            // into a reset on the next receive; with this off it is just ignored.
            const int SioUdpConnReset = -1744830452;
            udp.Client.IOControl((System.Net.Sockets.IOControlCode)SioUdpConnReset, new byte[4], null);
        }
        catch (Exception e) when (e is System.Net.Sockets.SocketException or PlatformNotSupportedException)
        {
        }

        try
        {
            udp.Connect(_cfg.Esp32Ip, port);
        }
        catch (System.Net.Sockets.SocketException)
        {
            MarkScreenMissed();
            return ScreenStreamEnd.Retry;
        }

        var subscribedAt = DateTime.UtcNow;
        var lastPunch = DateTime.MinValue;
        var lastPacket = DateTime.MinValue;
        var lastSeq = -1;
        var columns = new byte[32];

        while (!token.IsCancellationRequested)
        {
            if (!ScreenMirror.IsWatched)
                return ScreenStreamEnd.Retry;

            var now = DateTime.UtcNow;
            if (now - lastPunch >= ScreenPunchEvery)
            {
                try { await udp.SendAsync(punch, token); }
                catch (System.Net.Sockets.SocketException) { }
                lastPunch = now;
            }

            if (lastPacket == DateTime.MinValue)
            {
                // Subscribed, yet not one frame: something between here and the
                // clock drops UDP.
                if (now - subscribedAt >= ScreenNoUdpAfter)
                {
                    MarkScreenMissed();
                    return ScreenStreamEnd.NoUdp;
                }
            }
            else
            {
                if (now - lastPacket >= ScreenStaleAfter)
                    MarkScreenMissed();
                // Long enough that the clock may have restarted with a new key.
                if (now - lastPacket >= ScreenResubscribeAfter)
                    return ScreenStreamEnd.Retry;
            }

            System.Net.Sockets.UdpReceiveResult received;
            using (var wait = CancellationTokenSource.CreateLinkedTokenSource(token))
            {
                wait.CancelAfter(TimeSpan.FromMilliseconds(250));
                try
                {
                    received = await udp.ReceiveAsync(wait.Token);
                }
                catch (OperationCanceledException) when (!token.IsCancellationRequested)
                {
                    continue;
                }
                catch (System.Net.Sockets.SocketException)
                {
                    await Task.Delay(100, token);
                    continue;
                }
            }

            var data = received.Buffer;
            if (data.Length != 40 || data[0] != 'O' || data[1] != 'G' || data[2] != 'F' || data[3] != '1')
                continue;

            var seq = (data[4] << 8) | data[5];
            if (lastSeq >= 0)
            {
                var ahead = (seq - lastSeq) & 0xFFFF;
                // 0 is a duplicate and a small step back a datagram overtaken by a
                // newer one. Anything else is taken - a big jump back is the clock
                // having restarted its count.
                if (ahead == 0 || ahead >= 0xFF00)
                    continue;
            }

            lastSeq = seq;
            lastPacket = DateTime.UtcNow;
            Buffer.BlockCopy(data, 8, columns, 0, 32);
            PublishScreenFrame((data[6] & 1) != 0, Math.Clamp((int)data[7], 0, 16), columns);
        }

        return ScreenStreamEnd.Retry;
    }

    // The fallback: one /getscreen at a time, for as long as `duration`.
    private async Task RunScreenPollingAsync(TimeSpan duration, CancellationToken token)
    {
        var until = DateTime.UtcNow + duration;
        while (!token.IsCancellationRequested && DateTime.UtcNow < until && ScreenMirror.IsWatched)
        {
            var nextDelay = TimeSpan.FromMilliseconds(100);
            try
            {
                using var timeout = CancellationTokenSource.CreateLinkedTokenSource(token);
                timeout.CancelAfter(TimeSpan.FromMilliseconds(2500));

                using var request = new HttpRequestMessage(HttpMethod.Get, $"{BaseUrl}/getscreen");
                if (!string.IsNullOrWhiteSpace(_sessionCookie))
                    request.Headers.TryAddWithoutValidation("Cookie", _sessionCookie);
                using var response = await _http.SendAsync(request, timeout.Token);

                if (response.StatusCode == HttpStatusCode.OK)
                {
                    var json = await response.Content.ReadAsStringAsync(timeout.Token);
                    if (TryParseScreenFrame(json, out var on, out var brightness, out var frame))
                        PublishScreenFrame(on, brightness, frame);
                }
                else if (response.StatusCode == HttpStatusCode.Unauthorized)
                {
                    await LoginAsync(token, retries: 3, delaySeconds: 2);
                    continue;
                }
                else if (response.StatusCode == HttpStatusCode.NotFound)
                {
                    ScreenMirror.SetState(ScreenMirrorState.Unsupported);
                    if (!_screenUnsupportedLogged)
                    {
                        _screenUnsupportedLogged = true;
                        _log("[SCREEN] Firmware-ul ceasului nu are /getscreen.");
                    }
                    nextDelay = TimeSpan.FromSeconds(10);
                }
                else
                {
                    MarkScreenMissed();
                    nextDelay = TimeSpan.FromMilliseconds(500);
                }
            }
            catch (OperationCanceledException) when (token.IsCancellationRequested)
            {
                break;
            }
            catch (OperationCanceledException)
            {
                MarkScreenMissed();
                nextDelay = TimeSpan.FromMilliseconds(500);
            }
            catch (HttpRequestException)
            {
                MarkScreenMissed();
                nextDelay = TimeSpan.FromMilliseconds(500);
            }

            if (!await DelayAsync(nextDelay, token)) break;
        }
    }

    private void PublishScreenFrame(bool on, int brightness, byte[] columns)
    {
        _screenLastFrameUtc = DateTime.UtcNow;
        ScreenMirror.Publish(on, brightness, columns);
    }

    // A frame that did not come when it should have. The picture stays; the clock
    // only counts as offline once nothing has arrived for ScreenStaleAfter.
    private void MarkScreenMissed()
    {
        if (DateTime.UtcNow - _screenLastFrameUtc < ScreenStaleAfter) return;
        if (ScreenMirror.State is ScreenMirrorState.Offline or ScreenMirrorState.Unsupported) return;
        ScreenMirror.SetState(ScreenMirrorState.Offline);
        _log("[SCREEN] Ceasul nu răspunde.");
    }

    private static async Task<bool> DelayAsync(TimeSpan delay, CancellationToken token)
    {
        try
        {
            await Task.Delay(delay, token);
            return true;
        }
        catch (OperationCanceledException)
        {
            return false;
        }
    }

    private static bool TryParseScreenSubscription(string json, out int port, out byte[] key)
    {
        port = 0;
        key = [];
        try
        {
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            if (!root.TryGetProperty("port", out var portElement) ||
                !portElement.TryGetInt32(out port) ||
                port is < 1 or > 65535)
            {
                return false;
            }

            if (!root.TryGetProperty("key", out var keyElement) ||
                keyElement.GetString() is not { Length: 8 } hex)
            {
                return false;
            }

            key = Convert.FromHexString(hex);
            return true;
        }
        catch (Exception e) when (e is JsonException or FormatException or InvalidOperationException)
        {
            return false;
        }
    }

    private static bool TryParseScreenFrame(
        string json,
        out bool on,
        out int brightness,
        out byte[] columns)
    {
        on = false;
        brightness = 0;
        columns = [];

        try
        {
            using var document = JsonDocument.Parse(json);
            var root = document.RootElement;
            if (!root.TryGetProperty("on", out var onElement) ||
                (onElement.ValueKind != JsonValueKind.True && onElement.ValueKind != JsonValueKind.False) ||
                !root.TryGetProperty("b", out var brightnessElement) ||
                !brightnessElement.TryGetInt32(out brightness) ||
                !root.TryGetProperty("px", out var pixelsElement))
            {
                return false;
            }

            var pixels = pixelsElement.GetString();
            if (pixels is null || pixels.Length != 64)
                return false;

            columns = Convert.FromHexString(pixels);
            if (columns.Length != 32)
                return false;

            on = onElement.GetBoolean();
            brightness = Math.Clamp(brightness, 0, 16);
            return true;
        }
        catch (JsonException)
        {
            return false;
        }
        catch (FormatException)
        {
            return false;
        }
    }

    private async Task<HttpResponseMessage> PostAuthenticatedAsync(string path, Dictionary<string, string> data)
    {
        using var request = new HttpRequestMessage(HttpMethod.Post, $"{BaseUrl}{path}")
        {
            Content = new FormUrlEncodedContent(data)
        };
        if (!string.IsNullOrWhiteSpace(_sessionCookie))
            request.Headers.TryAddWithoutValidation("Cookie", _sessionCookie);
        return await _http.SendAsync(request);
    }
    private async Task SendAsync(string path, Dictionary<string, string> data)
    {
        try
        {
            using var resp = await PostAuthenticatedAsync(path, data);
            if (resp.IsSuccessStatusCode)
            {
                _log($"[{path.TrimStart('/').ToUpperInvariant()}] {string.Join(" ", data.Values)}");
            }
            else if ((int)resp.StatusCode == 401)
            {
                _log("[AUTH] Sesiune expirată - reautentificare...");
                if (await LoginAsync(CancellationToken.None, retries: 3, delaySeconds: 2))
                {
                    using var retryResponse = await PostAuthenticatedAsync(path, data);
                    if (retryResponse.IsSuccessStatusCode)
                        _log($"[{path.TrimStart('/').ToUpperInvariant()}] {string.Join(" ", data.Values)}");
                    else
                        _log($"[WARN] Status {(int)retryResponse.StatusCode} la {path} dupÄƒ reautentificare");
                }
            }
            else
            {
                _log($"[WARN] Status {(int)resp.StatusCode} la {path}");
            }
        }
        catch (HttpRequestException)
        {
            _log($"[ERR] Nu mă pot conecta la {_cfg.Esp32Ip}");
        }
        catch (TaskCanceledException)
        {
            _log($"[ERR] Timeout la {path}");
        }
    }
    private string Sanitize(string text)
    {
        var sb = new System.Text.StringBuilder();
        foreach (var ch in text)
        {
            if (DiacriticsMap.TryGetValue(ch, out var replacement))
            {
                sb.Append(replacement);
                continue;
            }
            if ((ch >= 0x20 && ch < 0x7F) || ch == '°')
                sb.Append(ch);
        }
        return sb.ToString();
    }
    private string Truncate(string text)
    {
        var max = _cfg.NotifMaxChars;
        if (text.Length <= max) return text;
        return text[..(max - 3)].TrimEnd() + "...";
    }

    private string BuildNotifText(string app, string title, string body)
    {
        var parts = new[] { app, title, body }.Where(p => !string.IsNullOrEmpty(p));
        var text = parts.Any() ? string.Join(" - ", parts) : "(notificare)";
        return Truncate(Sanitize(text));
    }
    private async Task RunNotificationListenerAsync(CancellationToken token)
    {
        try
        {
            var listener = UserNotificationListener.Current;
            var status = await listener.RequestAccessAsync();
            if (status != UserNotificationListenerAccessStatus.Allowed)
            {
                _log("[NOTIF] Acces REFUZAT de Windows. Activează în Settings > Notifications > acces la notificări.");
                return;
            }

            _log("[NOTIF] Ascultător activ.");
            var seenIds = new HashSet<uint>();

            var existing = await listener.GetNotificationsAsync(NotificationKinds.Toast);
            foreach (var n in existing) seenIds.Add(n.Id);

            while (!token.IsCancellationRequested)
            {
                try
                {
                    var notifs = await listener.GetNotificationsAsync(NotificationKinds.Toast);
                    var currentIds = new HashSet<uint>();
                    foreach (var n in notifs)
                    {
                        currentIds.Add(n.Id);
                        if (seenIds.Contains(n.Id)) continue;
                        seenIds.Add(n.Id);

                        var (app, title, body) = ParseNotification(n);
                        if (!string.IsNullOrEmpty(title) || !string.IsNullOrEmpty(body))
                        {
                            var text = BuildNotifText(app, title, body);
                            await SendAsync("/notification", new Dictionary<string, string> { ["text"] = text });
                        }
                    }
                    seenIds.IntersectWith(currentIds);
                }
                catch (Exception e)
                {
                    _log($"[NOTIF ERR] Eroare la interogare: {e.Message}");
                }
                await Task.Delay(350, token);
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception e)
        {
            _log($"[NOTIF] Ascultătorul s-a oprit: {e.Message}");
        }
    }

    private static (string app, string title, string body) ParseNotification(UserNotification n)
    {
        string app = "", title = "", body = "";
        try
        {
            app = n.AppInfo?.DisplayInfo?.DisplayName ?? "";
            var binding = n.Notification.Visual.GetBinding(KnownNotificationBindings.ToastGeneric);
            if (binding != null)
            {
                var texts = binding.GetTextElements();
                if (texts.Count > 0) title = texts[0].Text ?? "";
                if (texts.Count > 1) body = texts[1].Text ?? "";
            }
        }
        catch { /* best-effort parsing */ }
        return (app, title, body);
    }
    private async Task NowPlayingLoopAsync(CancellationToken token)
    {
        string? lastText = null;
        string? lastKind = null;
        var lastSend = DateTime.MinValue;
        while (!token.IsCancellationRequested)
        {
            try
            {
                var (text, kind) = await GetNowPlayingAsync();
                var now = DateTime.UtcNow;
                if (text != null)
                {
                    if (text != lastText || kind != lastKind || (now - lastSend).TotalSeconds >= _cfg.SendIntervalSeconds)
                    {
                        var data = new Dictionary<string, string> { ["text"] = text };
                        if (!string.IsNullOrWhiteSpace(kind)) data["kind"] = kind;
                        await SendAsync("/nowplaying", data);
                        lastText = text;
                        lastKind = kind;
                        lastSend = now;
                    }
                }
                else
                {
                    lastText = null;
                    lastKind = null;
                }
            }
            catch (Exception e)
            {
                _log($"[NP WARN] {e.Message}");
            }
            try { await Task.Delay(TimeSpan.FromSeconds(_cfg.PollIntervalSeconds), token); }
            catch (OperationCanceledException) { break; }
        }
    }
    private async Task Ets2LoopAsync(CancellationToken token)
    {
        _log("[ETS2] Modul detecție pornit, aștept eurotrucks2.exe...");
        _ets2 = new Ets2TelemetryService(_log);
        var wasRunning = false;
        int? lastSpeedSent = null;
        var lastSpeedSentAt = DateTime.MinValue;
        while (!token.IsCancellationRequested)
        {
            var running = _ets2.IsEts2RunningLogged();
            if (running != wasRunning)
            {
                _log(running ? "[ETS2] eurotrucks2.exe detectat." : "[ETS2] eurotrucks2.exe închis - deconectat.");
                wasRunning = running;
                lastSpeedSent = null;
            }
            if (running)
            {
                var speed = _ets2.GetSpeedKmh();
                var shouldRefreshPriorityTile = !ConfigStore.StopPriorityTileWhenTruckStops
                    && DateTime.UtcNow - lastSpeedSentAt >= TimeSpan.FromSeconds(2);
                if (speed.HasValue && (speed != lastSpeedSent || shouldRefreshPriorityTile))
                {
                    await SendAsync("/ets2speed", new Dictionary<string, string> { ["speed"] = speed.Value.ToString() });
                    lastSpeedSent = speed;
                    lastSpeedSentAt = DateTime.UtcNow;
                }
            }
            try { await Task.Delay(running ? 500 : 2000, token); }
            catch (OperationCanceledException) { break; }
        }
    }

    private async Task<(string? text, string kind)> GetNowPlayingAsync()
    {
        var mgr = await GlobalSystemMediaTransportControlsSessionManager.RequestAsync();
        // GetCurrentSession can point to an inactive music player while a browser or video
        // player is actually running. Prefer a session that is actively playing.
        var session = mgr.GetSessions()
            .FirstOrDefault(candidate => candidate.GetPlaybackInfo().PlaybackStatus ==
                GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing)
            ?? mgr.GetCurrentSession();
        if (session is null) return (null, "");
        if (!ConfigStore.KeepNowPlayingWhenPaused &&
            session.GetPlaybackInfo().PlaybackStatus != GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing)
            return (null, "");
        var props = await session.TryGetMediaPropertiesAsync();
        if (props is null) return (null, "");
        var title = (props.Title ?? "").Trim();
        var artist = (props.Artist ?? "").Trim();
        if (string.IsNullOrEmpty(title)) return (null, "");

        var text = string.IsNullOrEmpty(artist) ? title : $"{artist} - {title}";
        return (text, ClassifyNowPlayingSource(session.SourceAppUserModelId));
    }
    private string ClassifyNowPlayingSource(string? sourceApp)
    {
        if (string.IsNullOrWhiteSpace(sourceApp)) return "";
        var source = sourceApp.ToLowerInvariant();

        foreach (var player in ConfigStore.VideoPlayers)
            if (player.Enabled && !string.IsNullOrWhiteSpace(player.Match) && source.Contains(player.Match.ToLowerInvariant()))
                return "video";
        foreach (var player in ConfigStore.MusicPlayers)
            if (player.Enabled && !string.IsNullOrWhiteSpace(player.Match) && source.Contains(player.Match.ToLowerInvariant()))
                return "music";

        return "";
    }
}
