using System.Collections.ObjectModel;
using Microsoft.UI.Dispatching;
namespace OctoglowSender;
public static class AppLog
{
    private const int MaxEntries = 500;
    private static DispatcherQueue? _dispatcher;
    public static ObservableCollection<ActivityLogEntry> Entries { get; } = [];
    public static void Initialize(DispatcherQueue dispatcher) => _dispatcher = dispatcher;
    public static void Write(string message)
    {
        var entry = ActivityLogEntry.FromMessage(message);
        if (_dispatcher is not null) _dispatcher.TryEnqueue(() => Add(entry));
        else Add(entry);
    }
    private static void Add(ActivityLogEntry entry)
    {
        // Dacă ultima intrare raportează exact aceeași activitate (același context/mesaj),
        // nu o mai afișăm din nou - doar extindem intervalul de timp afișat pentru ea.
        if (Entries.Count > 0)
        {
            var last = Entries[^1];
            if (last.RawMessage == entry.RawMessage)
            {
                last.LastTimestamp = entry.Timestamp;
                return;
            }
        }

        Entries.Add(entry);
        while (Entries.Count > MaxEntries) Entries.RemoveAt(0);
    }
}

public sealed class ActivityLogEntry : System.ComponentModel.INotifyPropertyChanged
{
    public DateTime Timestamp { get; init; } = DateTime.Now;
    private DateTime _lastTimestamp;
    public DateTime LastTimestamp
    {
        get => _lastTimestamp;
        set
        {
            if (_lastTimestamp == value) return;
            _lastTimestamp = value;
            OnPropertyChanged(nameof(Time));
        }
    }
    public string Time => LastTimestamp > Timestamp
        ? $"{Timestamp:HH:mm:ss} - {LastTimestamp:HH:mm:ss}"
        : Timestamp.ToString("HH:mm:ss");
    public string RawMessage { get; init; } = "";
    public string Glyph => ActivityLogFormatter.GetGlyph(RawMessage);
    public string Description => ActivityLogFormatter.GetDescription(RawMessage);
    public static ActivityLogEntry FromMessage(string message)
    {
        var now = DateTime.Now;
        return new ActivityLogEntry { Timestamp = now, LastTimestamp = now, RawMessage = message };
    }
    public event System.ComponentModel.PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged(string propertyName) =>
        PropertyChanged?.Invoke(this, new System.ComponentModel.PropertyChangedEventArgs(propertyName));
}

public static class ActivityLogFormatter
{
    public static string GetGlyph(string message)
    {
        var type = message.ToUpperInvariant();
        var glyph = type.Contains("NOWPLAYING") || type.StartsWith("NP ") ? "\uE768" :
            type.Contains("ETS2") ? "\uE804" :
            type.Contains("NOTIF") ? "\uE8BD" :
            type.Contains("AUTH") ? "\uE77B" :
            type.Contains("CUSTOMIZE") ? "\uE70F" :
            type.Contains("WARN") || type.Contains("ERR") || type.Contains("EȘUAT") || type.Contains("REFUZAT") ? "\uE783" :
            type.Contains("PORNIT") || type.Contains("START") ? "\uE895" :
            type.Contains("OPRIT") || type.Contains("ÎNCHIS") || type.Contains("STOP") ? "\uE71A" : "\uE8BD";

        return glyph;
    }

    public static string GetDescription(string message)
    {
        var description = message.Trim();
        if (description.StartsWith('['))
        {
            var end = description.IndexOf(']');
            if (end > 1) description = description[(end + 1)..].Trim();
        }
        return AppStrings.IsEnglish ? TranslateToEnglish(description) : description;
    }
    private static string TranslateToEnglish(string description)
    {
        var replacements = new Dictionary<string, string>
        {
            ["Aplicația a fost pornită."] = "Application started.", ["Oprit."] = "Stopped.",
            ["Autentificat cu succes ca"] = "Authenticated successfully as", ["Login eșuat după toate încercările."] = "Login failed after all attempts.",
            ["Utilizator sau parolă incorectă."] = "Incorrect username or password.", ["Răspuns neașteptat:"] = "Unexpected response:",
            ["reîncerc"] = "retrying", ["Nu mă pot conecta la"] = "Cannot connect to", ["încercarea"] = "attempt",
            ["Timeout la login"] = "Login timed out", ["Sesiune expirată - reautentificare..."] = "Session expired - signing in again...",
            ["după reautentificare"] = "after signing in again", ["Timeout la"] = "Timed out at",
            ["Acces REFUZAT de Windows."] = "Access was DENIED by Windows.", ["Ascultător activ."] = "Listener active.",
            ["Ascultătorul s-a oprit:"] = "Listener stopped:", ["Eroare la interogare:"] = "Query error:",
            ["Modul detecție pornit, aștept eurotrucks2.exe..."] = "Detection module started; waiting for eurotrucks2.exe...",
            ["eurotrucks2.exe detectat."] = "eurotrucks2.exe detected.", ["eurotrucks2.exe închis - deconectat."] = "eurotrucks2.exe closed - disconnected.",
        };
        foreach (var (romanian, english) in replacements) description = description.Replace(romanian, english, StringComparison.OrdinalIgnoreCase);
        return description;
    }
}
