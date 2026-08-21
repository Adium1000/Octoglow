using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
namespace OctoglowSender;
public class NowPlayingPlayer
{
    public string Name { get; set; } = "";
    public string Match { get; set; } = "";
    public bool Enabled { get; set; } = true;
    public bool BuiltIn { get; set; } = false;

    public NowPlayingPlayer Clone() => new() { Name = Name, Match = Match, Enabled = Enabled, BuiltIn = BuiltIn };
}
public class AppConfig
{
    public string Esp32Ip { get; set; } = "192.168.0.170";
    public int Esp32Port { get; set; } = 80;
    public string ScUser { get; set; } = "";
    public string ScPass { get; set; } = ""; // decrypted in-memory; encrypted on disk
    public int PollIntervalSeconds { get; set; } = 3;
    public int SendIntervalSeconds { get; set; } = 10;
    public int NotifMaxChars { get; set; } = 110;
    public int Ets2PollIntervalMs { get; set; } = 500;
    public bool StopPriorityTileWhenTruckStops { get; set; } = true;
    public bool RunAtStartup { get; set; } = true;
    public bool MinimizeToTray { get; set; } = true;
    /// <summary>"system" | "light" | "dark"</summary>
    public string Theme { get; set; } = "system";
    /// <summary>"none" | "mica" | "micaalt" | "acrylic"</summary>
    public string BackdropMaterial { get; set; } = "micaalt";
    public bool KeepNowPlayingWhenPaused { get; set; } = false;
    /// <summary>"system" | "ro" | "en"</summary>
    public string Language { get; set; } = "system";
    public List<NowPlayingPlayer> VideoPlayers { get; set; } = DefaultVideoPlayers();
    public List<NowPlayingPlayer> MusicPlayers { get; set; } = DefaultMusicPlayers();
    public static List<NowPlayingPlayer> DefaultVideoPlayers() =>
    [
        new() { Name = "Google Chrome", Match = "chrome", BuiltIn = true },
        new() { Name = "Microsoft Edge", Match = "msedge", BuiltIn = true },
        new() { Name = "Opera", Match = "opera", BuiltIn = true },
        new() { Name = "Firefox", Match = "firefox", BuiltIn = true },
        new() { Name = "Brave", Match = "brave", BuiltIn = true },
    ];

    public static List<NowPlayingPlayer> DefaultMusicPlayers() =>
    [
        new() { Name = "Apple Music", Match = "applemusic", BuiltIn = true },
        new() { Name = "SoundCloud", Match = "soundcloud", BuiltIn = true },
        new() { Name = "Spotify", Match = "spotify", BuiltIn = true },
    ];
}

public static class ConfigStore
{
    private static volatile bool _stopPriorityTileWhenTruckStops = true;
    private static volatile bool _keepNowPlayingWhenPaused = true;

    public static bool StopPriorityTileWhenTruckStops => _stopPriorityTileWhenTruckStops;
    public static bool KeepNowPlayingWhenPaused => _keepNowPlayingWhenPaused;

    private static readonly string ConfigDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "OctoglowSender");

    private static readonly string ConfigPath = Path.Combine(ConfigDir, "config.json");
    private class OnDisk
    {
        public string Esp32Ip { get; set; } = "192.168.0.170";
        public int Esp32Port { get; set; } = 80;
        public string ScUser { get; set; } = "";
        public string ScPassEncrypted { get; set; } = "";
        public int PollIntervalSeconds { get; set; } = 3;
        public int SendIntervalSeconds { get; set; } = 10;
        public int NotifMaxChars { get; set; } = 110;
        public int Ets2PollIntervalMs { get; set; } = 500;
        public bool StopPriorityTileWhenTruckStops { get; set; } = true;
        public bool RunAtStartup { get; set; } = true;
        public bool MinimizeToTray { get; set; } = true;
        public bool DarkMode { get; set; } = false; // kept only to migrate configs saved before the Theme option existed
        public string? Theme { get; set; }
        public string? BackdropMaterial { get; set; }
        public bool KeepNowPlayingWhenPaused { get; set; } = false;
        public string Language { get; set; } = "system";
        public List<NowPlayingPlayer> VideoPlayers { get; set; } = AppConfig.DefaultVideoPlayers();
        public List<NowPlayingPlayer> MusicPlayers { get; set; } = AppConfig.DefaultMusicPlayers();
    }
    public static AppConfig Load()
    {
        var cfg = new AppConfig();
        try
        {
            if (!File.Exists(ConfigPath))
                return cfg;

            var json = File.ReadAllText(ConfigPath);
            var onDisk = JsonSerializer.Deserialize<OnDisk>(json);
            if (onDisk is null) return cfg;

            cfg.Esp32Ip = onDisk.Esp32Ip;
            cfg.Esp32Port = onDisk.Esp32Port;
            cfg.ScUser = onDisk.ScUser;
            cfg.PollIntervalSeconds = onDisk.PollIntervalSeconds;
            cfg.SendIntervalSeconds = onDisk.SendIntervalSeconds;
            cfg.NotifMaxChars = onDisk.NotifMaxChars;
            cfg.Ets2PollIntervalMs = onDisk.Ets2PollIntervalMs;
            cfg.StopPriorityTileWhenTruckStops = onDisk.StopPriorityTileWhenTruckStops;
            cfg.RunAtStartup = onDisk.RunAtStartup;
            cfg.MinimizeToTray = onDisk.MinimizeToTray;
            cfg.Theme = onDisk.Theme switch
            {
                "light" => "light",
                "dark" => "dark",
                "system" => "system",
                _ => onDisk.DarkMode ? "dark" : "system" // migrate configs saved before the Theme option existed
            };
            cfg.BackdropMaterial = onDisk.BackdropMaterial switch
            {
                "none" => "none",
                "mica" => "mica",
                "micaalt" => "micaalt",
                "acrylic" => "acrylic",
                _ => "micaalt" // default / migrate configs saved before this option existed (matches the previous hardcoded backdrop)
            };
            cfg.KeepNowPlayingWhenPaused = onDisk.KeepNowPlayingWhenPaused;
            cfg.Language = onDisk.Language is "en" or "ro" ? onDisk.Language : "system";
            cfg.VideoPlayers = onDisk.VideoPlayers?.Select(player => player.Clone()).ToList() ?? AppConfig.DefaultVideoPlayers();
            cfg.MusicPlayers = onDisk.MusicPlayers?.Select(player => player.Clone()).ToList() ?? AppConfig.DefaultMusicPlayers();
            cfg.ScPass = Decrypt(onDisk.ScPassEncrypted);
        }
        catch
        {

        }
        _stopPriorityTileWhenTruckStops = cfg.StopPriorityTileWhenTruckStops;
        _keepNowPlayingWhenPaused = cfg.KeepNowPlayingWhenPaused;
        return cfg;
    }

    public static void Save(AppConfig cfg)
    {
        _stopPriorityTileWhenTruckStops = cfg.StopPriorityTileWhenTruckStops;
        _keepNowPlayingWhenPaused = cfg.KeepNowPlayingWhenPaused;
        Directory.CreateDirectory(ConfigDir);
        var onDisk = new OnDisk
        {
            Esp32Ip = cfg.Esp32Ip,
            Esp32Port = cfg.Esp32Port,
            ScUser = cfg.ScUser,
            PollIntervalSeconds = cfg.PollIntervalSeconds,
            SendIntervalSeconds = cfg.SendIntervalSeconds,
            NotifMaxChars = cfg.NotifMaxChars,
            Ets2PollIntervalMs = cfg.Ets2PollIntervalMs,
            StopPriorityTileWhenTruckStops = cfg.StopPriorityTileWhenTruckStops,
            RunAtStartup = cfg.RunAtStartup,
            MinimizeToTray = cfg.MinimizeToTray,
            Theme = cfg.Theme,
            DarkMode = cfg.Theme == "dark", // kept in sync for backward compatibility with older builds
            BackdropMaterial = cfg.BackdropMaterial,
            KeepNowPlayingWhenPaused = cfg.KeepNowPlayingWhenPaused,
            Language = cfg.Language,
            VideoPlayers = cfg.VideoPlayers.Select(player => player.Clone()).ToList(),
            MusicPlayers = cfg.MusicPlayers.Select(player => player.Clone()).ToList(),
            ScPassEncrypted = Encrypt(cfg.ScPass),
        };
        var json = JsonSerializer.Serialize(onDisk, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(ConfigPath, json);
    }
    private static string Encrypt(string plain)
    {
        if (string.IsNullOrEmpty(plain)) return "";
        var bytes = Encoding.UTF8.GetBytes(plain);
        var encrypted = ProtectedData.Protect(bytes, null, DataProtectionScope.CurrentUser);
        return Convert.ToBase64String(encrypted);
    }

    private static string Decrypt(string stored)
    {
        if (string.IsNullOrEmpty(stored)) return "";
        try
        {
            var bytes = Convert.FromBase64String(stored);
            var decrypted = ProtectedData.Unprotect(bytes, null, DataProtectionScope.CurrentUser);
            return Encoding.UTF8.GetString(decrypted);
        }
        catch
        {
            return ""; // couldn't decrypt (moved to another PC/user) - don't crash, just ask again
        }
    }
}
