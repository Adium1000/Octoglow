using System.Collections.Specialized;
using Microsoft.UI.Xaml.Controls;
namespace OctoglowSender;
public sealed partial class LogsPage : Page
{
    private bool _subscribed;

    public LogsPage()
    {
        InitializeComponent();
        LogsList.ItemsSource = AppLog.DeskClockEntries;
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    private void OnLoaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (_subscribed) return;
        _subscribed = true;
        AppLog.DeskClockEntries.CollectionChanged += Entries_CollectionChanged;
        AppStrings.LanguageChanged += LanguageChanged;
        if (LogsList.Items.Count > 0) LogsList.ScrollIntoView(LogsList.Items[^1]);
    }

    private void OnUnloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        if (!_subscribed) return;
        _subscribed = false;
        AppLog.DeskClockEntries.CollectionChanged -= Entries_CollectionChanged;
        AppStrings.LanguageChanged -= LanguageChanged;
    }
    private void Entries_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) => DispatcherQueue.TryEnqueue(() =>
    {
        if (LogsList.Items.Count > 0) LogsList.ScrollIntoView(LogsList.Items[^1]);
    });
    private void LanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() => { ApplyLanguage(); LogsList.ItemsSource = null; LogsList.ItemsSource = AppLog.DeskClockEntries; });
    private void ApplyLanguage() { PageTitle.Text = AppStrings.Get("logs.title"); PageDescription.Text = AppStrings.Get("logs.description"); }
}
