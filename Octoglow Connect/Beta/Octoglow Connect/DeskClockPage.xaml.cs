using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;

namespace OctoglowSender;

public sealed partial class DeskClockPage : Page
{
    private DeviceDescriptor? _device;
    private bool _ready;

    public DeskClockPage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        _device = e.Parameter as DeviceDescriptor;
        _ready = true;
        OverviewItem.IsSelected = true;
        NavigateTo("overview");
    }

    private void OnLoaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged += OnLanguageChanged;
    private void OnUnloaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged -= OnLanguageChanged;
    private void OnLanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(ApplyLanguage);

    private void ClockNavigation_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        if (!_ready) return;
        var tag = (args.SelectedItemContainer as NavigationViewItem)?.Tag?.ToString() ?? "overview";
        NavigateTo(tag);
    }

    private void NavigateTo(string section)
    {
        var pageType = section switch
        {
            "integrations" => typeof(ClockIntegrationsPage),
            "activity" => typeof(LogsPage),
            _ => typeof(ConnectionPage),
        };

        if (ClockContentFrame.CurrentSourcePageType == pageType) return;
        ClockContentFrame.Navigate(pageType, _device);
    }

    private void ApplyLanguage()
    {
        OverviewItem.Content = AppStrings.Get("clock.overviewTab");
        IntegrationsItem.Content = AppStrings.Get("clock.integrationsTab");
        ActivityItem.Content = AppStrings.Get("clock.activityTab");
    }
}
