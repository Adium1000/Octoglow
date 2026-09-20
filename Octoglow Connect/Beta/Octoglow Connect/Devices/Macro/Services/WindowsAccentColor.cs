using Microsoft.UI.Xaml.Media;
using Windows.UI;
using Windows.UI.ViewManagement;

namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Citește culoarea de accent curentă din Windows (Setări -> Personalizare ->
/// Culori), fără să o hardcodăm. UISettings funcționează și în aplicații
/// WinUI 3 unpackaged, fără capabilități speciale în manifest.
/// </summary>
public static class WindowsAccentColor
{
    public static Color GetAccentColor() => new UISettings().GetColorValue(UIColorType.Accent);

    public static SolidColorBrush GetAccentBrush() => new(GetAccentColor());
}
