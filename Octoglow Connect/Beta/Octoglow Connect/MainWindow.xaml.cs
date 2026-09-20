using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace OctoglowSender;

public sealed partial class MainWindow : Window
{
    private const int InitialWidth = 1060;
    private const int InitialHeight = 760;
    private const int MinimumWidth = 880;
    private const int MinimumHeight = 640;

    private readonly Dictionary<string, NavigationViewItem> _deviceItems =
        new(StringComparer.OrdinalIgnoreCase);

    private string? _selectedDeviceId;
    private string _currentRoute = "home";
    private bool _isClosed;

    public MainWindow()
    {
        InitializeComponent();

        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        AppWindow.Resize(new SizeInt32(InitialWidth, InitialHeight));
        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.PreferredMinimumWidth = MinimumWidth;
            presenter.PreferredMinimumHeight = MinimumHeight;
        }

        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;
        AppWindow.TitleBar.ButtonInactiveBackgroundColor = Microsoft.UI.Colors.Transparent;
        SystemBackdrop = new MicaBackdrop { Kind = MicaKind.BaseAlt };

        AppStrings.LanguageChanged += AppStrings_LanguageChanged;
        App.DeviceHub.DevicesChanged += DeviceHub_DevicesChanged;
        Closed += MainWindow_Closed;

        ApplyLanguage();
        RebuildDeviceItems();
        ContentFrame.Navigate(typeof(HubPage));
    }

    public void NavigateToDevice(DeviceDescriptor device)
    {
        ArgumentNullException.ThrowIfNull(device);

        var pageType = device.Kind == DeviceKind.Macro
            ? typeof(Devices.Macro.MacroDevicePage)
            : typeof(DeskClockPage);
        var alreadyShowing = _currentRoute == "device" &&
                             string.Equals(_selectedDeviceId, device.Id, StringComparison.OrdinalIgnoreCase) &&
                             ContentFrame.CurrentSourcePageType == pageType;

        _selectedDeviceId = device.Id;
        _currentRoute = "device";
        SelectDeviceItem(device.Id);

        if (!alreadyShowing)
            ContentFrame.Navigate(pageType, device);
    }

    public void OpenDeskClockPanel(DeviceDescriptor device)
    {
        ArgumentNullException.ThrowIfNull(device);

        if (device.Kind != DeviceKind.DeskClock)
        {
            NavigateToDevice(device);
            return;
        }

        var alreadyShowing = _currentRoute == "desk-clock-panel" &&
                             string.Equals(_selectedDeviceId, device.Id, StringComparison.OrdinalIgnoreCase) &&
                             ContentFrame.CurrentSourcePageType == typeof(CustomizePage);

        _selectedDeviceId = device.Id;
        _currentRoute = "desk-clock-panel";
        SelectDeviceItem(device.Id);
        if (!alreadyShowing)
            ContentFrame.Navigate(typeof(CustomizePage), device);
    }

    private void RootNavigation_ItemInvoked(
        NavigationView sender,
        NavigationViewItemInvokedEventArgs args)
    {
        if (args.InvokedItemContainer is not NavigationViewItem item)
            return;

        if (item.Tag is DeviceDescriptor device)
        {
            NavigateToDevice(device);
            return;
        }

        switch (item.Tag?.ToString())
        {
            case "settings":
                NavigateToStaticPage("settings", typeof(SettingsPage));
                break;
            case "about":
                NavigateToStaticPage("about", typeof(AboutPage));
                break;
            default:
                NavigateToStaticPage("home", typeof(HubPage));
                break;
        }
    }

    private void NavigateToStaticPage(string route, Type pageType)
    {
        if (_currentRoute == route && ContentFrame.CurrentSourcePageType == pageType)
            return;

        _currentRoute = route;
        ContentFrame.Navigate(pageType);
    }

    private void RebuildDeviceItems()
    {
        foreach (var item in _deviceItems.Values)
            RootNavigation.MenuItems.Remove(item);

        _deviceItems.Clear();

        foreach (var device in App.DeviceHub.Devices)
        {
            var item = CreateDeviceItem(device);
            _deviceItems[device.Id] = item;
            RootNavigation.MenuItems.Add(item);
        }

        if (!IsShowingDevice())
            return;

        if (_selectedDeviceId is not null && _deviceItems.ContainsKey(_selectedDeviceId))
        {
            SelectDeviceItem(_selectedDeviceId);
            return;
        }

        _selectedDeviceId = null;
        _currentRoute = "home";
        RootNavigation.SelectedItem = HomeNavItem;
        ContentFrame.Navigate(typeof(HubPage));
    }

    private NavigationViewItem CreateDeviceItem(DeviceDescriptor device)
    {
        var nameText = new TextBlock
        {
            Text = device.DisplayName,
            MaxLines = 1,
            TextTrimming = TextTrimming.CharacterEllipsis,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold
        };

        var state = AppStrings.Get(device.IsOnline ? "device.connected" : "device.disconnected");
        var detail = BuildDeviceDetail(state, device.Detail);
        var detailText = new TextBlock
        {
            Text = detail,
            MaxLines = 1,
            TextTrimming = TextTrimming.CharacterEllipsis,
            FontSize = 12,
            Opacity = 0.68
        };

        var content = new StackPanel { Spacing = 1 };
        content.Children.Add(nameText);
        content.Children.Add(detailText);

        var item = new NavigationViewItem
        {
            Tag = device,
            Content = content,
            Icon = new SymbolIcon(device.Kind == DeviceKind.Macro ? Symbol.Keyboard : Symbol.Clock),
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            IsEnabled = true
        };

        ToolTipService.SetToolTip(item, $"{device.DisplayName}\n{detail}");
        return item;
    }

    private static string BuildDeviceDetail(string state, string deviceDetail)
    {
        if (string.IsNullOrWhiteSpace(deviceDetail))
            return state;

        var separator = deviceDetail.IndexOf('·');
        var startsWithState = deviceDetail.StartsWith("Online", StringComparison.OrdinalIgnoreCase) ||
                              deviceDetail.StartsWith("Offline", StringComparison.OrdinalIgnoreCase) ||
                              deviceDetail.StartsWith("Connected", StringComparison.OrdinalIgnoreCase) ||
                              deviceDetail.StartsWith("Disconnected", StringComparison.OrdinalIgnoreCase);

        if (!startsWithState)
            return $"{state} · {deviceDetail}";

        return separator >= 0 && separator + 1 < deviceDetail.Length
            ? $"{state} · {deviceDetail[(separator + 1)..].Trim()}"
            : state;
    }

    private void SelectDeviceItem(string deviceId)
    {
        if (_deviceItems.TryGetValue(deviceId, out var item))
            RootNavigation.SelectedItem = item;
    }

    private bool IsShowingDevice() =>
        _currentRoute is "device" or "desk-clock-panel";

    private void ApplyLanguage()
    {
        Title = AppStrings.Get("app.title");
        WindowTitle.Text = Title;
        HomeNavItem.Content = AppStrings.Get("nav.home");
        DevicesHeader.Content = AppStrings.Get("nav.devices");
        SettingsNavItem.Content = AppStrings.Get("nav.settings");
        AboutNavItem.Content = AppStrings.Get("nav.about");
    }

    private void AppStrings_LanguageChanged(object? sender, EventArgs e) =>
        DispatcherQueue.TryEnqueue(() =>
        {
            ApplyLanguage();
            RebuildDeviceItems();
        });

    private void DeviceHub_DevicesChanged(object? sender, EventArgs e) =>
        DispatcherQueue.TryEnqueue(RebuildDeviceItems);

    private void MainWindow_Closed(object sender, WindowEventArgs args)
    {
        if (_isClosed)
            return;

        _isClosed = true;
        AppStrings.LanguageChanged -= AppStrings_LanguageChanged;
        App.DeviceHub.DevicesChanged -= DeviceHub_DevicesChanged;
        Closed -= MainWindow_Closed;
    }
}
