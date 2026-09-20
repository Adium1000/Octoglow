using System.Collections.ObjectModel;
using System.Collections.Specialized;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace OctoglowSender;

public sealed partial class ClockIntegrationsPage : Page
{
    private readonly AppConfig _config = ConfigStore.Load();
    private readonly ObservableCollection<NowPlayingPlayer> _videoPlayers = [];
    private readonly ObservableCollection<NowPlayingPlayer> _musicPlayers = [];
    private bool _isRestoringDefaults;

    public ClockIntegrationsPage()
    {
        InitializeComponent();
        Ets2StopPriorityTileSwitch.IsOn = _config.StopPriorityTileWhenTruckStops;
        KeepNowPlayingWhenPausedSwitch.IsOn = _config.KeepNowPlayingWhenPaused;
        foreach (var player in _config.VideoPlayers) _videoPlayers.Add(player.Clone());
        foreach (var player in _config.MusicPlayers) _musicPlayers.Add(player.Clone());
        VideoPlayersList.ItemsSource = _videoPlayers;
        MusicPlayersList.ItemsSource = _musicPlayers;
        Ets2StopPriorityTileSwitch.Toggled += Ets2StopPriorityTileSwitch_Toggled;
        KeepNowPlayingWhenPausedSwitch.Toggled += KeepNowPlayingWhenPausedSwitch_Toggled;
        _videoPlayers.CollectionChanged += PlayerListsChanged;
        _musicPlayers.CollectionChanged += PlayerListsChanged;
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    private void OnLoaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged += OnLanguageChanged;
    private void OnUnloaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged -= OnLanguageChanged;
    private void OnLanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(ApplyLanguage);

    private void AddVideoPlayer_Click(object sender, RoutedEventArgs e) => AddPlayer(VideoPlayerNameBox, _videoPlayers);
    private void AddMusicPlayer_Click(object sender, RoutedEventArgs e) => AddPlayer(MusicPlayerNameBox, _musicPlayers);

    private static void AddPlayer(TextBox input, ObservableCollection<NowPlayingPlayer> players)
    {
        var name = input.Text.Trim();
        if (name.Length == 0 || players.Any(player => string.Equals(player.Name, name, StringComparison.OrdinalIgnoreCase))) return;
        players.Add(new NowPlayingPlayer { Name = name, Match = string.Concat(name.ToLowerInvariant().Where(char.IsLetterOrDigit)), Enabled = true });
        input.Text = "";
    }

    private void DeleteVideoPlayer_Click(object sender, RoutedEventArgs e) => RemovePlayer(sender, _videoPlayers);
    private void DeleteMusicPlayer_Click(object sender, RoutedEventArgs e) => RemovePlayer(sender, _musicPlayers);
    private static void RemovePlayer(object sender, ObservableCollection<NowPlayingPlayer> players)
    {
        if ((sender as FrameworkElement)?.Tag is NowPlayingPlayer player) players.Remove(player);
    }

    private void PlayerListsChanged(object? sender, NotifyCollectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.VideoPlayers = _videoPlayers.Select(player => player.Clone()).ToList();
        _config.MusicPlayers = _musicPlayers.Select(player => player.Clone()).ToList();
        ConfigStore.Save(_config);
    }

    private void Ets2StopPriorityTileSwitch_Toggled(object sender, RoutedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.StopPriorityTileWhenTruckStops = Ets2StopPriorityTileSwitch.IsOn;
        ConfigStore.Save(_config);
    }

    private void KeepNowPlayingWhenPausedSwitch_Toggled(object sender, RoutedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.KeepNowPlayingWhenPaused = KeepNowPlayingWhenPausedSwitch.IsOn;
        ConfigStore.Save(_config);
    }

    private async void RestoreDefaults_Click(object sender, RoutedEventArgs e)
    {
        var isEnglish = AppStrings.IsEnglish;
        var dialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = isEnglish ? "Restore Desk Clock integration defaults?" : "Restabilești integrările Desk Clock?",
            Content = isEnglish
                ? "This resets ETS2, now-playing behavior, and the video/music player lists."
                : "Se resetează opțiunile ETS2, comportamentul now-playing și listele de playere video/muzică.",
            PrimaryButtonText = isEnglish ? "Restore" : "Restabilește",
            CloseButtonText = isEnglish ? "Cancel" : "Anulează",
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dialog.ShowAsync() != ContentDialogResult.Primary) return;

        var defaults = new AppConfig();
        _isRestoringDefaults = true;
        try
        {
            _config.StopPriorityTileWhenTruckStops = defaults.StopPriorityTileWhenTruckStops;
            _config.KeepNowPlayingWhenPaused = defaults.KeepNowPlayingWhenPaused;
            _config.VideoPlayers = defaults.VideoPlayers.Select(player => player.Clone()).ToList();
            _config.MusicPlayers = defaults.MusicPlayers.Select(player => player.Clone()).ToList();
            Ets2StopPriorityTileSwitch.IsOn = _config.StopPriorityTileWhenTruckStops;
            KeepNowPlayingWhenPausedSwitch.IsOn = _config.KeepNowPlayingWhenPaused;
            _videoPlayers.Clear();
            foreach (var player in _config.VideoPlayers) _videoPlayers.Add(player.Clone());
            _musicPlayers.Clear();
            foreach (var player in _config.MusicPlayers) _musicPlayers.Add(player.Clone());
            ConfigStore.Save(_config);
        }
        finally
        {
            _isRestoringDefaults = false;
        }
    }

    private void ApplyLanguage()
    {
        var isEnglish = AppStrings.IsEnglish;
        PageTitle.Text = AppStrings.Get("clock.integrationsTitle");
        PageDescription.Text = AppStrings.Get("clock.integrationsSubtitle");
        Ets2Title.Text = "Euro Truck Simulator 2";
        Ets2StopPriorityTileSwitch.Header = isEnglish
            ? "Stop the priority tile when the truck stops moving"
            : "Oprește tile-ul prioritar când camionul nu se mai mișcă";
        NowPlayingTitle.Text = AppStrings.Get("settings.nowPlaying");
        NowPlayingDescription.Text = AppStrings.Get("settings.nowPlayingDescription");
        KeepNowPlayingWhenPausedSwitch.Header = isEnglish
            ? "Keep showing the current activity while audio or video is paused"
            : "Păstrează activitatea curentă afișată când audio/video este pe pauză";
        VideoPlayersLabel.Text = isEnglish ? "Video players" : "Playere video";
        MusicPlayersLabel.Text = isEnglish ? "Music players" : "Playere muzică";
        VideoPlayerNameBox.PlaceholderText = isEnglish ? "Video player name" : "Nume player video";
        MusicPlayerNameBox.PlaceholderText = isEnglish ? "Music player name" : "Nume player muzică";
        AddVideoButtonText.Text = isEnglish ? "Add" : "Adaugă";
        AddMusicButtonText.Text = isEnglish ? "Add" : "Adaugă";
        RestoreDefaultsButton.Content = AppStrings.Get("clock.restoreIntegrations");
    }
}
