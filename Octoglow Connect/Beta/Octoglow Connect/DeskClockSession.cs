namespace OctoglowSender;

/// <summary>Keeps the Desk Clock sender alive independently of navigation pages.</summary>
public static class DeskClockSession
{
    private static readonly object Gate = new();
    private static SenderBackend? _backend;
    private static string? _activeDeviceId;

    public static bool IsRunning
    {
        get
        {
            lock (Gate) return _backend is not null;
        }
    }

    public static bool Matches(string address, int port)
    {
        if (!DeskClockDiscoveryService.TryNormalizeConfiguredAddress(address, port, out var normalized))
            return false;
        var id = $"clock:{normalized.Address.ToLowerInvariant()}:{normalized.Port}";
        lock (Gate) return string.Equals(_activeDeviceId, id, StringComparison.OrdinalIgnoreCase);
    }

    public static void Start(AppConfig config)
    {
        SenderBackend? next = null;
        next = new SenderBackend(config, message =>
        {
            lock (Gate)
            {
                if (!ReferenceEquals(_backend, next)) return;
            }

            AppLog.WriteDeskClock(message);
            ConnectionStatus.ProcessBackendMessage(message);
        });

        SenderBackend? previous;
        lock (Gate)
        {
            previous = _backend;
            _backend = next;
            _activeDeviceId = DeskClockDiscoveryService.TryNormalizeConfiguredAddress(
                config.Esp32Ip,
                config.Esp32Port,
                out var normalized)
                ? $"clock:{normalized.Address.ToLowerInvariant()}:{normalized.Port}"
                : null;
        }

        previous?.Dispose();
        ConnectionStatus.Set(ConnectionStatusKind.Connecting);
        next.Start();
    }

    public static void Stop()
    {
        SenderBackend? previous;
        lock (Gate)
        {
            previous = _backend;
            _backend = null;
            _activeDeviceId = null;
        }

        previous?.Dispose();
        ScreenMirror.Reset();
        ConnectionStatus.Set(ConnectionStatusKind.Disconnected);
    }
}
