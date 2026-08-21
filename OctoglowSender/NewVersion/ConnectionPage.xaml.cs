using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Specialized;
namespace OctoglowSender;
public sealed partial class ConnectionPage : Page
{
    private readonly AppConfig _config = ConfigStore.Load();
    private SenderBackend? _backend;
    public ConnectionPage()
    {
        InitializeComponent();
        AppStrings.LanguageChanged += (_, _) => DispatcherQueue.TryEnqueue(ApplyLanguage);
        ApplyLanguage();
        IpBox.Text = _config.Esp32Ip; PortBox.Text = _config.Esp32Port.ToString(); UserBox.Text = _config.ScUser; PassBox.Password = _config.ScPass;
        AppLog.Entries.CollectionChanged += Entries_CollectionChanged;
        ConnectionStatus.Changed += (_, _) => DispatcherQueue.TryEnqueue(UpdateConnectionState);
        UpdateLastActivity();
        UpdateConnectionState();
    }
    private void Start_Click(object sender, RoutedEventArgs e)
        => StartSending();

    public void StartSending()
    {
        var config = ConfigStore.Load();
        config.Esp32Ip = IpBox.Text.Trim(); config.Esp32Port = int.TryParse(PortBox.Text, out var port) ? port : 80; config.ScUser = UserBox.Text.Trim(); config.ScPass = PassBox.Password;
        ConfigStore.Save(config); _backend?.Stop(); ConnectionStatus.Set(ConnectionStatusKind.Connecting); _backend = new SenderBackend(config, message =>
        {
            AppLog.Write(message);
            ConnectionStatus.ProcessBackendMessage(message);
        });
        _backend.Start();
    }
    private void Stop_Click(object sender, RoutedEventArgs e) { _backend?.Stop(); ConnectionStatus.Set(ConnectionStatusKind.Disconnected); AppLog.Write(AppStrings.Get("connection.stopped")); }
    private void Entries_CollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) => DispatcherQueue.TryEnqueue(UpdateLastActivity);
    private void UpdateLastActivity()
    {
        var activity = AppLog.Entries.LastOrDefault();
        if (activity is null)
        {
            LastActivityText.Text = AppStrings.Get("activity.empty");
            ActivityIcon.Glyph = "\uE823";
            return;
        }

        LastActivityText.Text = activity.Description;
        ActivityIcon.Glyph = activity.Glyph;
    }
    private void UpdateConnectionState()
    {
        LastActivityCard.Visibility = ConnectionStatus.IsConnected ? Visibility.Visible : Visibility.Collapsed;
        (ConnectionStateIcon.Glyph, ConnectionStateText.Text) = ConnectionStatus.Current switch
        {
            ConnectionStatusKind.Connecting => ("\uE895", AppStrings.Get("connection.connecting")),
            ConnectionStatusKind.Connected => ("\uE73E", AppStrings.Get("connection.connected")),
            ConnectionStatusKind.Failed => ("\uE783", AppStrings.Get("connection.failed")),
            _ => ("\uE711", AppStrings.Get("connection.disconnected"))
        };
    }
    private void ApplyLanguage() { PageTitle.Text = AppStrings.Get("connection.title"); IpBox.Header = AppStrings.Get("connection.ip"); PortBox.Header = AppStrings.Get("connection.port"); UserBox.Header = AppStrings.Get("connection.user"); PassBox.Header = AppStrings.Get("connection.password"); StartButtonText.Text = AppStrings.Get("connection.start"); StopButtonText.Text = AppStrings.Get("connection.stop"); LastActivityLabel.Text = AppStrings.Get("activity.last"); UpdateLastActivity(); UpdateConnectionState(); }
}
