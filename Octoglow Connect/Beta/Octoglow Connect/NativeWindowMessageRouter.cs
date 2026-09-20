using System.Runtime.InteropServices;

namespace OctoglowSender;

public delegate bool NativeWindowMessageHandler(
    nint windowHandle,
    uint message,
    nint wParam,
    nint lParam,
    out nint result);

/// <summary>
/// Owns the single Win32 WndProc subclass used by Octoglow Connect. Tray and
/// Raw Input register handlers here instead of independently replacing each
/// other's hooks.
/// </summary>
public sealed class NativeWindowMessageRouter : IDisposable
{
    private const int GwlpWndProc = -4;
    private readonly object _gate = new();
    private readonly nint _windowHandle;
    private readonly WndProc _windowProc;
    private readonly List<NativeWindowMessageHandler> _handlers = [];
    private nint _previousWindowProc;
    private bool _disposed;

    public NativeWindowMessageRouter(nint windowHandle)
    {
        _windowHandle = windowHandle;
        _windowProc = WindowProcedure;
        _previousWindowProc = SetWindowLongPtr(
            _windowHandle,
            GwlpWndProc,
            Marshal.GetFunctionPointerForDelegate(_windowProc));

        if (_previousWindowProc == nint.Zero)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "Nu s-a putut inițializa rutarea mesajelor ferestrei.");
    }

    public IDisposable Subscribe(NativeWindowMessageHandler handler)
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
        lock (_gate) _handlers.Add(handler);
        return new Subscription(this, handler);
    }

    private nint WindowProcedure(nint hWnd, uint message, nint wParam, nint lParam)
    {
        NativeWindowMessageHandler[] handlers;
        lock (_gate) handlers = _handlers.ToArray();

        foreach (var handler in handlers)
        {
            try
            {
                if (handler(hWnd, message, wParam, lParam, out var result))
                    return result;
            }
            catch (Exception error)
            {
                AppLog.Write($"[WINDOW WARN] Mesajul 0x{message:X} nu a putut fi procesat: {error.Message}");
            }
        }

        return CallWindowProc(_previousWindowProc, hWnd, message, wParam, lParam);
    }

    private void Unsubscribe(NativeWindowMessageHandler handler)
    {
        lock (_gate) _handlers.Remove(handler);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        lock (_gate) _handlers.Clear();

        if (_previousWindowProc != nint.Zero)
        {
            SetWindowLongPtr(_windowHandle, GwlpWndProc, _previousWindowProc);
            _previousWindowProc = nint.Zero;
        }
    }

    private sealed class Subscription(NativeWindowMessageRouter owner, NativeWindowMessageHandler handler) : IDisposable
    {
        private NativeWindowMessageRouter? _owner = owner;
        public void Dispose() => Interlocked.Exchange(ref _owner, null)?.Unsubscribe(handler);
    }

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate nint WndProc(nint hWnd, uint message, nint wParam, nint lParam);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    private static extern nint SetWindowLongPtr(nint hWnd, int nIndex, nint dwNewLong);

    [DllImport("user32.dll", EntryPoint = "CallWindowProcW", SetLastError = true)]
    private static extern nint CallWindowProc(nint lpPrevWndFunc, nint hWnd, uint msg, nint wParam, nint lParam);
}
