# HR Server Monitoring --- API & Protocol Reference

## Base URL

``` text
http://localhost:5186
```

------------------------------------------------------------------------

# REST API

## 1. SensorData

### GET /api/SensorData

Mengambil data sensor yang tersimpan.

Response berbentuk array `SensorData`.

Model:

``` json
{
  "id": 1,
  "deviceId": "srv-room-01",
  "gasLevel": 14.2,
  "temperature": 22.5,
  "humidity": 54.3,
  "timestamp": "2026-09-26T00:00:00Z"
}
```

### POST /api/SensorData

Endpoint untuk penyimpanan `SensorData` melalui controller.

Pada jalur MQTT utama, telemetry ESP32 diproses langsung oleh
`MqttBackgroundServices` dan disimpan menggunakan `AppDbContext`.

------------------------------------------------------------------------

# 2. Device

## GET /api/Device

Mengambil device status yang diketahui backend.

Contoh konsep response:

``` json
[
  {
    "deviceId": "srv-room-01",
    "status": "ONLINE",
    "lastUpdate": "2026-09-26T00:00:00Z"
  }
]
```

Status berasal dari `DeviceStatusService`.

------------------------------------------------------------------------

# 3. Sensor Status

## GET /api/SensorStatus/{deviceId}

Mengambil status sensor untuk device tertentu.

Digunakan Dashboard untuk mendapatkan status sensor berdasarkan device.

------------------------------------------------------------------------

# 4. Threshold

## GET /api/Threshold

Mengambil monitoring threshold dari SQLite.

Jika belum tersedia, backend membuat default:

``` json
{
  "temperatureMin": 20,
  "temperatureMax": 30,
  "humidityMin": 40,
  "humidityMax": 70,
  "gasLevelMin": 0,
  "gasLevelMax": 50
}
```

------------------------------------------------------------------------

## PUT /api/Threshold

Mengubah monitoring threshold.

Request:

``` json
{
  "temperatureMin": 17,
  "temperatureMax": 26,
  "humidityMin": 39,
  "humidityMax": 90,
  "gasLevelMin": 10,
  "gasLevelMax": 40
}
```

Validation:

``` text
temperatureMin < temperatureMax
humidityMin < humidityMax
gasLevelMin < gasLevelMax
```

Response:

``` json
{
  "threshold": {
    "id": 1,
    "temperatureMin": 17,
    "temperatureMax": 26,
    "humidityMin": 39,
    "humidityMax": 90,
    "gasLevelMin": 10,
    "gasLevelMax": 40
  },
  "esp32CommandSent": true
}
```

Jika MQTT tidak terhubung:

``` json
{
  "esp32CommandSent": false
}
```

Database tetap menyimpan threshold.

------------------------------------------------------------------------

# 5. Device Events

## GET /api/DeviceEvents

Mengambil maksimal 100 event terbaru.

Model:

``` json
{
  "id": 35,
  "deviceId": "srv-room-01",
  "eventType": "threshold",
  "fromLevel": "out_of_range",
  "toLevel": "in_range",
  "message": "Semua sensor kembali ke dalam monitoring range.",
  "timestamp": "2026-09-26T00:00:00Z"
}
```

------------------------------------------------------------------------

# SignalR

## Hub

``` text
/sensorhub
```

------------------------------------------------------------------------

## ReceiveSensorData

Digunakan untuk menerima data sensor realtime.

------------------------------------------------------------------------

## ReceiveMqttTelemetry

Digunakan untuk menerima telemetry yang diproses dari MQTT.

Data dapat berisi:

``` text
DeviceId
Location
Firmware
UptimeS
AgeMs
Temperature
Humidity
GasRaw
GasPercent
TemperatureLevel
HumidityLevel
GasLevel
OverallLevel
Rssi
ReceivedAt
```

------------------------------------------------------------------------

## ReceiveDeviceStatus

Digunakan ketika status device berubah.

Payload:

``` json
{
  "deviceId": "srv-room-01",
  "status": "ONLINE"
}
```

------------------------------------------------------------------------

## ReceiveMqttEvent

Digunakan untuk meneruskan event MQTT.

Payload yang dikirim backend mencakup:

``` json
{
  "deviceId": "srv-room-01",
  "payload": "{...original MQTT payload...}"
}
```

------------------------------------------------------------------------

# MQTT

## Broker

``` text
192.16x.xx.xxx:1883
```

------------------------------------------------------------------------

## Topic Matrix

  Topic                          Direction           QoS Retained
  ------------------------------ ----------------- ----- ----------
  `serverroom/+/telemetry`       ESP32 → Backend       0 No
  `serverroom/+/status`          ESP32 → Backend       0 Yes
  `serverroom/+/event`           ESP32 → Backend       0 No
  `serverroom/srv-room-01/cmd`   Backend → ESP32       0 No

Backend subscribes to:

``` text
serverroom/+/telemetry
serverroom/+/status
serverroom/+/event
```

------------------------------------------------------------------------

# Telemetry Payload

``` json
{
  "device_id": "srv-room-01",
  "location": "Ruang Server Lt.1",
  "fw": "1.0.0",
  "uptime_s": 1234,
  "age_ms": 25,
  "temp_c": 22.5,
  "hum_pct": 54.3,
  "gas_raw": 850,
  "gas_pct": 14.2,
  "level": {
    "temp": "ok",
    "hum": "ok",
    "gas": "ok"
  },
  "status": "ok",
  "rssi": -58
}
```

------------------------------------------------------------------------

# Status Payload

``` text
online
```

atau:

``` text
offline
```

Status retained.

Last Will digunakan untuk `offline`.

------------------------------------------------------------------------

# Event Payload

``` json
{
  "device_id": "srv-room-01",
  "what": "level_change",
  "from": "ok",
  "to": "alarm",
  "uptime_s": 1250
}
```

------------------------------------------------------------------------

# Command Payloads

## set_threshold

``` json
{"command":"set_threshold","temperature_min":17,"temperature_max":26,"humidity_min":39,"humidity_max":90,"gas_min":10,"gas_max":40}
```

## mute

``` text
mute
```

## unmute

``` text
unmute
```

## reboot

``` text
reboot
```

------------------------------------------------------------------------

# Protocol Semantics

## Connection state

Diperoleh dari:

``` text
/status
```

## Sensor state

Diperoleh dari:

``` text
telemetry.level
telemetry.status
```

## Monitoring state

Dihitung backend berdasarkan:

``` text
SensorThreshold
```

------------------------------------------------------------------------

# Timestamp Semantics

Telemetry memiliki:

``` text
uptime_s
age_ms
```

Backend saat ini menyimpan waktu pemrosesan/penerimaan backend sebagai
timestamp database.

`age_ms` belum digunakan sebagai koreksi timestamp.

Jika ingin memperkirakan waktu pengukuran:

``` text
measurement_time ≈ backend_receive_time - age_ms
```

------------------------------------------------------------------------

# Command Reliability

Saat backend mengirim command:

``` text
Publish MQTT berhasil
```

hanya berarti publish ke MQTT client berhasil.

Tidak terdapat:

``` text
command acknowledgment
```

dari firmware pada protocol saat ini.

Karena itu:

``` text
esp32CommandSent = true
```

tidak boleh diinterpretasikan sebagai:

``` text
ESP32 execution confirmed
```

------------------------------------------------------------------------

# Error Cases

### Threshold invalid

``` text
HTTP 400
```

jika minimum \>= maximum.

### MQTT unavailable

Threshold database tetap dapat disimpan.

Command tidak dikirim dan:

``` text
esp32CommandSent = false
```

### Nullable sensor

Telemetry sensor dapat `null`.

Backend hanya menyimpan `SensorData` jika temperature, humidity, dan gas
percentage tersedia semuanya.
