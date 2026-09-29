namespace HR.ServerMonitoring.Api.Models;

public class DeviceEvent
{
    public int Id { get; set; }

    public string DeviceId { get; set; } = "";

    public string EventType { get; set; } = "";

    public string? FromLevel { get; set; }

    public string? ToLevel { get; set; }

    public string? Message { get; set; }

    public DateTime Timestamp { get; set; }
}