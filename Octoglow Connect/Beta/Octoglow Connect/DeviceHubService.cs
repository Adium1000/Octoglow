using OctoglowSender.Devices.Macro.Services;

namespace OctoglowSender;

/// <summary>
/// Application-lifetime catalog that merges LAN Desk Clock discovery with USB
/// Macro discovery into one device list for the shell.
/// </summary>
public sealed class DeviceHubService : IDisposable
{
    private static readonly TimeSpan RefreshInterval = TimeSpan.FromMinutes(1);
    private readonly object _gate = new();
    private readonly Dictionary<string, DeviceDescriptor> _devices = new(StringComparer.OrdinalIgnoreCase);
    private readonly DeskClockDiscoveryService _clockDiscovery = new();
    private readonly SemaphoreSlim _refreshGate = new(1, 1);
    private readonly CancellationTokenSource _lifetime = new();
    private Task? _refreshLoop;
    private string? _configuredClockId;
    private bool _started;
    private bool _disposed;

    public event EventHandler? DevicesChanged;
    public event EventHandler? ScanStateChanged;

    public bool IsScanning { get; private set; }

    public IReadOnlyList<DeviceDescriptor> Devices
    {
        get
        {
            lock (_gate)
            {
                return _devices.Values
                    .OrderByDescending(device => device.IsOnline)
                    .ThenBy(device => device.Kind)
                    .ThenBy(device => device.DisplayName, StringComparer.CurrentCultureIgnoreCase)
                    .ThenBy(device => device.Address, StringComparer.OrdinalIgnoreCase)
                    .ToList();
            }
        }
    }

    public void Start()
    {
        if (_started || _disposed) return;
        _started = true;

        MacropadConnectionState.Initialize();
        MacropadConnectionState.Changed += OnMacroConnectionChanged;
        UpdateMacro(MacropadConnectionState.CheckNow());
        RememberConfiguredClock(ConfigStore.Load());

        _refreshLoop = RunRefreshLoopAsync(_lifetime.Token);
    }

    public async Task RefreshAsync()
    {
        if (_disposed || !await _refreshGate.WaitAsync(0).ConfigureAwait(false)) return;
        SetScanning(true);

        try
        {
            var config = ConfigStore.Load();
            RememberConfiguredClock(config);
            var endpoints = await _clockDiscovery.DiscoverAsync(config, _lifetime.Token).ConfigureAwait(false);
            ApplyClockResults(endpoints);
        }
        catch (OperationCanceledException) when (_lifetime.IsCancellationRequested)
        {
        }
        catch (Exception error)
        {
            AppLog.Write($"[DISCOVERY WARN] Scanarea dispozitivelor a eșuat: {error.Message}");
        }
        finally
        {
            SetScanning(false);
            _refreshGate.Release();
        }
    }

    public void RememberConfiguredClock(AppConfig config)
    {
        if (!DeskClockDiscoveryService.TryNormalizeConfiguredAddress(
                config.Esp32Ip,
                config.Esp32Port,
                out var configured))
        {
            return;
        }

        var id = $"clock:{configured.Address.ToLowerInvariant()}:{configured.Port}";
        var changed = false;
        lock (_gate)
        {
            if (_configuredClockId is not null &&
                !string.Equals(_configuredClockId, id, StringComparison.OrdinalIgnoreCase) &&
                _devices.TryGetValue(_configuredClockId, out var previousConfigured) &&
                !previousConfigured.IsOnline)
            {
                _devices.Remove(_configuredClockId);
                changed = true;
            }

            _configuredClockId = id;
            if (!_devices.TryGetValue(id, out var existing))
            {
                _devices[id] = DeviceDescriptor.DeskClock(
                    configured.Address,
                    configured.Port,
                    isOnline: false,
                    detail: $"Salvat · {configured.Address}:{configured.Port}");
                changed = true;
            }
            else if (!existing.IsOnline)
            {
                var updated = existing with { Detail = $"Salvat · {configured.Address}:{configured.Port}" };
                if (updated != existing)
                {
                    _devices[id] = updated;
                    changed = true;
                }
            }
        }

        if (changed) DevicesChanged?.Invoke(this, EventArgs.Empty);
    }

    public async Task ClearAllDevicesAsync()
    {
        if (_disposed) return;

        await _refreshGate.WaitAsync().ConfigureAwait(false);
        var changed = false;
        try
        {
            lock (_gate)
            {
                changed = _devices.Count > 0;
                _devices.Clear();
                _configuredClockId = null;
            }
        }
        finally
        {
            _refreshGate.Release();
        }

        if (changed)
        {
            AppLog.Write("[DEVICE] Toate dispozitivele au fost eliminate din hub.");
            DevicesChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private async Task RunRefreshLoopAsync(CancellationToken cancellationToken)
    {
        await RefreshAsync().ConfigureAwait(false);
        using var timer = new PeriodicTimer(RefreshInterval);
        try
        {
            while (await timer.WaitForNextTickAsync(cancellationToken).ConfigureAwait(false))
                await RefreshAsync().ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
        }
    }

    private void ApplyClockResults(IReadOnlyList<DeskClockEndpoint> endpoints)
    {
        var onlineIds = endpoints.Select(endpoint => endpoint.Id).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var changed = false;

        lock (_gate)
        {
            foreach (var id in _devices.Keys.Where(id => id.StartsWith("clock:", StringComparison.OrdinalIgnoreCase)).ToList())
            {
                var previous = _devices[id];
                if (!onlineIds.Contains(id) && previous.IsOnline)
                {
                    _devices[id] = previous with
                    {
                        IsOnline = false,
                        Detail = $"Disconnected · {previous.Address}:{previous.Port}",
                    };
                    changed = true;
                }
            }

            foreach (var endpoint in endpoints)
            {
                var detail = endpoint.HasAccount
                    ? $"Connected · {endpoint.Address}:{endpoint.Port}"
                    : $"Necesită configurare · {endpoint.Address}:{endpoint.Port}";
                var descriptor = DeviceDescriptor.DeskClock(endpoint.Address, endpoint.Port, isOnline: true, detail);

                if (!_devices.TryGetValue(endpoint.Id, out var previous) ||
                    previous.IsOnline != descriptor.IsOnline ||
                    previous.Detail != descriptor.Detail ||
                    previous.Address != descriptor.Address ||
                    previous.Port != descriptor.Port)
                {
                    _devices[endpoint.Id] = descriptor;
                    changed = true;
                }
            }
        }

        if (changed)
        {
            AppLog.Write(endpoints.Count == 0
                ? "[DISCOVERY] Niciun Desk Clock online găsit."
                : $"[DISCOVERY] {endpoints.Count} Desk Clock detectat(e)." );
            DevicesChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private void OnMacroConnectionChanged(object? sender, bool connected) => UpdateMacro(connected);

    private void UpdateMacro(bool connected)
    {
        var id = "macro:rp2040-macropad";
        var changed = false;

        lock (_gate)
        {
            if (connected)
            {
                var descriptor = DeviceDescriptor.Macro(true, "Connected · USB HID");
                if (!_devices.TryGetValue(id, out var previous) || previous != descriptor)
                {
                    _devices[id] = descriptor;
                    changed = true;
                }
            }
            else if (_devices.TryGetValue(id, out var existing) && existing.IsOnline)
            {
                _devices[id] = existing with { IsOnline = false, Detail = "Disconnected · USB HID" };
                changed = true;
            }
        }

        if (changed)
        {
            AppLog.Write(connected
                ? "[DEVICE] Octoglow Macro detectat prin USB."
                : "[DEVICE] Octoglow Macro a fost deconectat.");
            DevicesChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    private void SetScanning(bool scanning)
    {
        if (IsScanning == scanning) return;
        IsScanning = scanning;
        ScanStateChanged?.Invoke(this, EventArgs.Empty);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        MacropadConnectionState.Changed -= OnMacroConnectionChanged;
        MacropadConnectionState.Shutdown();
        _lifetime.Cancel();
        _ = DisposeInfrastructureAsync();
    }

    private async Task DisposeInfrastructureAsync()
    {
        try
        {
            await _refreshGate.WaitAsync().ConfigureAwait(false);
            _clockDiscovery.Dispose();
            _lifetime.Dispose();
            _refreshGate.Dispose();
        }
        catch (ObjectDisposedException)
        {
        }
    }
}
