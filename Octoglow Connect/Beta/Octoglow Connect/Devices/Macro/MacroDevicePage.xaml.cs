using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Animation;
using Microsoft.UI.Xaml.Navigation;
using OctoglowSender.Devices.Macro.Services;

namespace OctoglowSender.Devices.Macro;

public sealed partial class MacroDevicePage : Page
{
    private const double PressedBorderThickness = 3;
    private Dictionary<string, Border>? _keysById;
    private IMacropadDevice? _subscribedDevice;
    private double _knobAngle;
    private bool _isLoaded;

    public MacroDevicePage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Unloaded += OnUnloaded;
        ApplyLanguage();
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        UpdateConnectionStatus(MacropadConnectionState.IsConnected);
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        _isLoaded = true;
        _keysById = new Dictionary<string, Border>
        {
            ["b1"] = KeyB1, ["b2"] = KeyB2, ["b3"] = KeyB3, ["b4"] = KeyB4,
            ["b5"] = KeyB5, ["b6"] = KeyB6, ["b7"] = KeyB7,
        };
        AppStrings.LanguageChanged += OnLanguageChanged;
        MacropadConnectionState.Changed += OnConnectionChanged;
        SubscribeToInput();
        UpdateConnectionStatus(MacropadConnectionState.CheckNow());
    }

    private void OnUnloaded(object sender, RoutedEventArgs e)
    {
        _isLoaded = false;
        MacropadConnectionState.Changed -= OnConnectionChanged;
        UnsubscribeFromInput();
        AppStrings.LanguageChanged -= OnLanguageChanged;
    }

    private void SubscribeToInput()
    {
        var device = MacropadInputBus.Device;
        if (device is null || ReferenceEquals(device, _subscribedDevice)) return;
        UnsubscribeFromInput();
        device.KeyDown += OnDeviceKeyDown;
        device.KeyUp += OnDeviceKeyUp;
        device.KnobRotate += OnDeviceKnobRotate;
        device.KnobDown += OnDeviceKnobDown;
        device.KnobUp += OnDeviceKnobUp;
        device.Connected += OnInputConnected;
        device.Disconnected += OnInputDisconnected;
        _subscribedDevice = device;
    }

    private void UnsubscribeFromInput()
    {
        if (_subscribedDevice is null) return;
        _subscribedDevice.KeyDown -= OnDeviceKeyDown;
        _subscribedDevice.KeyUp -= OnDeviceKeyUp;
        _subscribedDevice.KnobRotate -= OnDeviceKnobRotate;
        _subscribedDevice.KnobDown -= OnDeviceKnobDown;
        _subscribedDevice.KnobUp -= OnDeviceKnobUp;
        _subscribedDevice.Connected -= OnInputConnected;
        _subscribedDevice.Disconnected -= OnInputDisconnected;
        _subscribedDevice = null;
    }

    private void OnLanguageChanged(object? sender, EventArgs e)
    {
        if (_isLoaded) DispatcherQueue.TryEnqueue(ApplyLanguage);
    }

    private void OnConnectionChanged(object? sender, bool connected) => DispatcherQueue.TryEnqueue(() =>
    {
        UpdateConnectionStatus(connected);
        if (connected) SubscribeToInput();
        else ResetAllVisualStates();
    });

    private void OnInputConnected(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() => UpdateConnectionStatus(true));
    private void OnInputDisconnected(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() =>
    {
        UpdateConnectionStatus(false);
        ResetAllVisualStates();
    });
    private void OnDeviceKeyDown(object? sender, string buttonId) => DispatcherQueue.TryEnqueue(() => SetKeyPressed(buttonId, true));
    private void OnDeviceKeyUp(object? sender, string buttonId) => DispatcherQueue.TryEnqueue(() => SetKeyPressed(buttonId, false));
    private void OnDeviceKnobRotate(object? sender, int delta) => DispatcherQueue.TryEnqueue(() => AnimateKnobRotate(delta));
    private void OnDeviceKnobDown(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() => SetKnobPressed(true));
    private void OnDeviceKnobUp(object? sender, EventArgs e) => DispatcherQueue.TryEnqueue(() => SetKnobPressed(false));

    private void UpdateConnectionStatus(bool connected)
    {
        StatusText.Text = AppStrings.Get(connected ? "device.connected" : "device.disconnected");
        StatusIcon.Glyph = connected ? "\uF13E" : "\uF13D";
        StatusIcon.Foreground = new SolidColorBrush(connected ? Colors.LimeGreen : Colors.Gray);
        StatusPill.Opacity = connected ? 1 : 0.72;
    }

    private void SetKeyPressed(string buttonId, bool pressed)
    {
        if (_keysById is null || !_keysById.TryGetValue(buttonId, out var key)) return;
        if (pressed)
        {
            key.BorderBrush = WindowsAccentColor.GetAccentBrush();
            key.BorderThickness = new Thickness(PressedBorderThickness);
        }
        else
        {
            key.ClearValue(Border.BorderBrushProperty);
            key.ClearValue(Border.BorderThicknessProperty);
        }
    }

    private void SetKnobPressed(bool pressed)
    {
        if (pressed)
        {
            EncoderBorder.BorderBrush = WindowsAccentColor.GetAccentBrush();
            EncoderBorder.BorderThickness = new Thickness(PressedBorderThickness);
        }
        else
        {
            EncoderBorder.ClearValue(Border.BorderBrushProperty);
            EncoderBorder.ClearValue(Border.BorderThicknessProperty);
        }
    }

    private void AnimateKnobRotate(int delta)
    {
        _knobAngle += delta * 25;
        var animation = new DoubleAnimation
        {
            To = _knobAngle,
            Duration = new Duration(TimeSpan.FromMilliseconds(120)),
            EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut },
        };
        Storyboard.SetTarget(animation, EncoderNotchRotation);
        Storyboard.SetTargetProperty(animation, "Angle");
        var storyboard = new Storyboard();
        storyboard.Children.Add(animation);
        storyboard.Begin();
    }

    private void ResetAllVisualStates()
    {
        if (_keysById is not null)
            foreach (var id in _keysById.Keys) SetKeyPressed(id, false);
        SetKnobPressed(false);
    }

    private void ApplyLanguage()
    {
        PageTitle.Text = AppStrings.Get("macro.title");
        MappingsTitle.Text = AppStrings.Get("macro.mappingsTitle");
        MappingsDescription.Text = AppStrings.Get("macro.mappingsDescription");
        UpdateConnectionStatus(MacropadConnectionState.IsConnected);
    }
}
