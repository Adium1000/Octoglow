namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Traduce codurile HID trimise efectiv de firmware (V2.ino, funcția
/// handleSinglePress) în id-urile butoanelor fizice din
/// <see cref="OctoglowSender.Devices.Macro.Models.MacropadLayout"/> ("b1".."b7") + knob (K1).
///
/// IMPORTANT: firmware-ul nu trimite un identificator de "buton fizic" — trimite
/// direct rezultatul (media key, shortcut). Ca să știm ce buton fizic a fost
/// apăsat, mapăm înapoi după codul rezultat. Dacă schimbi ce trimite un buton
/// în V2.ino (handleSinglePress), actualizează și mapările de aici.
///
/// Corespondența (GPIO din V2.ino ↔ id din MacropadLayout ↔ acțiune trimisă):
///   GPIO 3  (PIN_BTN_BACK) → b1 → Consumer: Scan Previous Track
///   GPIO 4  (PIN_BTN_PLAY) → b2 → Consumer: Play/Pause
///   GPIO 5  (PIN_BTN_NEXT) → b3 → Consumer: Scan Next Track
///   GPIO 6  (PIN_BTN_LOCK) → b4 → Keyboard: Win + L
///   GPIO 7  (PIN_BTN_SS)   → b5 → Keyboard: Win + PrintScreen
///   GPIO 8  (PIN_BTN_TSK)  → b6 → Keyboard: Ctrl + Shift + Esc
///   GPIO 14 (PIN_BTN_DESK) → b7 → Keyboard: Win + D
///   GPIO 15 (PIN_BTN_MUTE) → knob (K1, ButtonGpio 15) → Consumer: Mute
///   Encoder (pini 27/26)   → knob rotate → Consumer: Volume +/- (un puls per pas)
/// </summary>
internal static class MacropadKeyMap
{
    // Usage-uri HID Consumer Control (usage page 0x0C), exact ca în V2.ino.
    public const ushort ConsumerMute = 0x00E2;
    public const ushort ConsumerVolumeIncrement = 0x00E9;
    public const ushort ConsumerVolumeDecrement = 0x00EA;
    private const ushort ConsumerPlayPause = 0x00CD;
    private const ushort ConsumerScanNext = 0x00B5;
    private const ushort ConsumerScanPrevious = 0x00B6;

    // Virtual-key codes rezultate din combinațiile trimise pe raportul de tastatură
    // (report id 1). Nu ne interesează modificatorii (Win/Ctrl/Shift) — evenimentul
    // e deja filtrat strict după device (macropad), deci tasta principală ajunge.
    private const ushort VkL = 0x4C;        // 'L'          -> PIN_BTN_LOCK
    private const ushort VkSnapshot = 0x2C; // PrintScreen  -> PIN_BTN_SS
    private const ushort VkEscape = 0x1B;   // Escape       -> PIN_BTN_TSK
    private const ushort VkD = 0x44;        // 'D'          -> PIN_BTN_DESK

    public static bool TryMapConsumerButton(ushort usage, out string? buttonId)
    {
        buttonId = usage switch
        {
            ConsumerScanPrevious => "b1",
            ConsumerPlayPause => "b2",
            ConsumerScanNext => "b3",
            _ => null,
        };
        return buttonId is not null;
    }

    public static bool TryMapVKey(ushort vkey, out string? buttonId)
    {
        buttonId = vkey switch
        {
            VkL => "b4",
            VkSnapshot => "b5",
            VkEscape => "b6",
            VkD => "b7",
            _ => null,
        };
        return buttonId is not null;
    }
}
