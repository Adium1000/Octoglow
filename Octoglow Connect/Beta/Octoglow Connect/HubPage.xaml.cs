using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;

namespace OctoglowSender;

public sealed partial class HubPage : Page
{
    private bool _isSubscribed;

    public HubPage()
    {
        InitializeComponent();
        Loaded += HubPage_Loaded;
        Unloaded += HubPage_Unloaded;
    }

    private void HubPage_Loaded(object sender, RoutedEventArgs e)
    {
        if (!_isSubscribed)
        {
            App.DeviceHub.DevicesChanged += DeviceHub_DevicesChanged;
            App.DeviceHub.ScanStateChanged += DeviceHub_ScanStateChanged;
            AppStrings.LanguageChanged += AppStrings_LanguageChanged;
            _isSubscribed = true;
        }

        RefreshPage();
    }

    private void HubPage_Unloaded(object sender, RoutedEventArgs e)
    {
        if (!_isSubscribed) return;

        App.DeviceHub.DevicesChanged -= DeviceHub_DevicesChanged;
        App.DeviceHub.ScanStateChanged -= DeviceHub_ScanStateChanged;
        AppStrings.LanguageChanged -= AppStrings_LanguageChanged;
        _isSubscribed = false;
    }

    private async void ScanButton_Click(object sender, RoutedEventArgs e)
    {
        if (App.DeviceHub.IsScanning) return;

        try
        {
            await App.DeviceHub.RefreshAsync();
        }
        catch (Exception error)
        {
            AppLog.Write($"[DEVICE SCAN ERR] {error.Message}");
        }
    }

    private async void AddDeviceButton_Click(object sender, RoutedEventArgs e)
    {
        var clockOption = new RadioButton
        {
            Content = "Octoglow Desk Clock",
            IsChecked = true,
            MinWidth = 320,
            Padding = new Thickness(4, 10, 4, 10),
        };

        var chooseDialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = AppStrings.Get("hub.addDeviceQuestion"),
            Content = clockOption,
            PrimaryButtonText = AppStrings.Get("hub.continue"),
            CloseButtonText = AppStrings.Get("hub.cancel"),
            DefaultButton = ContentDialogButton.Primary,
        };

        if (await chooseDialog.ShowAsync() != ContentDialogResult.Primary)
            return;

        await ShowManualClockDialogAsync();
    }

    private async Task ShowManualClockDialogAsync()
    {
        var config = ConfigStore.Load();
        var addressBox = new TextBox
        {
            Header = AppStrings.Get("connection.ip"),
            Text = config.Esp32Ip,
            PlaceholderText = "192.168.1.100",
        };
        var portBox = new NumberBox
        {
            Header = AppStrings.Get("connection.port"),
            Value = config.Esp32Port is >= 1 and <= 65535 ? config.Esp32Port : 80,
            Minimum = 1,
            Maximum = 65535,
            SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Compact,
        };
        var validationText = new TextBlock
        {
            Text = AppStrings.Get("clock.invalidAddressMessage"),
            Foreground = new SolidColorBrush(Colors.OrangeRed),
            TextWrapping = TextWrapping.Wrap,
            Visibility = Visibility.Collapsed,
        };
        var content = new StackPanel { Spacing = 12 };
        content.Children.Add(new TextBlock
        {
            Text = AppStrings.Get("hub.manualClockDescription"),
            Opacity = 0.68,
            TextWrapping = TextWrapping.Wrap,
        });
        content.Children.Add(addressBox);
        content.Children.Add(portBox);
        content.Children.Add(validationText);

        DeskClockDiscoveryService.DeskClockCandidate normalized = default;
        var manualDialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = AppStrings.Get("hub.addClockTitle"),
            Content = content,
            PrimaryButtonText = AppStrings.Get("hub.add"),
            CloseButtonText = AppStrings.Get("hub.cancel"),
            DefaultButton = ContentDialogButton.Primary,
        };
        manualDialog.Closing += (_, args) =>
        {
            if (args.Result != ContentDialogResult.Primary) return;

            var port = double.IsNaN(portBox.Value) ? 0 : (int)portBox.Value;
            if (port is >= 1 and <= 65535 &&
                DeskClockDiscoveryService.TryNormalizeConfiguredAddress(addressBox.Text, port, out normalized))
            {
                return;
            }

            args.Cancel = true;
            validationText.Visibility = Visibility.Visible;
        };

        if (await manualDialog.ShowAsync() != ContentDialogResult.Primary)
            return;

        config.Esp32Ip = normalized.Address;
        config.Esp32Port = normalized.Port;
        ConfigStore.Save(config);
        App.DeviceHub.RememberConfiguredClock(config);
        _ = App.DeviceHub.RefreshAsync();
    }

    private void DeviceTile_Click(object sender, RoutedEventArgs e)
    {
        if ((sender as FrameworkElement)?.Tag is HubDeviceListItem item && App.MainWindow is MainWindow window)
            window.NavigateToDevice(item.Device);
    }

    private void DeviceHub_DevicesChanged(object? sender, EventArgs e) => EnqueueRefresh();
    private void DeviceHub_ScanStateChanged(object? sender, EventArgs e) => EnqueueRefresh();
    private void AppStrings_LanguageChanged(object? sender, EventArgs e) => EnqueueRefresh();

    private void EnqueueRefresh()
    {
        DispatcherQueue.TryEnqueue(() =>
        {
            if (_isSubscribed) RefreshPage();
        });
    }

    private void RefreshPage()
    {
        ApplyLanguage();

        var devices = App.DeviceHub.Devices;
        DeviceTiles.ItemsSource = devices
            .Select(device => new HubDeviceListItem(device, BuildDeviceDetail(device)))
            .ToList();

        var isEmpty = devices.Count == 0;
        EmptyState.Visibility = isEmpty ? Visibility.Visible : Visibility.Collapsed;
        DeviceTiles.Visibility = isEmpty ? Visibility.Collapsed : Visibility.Visible;

        UpdateScanState();
    }

    private void ApplyLanguage()
    {
        PageTitle.Text = AppStrings.Get("hub.title");
        PageSubtitle.Text = AppStrings.Get("hub.subtitle");
        EmptyTitle.Text = AppStrings.Get("hub.emptyTitle");
        EmptyDescription.Text = AppStrings.Get("hub.emptyDescription");
        ToolTipService.SetToolTip(AddDeviceButton, AppStrings.Get("hub.addDevice"));
    }

    private void UpdateScanState()
    {
        var isScanning = App.DeviceHub.IsScanning;
        ScanButton.IsEnabled = !isScanning;
        ScanButtonText.Text = AppStrings.Get(isScanning ? "nav.scanning" : "nav.scan");
        ScanIcon.Visibility = isScanning ? Visibility.Collapsed : Visibility.Visible;
        ScanProgress.IsActive = isScanning;
        ScanProgress.Visibility = isScanning ? Visibility.Visible : Visibility.Collapsed;
    }

    private static string BuildDeviceDetail(DeviceDescriptor device)
    {
        if (device.Kind == DeviceKind.Macro)
            return "USB HID";

        if (!string.IsNullOrWhiteSpace(device.Address))
            return device.Port > 0 ? $"{device.Address}:{device.Port}" : device.Address;

        return device.Detail;
    }
}

public sealed class HubDeviceListItem
{
    public HubDeviceListItem(DeviceDescriptor device, string detail)
    {
        Device = device;
        Detail = detail;
        Status = AppStrings.Get(device.IsOnline ? "device.connected" : "device.disconnected");
        StatusGlyph = device.IsOnline ? "\uF13E" : "\uF13D";
        StatusBrush = new SolidColorBrush(device.IsOnline ? Colors.LimeGreen : Colors.Gray);
        ImageOpacity = device.IsOnline ? 1 : 0.58;
        DeviceImage = new BitmapImage(new Uri(device.Kind == DeviceKind.Macro
            ? "ms-appx:///Assets/macropad-nobg.png"
            : "ms-appx:///Assets/smartclock-nobg_2.png"));
    }

    public DeviceDescriptor Device { get; }
    public string DisplayName => Device.DisplayName;
    public string Detail { get; }
    public string Status { get; }
    public string StatusGlyph { get; }
    public Brush StatusBrush { get; }
    public ImageSource DeviceImage { get; }
    public double ImageOpacity { get; }
}
