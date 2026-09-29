# HR Server Monitoring System

## 1. Ringkasan

**HR Server Monitoring System** adalah sistem monitoring kondisi
ruang/server berbasis IoT yang mengumpulkan data sensor dari ESP32,
mengirimkannya melalui MQTT, memproses dan menyimpan data pada backend
ASP.NET Core, kemudian menampilkannya secara real-time pada Dashboard
Blazor.

Sistem dirancang untuk memonitor:

-   Temperature
-   Humidity
-   Gas level
-   Status koneksi device
-   Level sensor dari firmware
-   Monitoring range/threshold yang dapat dikonfigurasi dari Dashboard
-   Event perubahan koneksi dan level
-   Riwayat data sensor
-   Command threshold dari backend ke ESP32
-   Mute/unmute dan command perangkat melalui MQTT

Arsitektur komunikasi utama:

``` text
ESP32
  │
  │ MQTT
  ▼
MQTT Broker
  │
  │ MQTT Subscribe
  ▼
ASP.NET Core Backend
  ├── SQLite
  ├── REST API
  └── SignalR
        │
        ▼
Blazor Dashboard
```

Backend tidak berkomunikasi dengan ESP32 melalui HTTP. Komunikasi device
menggunakan MQTT.

------------------------------------------------------------------------

# 2. Tujuan Sistem

Sistem memiliki empat fungsi utama:

1.  **Monitoring**
    -   menerima telemetry dari ESP32;
    -   menampilkan nilai sensor secara real-time;
    -   menampilkan status device.
2.  **Data logging**
    -   menyimpan telemetry ke SQLite;
    -   menyimpan event device;
    -   menyediakan endpoint untuk mengambil data.
3.  **Remote configuration**
    -   user mengubah threshold melalui Dashboard;
    -   backend menyimpan konfigurasi ke database;
    -   backend mengirim command `set_threshold` ke ESP32 melalui MQTT.
4.  **Real-time notification**
    -   backend menerima MQTT message;
    -   backend meneruskan informasi penting ke Dashboard melalui
        SignalR.

------------------------------------------------------------------------

# 3. Teknologi

  Komponen               Teknologi
  ---------------------- --------------------------
  Device                 ESP32
  Firmware               C/C++ Arduino-compatible
  Device protocol        MQTT
  MQTT library backend   MQTTnet
  Broker                 MQTT Broker
  Backend                ASP.NET Core 8
  Language               C#
  ORM                    Entity Framework Core
  Database               SQLite
  Real-time web          SignalR
  Frontend               Blazor
  API style              REST
  Dashboard route        `/dashboard`
  MQTT broker address    `192.16x.xx.xxx:1883`
  Backend URL            `http://localhost:5186`

Versi environment yang digunakan saat pengembangan:

-   .NET SDK: `8.0.424`
-   ASP.NET Core runtime: `8.0.30`
-   Windows 10

------------------------------------------------------------------------

# 4. Identitas Device

Device utama yang digunakan:

  Property    Value
  ----------- --------------------------
  Device ID   `srv-room-01`
  Location    `Ruang Server Lt.1`
  Firmware    `1.0.0`
  MQTT base   `serverroom/srv-room-01`

MQTT topic device dibentuk dari base tersebut.

``` text
serverroom/srv-room-01/telemetry
serverroom/srv-room-01/status
serverroom/srv-room-01/event
serverroom/srv-room-01/cmd
```

------------------------------------------------------------------------

# 5. Arsitektur Sistem

## 5.1 Arsitektur tingkat tinggi

``` mermaid
flowchart LR
    ESP[ESP32 Sensor Device]
    MQTT[MQTT Broker]
    API[ASP.NET Core Backend]
    DB[(SQLite)]
    REST[REST API]
    HUB[SignalR Hub]
    UI[Blazor Dashboard]

    ESP -->|Telemetry / Status / Event| MQTT
    MQTT -->|Subscribe| API

    API --> DB
    API --> REST
    API --> HUB

    REST --> UI
    HUB -->|Real-time events| UI

    UI -->|GET / PUT| REST
    REST --> API

    API -->|MQTT command| MQTT
    MQTT -->|Command| ESP
```

## 5.2 Pembagian tanggung jawab

### ESP32

Bertanggung jawab terhadap:

-   pembacaan sensor;
-   perhitungan level sensor;
-   publikasi telemetry;
-   publikasi status koneksi;
-   publikasi event perubahan level;
-   menerima command MQTT;
-   menyimpan threshold firmware secara persisten;
-   mengontrol output alarm/buzzer sesuai kondisi firmware.

### MQTT Broker

Bertindak sebagai message broker.

Broker tidak menjadi database dan tidak menjadi business-logic layer.

Tugasnya:

-   menerima message dari ESP32;
-   meneruskan message ke subscriber;
-   menerima command dari backend;
-   meneruskan command ke ESP32.

### ASP.NET Core Backend

Bertanggung jawab terhadap:

-   koneksi MQTT;
-   parsing message;
-   persistence;
-   REST API;
-   SignalR;
-   device status;
-   monitoring threshold dari sisi aplikasi;
-   pengiriman command ke ESP32;
-   pencatatan event.

### SQLite

Menyimpan:

-   sensor history;
-   monitoring threshold;
-   device event history.

### Blazor Dashboard

Bertanggung jawab terhadap:

-   visualisasi data;
-   real-time update;
-   konfigurasi threshold;
-   device status;
-   sensor status;
-   event/log viewer.

------------------------------------------------------------------------

# 6. Alur Telemetry

Alur normal telemetry:

``` mermaid
sequenceDiagram
    participant ESP as ESP32
    participant MQTT as MQTT Broker
    participant API as ASP.NET Core
    participant DB as SQLite
    participant UI as Blazor Dashboard

    ESP->>MQTT: Publish /telemetry
    MQTT->>API: MQTT message
    API->>API: Parse JSON
    API->>UI: SignalR ReceiveMqttTelemetry
    API->>API: Check monitoring threshold
    API->>DB: Save SensorData
    UI->>UI: Update dashboard
```

Telemetry dipublikasikan oleh ESP32 secara periodik dan juga dapat
dikirim ketika terjadi perubahan level keseluruhan.

------------------------------------------------------------------------

# 7. MQTT Protocol

## 7.1 Telemetry

### Topic

``` text
serverroom/srv-room-01/telemetry
```

Direction:

``` text
ESP32 → MQTT Broker → Backend
```

QoS:

``` text
0
```

Retained:

``` text
No
```

Payload:

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

### Field

  Field          Tipe           Keterangan
  -------------- -------------- ------------------------------------
  `device_id`    string         ID device
  `location`     string         Lokasi device
  `fw`           string         Firmware version
  `uptime_s`     integer        Uptime ESP32 dalam detik
  `age_ms`       integer        Umur data pada saat dikirim
  `temp_c`       number/null    Temperature
  `hum_pct`      number/null    Humidity
  `gas_raw`      integer/null   Nilai raw gas sensor
  `gas_pct`      number/null    Gas percentage
  `level.temp`   string         Level temperature dari firmware
  `level.hum`    string         Level humidity dari firmware
  `level.gas`    string         Level gas dari firmware
  `status`       string         Overall sensor level dari firmware
  `rssi`         integer        Wi-Fi RSSI

Nilai sensor dapat berupa `null`. Backend memperlakukannya sebagai
nullable value.

**Catatan penting:** `status` pada telemetry adalah **sensor danger
level**, bukan status koneksi MQTT.

------------------------------------------------------------------------

# 8. MQTT Status

## Topic

``` text
serverroom/srv-room-01/status
```

Direction:

``` text
ESP32 → MQTT Broker → Backend
```

QoS:

``` text
0
```

Retained:

``` text
Yes
```

Payload:

``` text
online
```

atau:

``` text
offline
```

Backend melakukan normalisasi payload menjadi uppercase:

``` text
ONLINE
OFFLINE
```

Status `online` merupakan retained message.

ESP32 juga menggunakan Last Will untuk status `offline`.

------------------------------------------------------------------------

# 9. MQTT Event

## Topic

``` text
serverroom/srv-room-01/event
```

Direction:

``` text
ESP32 → MQTT Broker → Backend
```

QoS:

``` text
0
```

Retained:

``` text
No
```

Contoh:

``` json
{
  "device_id": "srv-room-01",
  "what": "level_change",
  "from": "ok",
  "to": "alarm",
  "uptime_s": 1250
}
```

Backend:

1.  membaca event;
2.  menyimpan event ke `DeviceEvents`;
3.  meneruskan event realtime melalui SignalR.

------------------------------------------------------------------------

# 10. MQTT Command

## Topic

``` text
serverroom/srv-room-01/cmd
```

Direction:

``` text
Backend → MQTT Broker → ESP32
```

Topic command tidak digunakan untuk menerima telemetry.

Command yang didukung firmware:

-   `set_threshold`
-   `mute`
-   `unmute`
-   `reboot`

## 10.1 Set threshold

Contoh payload:

``` json
{"command":"set_threshold","temperature_min":17,"temperature_max":26,"humidity_min":39,"humidity_max":90,"gas_min":10,"gas_max":40}
```

Backend membuat payload JSON menggunakan serializer sehingga payload
dikirim sebagai satu JSON message.

## 10.2 Mute

Payload command:

``` text
mute
```

## 10.3 Unmute

Payload command:

``` text
unmute
```

## 10.4 Reboot

Payload command:

``` text
reboot
```

Tidak terdapat command acknowledgment dari ESP32 pada protocol saat ini.
Backend dapat mengetahui bahwa MQTT publish berhasil dilakukan, tetapi
tidak memperoleh konfirmasi aplikasi dari ESP32 bahwa command sudah
dieksekusi.

------------------------------------------------------------------------

# 11. Firmware Threshold vs Monitoring Threshold

Sistem memiliki dua konsep threshold yang harus dibedakan.

## Firmware threshold

Threshold yang dipakai ESP32 untuk menentukan level sensor dan
mengontrol perilaku alarm.

Threshold tersebut dapat dikirim melalui:

``` text
MQTT /cmd
```

Firmware menyimpan konfigurasi threshold secara persisten.

## Backend monitoring threshold

Threshold yang disimpan pada:

``` text
SensorThresholds
```

di SQLite.

Backend menggunakannya untuk menentukan apakah telemetry berada dalam
monitoring range.

Dashboard juga menggunakan threshold ini untuk menampilkan status range.

Keduanya disinkronkan ketika backend mengirim `set_threshold`.

------------------------------------------------------------------------

# 12. Threshold Flow

``` mermaid
sequenceDiagram
    participant UI as Dashboard
    participant API as ThresholdController
    participant DB as SQLite
    participant MQTT as MQTT Broker
    participant ESP as ESP32

    UI->>API: PUT /api/Threshold
    API->>API: Validate min < max
    API->>DB: Save threshold
    API->>MQTT: Publish set_threshold
    MQTT->>ESP: set_threshold
    API-->>UI: threshold + esp32CommandSent
```

Jika MQTT/ESP32 belum terhubung:

``` text
Database update = tetap dilakukan
MQTT command = tidak dikirim
esp32CommandSent = false
```

Jika ESP32 kemudian kembali online, backend melakukan sinkronisasi
threshold yang tersimpan di database ke device.

------------------------------------------------------------------------

# 13. Threshold GET

Endpoint:

``` http
GET /api/Threshold
```

GET threshold **tidak meminta threshold melalui MQTT dari ESP32**.

GET mengambil konfigurasi dari database backend.

Jika belum ada record threshold, backend membuat default:

``` text
Temperature: 20 – 30
Humidity:    40 – 70
Gas Level:    0 – 50
```

Response:

``` json
{
  "id": 1,
  "temperatureMin": 20,
  "temperatureMax": 30,
  "humidityMin": 40,
  "humidityMax": 70,
  "gasLevelMin": 0,
  "gasLevelMax": 50
}
```

------------------------------------------------------------------------

# 14. Monitoring Threshold Logic

Backend memeriksa tiga sensor:

``` text
Temperature
AND
Humidity
AND
Gas
```

Semua harus berada dalam range.

Secara logika:

``` text
in_range =
    temperature_min <= temperature <= temperature_max
    AND
    humidity_min <= humidity <= humidity_max
    AND
    gas_min <= gas <= gas_max
```

Jika salah satu keluar range:

``` text
out_of_range
```

Jika semua kembali dalam range:

``` text
in_range
```

Backend tidak membuat event pada pemeriksaan pertama. Event dibuat
ketika status monitoring berubah.

------------------------------------------------------------------------

# 15. Device Event

Model:

``` text
DeviceEvent
```

Field:

  Field         Tipe       Keterangan
  ------------- ---------- ---------------------
  `Id`          int        Primary key
  `DeviceId`    string     Device sumber
  `EventType`   string     Jenis event
  `FromLevel`   string?    Level sebelumnya
  `ToLevel`     string?    Level baru
  `Message`     string?    Deskripsi
  `Timestamp`   DateTime   Waktu event dicatat

Event yang digunakan antara lain:

``` text
connection
level_change
threshold
threshold_update
```

Endpoint history:

``` http
GET /api/DeviceEvents
```

Backend mengembalikan maksimal 100 event terbaru.

Dashboard dapat memfilter event berdasarkan `DeviceId`.

------------------------------------------------------------------------

# 16. Database

Database:

``` text
SQLite
```

Connection string:

``` text
Data Source=sensordata.db
```

Database berisi minimal tiga entity utama:

``` text
SensorData
SensorThreshold
DeviceEvent
```

## SensorData

``` text
Id
DeviceId
GasLevel
Temperature
Humidity
Timestamp
```

Telemetry hanya disimpan sebagai `SensorData` jika:

``` text
Temperature != null
Humidity != null
GasPercent != null
```

## SensorThreshold

``` text
Id
TemperatureMin
TemperatureMax
HumidityMin
HumidityMax
GasLevelMin
GasLevelMax
```

## DeviceEvent

``` text
Id
DeviceId
EventType
FromLevel
ToLevel
Message
Timestamp
```

------------------------------------------------------------------------

# 17. REST API

Base URL:

``` text
http://localhost:5186
```

## SensorData

### GET

``` http
GET /api/SensorData
```

Mengambil data sensor yang tersimpan.

### POST

``` http
POST /api/SensorData
```

Digunakan oleh controller SensorData untuk menerima penyimpanan data
sensor melalui API.

Pada arsitektur MQTT utama, telemetry dari ESP32 diproses langsung oleh
`MqttBackgroundServices` dan disimpan ke database.

------------------------------------------------------------------------

## Device

### GET

``` http
GET /api/Device
```

Mengambil daftar status device yang diketahui backend.

------------------------------------------------------------------------

## Device Sensor Status

Dashboard menggunakan:

``` http
GET /api/SensorStatus/{deviceId}
```

Endpoint ini digunakan untuk memperoleh status sensor berdasarkan
device.

------------------------------------------------------------------------

## Threshold

### GET

``` http
GET /api/Threshold
```

Mengambil monitoring threshold dari database.

### PUT

``` http
PUT /api/Threshold
```

Mengubah monitoring threshold.

Validasi:

``` text
TemperatureMin < TemperatureMax
HumidityMin < HumidityMax
GasLevelMin < GasLevelMax
```

Response update mencakup:

``` json
{
  "threshold": {},
  "esp32CommandSent": true
}
```

`esp32CommandSent` menunjukkan apakah backend berhasil mengirim command
MQTT berdasarkan koneksi MQTT saat itu.

------------------------------------------------------------------------

## Device Events

### GET

``` http
GET /api/DeviceEvents
```

Mengambil maksimal 100 event terbaru.

------------------------------------------------------------------------

# 18. SignalR

Hub:

``` text
/sensorhub
```

SignalR digunakan untuk update Dashboard tanpa polling terus-menerus.

Event yang digunakan:

  Event                    Fungsi
  ------------------------ --------------------------
  `ReceiveSensorData`      Data sensor realtime
  `ReceiveMqttTelemetry`   Telemetry MQTT realtime
  `ReceiveDeviceStatus`    Perubahan ONLINE/OFFLINE
  `ReceiveMqttEvent`       Event MQTT device

Alur:

``` mermaid
flowchart LR
    MQTT[MQTT]
    SERVICE[MqttBackgroundServices]
    HUB[SignalR Hub]
    DASH[Blazor Dashboard]

    MQTT --> SERVICE
    SERVICE --> HUB
    HUB --> DASH
```

------------------------------------------------------------------------

# 19. Device Status

Status device dikelola oleh:

``` text
DeviceStatusService
```

Service menyimpan status runtime dalam memory.

Informasi:

``` text
DeviceId
Status
LastUpdate
```

Status diperbarui ketika backend menerima MQTT `/status`.

Status runtime ini bukan historical database.

Historical connection events disimpan terpisah di:

``` text
DeviceEvents
```

------------------------------------------------------------------------

# 20. Dashboard

Route:

``` text
/dashboard
```

Dashboard menggunakan Blazor dengan:

``` text
InteractiveServer
```

Dashboard menyediakan:

-   system status;
-   total data;
-   jumlah device;
-   temperature;
-   humidity;
-   gas level;
-   node/device details;
-   sensor status;
-   threshold configuration;
-   device logs;
-   realtime telemetry;
-   realtime device status;
-   synchronization/refresh.

## Device detail

Device dapat dipilih dari Dashboard.

Log kemudian ditampilkan berdasarkan:

``` text
DeviceId
```

Dengan demikian event device lain tidak dicampur ke log device yang
sedang dilihat.

------------------------------------------------------------------------

# 21. Dashboard Data Flow

Saat Dashboard pertama kali dibuka:

``` mermaid
sequenceDiagram
    participant UI as Dashboard
    participant API as ASP.NET

    UI->>API: GET /api/SensorData
    API-->>UI: Sensor history

    UI->>API: GET /api/Threshold
    API-->>UI: Current monitoring threshold

    UI->>API: GET /api/Device
    API-->>UI: Device status

    UI->>UI: Start SignalR connection
```

Setelah SignalR aktif:

``` text
MQTT → Backend → SignalR → Dashboard
```

------------------------------------------------------------------------

# 22. Realtime Telemetry Processing

Ketika telemetry diterima:

``` mermaid
flowchart TD
    A[MQTT telemetry received]
    B[Parse JSON]
    C[Extract device and sensor values]
    D[Send ReceiveMqttTelemetry]
    E[Check monitoring threshold]
    F[Save SensorData]
    G[Dashboard updates]

    A --> B
    B --> C
    C --> D
    D --> G
    C --> E
    E --> F
```

Backend menggunakan:

``` text
gas_pct
```

untuk monitoring threshold gas, bukan `gas_raw`.

Level firmware tetap diambil dari:

``` text
level.temp
level.hum
level.gas
status
```

Backend tidak perlu menghitung ulang level firmware tersebut.

------------------------------------------------------------------------

# 23. Connection Recovery

Backend menjalankan MQTT worker sebagai `BackgroundService`.

Jika MQTT belum connected:

``` text
connect
↓
subscribe
```

Backend melakukan pengecekan koneksi secara periodik.

Jika terjadi error:

``` text
log error
↓
wait
↓
retry
```

Ketika device kembali online, retained status:

``` text
online
```

diterima backend.

Backend kemudian dapat melakukan sinkronisasi threshold database ke
ESP32.

------------------------------------------------------------------------

# 24. Threshold Synchronization

``` mermaid
flowchart TD
    A[ESP32 reconnects]
    B[Status = online]
    C[Backend receives /status]
    D[Read SensorThreshold from SQLite]
    E[Publish set_threshold]
    F[ESP32 applies threshold]

    A --> B
    B --> C
    C --> D
    D --> E
    E --> F
```

Hal ini memastikan threshold backend yang terakhir tersimpan dapat
dikirim kembali setelah device reconnect.

------------------------------------------------------------------------

# 25. Alarm Architecture

Firmware bertanggung jawab terhadap alarm fisik.

Contoh:

``` text
Monitoring threshold berubah
        ↓
Backend PUT threshold
        ↓
MQTT set_threshold
        ↓
ESP32
        ↓
Firmware mengevaluasi sensor
        ↓
Level alarm
        ↓
Buzzer
```

Buzzer menggunakan:

``` text
GPIO 16
```

Konfigurasi firmware:

``` text
HR_PIN_BUZZER = 16
HR_BUZZER_IS_ACTIVE = 1
```

Mute/unmute ditangani di firmware.

------------------------------------------------------------------------

# 26. Mute / Unmute

### Mute

``` text
Dashboard/backend
      ↓
MQTT cmd
      ↓
ESP32
      ↓
mute state aktif
      ↓
buzzer tidak berbunyi
```

### Unmute

``` text
Dashboard/backend
      ↓
MQTT cmd
      ↓
ESP32
      ↓
mute state nonaktif
      ↓
buzzer kembali mengikuti alarm
```

Firmware v1.1 menggunakan pemeriksaan command yang membedakan `mute` dan
`unmute`.

------------------------------------------------------------------------

# 27. Important Protocol Rules

## Jangan menganggap telemetry status sebagai connection status

Telemetry:

``` json
"status": "alarm"
```

berarti level sensor.

Connection status:

``` text
/status → online/offline
```

berarti koneksi device.

Keduanya berbeda.

------------------------------------------------------------------------

## Jangan menggunakan gas_raw sebagai monitoring percentage

Untuk monitoring threshold aplikasi:

``` text
gas_pct
```

digunakan.

`gas_raw` adalah nilai raw sensor.

------------------------------------------------------------------------

## Jangan menganggap GET threshold sebagai MQTT GET

Saat ini:

``` text
GET /api/Threshold
```

berasal dari database backend.

Firmware tidak menyediakan MQTT request/response khusus untuk mengambil
threshold aktif.

------------------------------------------------------------------------

# 28. Error Handling

Backend menangani error MQTT dengan:

``` text
ILogger
```

Jika parsing telemetry gagal:

``` text
log error
```

Jika MQTT command tidak dapat dikirim karena MQTT belum connected:

``` text
return false
```

Database threshold tetap dapat disimpan.

Dashboard mendapatkan status:

``` text
esp32CommandSent = false
```

sehingga user mengetahui bahwa database sudah diperbarui tetapi command
belum dikirim pada saat tersebut.

------------------------------------------------------------------------

# 29. Security Notes

Konfigurasi saat development:

``` text
MQTT host = 192.16x.xx.xxx
MQTT port = 1883
```

Port `1883` digunakan tanpa TLS pada konfigurasi development saat ini.

Karena itu, sistem ini sebaiknya dianggap sebagai deployment jaringan
internal/development sampai ditambahkan:

-   MQTT username/password;
-   TLS;
-   API authentication;
-   authorization;
-   secret management;
-   HTTPS;
-   network segmentation.

Jangan memasukkan credential asli ke repository Git.

------------------------------------------------------------------------

# 30. Project Structure

Struktur konseptual:

``` text
HR.ServerMonitoring.Api/
│
├── Controllers/
│   ├── SensorDataController.cs
│   ├── ThresholdController.cs
│   └── DeviceEventsController.cs
│
├── Data/
│   └── AppDbContext.cs
│
├── Hubs/
│   └── SensorHub.cs
│
├── Models/
│   ├── HR.SensorData.cs
│   ├── SensorThreshold.cs
│   ├── DeviceEvent.cs
│   └── DeviceStatusMessage.cs
│
├── Services/
│   ├── MqttBackgroundServices.cs
│   ├── IMqttCommandService.cs
│   └── DeviceStatusService.cs
│
├── Components/
│   └── Pages/
│       └── Dashboard.razor
│
├── sensordata.db
├── Program.cs
└── ...
```

Nama file dapat berbeda sesuai struktur aktual project, tetapi pembagian
tanggung jawab mengikuti struktur tersebut.

------------------------------------------------------------------------

# 31. Dependency Injection

MQTT background service didaftarkan sebagai singleton dan hosted service
yang menggunakan instance yang sama:

``` csharp
builder.Services.AddSingleton<MqttBackgroundServices>();

builder.Services.AddSingleton<IMqttCommandService>(
    sp => sp.GetRequiredService<MqttBackgroundServices>());

builder.Services.AddHostedService(
    sp => sp.GetRequiredService<MqttBackgroundServices>());
```

Tujuan:

-   `BackgroundService` memakai MQTT client yang sama;
-   controller dapat menggunakan `IMqttCommandService`;
-   command threshold menggunakan koneksi MQTT yang dikelola worker.

------------------------------------------------------------------------

# 32. Startup

Jalankan dari directory project:

``` powershell
cd "C:\FILE RISKI\C#_dan_DOTNET\HR-ServerMonitoring\HR.ServerMonitoring.Api"
```

Build:

``` powershell
dotnet build
```

Run:

``` powershell
dotnet run
```

Backend:

``` text
http://localhost:5186
```

Dashboard:

``` text
http://localhost:5186/dashboard
```

------------------------------------------------------------------------

# 33. Development Test Checklist

## Backend

``` text
[ ] dotnet build
[ ] backend starts
[ ] MQTT connects
[ ] MQTT topics subscribed
```

## Device

``` text
[ ] ESP32 boots
[ ] MQTT connects
[ ] status = online
[ ] telemetry published
```

## Dashboard

``` text
[ ] Dashboard opens
[ ] telemetry visible
[ ] device ONLINE
[ ] temperature updates
[ ] humidity updates
[ ] gas updates
[ ] SignalR connected
```

## Threshold

``` text
[ ] GET threshold
[ ] PUT threshold
[ ] invalid min/max rejected
[ ] database updated
[ ] command sent when MQTT connected
[ ] command not sent when MQTT unavailable
[ ] threshold synchronized after reconnect
```

## Alarm

``` text
[ ] alarm triggered
[ ] buzzer ON
[ ] threshold returned to range
[ ] buzzer OFF
[ ] mute
[ ] unmute
```

## Persistence

``` text
[ ] SensorData saved
[ ] DeviceEvent saved
[ ] threshold survives restart
[ ] device reconnect works
```

------------------------------------------------------------------------

# 34. Known Current Limitations

These are intentional/current protocol characteristics, not assumptions:

### 1. No command acknowledgment

ESP32 does not currently publish a dedicated command acknowledgment.

Therefore:

``` text
MQTT publish success
```

does not mean:

``` text
ESP32 execution confirmed
```

### 2. No absolute ESP32 timestamp

Telemetry contains:

``` text
uptime_s
age_ms
```

but not an absolute device timestamp.

The backend currently records reception time for its database records.

A future implementation could use `age_ms` to estimate the actual
measurement time:

``` text
estimated_event_time =
backend_receive_time - age_ms
```

This is not currently the database timestamp implementation.

### 3. Runtime device status is in memory

`DeviceStatusService` maintains current status in memory.

Historical events are stored in SQLite.

### 4. MQTT authentication is not configured

The development broker currently uses:

``` text
192.16x.xx.xxx:1883
```

without application-level MQTT authentication in this project
configuration.

### 5. Single primary device configuration

The current command publisher uses:

``` text
serverroom/srv-room-01/cmd
```

for threshold command publishing.

The receive subscriptions are wildcard-based:

``` text
serverroom/+/telemetry
serverroom/+/status
serverroom/+/event
```

------------------------------------------------------------------------

# 35. Operational Mental Model

Untuk memahami sistem sebagai programmer/automation engineer:

``` text
ESP32
= field device

MQTT
= communication bus

ASP.NET Core
= supervisory/backend layer

SQLite
= historian / persistence

SignalR
= realtime UI transport

Blazor
= HMI/dashboard
```

Dengan model ini:

``` text
Sensor → Device → MQTT → Backend → Database
                         ↓
                       SignalR
                         ↓
                      Dashboard
```

Sedangkan konfigurasi berjalan terbalik:

``` text
Dashboard
    ↓
REST API
    ↓
Backend
    ↓
MQTT
    ↓
ESP32
```

------------------------------------------------------------------------

# 36. End-to-End Example

Misalnya temperature ESP32 berada di luar threshold.

``` text
1. ESP32 membaca temperature.

2. Firmware menentukan level sensor.

3. ESP32 publish telemetry.

4. MQTT Broker menerima telemetry.

5. ASP.NET MqttBackgroundServices menerima message.

6. Backend parse JSON.

7. Backend mengirim ReceiveMqttTelemetry melalui SignalR.

8. Dashboard memperbarui nilai temperature.

9. Backend mengecek monitoring threshold.

10. Jika status monitoring berubah:
    DeviceEvent disimpan.

11. SensorData disimpan jika semua nilai sensor tersedia.

12. Dashboard menampilkan kondisi terbaru.
```

Jika user kemudian menaikkan threshold:

``` text
Dashboard
    ↓
PUT /api/Threshold
    ↓
ThresholdController
    ├── validate
    ├── save SQLite
    └── publish MQTT
            ↓
        ESP32 /cmd
            ↓
       update threshold
            ↓
       sensor evaluation
            ↓
       buzzer state
```

------------------------------------------------------------------------

# 37. Project Completion Criteria

Project dianggap memiliki fungsi utama lengkap apabila:

``` text
ESP32
  ✓
MQTT
  ✓
Backend
  ✓
SQLite
  ✓
REST API
  ✓
SignalR
  ✓
Dashboard
  ✓
Realtime telemetry
  ✓
Device status
  ✓
Threshold GET
  ✓
Threshold PUT
  ✓
MQTT threshold command
  ✓
Threshold persistence
  ✓
Threshold resync after reconnect
  ✓
Buzzer alarm
  ✓
Mute / unmute
  ✓
Device event history
  ✓
Build without errors
```

------------------------------------------------------------------------

# 38. Final Verification

Sebelum repository dipublikasikan:

``` powershell
dotnet build
git status
```

Pastikan tidak ada file sensitif.

Jangan commit:

``` text
*.db
bin/
obj/
.vs/
credential
password
secret
private key
```

sesuai kebutuhan repository dan `.gitignore`.

------------------------------------------------------------------------

# 39. Kesimpulan

HR Server Monitoring System menggunakan pola:

``` text
IoT Device
    ↓
MQTT
    ↓
ASP.NET Core
    ├── REST
    ├── SignalR
    └── SQLite
         ↓
      Dashboard
```

MQTT menjadi jalur komunikasi utama antara ESP32 dan backend.

REST digunakan untuk operasi request/response seperti:

``` text
GET SensorData
GET Device
GET Threshold
PUT Threshold
GET DeviceEvents
```

SignalR digunakan untuk komunikasi realtime dari backend menuju
Dashboard.

SQLite digunakan sebagai persistence layer.

ESP32 tetap menjadi pihak yang bertanggung jawab terhadap pembacaan
sensor dan perilaku alarm firmware, sedangkan backend menjadi
supervisory layer yang menerima telemetry, menyimpan data, mengelola
konfigurasi monitoring, dan meneruskan command.
