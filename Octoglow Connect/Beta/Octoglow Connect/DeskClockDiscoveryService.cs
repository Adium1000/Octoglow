using System.Collections.Concurrent;
using System.Net;
using System.Net.Http;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text.Json;

namespace OctoglowSender;

/// <summary>
/// Finds Desk Clock firmware on the local network by probing its public
/// /authstate endpoint. The current firmware does not advertise through mDNS,
/// so discovery is intentionally bounded to the configured endpoint, the
/// provisioning address and the local /24 slices.
/// </summary>
public sealed class DeskClockDiscoveryService : IDisposable
{
    private static readonly TimeSpan ProbeTimeout = TimeSpan.FromMilliseconds(650);
    private readonly HttpClient _httpClient = new(new SocketsHttpHandler
    {
        ConnectTimeout = TimeSpan.FromMilliseconds(400),
        PooledConnectionLifetime = TimeSpan.FromMinutes(2),
    })
    {
        Timeout = Timeout.InfiniteTimeSpan,
    };

    public async Task<IReadOnlyList<DeskClockEndpoint>> DiscoverAsync(AppConfig config, CancellationToken cancellationToken)
    {
        var candidates = BuildCandidates(config);
        var found = new ConcurrentDictionary<string, DeskClockEndpoint>(StringComparer.OrdinalIgnoreCase);

        await Parallel.ForEachAsync(
            candidates,
            new ParallelOptions { CancellationToken = cancellationToken, MaxDegreeOfParallelism = 48 },
            async (candidate, token) =>
            {
                var result = await ProbeAsync(candidate, token).ConfigureAwait(false);
                if (result is not null)
                    found.TryAdd(result.Id, result);
            }).ConfigureAwait(false);

        return found.Values
            .OrderByDescending(endpoint => endpoint.IsConfiguredAddress)
            .ThenBy(endpoint => ParseSortableAddress(endpoint.Address))
            .ToList();
    }

    private async Task<DeskClockEndpoint?> ProbeAsync(DeskClockCandidate candidate, CancellationToken cancellationToken)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(ProbeTimeout);

        try
        {
            var builder = new UriBuilder(Uri.UriSchemeHttp, candidate.Address, candidate.Port, "/authstate");
            using var request = new HttpRequestMessage(HttpMethod.Get, builder.Uri);
            request.Headers.UserAgent.ParseAdd("Octoglow-Connect/2.0");
            using var response = await _httpClient.SendAsync(
                request,
                HttpCompletionOption.ResponseHeadersRead,
                timeout.Token).ConfigureAwait(false);

            if (!response.IsSuccessStatusCode) return null;
            var body = await response.Content.ReadAsStringAsync(timeout.Token).ConfigureAwait(false);
            using var json = JsonDocument.Parse(body);
            if (!json.RootElement.TryGetProperty("configured", out var configured) ||
                configured.ValueKind is not (JsonValueKind.True or JsonValueKind.False))
            {
                return null;
            }

            return new DeskClockEndpoint(
                candidate.Address,
                candidate.Port,
                candidate.IsConfiguredAddress,
                configured.GetBoolean());
        }
        catch (Exception error) when (error is HttpRequestException or TaskCanceledException or JsonException)
        {
            return null;
        }
    }

    private static IReadOnlyList<DeskClockCandidate> BuildCandidates(AppConfig config)
    {
        var results = new Dictionary<string, DeskClockCandidate>(StringComparer.OrdinalIgnoreCase);

        if (TryNormalizeConfiguredAddress(config.Esp32Ip, config.Esp32Port, out var configured))
            Add(configured.Address, configured.Port, isConfigured: true);

        // Default address exposed by the ESP32 while it is in provisioning/AP mode.
        Add("192.168.4.1", 80, isConfigured: false);

        try
        {
            var prefixes = new HashSet<string>(StringComparer.Ordinal);
            var networks = NetworkInterface.GetAllNetworkInterfaces()
                .OrderByDescending(network => network.GetIPProperties().GatewayAddresses.Any(gateway =>
                    gateway.Address.AddressFamily == AddressFamily.InterNetwork));

            foreach (var network in networks)
            {
                if (network.OperationalStatus != OperationalStatus.Up ||
                    network.NetworkInterfaceType is NetworkInterfaceType.Loopback or NetworkInterfaceType.Tunnel)
                {
                    continue;
                }

                foreach (var unicast in network.GetIPProperties().UnicastAddresses)
                {
                    if (unicast.Address.AddressFamily != AddressFamily.InterNetwork ||
                        IPAddress.IsLoopback(unicast.Address))
                    {
                        continue;
                    }

                    var bytes = unicast.Address.GetAddressBytes();
                    if (bytes[0] == 169 && bytes[1] == 254) continue;

                    var prefix = $"{bytes[0]}.{bytes[1]}.{bytes[2]}";
                    if (!prefixes.Add(prefix)) continue;
                    if (prefixes.Count > 3) break;

                    // A full /16 or VPN range would be too invasive. The firmware is a
                    // LAN device, so scan only the host's current /24 slice (254 hosts).
                    for (var host = 1; host <= 254; host++)
                    {
                        if (host == bytes[3]) continue;
                        Add($"{prefix}.{host}", 80, isConfigured: false);
                    }
                }

                if (prefixes.Count >= 3) break;
            }
        }
        catch (NetworkInformationException)
        {
            // The saved address and AP fallback remain available.
        }

        return results.Values.ToList();

        void Add(string address, int port, bool isConfigured)
        {
            if (string.IsNullOrWhiteSpace(address) || port is < 1 or > 65535) return;
            var key = $"{address}:{port}";
            if (results.TryGetValue(key, out var existing))
                results[key] = existing with { IsConfiguredAddress = existing.IsConfiguredAddress || isConfigured };
            else
                results[key] = new DeskClockCandidate(address, port, isConfigured);
        }
    }

    internal static bool TryNormalizeConfiguredAddress(string rawAddress, int fallbackPort, out DeskClockCandidate candidate)
    {
        candidate = default;
        if (string.IsNullOrWhiteSpace(rawAddress)) return false;

        var value = rawAddress.Trim();
        var withScheme = value.Contains("://", StringComparison.Ordinal) ? value : $"http://{value}";
        if (!Uri.TryCreate(withScheme, UriKind.Absolute, out var uri) || string.IsNullOrWhiteSpace(uri.Host))
            return false;

        var port = uri.IsDefaultPort ? fallbackPort : uri.Port;
        if (port is < 1 or > 65535) port = 80;
        candidate = new DeskClockCandidate(uri.Host, port, IsConfiguredAddress: true);
        return true;
    }

    private static uint ParseSortableAddress(string address)
    {
        if (!IPAddress.TryParse(address, out var parsed)) return uint.MaxValue;
        var bytes = parsed.GetAddressBytes();
        return bytes.Length == 4
            ? ((uint)bytes[0] << 24) | ((uint)bytes[1] << 16) | ((uint)bytes[2] << 8) | bytes[3]
            : uint.MaxValue;
    }

    public void Dispose() => _httpClient.Dispose();

    internal readonly record struct DeskClockCandidate(string Address, int Port, bool IsConfiguredAddress);
}

public sealed record DeskClockEndpoint(
    string Address,
    int Port,
    bool IsConfiguredAddress,
    bool HasAccount)
{
    public string Id => $"clock:{Address.Trim().ToLowerInvariant()}:{Port}";
}
