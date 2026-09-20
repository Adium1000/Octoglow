using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using Microsoft.UI.Windowing;
using OctoglowSender.Devices.Macro.Services;
namespace OctoglowSender;
public partial class App : Application
{
    public static Window? MainWindow { get; private set; }
    public static DeviceHubService DeviceHub { get; private set; } = null!;
    private TrayService? _trayService;
    private NativeWindowMessageRouter? _messageRouter;
    private bool _isExiting;
    private bool _isRestoringFromTray;
    private bool _isCleanedUp;
    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        DeviceHub = new DeviceHubService();
        MainWindow = new MainWindow();
        AppLog.Initialize(MainWindow.DispatcherQueue);
        AppLog.Write("Octoglow Connect Hub a fost pornit.");
        var config = ConfigStore.Load();
        ThemeService.Apply(config.Theme);
        ThemeService.ApplyBackdrop(config.BackdropMaterial);
        StartupService.SetRunAtStartup(config.RunAtStartup);
        // Pentru aplicații neîmpachetate (unpackaged), WinUI 3 nu populează fiabil
        // LaunchActivatedEventArgs.Arguments la lansarea prin linia de comandă (ex: din
        // cheia de Run din registry), așa că citim argumentele direct din linia de comandă.
        var launchedAtStartup = config.RunAtStartup &&
            Environment.GetCommandLineArgs().Skip(1).Any(a => string.Equals(a.Trim(), "--startup", StringComparison.OrdinalIgnoreCase));
        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "octoglow_logo.ico");
        if (File.Exists(iconPath))
            MainWindow.AppWindow.SetIcon(iconPath);
        var windowHandle = WinRT.Interop.WindowNative.GetWindowHandle(MainWindow);
        try
        {
            _messageRouter = new NativeWindowMessageRouter(windowHandle);
            _trayService = new TrayService(windowHandle, _messageRouter, ShowMainWindow, ExitApplication, iconPath);
            MacropadInputBus.Initialize(windowHandle, _messageRouter);
        }
        catch (Exception error)
        {
            AppLog.Write($"[WINDOW WARN] Serviciile native nu au putut fi inițializate: {error.Message}");
        }

        DeviceHub.Start();
        MainWindow.AppWindow.Changed += (_, args) =>
        {
            if (_trayService is null || !ConfigStore.Load().MinimizeToTray || _isRestoringFromTray) return;
            if (MainWindow.AppWindow.Presenter is OverlappedPresenter presenter &&
                presenter.State == OverlappedPresenterState.Minimized)
            {
                _trayService.Show();
                MainWindow.AppWindow.Hide();
            }
        };
        MainWindow.AppWindow.Closing += (_, args) =>
        {
            if (_trayService is not null && !_isExiting && ConfigStore.Load().MinimizeToTray)
            {
                args.Cancel = true;
                _trayService.Show();
                MainWindow.AppWindow.Hide();
                return;
            }

            CleanupServices();
        };
        MainWindow.Closed += (_, _) => CleanupServices();
        MainWindow.Activate();

        if (launchedAtStartup && _trayService is not null)
        {
            _trayService.Show();
            MainWindow.AppWindow.Hide();
        }

        if (!string.IsNullOrWhiteSpace(config.Esp32Ip) &&
            !string.IsNullOrWhiteSpace(config.ScUser) &&
            !string.IsNullOrWhiteSpace(config.ScPass))
        {
            DeskClockSession.Start(config);
        }
    }

    private void ShowMainWindow()
    {
        if (MainWindow is null) return;
        _isRestoringFromTray = true;
        _trayService?.Hide();
        MainWindow.AppWindow.Show();
        MainWindow.Activate();
        MainWindow.DispatcherQueue.TryEnqueue(() => _isRestoringFromTray = false);
        PostMessage(WinRT.Interop.WindowNative.GetWindowHandle(MainWindow), WM_NCMOUSELEAVE, nint.Zero, nint.Zero);
    }
    private void ExitApplication()
    {
        _isExiting = true;
        MainWindow?.Close();
    }

    private void CleanupServices()
    {
        if (_isCleanedUp) return;
        _isCleanedUp = true;
        DeskClockSession.Stop();
        DeviceHub.Dispose();
        MacropadInputBus.Shutdown();
        _trayService?.Dispose();
        _messageRouter?.Dispose();
    }
    private const uint WM_NCMOUSELEAVE = 0x02A2;
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PostMessage(nint hWnd, uint msg, nint wParam, nint lParam);

}
