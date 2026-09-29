using System.Text;
using System.Text.Json;
using System.Collections.Concurrent;
using MQTTnet;
using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;
using HR.ServerMonitoring.Api.Hubs;
using HR.ServerMonitoring.Api.Data;
using HR.ServerMonitoring.Api.Models;

namespace HR.ServerMonitoring.Api.Services;

public class MqttBackgroundServices :
    BackgroundService,
    IMqttCommandService
{
    private readonly ILogger<MqttBackgroundServices> _logger;
    private readonly IHubContext<SensorHub> _hubContext;
    private readonly IServiceScopeFactory _scopeFactory;
    private readonly DeviceStatusService _deviceStatusService;

    private readonly ConcurrentDictionary<string, bool> _monitoringStates = new();

    private IMqttClient? _mqttClient;

    public MqttBackgroundServices(
        ILogger<MqttBackgroundServices> logger,
        IHubContext<SensorHub> hubContext,
        IServiceScopeFactory scopeFactory,
        DeviceStatusService deviceStatusService)
    {
        _logger = logger;
        _hubContext = hubContext;
        _scopeFactory = scopeFactory;
        _deviceStatusService = deviceStatusService;
    }

    protected override async Task ExecuteAsync(
        CancellationToken stoppingToken)
    {
        var factory = new MqttClientFactory();

        _mqttClient = factory.CreateMqttClient();

        _mqttClient.ApplicationMessageReceivedAsync += async e =>
        {
            var topic = e.ApplicationMessage.Topic;

            var payload = Encoding.UTF8.GetString(
                e.ApplicationMessage.Payload);

            _logger.LogInformation(
                "MQTT RECEIVED | {Topic} | {Payload}",
                topic,
                payload);

            await HandleMessageAsync(topic, payload);
        };

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                if (!_mqttClient.IsConnected)
                {
                    var options = new MqttClientOptionsBuilder()
                        .WithTcpServer("192.16x.xx.xxx", 1883)
                        .Build();

                    await _mqttClient.ConnectAsync(
                        options,
                        stoppingToken);

                    _logger.LogInformation(
                        "MQTT CONNECTED");

                    await SubscribeAsync(
                        "serverroom/+/telemetry",
                        stoppingToken);

                    await SubscribeAsync(
                        "serverroom/+/status",
                        stoppingToken);

                    await SubscribeAsync(
                        "serverroom/+/event",
                        stoppingToken);

                    _logger.LogInformation(
                        "MQTT SUBSCRIBED TO DEVICE TOPICS");
                }

                await Task.Delay(
                    TimeSpan.FromSeconds(5),
                    stoppingToken);
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(
                    ex,
                    "MQTT ERROR");

                await Task.Delay(
                    TimeSpan.FromSeconds(5),
                    stoppingToken);
            }
        }
    }

    private async Task SubscribeAsync(
        string topic,
        CancellationToken cancellationToken)
    {
        var filter = new MqttTopicFilterBuilder()
            .WithTopic(topic)
            .Build();

        await _mqttClient!.SubscribeAsync(
            filter,
            cancellationToken);
    }

    private async Task HandleMessageAsync(
        string topic,
        string payload)
    {
        // =========================
        // DEVICE CONNECTION STATUS
        // =========================

        if (topic.EndsWith("/status"))
        {
            var deviceId = ExtractDeviceId(topic);

            var status = payload
                .Trim()
                .ToUpperInvariant();

            var previousStatus =
                _deviceStatusService.GetStatus(deviceId);

            _deviceStatusService.UpdateStatus(
                deviceId,
                status);

            if (status == "ONLINE")
            {
                await SyncThresholdToDeviceAsync(deviceId);
            }
            

            // Simpan event hanya jika status benar-benar berubah
            if (previousStatus != status)
            {
                await SaveDeviceEventAsync(
                    deviceId,
                    "connection",
                    previousStatus,
                    status,
                    $"Device {status.ToLowerInvariant()}.");
            }

            await _hubContext.Clients.All.SendAsync(
                "ReceiveDeviceStatus",
                new DeviceStatusMessage
                {
                    DeviceId = deviceId,
                    Status = status
                });

            return;
        }

        // =========================
        // SENSOR TELEMETRY
        // =========================

        if (topic.EndsWith("/telemetry"))
        {
            try
            {
                using var json =
                    JsonDocument.Parse(payload);

                var root = json.RootElement;

                var level =
                    root.GetProperty("level");

                var data = new
                {
                    DeviceId =
                        root.GetProperty("device_id").GetString(),

                    Location =
                        root.GetProperty("location").GetString(),

                    Firmware =
                        root.GetProperty("fw").GetString(),

                    UptimeS =
                        root.GetProperty("uptime_s").GetInt64(),

                    AgeMs =
                        root.GetProperty("age_ms").GetInt64(),

                    Temperature =
                        GetNullableDouble(
                            root,
                            "temp_c"),

                    Humidity =
                        GetNullableDouble(
                            root,
                            "hum_pct"),

                    GasRaw =
                        GetNullableInt(
                            root,
                            "gas_raw"),

                    GasPercent =
                        GetNullableDouble(
                            root,
                            "gas_pct"),

                    TemperatureLevel =
                        level.GetProperty("temp").GetString(),

                    HumidityLevel =
                        level.GetProperty("hum").GetString(),

                    GasLevel =
                        level.GetProperty("gas").GetString(),

                    OverallLevel =
                        root.GetProperty("status").GetString(),

                    Rssi =
                        root.GetProperty("rssi").GetInt32(),

                    ReceivedAt =
                        DateTime.UtcNow.Subtract(
                            TimeSpan.FromMilliseconds(Math.Max(0, root.GetProperty("age_ms").GetInt64())))
                };

                // Kirim telemetry realtime ke Dashboard
                await _hubContext.Clients.All.SendAsync(
                    "ReceiveMqttTelemetry",
                    data);

                await CheckMonitoringThresholdAsync(
                    data.DeviceId!,
                    data.Temperature,
                    data.Humidity,
                    data.GasPercent);

                // Simpan telemetry ke database
                if (data.Temperature.HasValue &&
                    data.Humidity.HasValue &&
                    data.GasPercent.HasValue)
                {
                    var sensorData = new SensorData
                    {
                        DeviceId = data.DeviceId!,
                        Temperature = data.Temperature.Value,
                        Humidity = data.Humidity.Value,
                        GasLevel = data.GasPercent.Value,
                        Timestamp = data.ReceivedAt
                    };

                    using var scope =
                        _scopeFactory.CreateScope();

                    var context =
                        scope.ServiceProvider
                            .GetRequiredService<AppDbContext>();

                    context.SensorDataEntries.Add(
                        sensorData);

                    await context.SaveChangesAsync();
                }
            }
            catch (Exception ex)
            {
                _logger.LogError(
                    ex,
                    "Gagal memproses telemetry MQTT");
            }

            return;
        }

        // =========================
        // DEVICE EVENT
        // =========================

        if (topic.EndsWith("/event"))
        {
            var deviceId =
                ExtractDeviceId(topic);

            try
            {
                using var doc =
                    JsonDocument.Parse(payload);

                var root =
                    doc.RootElement;

                var what =
                    root.TryGetProperty(
                        "what",
                        out var whatProp)
                        ? whatProp.GetString()
                        : "unknown";

                var from =
                    root.TryGetProperty(
                        "from",
                        out var fromProp)
                        ? fromProp.GetString()
                        : null;

                var to =
                    root.TryGetProperty(
                        "to",
                        out var toProp)
                        ? toProp.GetString()
                        : null;

                var message =
                    $"Level berubah dari " +
                    $"{from ?? "-"} menjadi " +
                    $"{to ?? "-"}.";

                await SaveDeviceEventAsync(
                    deviceId,
                    what ?? "unknown",
                    from,
                    to,
                    message);

                // Kirim event realtime ke Dashboard
                await _hubContext.Clients.All.SendAsync(
                    "ReceiveMqttEvent",
                    new
                    {
                        DeviceId = deviceId,
                        Payload = payload
                    });
            }
            catch (Exception ex)
            {
                _logger.LogError(
                    ex,
                    "Gagal memproses MQTT event dari {DeviceId}",
                    deviceId);
            }

            return;
        }
    }

    private async Task CheckMonitoringThresholdAsync(
        string deviceId,
        double? temperature,
        double? humidity,
        double? gasPercent)
    {
        if (!temperature.HasValue ||
            !humidity.HasValue ||
            !gasPercent.HasValue)
        {
            return;
        }

        using var scope =
            _scopeFactory.CreateScope();

        var db =
            scope.ServiceProvider
                .GetRequiredService<AppDbContext>();

        var threshold =
            await db.SensorThresholds
                .FirstOrDefaultAsync();

        if (threshold == null)
            return;

        var inRange =
            temperature.Value >= threshold.TemperatureMin &&
            temperature.Value <= threshold.TemperatureMax &&

            humidity.Value >= threshold.HumidityMin &&
            humidity.Value <= threshold.HumidityMax &&

            gasPercent.Value >= threshold.GasLevelMin &&
            gasPercent.Value <= threshold.GasLevelMax;

        // Belum ada status sebelumnya.
        // Simpan status awal tanpa membuat event.
        if (!_monitoringStates.TryGetValue(
                deviceId,
                out var previousState))
        {
            _monitoringStates[deviceId] = inRange;
            return;
        }

        // Tidak ada perubahan.
        if (previousState == inRange)
            return;

        // Simpan status terbaru.
        _monitoringStates[deviceId] = inRange;

        var fromLevel =
            previousState
                ? "in_range"
                : "out_of_range";

        var toLevel =
            inRange
                ? "in_range"
                : "out_of_range";

        var message =
            inRange
                ? "Semua sensor kembali ke dalam monitoring range."
                : $"Sensor berada di luar monitoring range. " +
                  $"Temperature={temperature.Value:F1}°C, " +
                  $"Humidity={humidity.Value:F1}%, " +
                  $"Gas={gasPercent.Value:F1}.";

        await SaveDeviceEventAsync(
            deviceId,
            "threshold",
            fromLevel,
            toLevel,
            message);
    }

    // =========================
    // SEND THRESHOLD COMMAND
    // =========================

    public async Task<bool> SetThresholdAsync(
        double temperatureMin,
        double temperatureMax,
        double humidityMin,
        double humidityMax,
        double gasMin,
        double gasMax)
    {
        if (_mqttClient == null ||
            !_mqttClient.IsConnected)
        {
            _logger.LogWarning(
                "Tidak dapat mengirim threshold: MQTT belum terhubung.");

            return false;
        }

        var command = new
        {
            command = "set_threshold",

            temperature_min = temperatureMin,
            temperature_max = temperatureMax,

            humidity_min = humidityMin,
            humidity_max = humidityMax,

            gas_min = gasMin,
            gas_max = gasMax
        };

        var payload = JsonSerializer.Serialize(command);

        var message = new MqttApplicationMessageBuilder()
            .WithTopic("serverroom/srv-room-01/cmd")
            .WithPayload(payload)
            .Build();

        await _mqttClient.PublishAsync(message);

        _logger.LogInformation(
            "MQTT THRESHOLD SENT | {Payload}",
            payload);

        return true;
    }

    // =========================
    // SYNC THRESHOLD AFTER DEVICE ONLINE
    // =========================

    private async Task SyncThresholdToDeviceAsync(string deviceId)
    {
        using var scope = _scopeFactory.CreateScope();

        var db = scope.ServiceProvider
            .GetRequiredService<AppDbContext>();

        var threshold = await db.SensorThresholds
            .FirstOrDefaultAsync();

        if (threshold == null)
        {
            _logger.LogWarning(
                "THRESHOLD SYNC SKIPPED | Tidak ada threshold di database | Device={DeviceId}",
                deviceId);

            return;
        }

        var sent = await SetThresholdAsync(
            threshold.TemperatureMin,
            threshold.TemperatureMax,
            threshold.HumidityMin,
            threshold.HumidityMax,
            threshold.GasLevelMin,
            threshold.GasLevelMax);

        if (sent)
        {
            _logger.LogInformation(
                "THRESHOLD SYNC SENT | Device={DeviceId}",
                deviceId);
        }
        else
        {
            _logger.LogWarning(
                "THRESHOLD SYNC FAILED | Device={DeviceId}",
                deviceId);
        }
    }

    // =========================
    // SAVE DEVICE EVENT
    // =========================

    private async Task SaveDeviceEventAsync(
        string deviceId,
        string eventType,
        string? fromLevel,
        string? toLevel,
        string? message)
    {
        using var scope =
            _scopeFactory.CreateScope();

        var db =
            scope.ServiceProvider
                .GetRequiredService<AppDbContext>();

        var deviceEvent = new DeviceEvent
        {
            DeviceId = deviceId,
            EventType = eventType,
            FromLevel = fromLevel,
            ToLevel = toLevel,
            Message = message,
            Timestamp = DateTime.UtcNow
        };

        db.DeviceEvents.Add(
            deviceEvent);

        await db.SaveChangesAsync();
    }

    // =========================
    // MQTT TOPIC → DEVICE ID
    // =========================

    private static string ExtractDeviceId(
        string topic)
    {
        var parts =
            topic.Split('/');

        return parts.Length >= 3
            ? parts[1]
            : "";
    }

    // =========================
    // JSON NULLABLE DOUBLE
    // =========================

    private static double? GetNullableDouble(
        JsonElement root,
        string property)
    {
        var value =
            root.GetProperty(property);

        return value.ValueKind == JsonValueKind.Null
            ? null
            : value.GetDouble();
    }

    // =========================
    // JSON NULLABLE INT
    // =========================

    private static int? GetNullableInt(
        JsonElement root,
        string property)
    {
        var value =
            root.GetProperty(property);

        return value.ValueKind == JsonValueKind.Null
            ? null
            : value.GetInt32();
    }
}