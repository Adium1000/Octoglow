using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using Microsoft.UI.Windowing;
namespace OctoglowSender;
public partial class App : Application
{
    public static Window? MainWindow { get; private set; }
    private TrayService? _trayService;
    private bool _isExiting;
    private bool _isRestoringFromTray;
    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        MainWindow = new MainWindow();
        AppLog.Initialize(MainWindow.DispatcherQueue);
        AppLog.Write("Aplicația a fost pornită.");
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
        _trayService = new TrayService(WinRT.Interop.WindowNative.GetWindowHandle(MainWindow), ShowMainWindow, ExitApplication, iconPath);
        MainWindow.AppWindow.Changed += (_, args) =>
        {
            if (!ConfigStore.Load().MinimizeToTray || _isRestoringFromTray) return;
            if (MainWindow.AppWindow.Presenter is OverlappedPresenter presenter &&
                presenter.State == OverlappedPresenterState.Minimized)
            {
                _trayService.Show();
                MainWindow.AppWindow.Hide();
            }
        };
        MainWindow.AppWindow.Closing += (_, args) =>
        {
            if (!_isExiting && ConfigStore.Load().MinimizeToTray)
            {
                args.Cancel = true;
                _trayService.Show();
                MainWindow.AppWindow.Hide();
            }
        };
        MainWindow.Activate();

        if (launchedAtStartup)
        {
            _trayService.Show();
            MainWindow.AppWindow.Hide();
        }

        MainWindow.DispatcherQueue.TryEnqueue(((OctoglowSender.MainWindow)MainWindow).StartSending);
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
        _trayService?.Dispose();
        MainWindow?.Close();
    }
    private const uint WM_NCMOUSELEAVE = 0x02A2;
    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PostMessage(nint hWnd, uint msg, nint wParam, nint lParam);

}
