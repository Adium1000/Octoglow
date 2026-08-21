using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace OctoglowSender;

public sealed partial class CustomizePage : Page
{
    private AppConfig _config = ConfigStore.Load();
    private bool _hasStarted;

    public CustomizePage()
    {
        InitializeComponent();
        // Delay WebView creation until this page has been fully attached to the
        // navigation frame. Initializing it during the Loaded callback can make
        // WinUI terminate the process on some machines.
        Loaded += (_, _) => DispatcherQueue.TryEnqueue(async () =>
        {
            if (_hasStarted) return;
            _hasStarted = true;
            await Task.Yield();
            await OpenDevicePanelAsync();
        });
    }

    private async Task OpenDevicePanelAsync()
    {
        try
        {
            if (!ConnectionStatus.IsConnected) return;
            _config = ConfigStore.Load();
            if (string.IsNullOrWhiteSpace(_config.Esp32Ip))
            {
                AppLog.Write(AppStrings.Get("customize.missing"));
                return;
            }

            var rawAddress = _config.Esp32Ip.Trim();
            var addressWithScheme = rawAddress.Contains("://", StringComparison.Ordinal) ? rawAddress : $"http://{rawAddress}";
            if (!Uri.TryCreate(addressWithScheme, UriKind.Absolute, out var configuredUri) || string.IsNullOrWhiteSpace(configuredUri.Host))
            {
                AppLog.Write(AppStrings.Get("customize.missing"));
                return;
            }

            var configuredPort = configuredUri.IsDefaultPort ? _config.Esp32Port : configuredUri.Port;
            var port = configuredPort is > 0 and <= 65535 ? configuredPort : -1;
            var deviceUri = new UriBuilder(configuredUri) { Port = port, Path = string.Empty, Query = string.Empty, Fragment = string.Empty }.Uri;
            await DeviceWebView.EnsureCoreWebView2Async();
            DeviceWebView.CoreWebView2.BasicAuthenticationRequested -= OnBasicAuthenticationRequested;
            DeviceWebView.CoreWebView2.BasicAuthenticationRequested += OnBasicAuthenticationRequested;
            DeviceWebView.CoreWebView2.NavigationCompleted -= OnNavigationCompleted;
            DeviceWebView.CoreWebView2.NavigationCompleted += OnNavigationCompleted;
            DeviceWebView.CoreWebView2.Navigate(deviceUri.AbsoluteUri);
        }
        catch (Exception ex)
        {
            AppLog.Write($"[CUSTOMIZE ERR] Nu s-a putut deschide panoul: {ex.Message}");
        }
    }

    private void OnBasicAuthenticationRequested(Microsoft.Web.WebView2.Core.CoreWebView2 sender, Microsoft.Web.WebView2.Core.CoreWebView2BasicAuthenticationRequestedEventArgs args)
    {
        args.Response.UserName = _config.ScUser;
        args.Response.Password = _config.ScPass;
    }

    private void OnNavigationCompleted(Microsoft.Web.WebView2.Core.CoreWebView2 sender, Microsoft.Web.WebView2.Core.CoreWebView2NavigationCompletedEventArgs args)
    {
        if (args.IsSuccess) return;
        AppLog.Write($"[CUSTOMIZE ERR] Panoul nu a putut fi încărcat ({args.WebErrorStatus}).");
    }
}
