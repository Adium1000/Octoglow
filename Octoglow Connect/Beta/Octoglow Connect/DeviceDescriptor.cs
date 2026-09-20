namespace OctoglowSender;

public enum DeviceKind
{
    DeskClock,
    Macro,
}

/// <summary>
/// A device known by the hub. Records are immutable so UI consumers always get a
/// coherent snapshot when a discovery result arrives from a background thread.
/// </summary>
public sealed record DeviceDescriptor(
    string Id,
    DeviceKind Kind,
    string DisplayName,
    string Detail,
    bool IsOnline,
    string? Address = null,
    int Port = 0,
    DateTimeOffset? LastSeen = null)
{
    public static DeviceDescriptor DeskClock(string address, int port, bool isOnline, string detail) => new(
        Id: $"clock:{address.Trim().ToLowerInvariant()}:{port}",
        Kind: DeviceKind.DeskClock,
        DisplayName: "Octoglow Desk Clock",
        Detail: detail,
        IsOnline: isOnline,
        Address: address.Trim(),
        Port: port,
        LastSeen: isOnline ? DateTimeOffset.UtcNow : null);

    public static DeviceDescriptor Macro(bool isOnline, string detail) => new(
        Id: "macro:rp2040-macropad",
        Kind: DeviceKind.Macro,
        DisplayName: "Octoglow Macro",
        Detail: detail,
        IsOnline: isOnline,
        LastSeen: isOnline ? DateTimeOffset.UtcNow : null);
}
