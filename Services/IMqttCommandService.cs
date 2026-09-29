namespace HR.ServerMonitoring.Api.Services;

public interface IMqttCommandService
{
    Task<bool> SetThresholdAsync(
        double temperatureMin,
        double temperatureMax,
        double humidityMin,
        double humidityMax,
        double gasMin,
        double gasMax);
}