using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace OctoglowSender;

public sealed partial class SettingsPage : Page
{
    private readonly AppConfig _config = ConfigStore.Load();
    private bool _isRestoringDefaults;

    public SettingsPage()
    {
        InitializeComponent();
        LanguageBox.SelectedIndex = _config.Language switch { "ro" => 1, "en" => 2, _ => 0 };
        ThemeBox.SelectedIndex = _config.Theme switch { "light" => 1, "dark" => 2, _ => 0 };
        BackdropBox.SelectedIndex = _config.BackdropMaterial switch { "none" => 0, "mica" => 1, "acrylic" => 3, _ => 2 };
        StartupSwitch.IsOn = _config.RunAtStartup;
        TraySwitch.IsOn = _config.MinimizeToTray;

        LanguageBox.SelectionChanged += LanguageBox_SelectionChanged;
        ThemeBox.SelectionChanged += ThemeBox_SelectionChanged;
        BackdropBox.SelectionChanged += BackdropBox_SelectionChanged;
        StartupSwitch.Toggled += StartupSwitch_Toggled;
        TraySwitch.Toggled += TraySwitch_Toggled;
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    private void OnLoaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged += OnLanguageChanged;
    private void OnUnloaded(object sender, RoutedEventArgs e) => AppStrings.LanguageChanged -= OnLanguageChanged;
    private void OnLanguageChanged(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(ApplyLanguage);

    private void LanguageBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.Language = (LanguageBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() switch
        {
            "ro" => "ro", "en" => "en", _ => "system",
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
            "light" => "light", "dark" => "dark", _ => "system",
        };
        ConfigStore.Save(_config);
        ThemeService.Apply(_config.Theme);
    }

    private void BackdropBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_isRestoringDefaults) return;
        _config.BackdropMaterial = (BackdropBox.SelectedItem as ComboBoxItem)?.Tag?.ToString() switch
        {
            "none" => "none", "mica" => "mica", "acrylic" => "acrylic", _ => "micaalt",
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

    private async void ClearDevices_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = AppStrings.Get("settings.clearDevicesConfirmTitle"),
            Content = AppStrings.Get("settings.clearDevicesConfirmDescription"),
            PrimaryButtonText = AppStrings.Get("settings.clearDevices"),
            CloseButtonText = AppStrings.Get("hub.cancel"),
            DefaultButton = ContentDialogButton.Close,
        };
        if (await dialog.ShowAsync() != ContentDialogResult.Primary) return;

        ClearDevicesButton.IsEnabled = false;
        try
        {
            DeskClockSession.Stop();
            _config.Esp32Ip = "";
            _config.Esp32Port = 80;
            ConfigStore.Save(_config);
            await App.DeviceHub.ClearAllDevicesAsync();
        }
        finally
        {
            ClearDevicesButton.IsEnabled = true;
        }
    }

    private async void RestoreDefaults_Click(object sender, RoutedEventArgs e)
    {
        var isEnglish = AppStrings.IsEnglish;
        var dialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = isEnglish ? "Restore app defaults?" : "Restabilești setările aplicației?",
            Content = isEnglish
                ? "This resets appearance, startup, and tray behavior. Desk Clock settings are kept."
                : "Se resetează aspectul, pornirea automată și comportamentul tray. Setările Desk Clock rămân neschimbate.",
            PrimaryButtonText = isEnglish ? "Restore" : "Restabilește",
            CloseButtonText = isEnglish ? "Cancel" : "Anulează",
            DefaultButton = ContentDialogButton.Close,
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
            LanguageBox.SelectedIndex = 0;
            ThemeBox.SelectedIndex = 0;
            BackdropBox.SelectedIndex = 2;
            StartupSwitch.IsOn = _config.RunAtStartup;
            TraySwitch.IsOn = _config.MinimizeToTray;
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
        PageTitle.Text = AppStrings.Get("settings.title");
        PageSubtitle.Text = AppStrings.Get("settings.appSubtitle");
        AppearanceTitle.Text = AppStrings.Get("settings.appearance");
        LanguageTitle.Text = AppStrings.Get("settings.language");
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
        StartupSwitch.Header = AppStrings.Get("settings.startup");
        TraySwitch.Header = AppStrings.Get("settings.tray");
        DevicesSettingsTitle.Text = AppStrings.Get("settings.devices");
        DevicesSettingsDescription.Text = AppStrings.Get("settings.devicesDescription");
        ClearDevicesButton.Content = AppStrings.Get("settings.clearDevices");
        RestoreDefaultsButton.Content = isEnglish ? "Restore app defaults" : "Restabilește setările aplicației";
    }
}
