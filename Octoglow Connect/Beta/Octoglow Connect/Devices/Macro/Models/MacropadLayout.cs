namespace OctoglowSender.Devices.Macro.Models;

/// <summary>Un buton simplu de pe macropad.</summary>
public sealed record MacropadButtonInfo(string Id, int Gpio, int Row, int Col);

/// <summary>Encoder-ul rotativ (are și buton de apăsare).</summary>
public sealed record MacropadEncoderInfo(string Id, int PinA, int PinB, int ButtonGpio, int Row, int Col);

/// <summary>
/// Layout-ul fizic al macropad-ului, așa cum e definit în firmware (V2.ino).
///   [b1] [b2] [b3] [k1]
///   [b4] [b5] [b6] [b7]
/// </summary>
public static class MacropadLayout
{
    public static readonly IReadOnlyList<MacropadButtonInfo> Buttons = new List<MacropadButtonInfo>
    {
        new("b1", Gpio: 3,  Row: 0, Col: 0),
        new("b2", Gpio: 4,  Row: 0, Col: 1),
        new("b3", Gpio: 5,  Row: 0, Col: 2),
        new("b4", Gpio: 6,  Row: 1, Col: 0),
        new("b5", Gpio: 7,  Row: 1, Col: 1),
        new("b6", Gpio: 8,  Row: 1, Col: 2),
        new("b7", Gpio: 14, Row: 1, Col: 3),
    };

    public static readonly MacropadEncoderInfo Encoder =
        new("k1", PinA: 27, PinB: 26, ButtonGpio: 15, Row: 0, Col: 3);

    public const int Rows = 2;
    public const int Cols = 4;
}
