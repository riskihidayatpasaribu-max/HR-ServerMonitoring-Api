# HR Server Monitoring --- System Architecture

## 1. Architecture Overview

``` mermaid
flowchart TB
    subgraph FIELD["FIELD / EDGE"]
        ESP[ESP32 srv-room-01]
        SENSOR[Temperature / Humidity / Gas]
        BUZZER[Buzzer]
        SENSOR --> ESP
        ESP --> BUZZER
    end

    subgraph BUS["MESSAGE BUS"]
        MQTT[MQTT Broker<br/>192.16x.xx.xxx:1883]
    end

    subgraph BACKEND["BACKEND — ASP.NET CORE 8"]
        MQTTWORKER[MqttBackgroundServices]
        STATUS[DeviceStatusService]
        API[REST Controllers]
        HUB[SensorHub / SignalR]
        DBCTX[EF Core / AppDbContext]
    end

    subgraph DATA["PERSISTENCE"]
        DB[(SQLite<br/>sensordata.db)]
    end

    subgraph UI["PRESENTATION"]
        DASH[Blazor Dashboard<br/>/dashboard]
    end

    SENSOR --> ESP
    ESP -->|telemetry / status / event| MQTT
    MQTT -->|subscribe| MQTTWORKER

    MQTTWORKER --> STATUS
    MQTTWORKER --> DBCTX
    DBCTX --> DB

    API --> DBCTX
    MQTTWORKER --> HUB
    HUB --> DASH

    DASH -->|HTTP GET / PUT| API
    API --> DBCTX

    API -->|IMqttCommandService| MQTTWORKER
    MQTTWORKER -->|command| MQTT
    MQTT --> ESP
```

------------------------------------------------------------------------

## 2. Layer Model

``` text
┌─────────────────────────────────────────────┐
│ Presentation Layer                           │
│ Blazor Dashboard                            │
└───────────────────┬─────────────────────────┘
                    │
              REST / SignalR
                    │
┌───────────────────▼─────────────────────────┐
│ Application / Backend Layer                 │
│ ASP.NET Core                                 │
│ Controllers + MQTT Background Service       │
└───────────────┬───────────────┬─────────────┘
                │               │
             EF Core          MQTT
                │               │
┌───────────────▼───────┐   ┌──▼─────────────┐
│ Persistence Layer     │   │ Message Layer  │
│ SQLite                │   │ MQTT Broker    │
└───────────────────────┘   └──────┬─────────┘
                                   │
                              ┌────▼─────┐
                              │ ESP32    │
                              └──────────┘
```

------------------------------------------------------------------------

# 3. Communication Matrix

  Source        Destination   Protocol   Purpose
  ------------- ------------- ---------- -----------------
  ESP32         MQTT Broker   MQTT       Telemetry
  ESP32         MQTT Broker   MQTT       Device status
  ESP32         MQTT Broker   MQTT       Device event
  Backend       MQTT Broker   MQTT       Device command
  MQTT Broker   Backend       MQTT       Device messages
  Dashboard     Backend       HTTP       REST request
  Backend       Dashboard     SignalR    Realtime update
  Backend       SQLite        EF Core    Persistence

------------------------------------------------------------------------

# 4. MQTT Topic Architecture

``` text
serverroom/
└── srv-room-01/
    ├── telemetry
    ├── status
    ├── event
    └── cmd
```

Direction:

``` text
telemetry:
ESP32 ───────────────► Broker ───────────────► Backend

status:
ESP32 ───────────────► Broker ───────────────► Backend

event:
ESP32 ───────────────► Broker ───────────────► Backend

cmd:
Backend ─────────────► Broker ───────────────► ESP32
```

------------------------------------------------------------------------

# 5. Backend Component Architecture

``` mermaid
flowchart LR
    subgraph Controllers
        SDC[SensorDataController]
        TC[ThresholdController]
        DEC[DeviceEventsController]
    end

    subgraph Services
        MBS[MqttBackgroundServices]
        DSS[DeviceStatusService]
    end

    subgraph Realtime
        SH[SensorHub]
    end

    subgraph Persistence
        CTX[AppDbContext]
    end

    SDC --> CTX
    TC --> CTX
    TC --> MBS
    DEC --> CTX

    MBS --> CTX
    MBS --> DSS
    MBS --> SH
```

------------------------------------------------------------------------

# 6. MqttBackgroundServices

`MqttBackgroundServices` memiliki dua tanggung jawab besar:

### Inbound

Menerima:

``` text
/telemetry
/status
/event
```

Kemudian:

-   parse;
-   update runtime state;
-   save database;
-   broadcast SignalR.

### Outbound

Mengirim:

``` text
/cmd
```

untuk command ke ESP32.

Service juga berfungsi sebagai `IMqttCommandService`.

------------------------------------------------------------------------

# 7. MQTT Receive Pipeline

``` mermaid
flowchart TD
    A[MQTT message]
    B[Read topic]
    C{Topic type}

    T[/telemetry]
    S[/status]
    E[/event]

    A --> B
    B --> C

    C --> T
    C --> S
    C --> E

    T --> TP[Parse telemetry]
    TP --> TH[Check monitoring threshold]
    TP --> SAVE1[Save SensorData]
    TP --> SIG1[SignalR telemetry]

    S --> SP[Normalize ONLINE/OFFLINE]
    SP --> DS[Update DeviceStatusService]
    SP --> SAVE2[Save connection event if changed]
    SP --> SIG2[SignalR device status]
    SP --> SYNC[Sync threshold when ONLINE]

    E --> EP[Parse event JSON]
    EP --> SAVE3[Save DeviceEvent]
    EP --> SIG3[SignalR MQTT event]
```

------------------------------------------------------------------------

# 8. Threshold Architecture

There are two related but distinct states:

``` text
┌─────────────────────────────┐
│ Backend Monitoring State    │
│ SQLite SensorThreshold      │
└─────────────┬───────────────┘
              │
              │ set_threshold
              ▼
┌─────────────────────────────┐
│ ESP32 Firmware State        │
│ Runtime + persistent config │
└─────────────────────────────┘
```

Backend threshold:

-   digunakan oleh backend;
-   digunakan oleh Dashboard;
-   disimpan SQLite.

Firmware threshold:

-   digunakan ESP32;
-   memengaruhi evaluasi firmware;
-   memengaruhi alarm/buzzer;
-   dipersistenkan oleh firmware.

------------------------------------------------------------------------

# 9. Reconnect Architecture

``` mermaid
sequenceDiagram
    participant E as ESP32
    participant B as MQTT Broker
    participant M as MqttBackgroundServices
    participant D as SQLite

    E->>B: status = online
    B->>M: retained status
    M->>M: Update DeviceStatusService
    M->>D: Read SensorThreshold
    D-->>M: Current threshold
    M->>B: Publish set_threshold
    B->>E: set_threshold
```

------------------------------------------------------------------------

# 10. Data Persistence Architecture

## Sensor history

``` text
MQTT telemetry
     ↓
MqttBackgroundServices
     ↓
validate nullable sensor fields
     ↓
SensorData
     ↓
SQLite
```

## Event history

``` text
MQTT status/event
       ↓
MqttBackgroundServices
       ↓
DeviceEvent
       ↓
SQLite
```

## Threshold

``` text
Dashboard PUT
       ↓
ThresholdController
       ↓
SensorThreshold
       ↓
SQLite
```

------------------------------------------------------------------------

# 11. Realtime Architecture

``` text
                 ┌───────────────┐
                 │ MQTT Broker   │
                 └───────┬───────┘
                         │
                         ▼
                ┌──────────────────┐
                │ MQTT Background  │
                │ Service          │
                └────────┬─────────┘
                         │
                         ▼
                ┌──────────────────┐
                │ SignalR Hub      │
                └────────┬─────────┘
                         │
                         ▼
                ┌──────────────────┐
                │ Blazor Dashboard │
                └──────────────────┘
```

SignalR menghindari kebutuhan Dashboard untuk melakukan polling
telemetry secara terus-menerus.

------------------------------------------------------------------------

# 12. REST Architecture

``` text
Browser
  │
  ├── GET /api/SensorData
  ├── GET /api/Device
  ├── GET /api/Threshold
  ├── PUT /api/Threshold
  └── GET /api/DeviceEvents
           │
           ▼
      ASP.NET Core
           │
           ▼
        SQLite
```

Untuk threshold PUT terdapat jalur tambahan:

``` text
PUT /api/Threshold
       ↓
SQLite
       ↓
IMqttCommandService
       ↓
MQTT
       ↓
ESP32
```

------------------------------------------------------------------------

# 13. Device Status State Machine

``` mermaid
stateDiagram-v2
    [*] --> UNKNOWN
    UNKNOWN --> ONLINE: status online
    ONLINE --> OFFLINE: status offline
    OFFLINE --> ONLINE: status online
```

Setiap perubahan status dapat dibuat sebagai historical `connection`
event.

------------------------------------------------------------------------

# 14. Monitoring State Machine

``` mermaid
stateDiagram-v2
    [*] --> INITIAL
    INITIAL --> IN_RANGE: first valid evaluation
    INITIAL --> OUT_OF_RANGE: first valid evaluation

    IN_RANGE --> OUT_OF_RANGE: threshold violation
    OUT_OF_RANGE --> IN_RANGE: values return to range
```

Pada evaluasi pertama backend menyimpan state internal tanpa membuat
transition event.

------------------------------------------------------------------------

# 15. Sensor Level vs Monitoring Level

Firmware menyediakan:

``` text
level.temp
level.hum
level.gas
status
```

Backend juga memiliki konsep:

``` text
in_range
out_of_range
```

Keduanya bukan field yang sama.

### Firmware level

Menjelaskan level yang ditentukan firmware.

### Monitoring state

Menjelaskan apakah nilai telemetry memenuhi monitoring threshold yang
tersimpan di backend.

------------------------------------------------------------------------

# 16. Dashboard Architecture

``` mermaid
flowchart TD
    INIT[Dashboard initialization]
    SENSOR[GET SensorData]
    THRESH[GET Threshold]
    DEVICE[GET Device]
    SIGNALR[Start SignalR]

    INIT --> SENSOR
    INIT --> THRESH
    INIT --> DEVICE
    INIT --> SIGNALR

    SIGNALR --> RT1[ReceiveMqttTelemetry]
    SIGNALR --> RT2[ReceiveDeviceStatus]
    SIGNALR --> RT3[ReceiveMqttEvent]

    RT1 --> UI1[Update telemetry cards]
    RT2 --> UI2[Update device status]
    RT3 --> UI3[Update realtime event state]
```

------------------------------------------------------------------------

# 17. Threshold Update Architecture

``` mermaid
flowchart TD
    U[User changes threshold]
    V[Frontend validation]
    P[PUT /api/Threshold]
    C[ThresholdController]
    DB[Save SQLite]
    MQTT[SetThresholdAsync]
    BROKER[MQTT Broker]
    ESP[ESP32]

    U --> V
    V --> P
    P --> C
    C --> DB
    C --> MQTT
    MQTT --> BROKER
    BROKER --> ESP
```

------------------------------------------------------------------------

# 18. Failure Scenarios

## MQTT broker unavailable

``` text
Backend
  ↓
MQTT connect fails
  ↓
log error
  ↓
wait
  ↓
retry
```

## ESP32 offline during threshold update

``` text
Dashboard
  ↓
PUT threshold
  ↓
SQLite updated
  ↓
MQTT unavailable
  ↓
esp32CommandSent = false
```

Database state therefore remains available even if the device is
temporarily unavailable.

## ESP32 reconnect

``` text
status online
   ↓
backend receives retained status
   ↓
read database threshold
   ↓
publish set_threshold
```

------------------------------------------------------------------------

# 19. Current Trust Boundaries

``` text
ESP32
  └─ sensor reading and firmware behavior

MQTT
  └─ transport

Backend
  ├─ API
  ├─ persistence
  ├─ monitoring logic
  └─ command dispatch

Dashboard
  └─ presentation and configuration UI
```

Tidak ada business logic penting yang seharusnya bergantung pada nilai
statis Dashboard.

------------------------------------------------------------------------

# 20. Current Architecture Limitations

1.  MQTT command tidak memiliki acknowledgment application-level.
2.  MQTT development configuration menggunakan port 1883 tanpa TLS.
3.  Device status runtime disimpan in-memory.
4.  Database menggunakan SQLite.
5.  Command threshold saat ini diarahkan ke device `srv-room-01`.
6.  Timestamp database saat ini menggunakan waktu backend ketika message
    diproses; `age_ms` belum digunakan untuk mengoreksi timestamp.
7.  Firmware tidak menyediakan request/response MQTT untuk membaca
    threshold aktif.

------------------------------------------------------------------------

# 21. Future Extension Points

Bagian berikut adalah extension point, bukan fitur yang saat ini diklaim
sudah tersedia.

### Multi-device

Dapat diperluas dari:

``` text
srv-room-01
```

menjadi:

``` text
srv-room-01
srv-room-02
srv-room-03
...
```

### Command acknowledgment

Dapat ditambahkan topic:

``` text
serverroom/{deviceId}/ack
```

### MQTT authentication

Dapat ditambahkan:

``` text
username
password
TLS
```

### Historical analytics

Dapat ditambahkan:

-   charts;
-   aggregation;
-   daily/monthly statistics;
-   retention policy.

### Better timestamping

`age_ms` dapat digunakan untuk memperkirakan waktu pengukuran aktual.

------------------------------------------------------------------------

# 22. Architecture Principle

Prinsip utama sistem:

``` text
Device
  = source of physical data

MQTT
  = transport

Backend
  = system coordinator

Database
  = persistence

SignalR
  = realtime delivery

Dashboard
  = visualization + configuration
```

Ini membuat sistem dapat dikembangkan tanpa mengubah fungsi utama
device.
