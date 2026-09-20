namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Wrapper static peste <see cref="MacropadInputService"/>, după același model
/// ca <see cref="MacropadConnectionState"/>: pornit o singură dată la lansarea
/// aplicației (când avem un HWND disponibil), accesibil din orice pagină
/// indiferent de navigare, fără conexiuni/hook-uri duplicate.
/// </summary>
public static class MacropadInputBus
{
    private static MacropadInputService? _service;

    public static IMacropadDevice? Device => _service;

    public static void Initialize(IntPtr hwnd, NativeWindowMessageRouter messageRouter)
    {
        if (_service is not null) return;
        _service = new MacropadInputService();
        _service.Attach(hwnd, messageRouter);
    }

    public static void Shutdown()
    {
        _service?.Dispose();
        _service = null;
    }
}
