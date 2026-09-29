/*
 * ============================================================
 *  RH_ServerRoomMonitor
 *  Monitoring Ruang Server berbasis ESP32 NodeMCU-32S
 * ============================================================
 *
 *  Sensor & aktuator
 *    DHT22   -> GPIO17        (suhu + kelembapan)
 *    MQ2 AO  -> GPIO33        (gas / asap, ADC1 -> aman saat WiFi on)
 *    Buzzer  -> GPIO16
 *    Button  -> GPIO32, GPIO35, GPIO34
 *
 *  Display 3.5" 480x320, SPI langsung (driver RH_Tft, tanpa TFT_eSPI).
 *  Pin & init SAMA dengan AB_test_fullscreen.ino (terbukti jalan):
 *    CS GPIO5 | DC GPIO2 | RST GPIO4 | SCLK GPIO18 | MOSI GPIO23
 *  Butuh library "Adafruit GFX Library". Touch tidak dipakai.
 *
 *  PERINGATAN WIRING
 *    1. GPIO34 & GPIO35 input-only tanpa pull-up internal.
 *       WAJIB resistor 10k dari pin ke 3.3V.
 *    2. MQ2 disuplai 5V -> pin AO bisa mencapai ~5V.
 *       WAJIB pembagi tegangan (10k + 20k) sebelum masuk GPIO33.
 *
 *  Prinsip kerja loop: tidak ada delay() sama sekali. Semua modul
 *  "dicek sebentar" tiap putaran, seperti satu petugas yang keliling
 *  memeriksa beberapa meja, bukan duduk menunggu di satu meja.
 * ============================================================
 */

#include "RH_Config.h"
#include "RH_Sensors.h"
#include "RH_Buttons.h"
#include "RH_Alarm.h"
#include "RH_Mqtt.h"
#include "RH_Display.h"
#include <WiFi.h>
#include <cstring>
#include <cstdio>

static HrSensors  sensors;
static HrButtons  buttons;
static HrAlarm    alarmCtl;
static HrMqtt     net;
static HrDisplay  screen;

static uint32_t lastPublish  = 0;
static uint32_t lastDisplay  = 0;
static HrLevel  lastOverall  = HR_LEVEL_OK;

// ------------------------------------------------------------
//  Perintah dari backend (nanti diisi kalau backend sudah ada)
// ------------------------------------------------------------
static void onMqttCommand(const char* topic, const char* payload) {
  (void)topic;

  // Perintah sederhana lama tetap didukung, tetapi exact match dipakai
  // agar "unmute" tidak terbaca sebagai "mute".
  if (strcmp(payload, "mute") == 0) {
    alarmCtl.mute();
    return;
  }

  if (strcmp(payload, "unmute") == 0) {
    alarmCtl.unmute();
    return;
  }

  if (strcmp(payload, "reboot") == 0) {
    ESP.restart();
    return;
  }

  // Format threshold dari backend:
  // {"command":"set_threshold","temperature_min":18,...}
  if (strncmp(
      payload,
      "{\"command\":\"set_threshold\"",
      strlen("{\"command\":\"set_threshold\"")
    ) == 0) {
      
    HrThresholds t{};
    int parsed = sscanf(
      payload,
      "{\"command\":\"set_threshold\",\"temperature_min\":%f,\"temperature_max\":%f,\"humidity_min\":%f,\"humidity_max\":%f,\"gas_min\":%f,\"gas_max\":%f}",
      &t.temperatureMin, &t.temperatureMax,
      &t.humidityMin, &t.humidityMax,
      &t.gasMinPct, &t.gasMaxPct);

    if (parsed == 6 &&
        t.temperatureMin < t.temperatureMax &&
        t.humidityMin < t.humidityMax &&
        t.gasMinPct < t.gasMaxPct) {
      sensors.setThresholds(t, true);
      alarmCtl.setLevel(sensors.reading().overall);
      HR_LOG("[THR ] Threshold MQTT diterapkan\n");
    } else {
      HR_LOG("[THR ] Payload threshold tidak valid\n");
    }
  }
}

// ------------------------------------------------------------
//  FUNGSI TOMBOL - BELUM DITENTUKAN
//  Kejadian yang tersedia:
//    HR_BTN_SHORT  -> tekan lalu lepas
//    HR_BTN_LONG   -> ditahan >= 0.8 detik
//  Index: 0 = GPIO32, 1 = GPIO35, 2 = GPIO34
//
//  Sementara ini diisi perilaku aman/netral. Ganti isinya
//  begitu kamu sudah menentukan fungsi tiap tombol.
// ------------------------------------------------------------
static void hrHandleButtons() {
  for (uint8_t i = 0; i < HrButtons::COUNT; i++) {
    HrBtnEvent e = buttons.take(i);
    if (e == HR_BTN_NONE) continue;

    alarmCtl.beepOnce(40);                 // umpan balik "klik"
    HR_LOG("[BTN] Tombol %u : %s\n", i, (e == HR_BTN_LONG) ? "TAHAN" : "TEKAN");

    switch (i) {
      case 0:   // GPIO32
        if (e == HR_BTN_SHORT) screen.nextPage();
        // TODO: fungsi tahan-lama untuk tombol 1
        break;

      case 1:   // GPIO35
        if (e == HR_BTN_SHORT) alarmCtl.mute();
        // TODO: fungsi tahan-lama untuk tombol 2
        break;

      case 2:   // GPIO34
        if (e == HR_BTN_SHORT) screen.forceRedraw();
        // TODO: fungsi tahan-lama untuk tombol 3
        break;
    }
  }
}

// ------------------------------------------------------------
void setup() {
  Serial.begin(HR_SERIAL_BAUD);
  delay(300);
  HR_LOG("\n\n=== HR Server Room Monitor v%s ===\n", HR_FW_VERSION);

  screen.begin();
  screen.splash("Server Room Monitor", "menyiapkan sensor...");

  sensors.begin();
  buttons.begin();
  alarmCtl.begin();
  net.begin();
  net.setCommandHandler(onMqttCommand);

  // Bip pendek satu kali: tanda alat hidup dan buzzer normal.
  alarmCtl.beepOnce(120);

  screen.forceRedraw();
  HR_LOG("[SYS ] Setup selesai\n");
}

// ------------------------------------------------------------
void loop() {
  uint32_t now = millis();

  buttons.update();
  hrHandleButtons();

  bool fresh = sensors.update();
  const HrReading& r = sensors.reading();

  alarmCtl.setLevel(r.overall);
  alarmCtl.update();

  net.update();

  // Status berubah -> kirim saat itu juga, jangan tunggu jadwal.
  if (fresh && r.overall != lastOverall) {
    HR_LOG("[SYS ] Status: %s -> %s\n",
           hrLevelName(lastOverall), hrLevelName(r.overall));
    net.publishEvent("level_change", lastOverall, r.overall);
    net.publishReading(r);
    lastOverall = r.overall;
    lastPublish = now;
  }

  // Kiriman rutin.
  if (now - lastPublish >= HR_PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    net.publishReading(r);
  }

  // Perbarui layar.
  if (now - lastDisplay >= HR_DISPLAY_INTERVAL_MS) {
    lastDisplay = now;

    HrNetInfo info;
    info.wifiOk    = net.wifiOk();
    info.mqttOk    = net.mqttOk();
    info.rssi      = net.rssi();
    info.pending   = net.pending();
    info.muted     = alarmCtl.isMuted();
    info.warmupSec = sensors.warmupRemainSec();
    info.ip        = info.wifiOk ? WiFi.localIP().toString() : String("-");

    screen.update(r, info);
  }
}
