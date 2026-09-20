using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;

namespace OctoglowSender;

public sealed partial class CustomizePage : Page
{
    private AppConfig _config = ConfigStore.Load();
    private DeviceDescriptor? _device;
    private bool _hasStarted;

    public CustomizePage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        if (e.Parameter is DeviceDescriptor { Kind: DeviceKind.DeskClock } device)
            _device = device;
        UpdateAddress();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        AppStrings.LanguageChanged += OnLanguageChanged;
        if (_hasStarted) return;
        _hasStarted = true;
        DispatcherQueue.TryEnqueue(async () =>
        {
            await Task.Yield();
            await OpenDevicePanelAsync();
        });
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        _hasStarted = false;
        AppStrings.LanguageChanged -= OnLanguageChanged;
        if (DeviceWebView.CoreWebView2 is null) return;
        DeviceWebView.CoreWebView2.BasicAuthenticationRequested -= OnBasicAuthenticationRequested;
        DeviceWebView.CoreWebView2.NavigationStarting -= OnNavigationStarting;
        DeviceWebView.CoreWebView2.NavigationCompleted -= OnNavigationCompleted;
    }

    private void OnLanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(ApplyLanguage);

    private async void Reload_Click(object sender, RoutedEventArgs e) => await OpenDevicePanelAsync();

    private void Back_Click(object sender, RoutedEventArgs e)
    {
        if (_device is not null)
        {
            var current = App.DeviceHub.Devices.FirstOrDefault(device => device.Id == _device.Id) ?? _device;
            ((MainWindow?)App.MainWindow)?.NavigateToDevice(current);
        }
    }

    private async Task OpenDevicePanelAsync()
    {
        try
        {
            PanelError.IsOpen = false;
            _config = ConfigStore.Load();
            var rawAddress = _device?.Address ?? _config.Esp32Ip;
            var configuredPort = _device?.Port is > 0 and <= 65535 ? _device.Port : _config.Esp32Port;
            if (!DeskClockDiscoveryService.TryNormalizeConfiguredAddress(rawAddress, configuredPort, out var endpoint))
            {
                ShowError(AppStrings.Get("customize.missing"));
                return;
            }

            _device ??= DeviceDescriptor.DeskClock(endpoint.Address, endpoint.Port, false, $"{endpoint.Address}:{endpoint.Port}");
            UpdateAddress();

            var uri = new UriBuilder(Uri.UriSchemeHttp, endpoint.Address, endpoint.Port).Uri;
            await DeviceWebView.EnsureCoreWebView2Async();
            DeviceWebView.CoreWebView2.BasicAuthenticationRequested -= OnBasicAuthenticationRequested;
            DeviceWebView.CoreWebView2.BasicAuthenticationRequested += OnBasicAuthenticationRequested;
            DeviceWebView.CoreWebView2.NavigationStarting -= OnNavigationStarting;
            DeviceWebView.CoreWebView2.NavigationStarting += OnNavigationStarting;
            DeviceWebView.CoreWebView2.NavigationCompleted -= OnNavigationCompleted;
            DeviceWebView.CoreWebView2.NavigationCompleted += OnNavigationCompleted;
            await TrySeedSessionCookieAsync(uri);
            DeviceWebView.CoreWebView2.Navigate(uri.AbsoluteUri);
        }
        catch (Exception error)
        {
            ShowError($"{AppStrings.Get("customize.error")} {error.Message}");
            AppLog.WriteDeskClock($"[CUSTOMIZE ERR] Nu s-a putut deschide panoul: {error.Message}");
        }
    }

    private void OnBasicAuthenticationRequested(
        Microsoft.Web.WebView2.Core.CoreWebView2 sender,
        Microsoft.Web.WebView2.Core.CoreWebView2BasicAuthenticationRequestedEventArgs args)
    {
        args.Response.UserName = _config.ScUser;
        args.Response.Password = _config.ScPass;
    }

    private async Task TrySeedSessionCookieAsync(Uri deviceUri)
    {
        if (string.IsNullOrWhiteSpace(_config.ScUser) || string.IsNullOrWhiteSpace(_config.ScPass)) return;

        try
        {
            using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(3) };
            using var form = new FormUrlEncodedContent(new Dictionary<string, string>
            {
                ["user"] = _config.ScUser,
                ["pass"] = _config.ScPass,
            });
            using var response = await client.PostAsync(new Uri(deviceUri, "/login"), form);
            if (!response.IsSuccessStatusCode ||
                !response.Headers.TryGetValues("Set-Cookie", out var values)) return;

            var pair = values.FirstOrDefault()?.Split(';', 2)[0].Split('=', 2);
            if (pair is not { Length: 2 } || string.IsNullOrWhiteSpace(pair[0])) return;

            var cookie = DeviceWebView.CoreWebView2.CookieManager.CreateCookie(
                pair[0].Trim(), pair[1], deviceUri.Host, "/");
            cookie.IsHttpOnly = true;
            cookie.IsSecure = deviceUri.Scheme == Uri.UriSchemeHttps;
            DeviceWebView.CoreWebView2.CookieManager.AddOrUpdateCookie(cookie);
        }
        catch (Exception error) when (error is HttpRequestException or TaskCanceledException)
        {
            // The normal web login remains available inside the panel.
        }
    }

    private void OnNavigationStarting(
        Microsoft.Web.WebView2.Core.CoreWebView2 sender,
        Microsoft.Web.WebView2.Core.CoreWebView2NavigationStartingEventArgs args)
    {
        LoadingRing.IsActive = true;
        LoadingRing.Visibility = Visibility.Visible;
    }

    private void OnNavigationCompleted(
        Microsoft.Web.WebView2.Core.CoreWebView2 sender,
        Microsoft.Web.WebView2.Core.CoreWebView2NavigationCompletedEventArgs args)
    {
        LoadingRing.IsActive = false;
        LoadingRing.Visibility = Visibility.Collapsed;
        if (args.IsSuccess) return;
        ShowError($"{AppStrings.Get("customize.error")} ({args.WebErrorStatus})");
        AppLog.WriteDeskClock($"[CUSTOMIZE ERR] Panoul nu a putut fi încărcat ({args.WebErrorStatus}).");
    }

    private void ShowError(string message)
    {
        PanelError.Title = AppStrings.Get("customize.errorTitle");
        PanelError.Message = message;
        PanelError.IsOpen = true;
        LoadingRing.IsActive = false;
        LoadingRing.Visibility = Visibility.Collapsed;
    }

    private void UpdateAddress()
    {
        var address = _device?.Address ?? _config.Esp32Ip;
        var port = _device?.Port is > 0 and <= 65535 ? _device.Port : _config.Esp32Port;
        AddressText.Text = string.IsNullOrWhiteSpace(address) ? "—" : $"{address}:{port}";
    }

    private void ApplyLanguage()
    {
        PageTitle.Text = AppStrings.Get("customize.title");
        ReloadButtonText.Text = AppStrings.Get("customize.reload");
        ToolTipService.SetToolTip(BackButton, AppStrings.Get("customize.back"));
        UpdateAddress();
    }
}
