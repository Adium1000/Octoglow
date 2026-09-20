namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Wrapper static peste <see cref="MacropadDetector"/> — pornit o singură dată
/// la lansarea aplicației, astfel încât starea de conexiune să existe indiferent
/// de ce pagină e afișată în acel moment (Conexiune, Layout, etc).
/// </summary>
public static class MacropadConnectionState
{
    private static MacropadDetector? _detector;

    public static event EventHandler<bool>? Changed;

    public static bool IsConnected => _detector?.IsConnected ?? false;

    public static void Initialize()
    {
        if (_detector is not null) return;
        _detector = new MacropadDetector();
        _detector.ConnectionChanged += (_, connected) => Changed?.Invoke(null, connected);
    }

    public static bool CheckNow() => _detector?.CheckNow() ?? false;

    public static void Shutdown()
    {
        if (_detector is null) return;
        _detector.Dispose();
        _detector = null;
    }
}
