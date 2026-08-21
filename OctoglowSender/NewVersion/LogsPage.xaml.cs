using System.Collections.Specialized;
using Microsoft.UI.Xaml.Controls;
namespace OctoglowSender;
public sealed partial class LogsPage : Page
{
    public LogsPage()
    {
        InitializeComponent();
        LogsList.ItemsSource = AppLog.Entries;
        AppLog.Entries.CollectionChanged += Entries_CollectionChanged;
        AppStrings.LanguageChanged += LanguageChanged;
        ApplyLanguage();
    }
    private void Entries_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) => DispatcherQueue.TryEnqueue(() =>
    {
        if (LogsList.Items.Count > 0) LogsList.ScrollIntoView(LogsList.Items[^1]);
    });
    private void LanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() => { ApplyLanguage(); LogsList.ItemsSource = null; LogsList.ItemsSource = AppLog.Entries; });
    private void ApplyLanguage() { PageTitle.Text = AppStrings.Get("logs.title"); PageDescription.Text = AppStrings.Get("logs.description"); }
}
