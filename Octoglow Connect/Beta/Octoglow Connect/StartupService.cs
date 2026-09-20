using Microsoft.Win32;
namespace OctoglowSender;
public static class StartupService
{
    private const string RunKeyPath = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string AppName = "OctoglowSender";
    public static bool SetRunAtStartup(bool enabled)
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(RunKeyPath, writable: true);
            if (key is null) return false;

            if (enabled)
            {
                var exePath = Environment.ProcessPath ?? System.Reflection.Assembly.GetExecutingAssembly().Location;
                key.SetValue(AppName, $"\"{exePath}\" --startup");
            }
            else
            {
                if (key.GetValue(AppName) != null)
                    key.DeleteValue(AppName, throwOnMissingValue: false);
            }
            return true;
        }
        catch (Exception)
        {
            // No admin rights needed for HKCU, but keep this defensive - a registry failure
            // shouldn't crash the app, just silently not persist the settin
            return false;
        }
    }
}
