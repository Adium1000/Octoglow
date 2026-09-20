namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Criteriul central de identificare a macropad-ului (firmware V2.ino) printre
/// dispozitivele HID din sistem. Un singur loc de adevăr, folosit atât de
/// polling-ul de conexiune (<see cref="MacropadDetector"/>) cât și de citirea
/// de input la nivel de device (<see cref="MacropadInputService"/>), ca să nu
/// ajungem cu două definiții care pot diverge în timp.
/// </summary>
internal static class MacropadIdentity
{
    // Dacă redenumești firmware-ul (TinyUSBDevice.setProductDescriptor / setManufacturerDescriptor),
    // actualizează aici — restul aplicației citește de aici.
    internal const string ExpectedProduct = "RP2040 Macropad";
    internal const string ExpectedManufacturer = "Adrian";

    internal static bool Matches(NativeHid.HidDeviceInfo device)
        => string.Equals(device.Product?.Trim(), ExpectedProduct, StringComparison.OrdinalIgnoreCase)
           && (string.IsNullOrWhiteSpace(device.Manufacturer)
               || string.Equals(device.Manufacturer.Trim(), ExpectedManufacturer, StringComparison.OrdinalIgnoreCase));

    /// <summary>
    /// Toate interfețele HID prezente acum care aparțin macropad-ului — de regulă mai mult
    /// de una, pentru că firmware-ul expune două top-level collections HID pe aceeași
    /// interfață fizică (tastatură + consumer control), iar Windows le enumeră separat.
    /// </summary>
    internal static IReadOnlyList<NativeHid.HidDeviceInfo> FindAll()
        => NativeHid.EnumerateHidDevices().Where(Matches).ToList();
}
