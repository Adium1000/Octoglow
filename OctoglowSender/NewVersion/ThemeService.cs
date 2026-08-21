using Microsoft.UI;
using Microsoft.UI.Composition.SystemBackdrops;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Windows.UI;
using Windows.UI.ViewManagement;
namespace OctoglowSender;
public static class ThemeService
{
    /// <summary>Applies "none" | "mica" | "micaalt" | "acrylic" as the window's SystemBackdrop.</summary>
    public static void ApplyBackdrop(string material)
    {
        var window = App.MainWindow;
        if (window is null) return;

        window.SystemBackdrop = material switch
        {
            "none" => null,
            "mica" => new MicaBackdrop { Kind = MicaKind.Base },
            "acrylic" => new DesktopAcrylicBackdrop(),
            _ => new MicaBackdrop { Kind = MicaKind.BaseAlt } // "micaalt" and fallback
        };
    }

    /// <summary>Applies "system" | "light" | "dark" to the whole app by setting RequestedTheme
    /// on the root element of the main window; WinUI propagates it down the visual tree.
    /// Also fixes the min/maximize/close caption button glyph colors, which the OS otherwise
    /// draws with a fixed color that can end up invisible against a light background.</summary>
    public static void Apply(string theme)
    {
        var window = App.MainWindow;
        if (window is null) return;

        var elementTheme = theme switch
        {
            "light" => ElementTheme.Light,
            "dark" => ElementTheme.Dark,
            _ => ElementTheme.Default
        };
        if (window.Content is FrameworkElement root)
            root.RequestedTheme = elementTheme;

        var isDark = theme switch
        {
            "light" => false,
            "dark" => true,
            _ => IsSystemInDarkMode()
        };
        ApplyTitleBarButtonColors(window, isDark);
    }

    private static void ApplyTitleBarButtonColors(Window window, bool isDark)
    {
        var titleBar = window.AppWindow.TitleBar;
        var foreground = isDark ? Colors.White : Colors.Black;
        titleBar.ButtonForegroundColor = foreground;
        titleBar.ButtonHoverForegroundColor = foreground;
        titleBar.ButtonPressedForegroundColor = foreground;
        titleBar.ButtonInactiveForegroundColor = isDark
            ? Color.FromArgb(150, 255, 255, 255)
            : Color.FromArgb(150, 0, 0, 0);
    }

    // Standard heuristic (used in Microsoft's own WinUI samples) for reading the current
    // Windows app theme (light/dark) so "System default" resolves to the right colors.
    private static bool IsSystemInDarkMode()
    {
        var background = new UISettings().GetColorValue(UIColorType.Background);
        return (5 * background.G + 2 * background.R + background.B) <= 8 * 128;
    }
}
