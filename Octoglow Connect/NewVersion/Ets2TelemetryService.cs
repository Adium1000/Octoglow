using System.Diagnostics;
using System.IO;
using System.Reflection;
namespace OctoglowSender;

/// <summary>
/// ETS2/ATS speed telemetry.
///
/// Reliable part (always works): detects whether eurotrucks2.exe is running,
/// same as the python psutil/tasklist check.
///
/// Optional part (best-effort): if you drop RenCloud's official "SCSSdkClient.dll"
/// (built from https://github.com/RenCloud/scs-sdk-plugin, folder scs-client/C#/SCSSdkClient)
/// next to OctoglowSender.exe, this loads it via reflection and reads truck speed from
/// the shared-memory telemetry the scs-sdk-plugin writes in-game. No hard reference/NuGet
/// dependency is needed at build time - exactly like the python code's try/except ImportError
/// pattern for optional features. If the DLL isn't present, ETS2 support just logs
/// connect/disconnect without speed, instead of crashing or failing to build.
/// </summary>
public class Ets2TelemetryService
{
    private readonly Action<string> _log;
    private object? _telemetry;       // SCSSdkTelemetry instance, if the optional DLL loaded
    private PropertyInfo? _currentValuesProp;
    private PropertyInfo? _dashboardValuesProp;
    private PropertyInfo? _speedProp;
    private PropertyInfo? _kphProp;
    private PropertyInfo? _truckValuesProp;
    public bool RealSpeedAvailable { get; private set; }
    public Ets2TelemetryService(Action<string> log)
    {
        _log = log;
        TryLoadOptionalSdk();
    }
    private void TryLoadOptionalSdk()
    {
        try
        {
            var dllPath = Path.Combine(AppContext.BaseDirectory, "SCSSdkClient.dll");
            if (!File.Exists(dllPath))
            {
                _log("[ETS2] SCSSdkClient.dll nu e prezent - se detectează doar rularea jocului, fără viteză. " +
                     "Adaugă DLL-ul oficial RenCloud lângă .exe pentru viteză reală.");
                return;
            }
            var asm = Assembly.LoadFrom(dllPath);
            var telemetryType = asm.GetType("SCSSdkClient.SCSSdkTelemetry")
                                 ?? asm.GetType("SCSSdkClient.ScsSdkTelemetry");
            if (telemetryType is null)
            {
                _log("[ETS2] SCSSdkClient.dll găsit, dar tipul așteptat lipsește (versiune diferită). Viteza rămâne dezactivată.");
                return;
            }
            _telemetry = Activator.CreateInstance(telemetryType);
            var dataEvent = telemetryType.GetEvent("Data");
            if (dataEvent != null)
            {
                var handlerType = dataEvent.EventHandlerType!;
                var invokeMethod = GetType().GetMethod(nameof(OnTelemetryData),
                    BindingFlags.NonPublic | BindingFlags.Instance)!;
                var handler = Delegate.CreateDelegate(handlerType, this, invokeMethod);
                dataEvent.AddEventHandler(_telemetry, handler);
                RealSpeedAvailable = true;
                _log("[ETS2] SCSSdkClient.dll încărcat - viteză reală activă.");
            }
        }
        catch (Exception e)
        {
            _log($"[ETS2] Nu am putut încărca SCSSdkClient.dll opțional: {e.Message}. Viteza rămâne dezactivată.");
        }
    }
    private float? _lastSpeedKph;
    private void OnTelemetryData(object data, bool updated)
    {
        try
        {
            _truckValuesProp ??= data.GetType().GetProperty("TruckValues");
            var truckValues = _truckValuesProp?.GetValue(data);
            if (truckValues is null) return;
            _currentValuesProp ??= truckValues.GetType().GetProperty("CurrentValues");
            var currentValues = _currentValuesProp?.GetValue(truckValues);
            if (currentValues is null) return;
            _dashboardValuesProp ??= currentValues.GetType().GetProperty("DashboardValues");
            var dashboardValues = _dashboardValuesProp?.GetValue(currentValues);
            if (dashboardValues is null) return;
            _speedProp ??= dashboardValues.GetType().GetProperty("Speed");
            var speed = _speedProp?.GetValue(dashboardValues);
            _kphProp ??= speed?.GetType().GetProperty("Kph");
            if (_kphProp?.GetValue(speed) is float kph)
                _lastSpeedKph = kph;
        }
        catch (Exception e)
        {
            _log($"[ETS2 WARN] Nu pot citi viteza din SDK: {e.Message}");
        }
    }
    private static bool _loggedDetectionError;
    public bool IsEts2RunningLogged()
    {
        try
        {
            var byName = Process.GetProcessesByName("eurotrucks2");
            if (byName.Length > 0) return true;
            return IsEts2RunningViaTasklist();
        }
        catch (Exception e)
        {
            if (!_loggedDetectionError)
            {
                _loggedDetectionError = true;
                _log($"[ETS2 WARN] Process.GetProcessesByName a eșuat ({e.Message}). " +
                     "Trec pe verificare de rezervă cu 'tasklist'.");
            }
            return IsEts2RunningViaTasklist();
        }
    }
    public static bool IsEts2Running()
    {
        try
        {
            return Process.GetProcessesByName("eurotrucks2").Length > 0
                   || IsEts2RunningViaTasklist();
        }
        catch
        {
            return IsEts2RunningViaTasklist();
        }
    }
    private static bool IsEts2RunningViaTasklist()
    {
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = "tasklist",
                Arguments = "/FI \"IMAGENAME eq eurotrucks2.exe\" /NH",
                RedirectStandardOutput = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };
            using var proc = Process.Start(psi);
            if (proc is null) return false;
            var output = proc.StandardOutput.ReadToEnd();
            proc.WaitForExit(3000);
            return output.Contains("eurotrucks2.exe", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }
    /// <summary>Returns speed in km/h if the optional real telemetry is active, otherwise null.</summary>
    public int? GetSpeedKmh()
    {
        if (!RealSpeedAvailable || _lastSpeedKph is null) return null;
        return (int)Math.Round(Math.Abs(_lastSpeedKph.Value));
    }
}
