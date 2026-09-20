using System.Collections.Specialized;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.UI.Xaml.Shapes;

namespace OctoglowSender;

public sealed partial class ConnectionPage : Page
{
    private AppConfig _config = ConfigStore.Load();
    private DeviceDescriptor? _device;
    private bool _subscribed;
    private readonly Ellipse[,] _screenLeds = new Ellipse[8, 32];
    private readonly byte[] _renderedScreenColumns = new byte[32];
    private readonly Brush _screenLedOnBrush;
    private readonly Brush _screenLedOffBrush;

    public ConnectionPage()
    {
        InitializeComponent();
        _screenLedOnBrush = (Brush)Resources["ScreenLedOnBrush"];
        _screenLedOffBrush = (Brush)Resources["ScreenLedOffBrush"];
        CreateScreenLeds();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        PopulateForm(_config);
        ApplyLanguage();
        UpdateLastActivity();
        UpdateConnectionState();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        if (e.Parameter is not DeviceDescriptor { Kind: DeviceKind.DeskClock } device) return;
        _device = device;
        if (!string.IsNullOrWhiteSpace(device.Address)) IpBox.Text = device.Address;
        if (device.Port is > 0 and <= 65535) PortBox.Text = device.Port.ToString();
        UpdateDevicePresence();
        UpdateConnectionState();
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        if (_subscribed) return;
        _subscribed = true;
        AppStrings.LanguageChanged += OnLanguageChanged;
        AppLog.DeskClockEntries.CollectionChanged += Entries_CollectionChanged;
        ConnectionStatus.Changed += ConnectionStatus_Changed;
        ScreenMirror.Changed += ScreenMirror_Changed;
        ScreenMirror.AddViewer();
        App.DeviceHub.DevicesChanged += DeviceHub_DevicesChanged;
        RenderScreen();
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        if (!_subscribed) return;
        _subscribed = false;
        AppStrings.LanguageChanged -= OnLanguageChanged;
        AppLog.DeskClockEntries.CollectionChanged -= Entries_CollectionChanged;
        ConnectionStatus.Changed -= ConnectionStatus_Changed;
        ScreenMirror.Changed -= ScreenMirror_Changed;
        ScreenMirror.RemoveViewer();
        App.DeviceHub.DevicesChanged -= DeviceHub_DevicesChanged;
    }

    private void Start_Click(object sender, RoutedEventArgs e) => StartSending();

    public void StartSending()
    {
        if (!TryReadForm(out var config)) return;
        _config = config;
        ConfigStore.Save(config);
        App.DeviceHub.RememberConfiguredClock(config);
        DeskClockSession.Start(config);
        _device = DeviceDescriptor.DeskClock(
            config.Esp32Ip,
            config.Esp32Port,
            isOnline: _device?.IsOnline == true &&
                      string.Equals(_device.Address, config.Esp32Ip, StringComparison.OrdinalIgnoreCase),
            detail: $"{config.Esp32Ip}:{config.Esp32Port}");
        UpdateConnectionState();
        _ = App.DeviceHub.RefreshAsync();
    }

    private void Stop_Click(object sender, RoutedEventArgs e)
    {
        if (!IsCurrentSessionDevice()) return;
        DeskClockSession.Stop();
        AppLog.WriteDeskClock(AppStrings.Get("connection.stopped"));
    }

    private void OpenPanel_Click(object sender, RoutedEventArgs e)
    {
        if (!TryReadForm(out var config)) return;
        _config = config;
        ConfigStore.Save(config);
        App.DeviceHub.RememberConfiguredClock(config);

        var descriptor = DeviceDescriptor.DeskClock(
            config.Esp32Ip,
            config.Esp32Port,
            isOnline: _device?.IsOnline ?? ConnectionStatus.IsConnected,
            detail: $"{config.Esp32Ip}:{config.Esp32Port}");
        ((MainWindow?)App.MainWindow)?.OpenDeskClockPanel(descriptor);
    }

    private bool TryReadForm(out AppConfig config)
    {
        config = ConfigStore.Load();
        var address = IpBox.Text.Trim();
        if (!DeskClockDiscoveryService.TryNormalizeConfiguredAddress(
                address,
                int.TryParse(PortBox.Text, out var parsedPort) ? parsedPort : 80,
                out var normalized))
        {
            PresenceInfo.Title = AppStrings.Get("clock.invalidAddressTitle");
            PresenceInfo.Message = AppStrings.Get("clock.invalidAddressMessage");
            PresenceInfo.Severity = InfoBarSeverity.Error;
            PresenceInfo.IsOpen = true;
            return false;
        }

        config.Esp32Ip = normalized.Address;
        config.Esp32Port = normalized.Port;
        config.ScUser = UserBox.Text.Trim();
        config.ScPass = PassBox.Password;
        return true;
    }

    private void PopulateForm(AppConfig config)
    {
        IpBox.Text = config.Esp32Ip;
        PortBox.Text = config.Esp32Port.ToString();
        UserBox.Text = config.ScUser;
        PassBox.Password = config.ScPass;
    }

    private void OnLanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(ApplyLanguage);
    private void Entries_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) => DispatcherQueue.TryEnqueue(UpdateLastActivity);
    private void ConnectionStatus_Changed(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(UpdateConnectionState);
    // Frames can arrive 40 times a second; one render waiting on the UI thread is
    // enough, and it always draws the newest frame.
    private int _screenRenderQueued;
    private void ScreenMirror_Changed(object? sender, EventArgs e)
    {
        if (Interlocked.Exchange(ref _screenRenderQueued, 1) != 0) return;
        if (!DispatcherQueue.TryEnqueue(() =>
            {
                Interlocked.Exchange(ref _screenRenderQueued, 0);
                RenderScreen();
            }))
        {
            Interlocked.Exchange(ref _screenRenderQueued, 0);
        }
    }
    private void DeviceHub_DevicesChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() =>
    {
        if (_device is not null)
            _device = App.DeviceHub.Devices.FirstOrDefault(device => device.Id == _device.Id) ?? _device;
        UpdateDevicePresence();
        UpdateConnectionState();
    });

    private void UpdateLastActivity()
    {
        var activity = AppLog.DeskClockEntries.LastOrDefault();
        if (activity is null)
        {
            LastActivityText.Text = AppStrings.Get("activity.empty");
            ActivityIcon.Glyph = "\uE823";
            return;
        }
        LastActivityText.Text = activity.Description;
        ActivityIcon.Glyph = activity.Glyph;
    }

    private void UpdateDevicePresence()
    {
        var address = _device?.Address ?? IpBox.Text.Trim();
        var port = _device?.Port is > 0 and <= 65535 ? _device.Port : (int.TryParse(PortBox.Text, out var parsed) ? parsed : 80);
        DetectedAddressText.Text = string.IsNullOrWhiteSpace(address) ? "—" : $"{address}:{port}";

        var online = _device?.IsOnline ?? false;
        PresenceInfo.IsOpen = !online;
        PresenceInfo.Severity = InfoBarSeverity.Warning;
        PresenceInfo.Title = AppStrings.Get("clock.offlineTitle");
        PresenceInfo.Message = AppStrings.Get("clock.offlineMessage");
        OpenPanelButton.IsEnabled = online || ConnectionStatus.IsConnected;
    }

    private void UpdateConnectionState()
    {
        var isCurrentSessionDevice = IsCurrentSessionDevice();
        LastActivityCard.Visibility = DeskClockSession.IsRunning && isCurrentSessionDevice
            ? Visibility.Visible
            : Visibility.Collapsed;
        StopButton.IsEnabled = DeskClockSession.IsRunning && isCurrentSessionDevice;

        if (!isCurrentSessionDevice && DeskClockSession.IsRunning)
        {
            ConnectionStateIcon.Glyph = _device?.IsOnline == true ? "\uE8CE" : "\uE711";
            ConnectionStateText.Text = AppStrings.Get(_device?.IsOnline == true ? "clock.detected" : "connection.disconnected");
            return;
        }

        var state = ConnectionStatus.Current;
        if (state == ConnectionStatusKind.Disconnected && _device?.IsOnline == true)
        {
            ConnectionStateIcon.Glyph = "\uE8CE";
            ConnectionStateText.Text = AppStrings.Get("clock.detected");
            return;
        }

        (ConnectionStateIcon.Glyph, ConnectionStateText.Text) = state switch
        {
            ConnectionStatusKind.Connecting => ("\uE895", AppStrings.Get("connection.connecting")),
            ConnectionStatusKind.Connected => ("\uE73E", AppStrings.Get("connection.connected")),
            ConnectionStatusKind.Failed => ("\uE783", AppStrings.Get("connection.failed")),
            _ => ("\uE711", AppStrings.Get("connection.disconnected")),
        };
    }

    private bool IsCurrentSessionDevice()
    {
        var port = int.TryParse(PortBox.Text, out var parsedPort) ? parsedPort : 80;
        return DeskClockSession.Matches(IpBox.Text.Trim(), port);
    }

    private void CreateScreenLeds()
    {
        for (var y = 0; y < 8; y++)
        {
            for (var x = 0; x < 32; x++)
            {
                var led = new Ellipse
                {
                    Width = 12,
                    Height = 12,
                    Fill = _screenLedOffBrush,
                    IsHitTestVisible = false,
                };

                Canvas.SetLeft(led, x * 16 + 2);
                Canvas.SetTop(led, y * 16 + 2);
                ScreenCanvas.Children.Add(led);
                _screenLeds[y, x] = led;
            }
        }
    }

    private void RenderScreen()
    {
        var snapshot = ScreenMirror.GetSnapshot();
        // A late or missing frame leaves the last one up, dimmed, instead of wiping
        // the panel: the clock is usually only busy for a moment.
        var hasFrame = snapshot.LastUpdate != default;
        var showFrame = hasFrame && snapshot.IsOn &&
                        snapshot.State is ScreenMirrorState.Live or ScreenMirrorState.Offline;
        ScreenCanvas.Opacity = snapshot.State == ScreenMirrorState.Live ? 1.0 : 0.45;

        for (var x = 0; x < 32; x++)
        {
            var nextColumn = showFrame ? snapshot.Columns[x] : (byte)0;
            var previousColumn = _renderedScreenColumns[x];
            if (nextColumn == previousColumn) continue;

            for (var y = 0; y < 8; y++)
            {
                var mask = 1 << y;
                if ((nextColumn & mask) == (previousColumn & mask)) continue;
                _screenLeds[y, x].Fill = (nextColumn & mask) != 0
                    ? _screenLedOnBrush
                    : _screenLedOffBrush;
            }

            _renderedScreenColumns[x] = nextColumn;
        }

        var statusKey = snapshot.State switch
        {
            ScreenMirrorState.Offline => "clock.screenOffline",
            ScreenMirrorState.Unsupported => "clock.screenUnsupported",
            ScreenMirrorState.Live when !snapshot.IsOn => "clock.screenOff",
            ScreenMirrorState.Live => null,
            _ => "clock.screenWaiting",
        };

        ScreenStateText.Text = statusKey is null ? string.Empty : AppStrings.Get(statusKey);
        ScreenStateText.Visibility = statusKey is null ? Visibility.Collapsed : Visibility.Visible;
    }

    private void ApplyLanguage()
    {
        PageTitle.Text = AppStrings.Get("clock.title");
        PageSubtitle.Text = AppStrings.Get("clock.subtitle");
        ModelLabel.Text = AppStrings.Get("clock.model");
        TransportLabel.Text = AppStrings.Get("clock.transport");
        AddressLabel.Text = AppStrings.Get("clock.address");
        ConnectionSectionTitle.Text = AppStrings.Get("clock.connectionTitle");
        ConnectionSectionDescription.Text = AppStrings.Get("clock.connectionDescription");
        IpBox.Header = AppStrings.Get("connection.ip");
        PortBox.Header = AppStrings.Get("connection.port");
        UserBox.Header = AppStrings.Get("connection.user");
        PassBox.Header = AppStrings.Get("connection.password");
        StartButtonText.Text = AppStrings.Get("connection.start");
        StopButtonText.Text = AppStrings.Get("connection.stop");
        OpenPanelButtonText.Text = AppStrings.Get("clock.openPanel");
        LastActivityLabel.Text = AppStrings.Get("activity.last");
        UpdateLastActivity();
        UpdateDevicePresence();
        UpdateConnectionState();
        RenderScreen();
    }
}
