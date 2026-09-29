using System.Collections.Concurrent;


namespace HR.ServerMonitoring.Api.Services;

public class DeviceStatusService
{
    private readonly ConcurrentDictionary<string, DeviceStatus> _devices = new();

    public void UpdateStatus(
        string deviceId,
        string status)
    {
        if (_devices.TryGetValue(deviceId, out var device))
        {
            device.Status = status;
            device.LastUpdate = DateTime.UtcNow;
        }
        else
        {
            _devices[deviceId] = new DeviceStatus
            {
                DeviceId = deviceId,
                Status = status,
                LastUpdate = DateTime.UtcNow
            };
        }
    }

    public List<DeviceStatus> GetDevices()
    {
        return _devices.Values.ToList();
    }

    public string? GetStatus(string deviceId)
    {
        return _devices.TryGetValue(
            deviceId,
            out var device)
            ? device.Status
            : null;
    }
}

public class DeviceStatus
{
    public string DeviceId { get; set; } = "";

    public string Status { get; set; } = "";

    public DateTime LastUpdate { get; set; }
}