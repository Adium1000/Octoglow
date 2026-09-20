using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;

namespace OctoglowSender.Devices.Macro.Services;

/// <summary>
/// P/Invoke minimal peste SetupAPI + hid.dll, suficient pentru a enumera
/// dispozitivele HID conectate și a le citi VID/PID + stringurile
/// Manufacturer/Product. Nu necesită capabilități speciale în manifest,
/// funcționează și pentru aplicații unpackaged.
/// </summary>
internal static class NativeHid
{
    public static readonly Guid GUID_DEVINTERFACE_HID = new("4D1E55B2-F16F-11CF-88CB-001111000030");

    private const uint DIGCF_PRESENT = 0x02;
    private const uint DIGCF_DEVICEINTERFACE = 0x10;

    private const uint GENERIC_READ = 0x80000000;
    private const uint GENERIC_WRITE = 0x40000000;
    private const uint FILE_SHARE_READ = 0x1;
    private const uint FILE_SHARE_WRITE = 0x2;
    private const uint OPEN_EXISTING = 3;

    [StructLayout(LayoutKind.Sequential)]
    private struct SP_DEVICE_INTERFACE_DATA
    {
        public int cbSize;
        public Guid InterfaceClassGuid;
        public int Flags;
        public IntPtr Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct HIDD_ATTRIBUTES
    {
        public int Size;
        public ushort VendorID;
        public ushort ProductID;
        public ushort VersionNumber;
    }

    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern IntPtr SetupDiGetClassDevs(ref Guid classGuid, IntPtr enumerator, IntPtr hwndParent, uint flags);

    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern bool SetupDiEnumDeviceInterfaces(IntPtr deviceInfoSet, IntPtr deviceInfoData, ref Guid interfaceClassGuid, uint memberIndex, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData);

    [DllImport("setupapi.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern bool SetupDiGetDeviceInterfaceDetail(IntPtr deviceInfoSet, ref SP_DEVICE_INTERFACE_DATA deviceInterfaceData, IntPtr deviceInterfaceDetailData, uint deviceInterfaceDetailDataSize, out uint requiredSize, IntPtr deviceInfoData);

    [DllImport("setupapi.dll", SetLastError = true)]
    private static extern bool SetupDiDestroyDeviceInfoList(IntPtr deviceInfoSet);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    private static extern SafeFileHandle CreateFile(string fileName, uint desiredAccess, uint shareMode, IntPtr securityAttributes, uint creationDisposition, uint flagsAndAttributes, IntPtr templateFile);

    [DllImport("hid.dll", SetLastError = true)]
    private static extern bool HidD_GetAttributes(SafeFileHandle hidDeviceObject, ref HIDD_ATTRIBUTES attributes);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool HidD_GetProductString(SafeFileHandle hidDeviceObject, StringBuilder buffer, int bufferLength);

    [DllImport("hid.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool HidD_GetManufacturerString(SafeFileHandle hidDeviceObject, StringBuilder buffer, int bufferLength);

    public record HidDeviceInfo(string Path, ushort VendorId, ushort ProductId, ushort VersionNumber, string? Manufacturer, string? Product);

    /// <summary>Enumeră toate interfețele HID prezente în sistem în acest moment.</summary>
    public static IEnumerable<HidDeviceInfo> EnumerateHidDevices()
    {
        var results = new List<HidDeviceInfo>();
        var guid = GUID_DEVINTERFACE_HID;
        var deviceInfoSet = SetupDiGetClassDevs(ref guid, IntPtr.Zero, IntPtr.Zero, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (deviceInfoSet == IntPtr.Zero || deviceInfoSet == new IntPtr(-1))
            return results;

        try
        {
            uint index = 0;
            while (true)
            {
                var interfaceData = new SP_DEVICE_INTERFACE_DATA();
                interfaceData.cbSize = Marshal.SizeOf<SP_DEVICE_INTERFACE_DATA>();

                if (!SetupDiEnumDeviceInterfaces(deviceInfoSet, IntPtr.Zero, ref guid, index, ref interfaceData))
                    break; // nu mai sunt device-uri

                index++;

                // primul apel aflăm dimensiunea necesară
                SetupDiGetDeviceInterfaceDetail(deviceInfoSet, ref interfaceData, IntPtr.Zero, 0, out uint requiredSize, IntPtr.Zero);
                if (requiredSize == 0) continue;

                var detailBuffer = Marshal.AllocHGlobal((int)requiredSize);
                try
                {
                    // cbSize pentru SP_DEVICE_INTERFACE_DETAIL_DATA: 6 pe x64, 5 pe x86 (particularitate cunoscută a Windows API)
                    Marshal.WriteInt32(detailBuffer, Environment.Is64BitProcess ? 8 : 5);

                    if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, ref interfaceData, detailBuffer, requiredSize, out _, IntPtr.Zero))
                        continue;

                    var pathPtr = IntPtr.Add(detailBuffer, 4);
                    var devicePath = Marshal.PtrToStringAuto(pathPtr);
                    if (string.IsNullOrEmpty(devicePath)) continue;

                    using var handle = CreateFile(devicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, IntPtr.Zero, OPEN_EXISTING, 0, IntPtr.Zero);
                    if (handle.IsInvalid) continue;

                    var attributes = new HIDD_ATTRIBUTES { Size = Marshal.SizeOf<HIDD_ATTRIBUTES>() };
                    if (!HidD_GetAttributes(handle, ref attributes)) continue;

                    var productBuf = new StringBuilder(256);
                    var manufacturerBuf = new StringBuilder(256);
                    string? product = HidD_GetProductString(handle, productBuf, productBuf.Capacity * 2) ? productBuf.ToString() : null;
                    string? manufacturer = HidD_GetManufacturerString(handle, manufacturerBuf, manufacturerBuf.Capacity * 2) ? manufacturerBuf.ToString() : null;

                    results.Add(new HidDeviceInfo(devicePath, attributes.VendorID, attributes.ProductID, attributes.VersionNumber, manufacturer, product));
                }
                finally
                {
                    Marshal.FreeHGlobal(detailBuffer);
                }
            }
        }
        finally
        {
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
        }

        return results;
    }
}
