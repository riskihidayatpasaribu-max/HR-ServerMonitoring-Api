namespace HR.ServerMonitoring.Api.Models;

public class MqttTelemetry
{
    public string DeviceId { get; set; } = "";
    public string Location { get; set; } = "";
    public string Firmware { get; set; } = "";

    public long UptimeS { get; set; }
    public long AgeMs { get; set; }

    public double? Temperature { get; set; }
    public double? Humidity { get; set; }
    public int? GasRaw { get; set; }
    public double? GasPercent { get; set; }

    public string TemperatureLevel { get; set; } = "";
    public string HumidityLevel { get; set; } = "";
    public string GasLevel { get; set; } = "";

    public string OverallLevel { get; set; } = "";

    public int Rssi { get; set; }

    public DateTime ReceivedAt { get; set; }
}