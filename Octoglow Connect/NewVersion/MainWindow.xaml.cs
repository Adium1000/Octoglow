using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Windowing;
namespace OctoglowSender;
public sealed partial class MainWindow : Window
{
    private const int InitialWidth = 871;
    private const int InitialHeight = 820;
    public MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        AppWindow.Resize(new SizeInt32(InitialWidth, InitialHeight));
        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            // Nu permitem micșorarea ferestrei sub rezoluția cu care s-a deschis prima dată.
            presenter.PreferredMinimumWidth = InitialWidth;
            presenter.PreferredMinimumHeight = InitialHeight;
        }
        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;
        AppWindow.TitleBar.ButtonInactiveBackgroundColor = Microsoft.UI.Colors.Transparent;
        SystemBackdrop = new MicaBackdrop { Kind = MicaKind.BaseAlt };
        AppStrings.LanguageChanged += (_, _) => ApplyLanguage();
        ConnectionStatus.Changed += (_, _) => DispatcherQueue.TryEnqueue(UpdateCustomizeAccess);
        ApplyLanguage();
        UpdateCustomizeAccess();
        ContentFrame.Navigate(typeof(ConnectionPage));
    }
    private void ApplyLanguage()
    {
        Title = AppStrings.Get("app.title");
        WindowTitle.Text = Title;
        ConnectionNavItem.Content = AppStrings.Get("nav.connection");
        CustomizeNavItem.Content = AppStrings.Get("nav.customize");
        SettingsNavItem.Content = AppStrings.Get("nav.settings");
        LogsNavItem.Content = AppStrings.Get("nav.logs");
        AboutNavItem.Content = AppStrings.Get("nav.about");
    }
    private void UpdateCustomizeAccess() => CustomizeNavItem.IsEnabled = ConnectionStatus.IsConnected;
    public void StartSending()
    {
        if (ContentFrame.Content is ConnectionPage connectionPage)
            connectionPage.StartSending();
    }
    private void RootNavigation_SelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
    {
        var tag = (args.SelectedItem as NavigationViewItem)?.Tag?.ToString();
        ContentFrame.Navigate(tag switch
        {
            "settings" => typeof(SettingsPage),
            "logs" => typeof(LogsPage),
            "customize" => typeof(CustomizePage),
            "about" => typeof(AboutPage),
            _ => typeof(ConnectionPage)
        });
    }
}
