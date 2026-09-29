# HR Server Monitoring — API Reference

Base URL:

```text
http://localhost:5186

# REST API

1. SENSOR DATA
GET /api/

POST /api/SensorData
Content-Type: application/json
{
  "deviceId": "srv-room-01",
  "gasLevel": 16,
  "temperature": 23.5,
  "humidity": 49.5,
  "timestamp": "2026-09-25T01:00:00Z"
}

2. DEVICE
GET /api/Device

3. THRESHOLD
GET /api/Threshold
Response: 
{
  "id": 1,
  "temperatureMin": 20,
  "temperatureMax": 30,
  "humidityMin": 40,
  "humidityMax": 70,
  "gasLevelMin": 0,
  "gasLevelMax": 50
}

PUT /api/Threshold
Content-Type: application/json
{
  "temperatureMin": 20,
  "temperatureMax": 30,
  "humidityMin": 40,
  "humidityMax": 70,
  "gasLevelMin": 0,
  "gasLevelMax": 50
}
validation:
TemperatureMin < TemperatureMax
HumidityMin < HumidityMax
GasLevelMin < GasLevelMax
Response:
{
  "threshold": {
    "id": 1,
    "temperatureMin": 20,
    "temperatureMax": 30,
    "humidityMin": 40,
    "humidityMax": 70,
    "gasLevelMin": 0,
    "gasLevelMax": 50
  },
  "esp32CommandSent": true
}

4. DEVICE EVENTS
GET /api/DeviceEvents
Response:
[
  {
    "id": 35,
    "deviceId": "srv-room-01",
    "eventType": "threshold",
    "fromLevel": "out_of_range",
    "toLevel": "in_range",
    "message": "Semua sensor kembali ke dalam monitoring range.",
    "timestamp": "2026-09-25T01:10:00Z"
  }
]

# SignalR API
Hub : /sensorhub
Events :
ReceiveSensorData
ReceiveMqttTelemetry
ReceiveDeviceStatus
ReceiveMqttEvent

#MQTT API
Base : serverroom/srv-room-01

Telemetry
1. Topic : serverroom/srv-room-01/telemetry
2. Direction : ESP32 → Backend
3. Payload :
{
  "device_id": "srv-room-01",
  "location": "Ruang Server Lt.1",
  "fw": "1.0.0",
  "uptime_s": 680,
  "age_ms": 0,
  "temp_c": 23.5,
  "hum_pct": 49.5,
  "gas_raw": 656,
  "gas_pct": 16.0,
  "level": {
    "temp": "ok",
    "hum": "ok",
    "gas": "ok"
  },
  "status": "ok",
  "rssi": -45
}

4. Device Status : serverroom/srv-room-01/status
5. Device Events : serverroom/srv-room-01/event
Payload:
{
  "device_id": "srv-room-01",
  "what": "level_change",
  "from": "ok",
  "to": "alarm",
  "uptime_s": 3600
}

# MQTT Commands
1. Set Threshold : serverroom/srv-room-01/
Payload:
{
  "command": "set_threshold",
  "temperature_min": 20,
  "temperature_max": 30,
  "humidity_min": 40,
  "humidity_max": 70,
  "gas_min": 0,
  "gas_max": 50
}

2. serverroom/srv-room-01/cmd (mute, unmute, reboot)
