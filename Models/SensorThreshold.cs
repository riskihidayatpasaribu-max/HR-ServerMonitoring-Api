namespace HR.ServerMonitoring.Api.Models;

public class SensorThreshold
{
    public int Id { get; set; }

    public double TemperatureMin { get; set; }
    public double TemperatureMax { get; set; }

    public double HumidityMin { get; set; }
    public double HumidityMax { get; set; }

    public double GasLevelMin { get; set; }
    public double GasLevelMax { get; set; }
}