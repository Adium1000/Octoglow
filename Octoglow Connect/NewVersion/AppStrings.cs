namespace OctoglowSender;
public static class AppStrings
{
    private static string _language = ResolveLanguage(ConfigStore.Load().Language);
    public static bool IsEnglish => _language == "en";
    public static event EventHandler? LanguageChanged;
    private static readonly Dictionary<string, (string ro, string en)> Text = new()
    {
        ["app.title"] = ("Octoglow Connect", "Octoglow Connect"),
        ["nav.connection"] = ("Conexiune", "Connection"), ["nav.customize"] = ("Customize", "Customize"), ["nav.settings"] = ("Setări", "Settings"), ["nav.logs"] = ("Jurnal activitate", "Activity log"), ["nav.about"] = ("Despre", "About"),
        ["connection.title"] = ("Conexiune", "Connection"), ["connection.ip"] = ("Adresă IP", "IP address"), ["connection.port"] = ("Port", "Port"), ["connection.user"] = ("Utilizator", "Username"), ["connection.password"] = ("Parolă", "Password"), ["connection.start"] = ("Pornește și salvează", "Start and save"), ["connection.stop"] = ("Oprește", "Stop"), ["connection.starting"] = ("Pornire...", "Starting..."), ["connection.stopped"] = ("Oprit.", "Stopped."), ["connection.connecting"] = ("Se conectează…", "Connecting…"), ["connection.connected"] = ("Conectat", "Connected"), ["connection.failed"] = ("Conexiune eșuată", "Connection failed"), ["connection.disconnected"] = ("Deconectat", "Disconnected"),
        ["settings.title"] = ("Setări", "Settings"), ["settings.language"] = ("Limbă", "Language"), ["settings.language.system"] = ("Sistem implicit", "System default"), ["settings.theme"] = ("Temă", "Theme"), ["settings.theme.system"] = ("Sistem implicit", "System default"), ["settings.theme.light"] = ("Luminos", "Light"), ["settings.theme.dark"] = ("Întunecat", "Dark"), ["settings.backdrop"] = ("Material fundal", "Backdrop material"), ["settings.backdrop.none"] = ("Fără", "None"), ["settings.backdrop.mica"] = ("Mica", "Mica"), ["settings.backdrop.micaalt"] = ("Mica Alt", "Mica Alt"), ["settings.backdrop.acrylic"] = ("Acrylic", "Acrylic"), ["settings.appBehavior"] = ("Comportament aplicație", "App behavior"), ["settings.startup"] = ("Pornește odată cu Windows", "Run at Windows startup"), ["settings.tray"] = ("Minimizează în zona de notificări", "Minimize to notification area"), ["settings.nowPlaying"] = ("Video / Muzică în redare", "Video / Music now playing"), ["settings.nowPlayingDescription"] = ("Listele păstrează clasificarea trimisă la HTTP: kind=video sau kind=music.", "Lists preserve the HTTP classification: kind=video or kind=music."), ["settings.video"] = ("Playere video (un nume pe rând)", "Video players (one name per line)"), ["settings.music"] = ("Playere muzică (un nume pe rând)", "Music players (one name per line)"), ["settings.save"] = ("Salvează setările", "Save settings"),
        ["logs.title"] = ("Jurnal de activitate", "Activity log"), ["logs.description"] = ("Evenimentele aplicației apar aici în timp real.", "Application events appear here in real time."),
        ["activity.last"] = ("Ultima activitate", "Latest activity"), ["activity.empty"] = ("Încă nu există activitate.", "No activity yet."),
        ["customize.title"] = ("Customize", "Customize"), ["customize.description"] = ("Panoul dispozitivului se deschide folosind datele salvate la Conexiune.", "The device panel opens using the credentials saved in Connection."), ["customize.reload"] = ("Reîncarcă", "Reload"), ["customize.missing"] = ("Salvează mai întâi o adresă IP validă în pagina Conexiune.", "Save a valid IP address in the Connection page first.")
    };
    public static string Get(string key) => Text.TryGetValue(key, out var value) ? (_language == "en" ? value.en : value.ro) : key;
    public static void SetLanguage(string language) { _language = ResolveLanguage(language); LanguageChanged?.Invoke(null, EventArgs.Empty); }
    /// <summary>Turns a stored language ("system" | "ro" | "en") into the effective "ro"/"en" used for display text.</summary>
    private static string ResolveLanguage(string language) => language switch
    {
        "en" => "en",
        "ro" => "ro",
        _ => System.Globalization.CultureInfo.CurrentUICulture.TwoLetterISOLanguageName.Equals("ro", StringComparison.OrdinalIgnoreCase) ? "ro" : "en"
    };
}
