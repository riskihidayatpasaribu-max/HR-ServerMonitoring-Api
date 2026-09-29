namespace HR.ServerMonitoring.Api.Models;

public class SensorStatus
{
    public string DeviceId { get; set; } = "";

    public double Temperature { get; set; }
    public double Humidity { get; set; }
    public double GasLevel { get; set; }

    public bool TemperatureSafe { get; set; }
    public bool HumiditySafe { get; set; }
    public bool GasLevelSafe { get; set; }

    public bool IsSafe { get; set; }

    public string Status { get; set; } = "";

    public DateTime Timestamp { get; set; }
}