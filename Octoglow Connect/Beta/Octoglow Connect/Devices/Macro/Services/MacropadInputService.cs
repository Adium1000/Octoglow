using System.Runtime.InteropServices;
using System.Threading;

namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// Comunicare DIRECTĂ cu macropad-ul, prin Raw Input API (WM_INPUT), NU prin
/// global keyboard hooks / RegisterHotKey / KeyDetect.
///
/// De ce Raw Input și nu detecție globală de taste:
/// fiecare eveniment WM_INPUT vine cu RAWINPUTHEADER.hDevice — handle-ul exact
/// al device-ului HID care l-a generat. Putem deci ști cu certitudine că o
/// apăsare vine de la macropad și nu de la tastatura normală, chiar dacă
/// ambele ar trimite aceeași tastă. Global hooks (RegisterHotKey, low-level
/// keyboard hooks) NU oferă această informație — de-asta nu le folosim.
///
/// Flux: Macropad hardware -> Raw Input (WM_INPUT, filtrat pe hDevice) ->
///       evenimente de nivel înalt (KeyDown/KeyUp/KnobRotate/KnobDown/KnobUp) -> UI.
/// UI-ul (LayoutPage) nu știe nimic despre HID/Raw Input.
/// </summary>
public sealed class MacropadInputService : IMacropadDevice, IDisposable
{
    private static readonly TimeSpan KeyboardReleaseFallback = TimeSpan.FromMilliseconds(150);

    private IntPtr _hwnd;
    private IDisposable? _messageSubscription;
    private bool _attached;
    private bool _registered;

    private readonly HashSet<IntPtr> _macropadDeviceHandles = new();
    private readonly Dictionary<string, System.Threading.Timer> _pendingKeyReleaseTimers = new();
    private readonly object _stateLock = new();
    private ushort _lastConsumerUsage;

    public bool IsConnected { get; private set; }

    public event EventHandler<string>? KeyDown;
    public event EventHandler<string>? KeyUp;
    public event EventHandler<int>? KnobRotate;
    public event EventHandler? KnobDown;
    public event EventHandler? KnobUp;
    public event EventHandler? Connected;
    public event EventHandler? Disconnected;

    /// <summary>Atașează serviciul la fereastra principală. Idempotent — nu creează hook-uri duplicate.</summary>
    public void Attach(IntPtr hwnd, NativeWindowMessageRouter messageRouter)
    {
        if (_attached) return;
        _attached = true;
        _hwnd = hwnd;

        // Tray + Raw Input share one WndProc owner. This avoids hook-order bugs
        // when either service is disposed while the other one is still active.
        _messageSubscription = messageRouter.Subscribe(HandleWindowMessage);

        // 2) Înregistrăm interes pentru cele două top-level collections pe care le
        //    trimite firmware-ul: tastatură generică (usage page 1, usage 6) și
        //    consumer control (usage page 0x0C, usage 1).
        //    RIDEV_INPUTSINK: primim evenimente chiar dacă fereastra nu are focus.
        //    RIDEV_DEVNOTIFY: primim WM_INPUT_DEVICE_CHANGE la conectare/deconectare,
        //    fără polling agresiv.
        var devices = new[]
        {
            new RawInputInterop.RAWINPUTDEVICE
            {
                usUsagePage = 0x01,
                usUsage = 0x06,
                dwFlags = RawInputInterop.RIDEV_INPUTSINK | RawInputInterop.RIDEV_DEVNOTIFY,
                hwndTarget = _hwnd,
            },
            new RawInputInterop.RAWINPUTDEVICE
            {
                usUsagePage = 0x0C,
                usUsage = 0x01,
                dwFlags = RawInputInterop.RIDEV_INPUTSINK | RawInputInterop.RIDEV_DEVNOTIFY,
                hwndTarget = _hwnd,
            },
        };

        var ok = RawInputInterop.RegisterRawInputDevices(
            devices, (uint)devices.Length, (uint)Marshal.SizeOf<RawInputInterop.RAWINPUTDEVICE>());

        if (!ok)
        {
            // Nu blocăm aplicația dacă înregistrarea eșuează dintr-un motiv oarecare
            // (ex: rulează într-un mediu restricționat). Detectarea de conexiune prin
            // polling (MacropadDetector) rămâne oricum funcțională, independent de asta.
            _messageSubscription.Dispose();
            _messageSubscription = null;
            _attached = false;
            return;
        }

        _registered = true;
        RefreshKnownDeviceHandles();
    }

    /// <summary>Dezînregistrează device-urile raw input și handler-ul din router.</summary>
    public void Detach()
    {
        if (_registered)
        {
            try
            {
                var remove = new[]
                {
                    new RawInputInterop.RAWINPUTDEVICE { usUsagePage = 0x01, usUsage = 0x06, dwFlags = RawInputInterop.RIDEV_REMOVE, hwndTarget = IntPtr.Zero },
                    new RawInputInterop.RAWINPUTDEVICE { usUsagePage = 0x0C, usUsage = 0x01, dwFlags = RawInputInterop.RIDEV_REMOVE, hwndTarget = IntPtr.Zero },
                };
                RawInputInterop.RegisterRawInputDevices(remove, (uint)remove.Length, (uint)Marshal.SizeOf<RawInputInterop.RAWINPUTDEVICE>());
            }
            catch
            {
                // best-effort la cleanup — nu vrem să aruncăm la închiderea aplicației
            }
        }

        _messageSubscription?.Dispose();
        _messageSubscription = null;

        lock (_stateLock)
        {
            foreach (var timer in _pendingKeyReleaseTimers.Values) timer.Dispose();
            _pendingKeyReleaseTimers.Clear();
            _macropadDeviceHandles.Clear();
        }

        _attached = false;
        _registered = false;
        _hwnd = IntPtr.Zero;
        SetConnected(false);
    }

    public void Dispose() => Detach();

    private bool HandleWindowMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam, out IntPtr result)
    {
        result = IntPtr.Zero;
        try
        {
            switch (msg)
            {
                case RawInputInterop.WM_INPUT:
                    HandleRawInput(lParam);
                    break;
                case RawInputInterop.WM_INPUT_DEVICE_CHANGE:
                    HandleDeviceChange(wParam, lParam);
                    break;
            }
        }
        catch
        {
            // Un pachet malformat sau o eroare de citire nu trebuie să darâme fereastra.
        }

        // false tells the shared router to continue to the original WndProc. Windows
        // needs that path even for WM_INPUT so it can perform internal cleanup.
        return false;
    }

    // ───────────────────────────── Device lifecycle ─────────────────────────────

    private void HandleDeviceChange(IntPtr wParam, IntPtr lParam)
    {
        var hDevice = lParam;
        var code = (long)wParam;

        if (code == RawInputInterop.GIDC_ARRIVAL)
        {
            if (IsMacropadDevicePath(hDevice))
            {
                lock (_stateLock) _macropadDeviceHandles.Add(hDevice);
                SetConnected(true);
            }
        }
        else if (code == RawInputInterop.GIDC_REMOVAL)
        {
            bool anyLeft;
            lock (_stateLock)
            {
                _macropadDeviceHandles.Remove(hDevice);
                anyLeft = _macropadDeviceHandles.Count > 0;
            }

            if (!anyLeft)
            {
                ResetTransientState();
                SetConnected(false);
            }
        }
    }

    private void RefreshKnownDeviceHandles()
    {
        lock (_stateLock) _macropadDeviceHandles.Clear();

        uint count = 0;
        RawInputInterop.GetRawInputDeviceList(IntPtr.Zero, ref count, (uint)Marshal.SizeOf<RawInputInterop.RAWINPUTDEVICELIST>());
        if (count == 0)
        {
            SetConnected(false);
            return;
        }

        int itemSize = Marshal.SizeOf<RawInputInterop.RAWINPUTDEVICELIST>();
        var buffer = Marshal.AllocHGlobal(itemSize * (int)count);
        try
        {
            uint written = RawInputInterop.GetRawInputDeviceList(buffer, ref count, (uint)itemSize);
            if (written == unchecked((uint)-1))
            {
                SetConnected(false);
                return;
            }

            for (int i = 0; i < written; i++)
            {
                var item = Marshal.PtrToStructure<RawInputInterop.RAWINPUTDEVICELIST>(IntPtr.Add(buffer, i * itemSize));
                if (IsMacropadDevicePath(item.hDevice))
                {
                    lock (_stateLock) _macropadDeviceHandles.Add(item.hDevice);
                }
            }
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }

        bool anyFound;
        lock (_stateLock) anyFound = _macropadDeviceHandles.Count > 0;
        SetConnected(anyFound);
    }

    private static string? GetRawInputDeviceName(IntPtr hDevice)
    {
        uint size = 0;
        RawInputInterop.GetRawInputDeviceInfoW(hDevice, RawInputInterop.RIDI_DEVICENAME, IntPtr.Zero, ref size);
        if (size == 0) return null;

        var buffer = Marshal.AllocHGlobal((int)size * 2);
        try
        {
            var written = RawInputInterop.GetRawInputDeviceInfoW(hDevice, RawInputInterop.RIDI_DEVICENAME, buffer, ref size);
            if (written == unchecked((uint)-1)) return null;
            return Marshal.PtrToStringUni(buffer);
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }

    private static string NormalizeDevicePath(string path)
        => path.Replace(@"\\?\", string.Empty, StringComparison.OrdinalIgnoreCase)
               .Replace(@"\??\", string.Empty, StringComparison.OrdinalIgnoreCase)
               .Replace("\\", string.Empty)
               .ToUpperInvariant();

    /// <summary>
    /// Verificarea de identitate reală a device-ului: comparăm path-ul de device
    /// întors de Raw Input cu path-urile HID (SetupAPI) care au Product String
    /// "RP2040 Macropad" (vezi MacropadIdentity). Astfel nu presupunem că orice
    /// tastatură/HID e macropad-ul — verificăm explicit.
    /// </summary>
    private static bool IsMacropadDevicePath(IntPtr hDevice)
    {
        var rawName = GetRawInputDeviceName(hDevice);
        if (string.IsNullOrEmpty(rawName)) return false;

        var normalizedRaw = NormalizeDevicePath(rawName);

        foreach (var candidate in MacropadIdentity.FindAll())
        {
            var normalizedCandidate = NormalizeDevicePath(candidate.Path);
            if (normalizedRaw == normalizedCandidate
                || normalizedRaw.Contains(normalizedCandidate, StringComparison.Ordinal)
                || normalizedCandidate.Contains(normalizedRaw, StringComparison.Ordinal))
            {
                return true;
            }
        }

        return false;
    }

    // ───────────────────────────── Raw input parsing ─────────────────────────────

    private void HandleRawInput(IntPtr hRawInput)
    {
        uint headerSize = (uint)Marshal.SizeOf<RawInputInterop.RAWINPUTHEADER>();
        uint size = 0;

        RawInputInterop.GetRawInputData(hRawInput, RawInputInterop.RID_INPUT, IntPtr.Zero, ref size, headerSize);
        if (size == 0 || size > 4096) return; // pachet gol sau suspect de mare -> ignorat, nu crapăm

        var buffer = Marshal.AllocHGlobal((int)size);
        try
        {
            var written = RawInputInterop.GetRawInputData(hRawInput, RawInputInterop.RID_INPUT, buffer, ref size, headerSize);
            if (written != size) return; // pachet incomplet/malformat

            var header = Marshal.PtrToStructure<RawInputInterop.RAWINPUTHEADER>(buffer);

            bool known;
            lock (_stateLock) known = _macropadDeviceHandles.Contains(header.hDevice);

            if (!known)
            {
                // Nu e un device pe care-l știam deja — verificăm explicit o singură dată
                // (ex: reconectare foarte rapidă, între poll-uri). Dacă nu e macropad-ul,
                // evenimentul e ignorat complet — inclusiv dacă vine de la tastatura normală.
                if (!IsMacropadDevicePath(header.hDevice)) return;
                lock (_stateLock) _macropadDeviceHandles.Add(header.hDevice);
                SetConnected(true);
            }

            if (header.dwType == RawInputInterop.RIM_TYPEKEYBOARD)
            {
                var kb = Marshal.PtrToStructure<RawInputInterop.RAWKEYBOARD>(IntPtr.Add(buffer, (int)headerSize));
                HandleKeyboard(kb);
            }
            else if (header.dwType == RawInputInterop.RIM_TYPEHID)
            {
                HandleHid(buffer, headerSize, size);
            }
        }
        catch
        {
            // orice eroare de parsare -> ignorăm pachetul, nu propagăm mai departe
        }
        finally
        {
            Marshal.FreeHGlobal(buffer);
        }
    }

    private void HandleKeyboard(RawInputInterop.RAWKEYBOARD kb)
    {
        if (!MacropadKeyMap.TryMapVKey(kb.VKey, out var buttonId) || buttonId is null) return;

        bool isBreak = (kb.Flags & RawInputInterop.RI_KEY_BREAK) != 0;
        if (!isBreak)
        {
            // armFallback = true: unele combinații (notabil PrintScreen) au un comportament
            // cunoscut de a nu trimite mereu un eveniment de "break" curat prin Raw Input.
            // Fallback-ul eliberează vizual tasta oricum, ca UI-ul să nu rămână "blocat" apăsat.
            RaiseKeyDown(buttonId, armFallback: true);
        }
        else
        {
            RaiseKeyUp(buttonId);
        }
    }

    private void HandleHid(IntPtr buffer, uint headerSize, uint totalSize)
    {
        // RAWHID e urmat de bRawData: [ReportID][usage low][usage high] pentru
        // raportul Consumer Control (report id 2, vezi V2.ino: usb_hid.sendReport16(2, usage)).
        var hidHeaderSize = (uint)Marshal.SizeOf<RawInputInterop.RAWHID>();
        if (totalSize < headerSize + hidHeaderSize) return;

        var rawHid = Marshal.PtrToStructure<RawInputInterop.RAWHID>(IntPtr.Add(buffer, (int)headerSize));
        if (rawHid.dwCount == 0 || rawHid.dwSizeHid < 3) return; // prea mic ca să conțină report id + usage pe 2 bytes

        var payloadSize = (ulong)rawHid.dwSizeHid * rawHid.dwCount;
        if ((ulong)totalSize < (ulong)headerSize + hidHeaderSize + payloadSize) return; // pachet trunchiat

        var firstReport = IntPtr.Add(buffer, (int)(headerSize + hidHeaderSize));
        for (uint index = 0; index < rawHid.dwCount; index++)
        {
            var report = IntPtr.Add(firstReport, checked((int)(index * rawHid.dwSizeHid)));
            byte reportId = Marshal.ReadByte(report, 0);
            if (reportId != 2) continue; // ne interesează doar Consumer Control (report id 2)

            byte low = Marshal.ReadByte(report, 1);
            byte high = Marshal.ReadByte(report, 2);
            ushort usage = (ushort)(low | (high << 8));
            HandleConsumerUsage(usage);
        }
    }

    private void HandleConsumerUsage(ushort usage)
    {
        if (usage != 0)
        {
            _lastConsumerUsage = usage;

            if (MacropadKeyMap.TryMapConsumerButton(usage, out var buttonId) && buttonId is not null)
                RaiseKeyDown(buttonId, armFallback: false); // consumer control trimite mereu release explicit
            else if (usage == MacropadKeyMap.ConsumerMute)
                KnobDown?.Invoke(this, EventArgs.Empty);
            else if (usage == MacropadKeyMap.ConsumerVolumeIncrement)
                KnobRotate?.Invoke(this, +1);
            else if (usage == MacropadKeyMap.ConsumerVolumeDecrement)
                KnobRotate?.Invoke(this, -1);
        }
        else
        {
            var last = _lastConsumerUsage;
            _lastConsumerUsage = 0;

            if (MacropadKeyMap.TryMapConsumerButton(last, out var buttonId) && buttonId is not null)
                RaiseKeyUp(buttonId);
            else if (last == MacropadKeyMap.ConsumerMute)
                KnobUp?.Invoke(this, EventArgs.Empty);
            // Volumul (rotația) nu are stare de eliberare — e doar un puls per pas.
        }
    }

    // ───────────────────────────── Helpers ─────────────────────────────

    private void RaiseKeyDown(string buttonId, bool armFallback)
    {
        KeyDown?.Invoke(this, buttonId);
        if (!armFallback) return;

        lock (_stateLock)
        {
            if (_pendingKeyReleaseTimers.TryGetValue(buttonId, out var existing))
                existing.Dispose();

            _pendingKeyReleaseTimers[buttonId] = new System.Threading.Timer(
                _ => RaiseKeyUp(buttonId), null, KeyboardReleaseFallback, Timeout.InfiniteTimeSpan);
        }
    }

    private void RaiseKeyUp(string buttonId)
    {
        lock (_stateLock)
        {
            if (_pendingKeyReleaseTimers.TryGetValue(buttonId, out var timer))
            {
                timer.Dispose();
                _pendingKeyReleaseTimers.Remove(buttonId);
            }
        }

        KeyUp?.Invoke(this, buttonId);
    }

    private void ResetTransientState()
    {
        lock (_stateLock)
        {
            foreach (var timer in _pendingKeyReleaseTimers.Values) timer.Dispose();
            _pendingKeyReleaseTimers.Clear();
        }
        _lastConsumerUsage = 0;
    }

    private void SetConnected(bool connected)
    {
        if (IsConnected == connected) return;
        IsConnected = connected;

        if (connected) Connected?.Invoke(this, EventArgs.Empty);
        else Disconnected?.Invoke(this, EventArgs.Empty);
    }
}
