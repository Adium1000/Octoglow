namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Nivelul de abstractizare pe care îl vede UI-ul: evenimente de nivel înalt
/// (tastă, knob), fără nicio noțiune de HID/USB/Raw Input dedesubt.
/// Toate evenimentele provin, garantat, exclusiv de la macropad — niciodată
/// de la tastatura normală sau alt HID din sistem.
/// </summary>
public interface IMacropadDevice
{
    bool IsConnected { get; }

    /// <summary>Id-ul butonului ("b1".."b7") din MacropadLayout, apăsat.</summary>
    event EventHandler<string>? KeyDown;

    /// <summary>Id-ul butonului ("b1".."b7") din MacropadLayout, eliberat.</summary>
    event EventHandler<string>? KeyUp;

    /// <summary>+1 = un pas clockwise, -1 = un pas counter-clockwise.</summary>
    event EventHandler<int>? KnobRotate;

    event EventHandler? KnobDown;
    event EventHandler? KnobUp;

    event EventHandler? Connected;
    event EventHandler? Disconnected;
}
