namespace OctoglowSender;

public enum ScreenMirrorState
{
    Waiting,
    Live,
    Unsupported,
    Offline,
}

public readonly record struct ScreenMirrorSnapshot(
    bool IsOn,
    int Brightness,
    byte[] Columns,
    ScreenMirrorState State,
    DateTime LastUpdate);

/// <summary>Keeps the latest Desk Clock LED frame available across page navigation.</summary>
public static class ScreenMirror
{
    private static readonly object Gate = new();
    private static bool _isOn;
    private static int _brightness;
    private static byte[] _columns = new byte[32];
    private static ScreenMirrorState _state = ScreenMirrorState.Waiting;
    private static DateTime _lastUpdate;

    public static event EventHandler? Changed;

    // Pages showing the panel register while they are on screen. With none, the
    // backend leaves the clock alone - no stream, and its WiFi can sleep again.
    private static int _viewers;
    public static bool IsWatched => Volatile.Read(ref _viewers) > 0;
    public static void AddViewer() => Interlocked.Increment(ref _viewers);
    public static void RemoveViewer()
    {
        if (Interlocked.Decrement(ref _viewers) < 0)
            Interlocked.Exchange(ref _viewers, 0);
    }

    public static bool IsOn
    {
        get { lock (Gate) return _isOn; }
    }

    public static int Brightness
    {
        get { lock (Gate) return _brightness; }
    }

    public static byte[] Columns
    {
        get { lock (Gate) return _columns.ToArray(); }
    }

    public static ScreenMirrorState State
    {
        get { lock (Gate) return _state; }
    }

    public static DateTime LastUpdate
    {
        get { lock (Gate) return _lastUpdate; }
    }

    public static ScreenMirrorSnapshot GetSnapshot()
    {
        lock (Gate)
        {
            return new ScreenMirrorSnapshot(
                _isOn,
                _brightness,
                _columns.ToArray(),
                _state,
                _lastUpdate);
        }
    }

    public static void Publish(bool on, int brightness, byte[] columns)
    {
        ArgumentNullException.ThrowIfNull(columns);
        if (columns.Length != 32)
            throw new ArgumentException("A screen frame must contain exactly 32 columns.", nameof(columns));

        bool changed;
        lock (Gate)
        {
            changed = _state != ScreenMirrorState.Live ||
                      _isOn != on ||
                      _brightness != brightness ||
                      !_columns.AsSpan().SequenceEqual(columns);

            _isOn = on;
            _brightness = brightness;
            _columns = columns.ToArray();
            _state = ScreenMirrorState.Live;
            _lastUpdate = DateTime.UtcNow;
        }

        if (changed)
            Changed?.Invoke(null, EventArgs.Empty);
    }

    public static void SetState(ScreenMirrorState state)
    {
        lock (Gate)
        {
            if (_state == state) return;
            _state = state;
        }

        Changed?.Invoke(null, EventArgs.Empty);
    }

    public static void Reset()
    {
        bool changed;
        lock (Gate)
        {
            changed = _state != ScreenMirrorState.Waiting ||
                      _isOn ||
                      _brightness != 0 ||
                      _columns.Any(value => value != 0);

            _isOn = false;
            _brightness = 0;
            _columns = new byte[32];
            _state = ScreenMirrorState.Waiting;
            _lastUpdate = default;
        }

        if (changed)
            Changed?.Invoke(null, EventArgs.Empty);
    }
}
