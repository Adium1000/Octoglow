namespace OctoglowSender;

public enum ConnectionStatusKind { Disconnected, Connecting, Connected, Failed }

public static class ConnectionStatus
{
    public static ConnectionStatusKind Current { get; private set; } = ConnectionStatusKind.Disconnected;
    public static bool IsConnected => Current == ConnectionStatusKind.Connected;
    public static event EventHandler? Changed;

    public static void Set(ConnectionStatusKind status)
    {
        if (Current == status) return;
        Current = status;
        Changed?.Invoke(null, EventArgs.Empty);
    }

    public static void ProcessBackendMessage(string message)
    {
        var value = message.ToUpperInvariant();
        if (value.Contains("[AUTH] AUTENTIFICAT CU SUCCES") || value.Contains("[AUTH] AUTHENTICATED")) Set(ConnectionStatusKind.Connected);
        else if (value.Contains("[AUTH]") && (value.Contains("EȘUAT") || value.Contains("INCORECT") || value.Contains("TIMEOUT") || value.Contains("NU MĂ POT CONECTA"))) Set(ConnectionStatusKind.Failed);
    }
}
