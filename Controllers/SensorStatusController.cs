using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using HR.ServerMonitoring.Api.Data;
using HR.ServerMonitoring.Api.Models;

namespace HR.ServerMonitoring.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class SensorStatusController : ControllerBase
{
    private readonly AppDbContext _context;

    public SensorStatusController(AppDbContext context)
    {
        _context = context;
    }

    [HttpGet("{deviceId}")]
    public async Task<IActionResult> GetStatus(string deviceId)
    {
        var sensor = await _context.SensorDataEntries
            .Where(x => x.DeviceId == deviceId)
            .OrderByDescending(x => x.Timestamp)
            .FirstOrDefaultAsync();

        if (sensor == null)
        {
            return NotFound("Data sensor tidak ditemukan.");
        }

        var threshold = await _context.SensorThresholds
            .FirstOrDefaultAsync();

        if (threshold == null)
        {
            return NotFound("Threshold belum tersedia.");
        }

        var temperatureSafe =
            sensor.Temperature >= threshold.TemperatureMin &&
            sensor.Temperature <= threshold.TemperatureMax;

        var humiditySafe =
            sensor.Humidity >= threshold.HumidityMin &&
            sensor.Humidity <= threshold.HumidityMax;

        var gasLevelSafe =
            sensor.GasLevel >= threshold.GasLevelMin &&
            sensor.GasLevel <= threshold.GasLevelMax;

        var isSafe =
            temperatureSafe &&
            humiditySafe &&
            gasLevelSafe;

        return Ok(new SensorStatus
        {
            DeviceId = sensor.DeviceId,

            Temperature = sensor.Temperature,
            Humidity = sensor.Humidity,
            GasLevel = sensor.GasLevel,

            TemperatureSafe = temperatureSafe,
            HumiditySafe = humiditySafe,
            GasLevelSafe = gasLevelSafe,

            IsSafe = isSafe,
            Status = isSafe ? "SAFE" : "UNSAFE",

            Timestamp = sensor.Timestamp
        });
    }
}