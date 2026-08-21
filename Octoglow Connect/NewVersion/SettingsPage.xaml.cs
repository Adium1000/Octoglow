using System.Collections.ObjectModel;
using System.Collections.Specialized;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
namespace OctoglowSender;
public sealed partial class SettingsPage : Page
{
    private readonly AppConfig _config = ConfigStore.Load();
    private readonly ObservableCollection<NowPlayingPlayer> _videoPlayers = [];
    private readonly ObservableCollection<NowPlayingPlayer> _musicPlayers = [];
    private bool _isRestoringDefaults;
    public SettingsPage()
    {
        InitializeComponent();
        LanguageBox.SelectedIndex = _config.Language switch { "ro" => 1, "en" => 2, _ => 0 };
        ThemeBox.SelectedIndex = _config.Theme switch { "light" => 1, "dark" => 2, _ => 0 };
        BackdropBox.SelectedIndex = _config.BackdropMaterial switch { "none" => 0, "mica" => 1, "acrylic" => 3, _ => 2 };
        StartupSwitch.IsOn = _config.RunAtStartup;
        TraySwitch.IsOn = _config.MinimizeToTray;
        Ets2StopPriorityTileSwitch.IsOn = _config.StopPriorityTileWhenTruckStops;
        KeepNowPlayingWhenPausedSwitch.IsOn = _config.KeepNowPlayingWhenPaused;
        foreach (var player in _config.VideoPlayers) _videoPlayers.Add(player.Clone());
        foreach (var player in _config.MusicPlayers) _musicPlayers.Add(player.Clone());
        VideoPlayersList.ItemsSource = _videoPlayers;
        MusicPlayersList.ItemsSource = _musicPlayers;
        _videoPlayers.CollectionChanged += PlayerListsChanged;
        _musicPlayers.CollectionChanged += PlayerListsChanged;
        LanguageBox.SelectionChanged += LanguageBox_SelectionChanged;
        ThemeBox.SelectionChanged += ThemeBox_SelectionChanged;
        BackdropBox.SelectionChanged += BackdropBox_SelectionChanged;
        StartupSwitch.Toggled += StartupSwitch_Toggled;
        TraySwitch.Toggled += TraySwitch_Toggled;
        Ets2StopPriorityTileSwitch.Toggled += Ets2StopPriorityTileSwitch_Toggled;
        KeepNowPlayingWhenPausedSwitch.Toggled += KeepNowPlayingWhenPausedSwitch_Toggled;
        ApplyLanguage();
    }
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
    private void LanguageBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.Language = (LanguageBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() switch
        {
            "ro" => "ro",
            "en" => "en",
            _ => "system"
        };
        ConfigStore.Save(_config);
        AppStrings.SetLanguage(_config.Language);
        ApplyLanguage();
    }
    private void ThemeBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.Theme = (ThemeBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() switch
        {
            "light" => "light",
            "dark" => "dark",
            _ => "system"
        };
        ConfigStore.Save(_config);
        ThemeService.Apply(_config.Theme);
    }
    private void BackdropBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.BackdropMaterial = (BackdropBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() switch
        {
            "none" => "none",
            "mica" => "mica",
            "acrylic" => "acrylic",
            _ => "micaalt"
        };
        ConfigStore.Save(_config);
        ThemeService.ApplyBackdrop(_config.BackdropMaterial);
    }
    private void StartupSwitch_Toggled(object sender, RoutedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.RunAtStartup = StartupSwitch.IsOn;
        if (!StartupService.SetRunAtStartup(_config.RunAtStartup))
        {
            _config.RunAtStartup = !_config.RunAtStartup;
            StartupSwitch.IsOn = _config.RunAtStartup;
            return;
        }
        ConfigStore.Save(_config);
    }
    private void TraySwitch_Toggled(object sender, RoutedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.MinimizeToTray = TraySwitch.IsOn;
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
            Title = isEnglish ? "Restore default settings?" : "Restabilești setările implicite?",
            Content = isEnglish
                ? "This resets the settings on this page and the video/music player lists. Connection details are kept."
                : "Se resetează setările acestei pagini și listele de playere video/muzică. Datele de conectare rămân neschimbate.",
            PrimaryButtonText = isEnglish ? "Restore" : "Restabilește",
            CloseButtonText = isEnglish ? "Cancel" : "Anulează",
            DefaultButton = ContentDialogButton.Close
        };
        if (await dialog.ShowAsync() != ContentDialogResult.Primary) return;
        var defaults = new AppConfig();
        _isRestoringDefaults = true;
        try
        {
            _config.Language = defaults.Language;
            _config.Theme = defaults.Theme;
            _config.BackdropMaterial = defaults.BackdropMaterial;
            _config.RunAtStartup = defaults.RunAtStartup;
            _config.MinimizeToTray = defaults.MinimizeToTray;
            _config.StopPriorityTileWhenTruckStops = defaults.StopPriorityTileWhenTruckStops;
            _config.KeepNowPlayingWhenPaused = defaults.KeepNowPlayingWhenPaused;
            _config.VideoPlayers = defaults.VideoPlayers.Select(player => player.Clone()).ToList();
            _config.MusicPlayers = defaults.MusicPlayers.Select(player => player.Clone()).ToList();

            LanguageBox.SelectedIndex = _config.Language switch { "ro" => 1, "en" => 2, _ => 0 };
            ThemeBox.SelectedIndex = _config.Theme switch { "light" => 1, "dark" => 2, _ => 0 };
            BackdropBox.SelectedIndex = _config.BackdropMaterial switch { "none" => 0, "mica" => 1, "acrylic" => 3, _ => 2 };
            StartupSwitch.IsOn = _config.RunAtStartup;
            TraySwitch.IsOn = _config.MinimizeToTray;
            Ets2StopPriorityTileSwitch.IsOn = _config.StopPriorityTileWhenTruckStops;
            KeepNowPlayingWhenPausedSwitch.IsOn = _config.KeepNowPlayingWhenPaused;

            _videoPlayers.Clear();
            foreach (var player in _config.VideoPlayers) _videoPlayers.Add(player.Clone());
            _musicPlayers.Clear();
            foreach (var player in _config.MusicPlayers) _musicPlayers.Add(player.Clone());

            StartupService.SetRunAtStartup(_config.RunAtStartup);
            ConfigStore.Save(_config);
            ThemeService.Apply(_config.Theme);
            ThemeService.ApplyBackdrop(_config.BackdropMaterial);
        }
        finally
        {
            _isRestoringDefaults = false;
        }
        AppStrings.SetLanguage(_config.Language);
        ApplyLanguage();
    }
    private void ApplyLanguage()
    {
        var isEnglish = AppStrings.IsEnglish;
        PageTitle.Text = AppStrings.Get("settings.title"); LanguageTitle.Text = AppStrings.Get("settings.language");
        LanguageSystemItem.Content = AppStrings.Get("settings.language.system");
        ThemeTitle.Text = AppStrings.Get("settings.theme");
        ThemeSystemItem.Content = AppStrings.Get("settings.theme.system");
        ThemeLightItem.Content = AppStrings.Get("settings.theme.light");
        ThemeDarkItem.Content = AppStrings.Get("settings.theme.dark");
        BackdropTitle.Text = AppStrings.Get("settings.backdrop");
        BackdropNoneItem.Content = AppStrings.Get("settings.backdrop.none");
        BackdropMicaItem.Content = AppStrings.Get("settings.backdrop.mica");
        BackdropMicaAltItem.Content = AppStrings.Get("settings.backdrop.micaalt");
        BackdropAcrylicItem.Content = AppStrings.Get("settings.backdrop.acrylic");
        AppBehaviorTitle.Text = AppStrings.Get("settings.appBehavior");
        StartupSwitch.Header = AppStrings.Get("settings.startup"); TraySwitch.Header = AppStrings.Get("settings.tray");
        Ets2StopPriorityTileSwitch.Header = isEnglish
            ? "If the truck stops moving, stop the priority tile"
            : "Oprește tile-ul prioritar când camionul nu se mai mișcă";
        Ets2Title.Text = "Euro Truck Simulator";
        NowPlayingTitle.Text = AppStrings.Get("settings.nowPlaying"); NowPlayingDescription.Text = AppStrings.Get("settings.nowPlayingDescription");
        KeepNowPlayingWhenPausedSwitch.Header = isEnglish
            ? "Keep showing the current activity when audio or video is paused"
            : "Păstrează activitatea curentă afișată când audio/video este pe pauză";
        VideoPlayersLabel.Text = isEnglish ? "Video players" : "Playere video";
        MusicPlayersLabel.Text = isEnglish ? "Music players" : "Playere muzica";
        VideoPlayerNameBox.PlaceholderText = isEnglish ? "Video player name" : "Nume player video";
        MusicPlayerNameBox.PlaceholderText = isEnglish ? "Music player name" : "Nume player muzica";
        AddVideoButtonText.Text = isEnglish ? "Add" : "Adauga";
        AddMusicButtonText.Text = isEnglish ? "Add" : "Adauga";
        RestoreDefaultsButton.Content = isEnglish ? "Restore default settings" : "Restabilește setările implicite";
    }
}
