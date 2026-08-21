using System.IO;
using System.Runtime.InteropServices;
namespace OctoglowSender;
public sealed class TrayService : IDisposable
{
    private const uint NIM_ADD = 0x00000000;
    private const uint NIM_DELETE = 0x00000002;
    private const uint NIF_MESSAGE = 0x00000001;
    private const uint NIF_ICON = 0x00000002;
    private const uint NIF_TIP = 0x00000004;
    private const uint WM_APP = 0x8000;
    private const uint WM_LBUTTONUP = 0x0202;
    private const uint WM_RBUTTONUP = 0x0205;
    private const int GWL_WNDPROC = -4;
    private const uint TrayMessage = WM_APP + 1;
    private const uint IMAGE_ICON = 1;
    private const uint LR_LOADFROMFILE = 0x00000010;
    private const uint LR_DEFAULTSIZE = 0x00000040;
    private const uint MF_STRING = 0x00000000;
    private const uint TPM_RETURNCMD = 0x0100;
    private const uint TPM_RIGHTBUTTON = 0x0002;
    private const uint ShowCommand = 1;
    private const uint ExitCommand = 2;
    private readonly nint _windowHandle;
    private readonly Action _onShow;
    private readonly Action _onExit;
    private readonly WndProc _windowProc;
    private readonly string? _iconPath;
    private nint _previousWindowProc;
    private nint _hIcon;
    private bool _visible;
    public TrayService(nint windowHandle, Action onShow, Action onExit, string? iconPath = null)
    {
        _windowHandle = windowHandle;
        _onShow = onShow;
        _onExit = onExit;
        _iconPath = iconPath;
        _windowProc = WindowProcedure;
        _previousWindowProc = SetWindowLongPtr(_windowHandle, GWL_WNDPROC, Marshal.GetFunctionPointerForDelegate(_windowProc));
    }
    public void Show()
    {
        if (_visible) return;
        if (_hIcon == nint.Zero)
        {
            _hIcon = _iconPath is not null && File.Exists(_iconPath)
                ? LoadImage(nint.Zero, _iconPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE)
                : nint.Zero;
            if (_hIcon == nint.Zero)
                _hIcon = LoadImage(nint.Zero, "#32512", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE); // IDI_APPLICATION fallback
        }
        var icon = new NotifyIconData
        {
            cbSize = (uint)Marshal.SizeOf<NotifyIconData>(),
            hWnd = _windowHandle,
            uID = 1,
            uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
            uCallbackMessage = TrayMessage,
            hIcon = _hIcon,
            szTip = "Octoglow Connect"
        };
        _visible = ShellNotifyIcon(NIM_ADD, ref icon);
    }
    public void Hide()
    {
        if (!_visible) return;
        var icon = CreateIconData();
        ShellNotifyIcon(NIM_DELETE, ref icon);
        _visible = false;
    }
    public void Dispose()
    {
        Hide();
        if (_previousWindowProc != nint.Zero)
        {
            SetWindowLongPtr(_windowHandle, GWL_WNDPROC, _previousWindowProc);
            _previousWindowProc = nint.Zero;
        }
        if (_hIcon != nint.Zero)
        {
            DestroyIcon(_hIcon);
            _hIcon = nint.Zero;
        }
    }
    private NotifyIconData CreateIconData() => new()
    {
        cbSize = (uint)Marshal.SizeOf<NotifyIconData>(),
        hWnd = _windowHandle,
        uID = 1
    };
    private nint WindowProcedure(nint hWnd, uint message, nint wParam, nint lParam)
    {
        if (message == TrayMessage)
        {
            if ((uint)lParam == WM_LBUTTONUP)
                _onShow();
            else if ((uint)lParam == WM_RBUTTONUP)
                ShowContextMenu();
            return nint.Zero;
        }

        return CallWindowProc(_previousWindowProc, hWnd, message, wParam, lParam);
    }
    private void ShowContextMenu()
    {
        var menu = CreatePopupMenu();
        if (menu == nint.Zero) return;

        try
        {
            var isEnglish = AppStrings.IsEnglish;
            AppendMenu(menu, MF_STRING, ShowCommand, isEnglish ? "Show" : "Arată");
            AppendMenu(menu, MF_STRING, ExitCommand, isEnglish ? "Exit" : "Ieșire");
            if (!GetCursorPos(out var cursor)) return;

            SetForegroundWindow(_windowHandle);
            var command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, cursor.X, cursor.Y, 0, _windowHandle, nint.Zero);
            if (command == ShowCommand) _onShow();
            else if (command == ExitCommand) _onExit();
        }
        finally
        {
            DestroyMenu(menu);
        }
    }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct NotifyIconData
    {
        public uint cbSize;
        public nint hWnd;
        public uint uID;
        public uint uFlags;
        public uint uCallbackMessage;
        public nint hIcon;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string szTip;
        public uint dwState;
        public uint dwStateMask;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)] public string szInfo;
        public uint uTimeoutOrVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string szInfoTitle;
        public uint dwInfoFlags;
        public Guid guidItem;
        public nint hBalloonIcon;
    }
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate nint WndProc(nint hWnd, uint message, nint wParam, nint lParam);
    [DllImport("shell32.dll", EntryPoint = "Shell_NotifyIconW", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool ShellNotifyIcon(uint dwMessage, ref NotifyIconData lpData);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern nint LoadImage(nint hinst, string lpszName, uint uType, int cxDesired, int cyDesired, uint fuLoad);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern nint SetWindowLongPtr(nint hWnd, int nIndex, nint dwNewLong);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern nint CallWindowProc(nint lpPrevWndFunc, nint hWnd, uint msg, nint wParam, nint lParam);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool DestroyIcon(nint hIcon);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern nint CreatePopupMenu();
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool AppendMenu(nint hMenu, uint uFlags, uint uIDNewItem, string lpNewItem);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool GetCursorPos(out Point lpPoint);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetForegroundWindow(nint hWnd);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint TrackPopupMenu(nint hMenu, uint uFlags, int x, int y, int nReserved, nint hWnd, nint prcRect);
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool DestroyMenu(nint hMenu);
    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X;
        public int Y;
    }
}
