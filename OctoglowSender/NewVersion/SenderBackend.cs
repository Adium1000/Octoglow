using System.Diagnostics;
using System.Net.Http;
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
        _cts = new CancellationTokenSource();
        var token = _cts.Token;

        _tasks.Add(Task.Run(() => LoginThenRunAsync(token), token));
    }
    public void Stop()
    {
        _cts?.Cancel();
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
        _tasks.Add(notifTask);
        _tasks.Add(npTask);
        _tasks.Add(ets2Task);
        try { await Task.WhenAll(notifTask, npTask, ets2Task); }
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

        foreach (var player in _cfg.VideoPlayers)
            if (player.Enabled && !string.IsNullOrWhiteSpace(player.Match) && source.Contains(player.Match.ToLowerInvariant()))
                return "video";
        foreach (var player in _cfg.MusicPlayers)
            if (player.Enabled && !string.IsNullOrWhiteSpace(player.Match) && source.Contains(player.Match.ToLowerInvariant()))
                return "music";

        return "";
    }
}
