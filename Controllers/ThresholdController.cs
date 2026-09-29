using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using HR.ServerMonitoring.Api.Data;
using HR.ServerMonitoring.Api.Models;
using HR.ServerMonitoring.Api.Services;

namespace HR.ServerMonitoring.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class ThresholdController : ControllerBase
{
    private readonly AppDbContext _context;
    private readonly IMqttCommandService _mqttCommandService;

    public ThresholdController(
        AppDbContext context,
        IMqttCommandService mqttCommandService)
    {
        _context = context;
        _mqttCommandService = mqttCommandService;
    }

    [HttpGet]
    public async Task<IActionResult> Get()
    {
        var threshold = await _context.SensorThresholds
            .FirstOrDefaultAsync();

        if (threshold == null)
        {
            threshold = new SensorThreshold
            {
                TemperatureMin = 20,
                TemperatureMax = 30,
                HumidityMin = 40,
                HumidityMax = 70,
                GasLevelMin = 0,
                GasLevelMax = 50
            };

            _context.SensorThresholds.Add(threshold);
            await _context.SaveChangesAsync();
        }

        return Ok(threshold);
    }

    [HttpPut]
    public async Task<IActionResult> Update(SensorThreshold input)
    {
        // =========================
        // VALIDASI
        // =========================

        if (input.TemperatureMin >= input.TemperatureMax)
            return BadRequest(
                "Temperature minimum harus lebih kecil dari maximum.");

        if (input.HumidityMin >= input.HumidityMax)
            return BadRequest(
                "Humidity minimum harus lebih kecil dari maximum.");

        if (input.GasLevelMin >= input.GasLevelMax)
            return BadRequest(
                "Gas level minimum harus lebih kecil dari maximum.");

        // =========================
        // SIMPAN DATABASE
        // =========================

        var threshold = await _context.SensorThresholds
            .FirstOrDefaultAsync();

        if (threshold == null)
        {
            threshold = new SensorThreshold
            {
                TemperatureMin = input.TemperatureMin,
                TemperatureMax = input.TemperatureMax,
                HumidityMin = input.HumidityMin,
                HumidityMax = input.HumidityMax,
                GasLevelMin = input.GasLevelMin,
                GasLevelMax = input.GasLevelMax
            };

            _context.SensorThresholds.Add(threshold);
        }
        else
        {
            threshold.TemperatureMin = input.TemperatureMin;
            threshold.TemperatureMax = input.TemperatureMax;
            threshold.HumidityMin = input.HumidityMin;
            threshold.HumidityMax = input.HumidityMax;
            threshold.GasLevelMin = input.GasLevelMin;
            threshold.GasLevelMax = input.GasLevelMax;
        }

        await _context.SaveChangesAsync();

        // =========================
        // KIRIM KE ESP32
        // =========================

        var sent = await _mqttCommandService.SetThresholdAsync(
            threshold.TemperatureMin,
            threshold.TemperatureMax,
            threshold.HumidityMin,
            threshold.HumidityMax,
            threshold.GasLevelMin,
            threshold.GasLevelMax);

        // =========================
        // SIMPAN EVENT KONFIGURASI
        // =========================

        var thresholdEvent = new DeviceEvent
        {
            DeviceId = "srv-room-01",
            EventType = "threshold_update",
            FromLevel = null,
            ToLevel = null,
            Message = sent
                ? "Monitoring threshold diperbarui dan command dikirim ke ESP32."
                : "Monitoring threshold diperbarui di database, tetapi command belum dikirim karena MQTT/ESP32 belum terhubung.",
            Timestamp = DateTime.UtcNow
        };

        _context.DeviceEvents.Add(thresholdEvent);
        await _context.SaveChangesAsync();

        // =========================
        // RESPONSE
        // =========================

        return Ok(new
        {
            threshold,
            esp32CommandSent = sent
        });
    }
}
