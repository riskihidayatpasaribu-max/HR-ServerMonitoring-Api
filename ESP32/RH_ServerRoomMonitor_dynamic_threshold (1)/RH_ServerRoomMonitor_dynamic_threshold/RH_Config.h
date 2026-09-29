/*
 * RH_Config.h
 * Monitoring Ruang Server - ESP32
 * Semua pengaturan yang mungkin kamu ubah ada di file ini saja.
 */
#ifndef HR_CONFIG_H
#define HR_CONFIG_H

#include <Arduino.h>
#include "RH_Thresholds.h"

// ============================================================
//  1. IDENTITAS PERANGKAT
// ============================================================
#define HR_DEVICE_ID      "srv-room-01"
#define HR_LOCATION       "Ruang Server 1"
#define HR_FW_VERSION     "1.3.0"

// ============================================================
//  2. PIN SENSOR / AKTUATOR
// ============================================================
#define HR_PIN_DHT22      17   // DHT22 data
#define HR_PIN_MQ2_AO     33   // MQ2 analog out -> ADC1_CH5 (aman saat WiFi aktif)
#define HR_PIN_BUZZER     16   // Buzzer

// Button panel.
// GPIO32  : punya pull-up internal  -> cukup INPUT_PULLUP
// GPIO35  : INPUT-ONLY, tanpa pull-up internal -> WAJIB resistor 10k ke 3.3V
// GPIO34  : INPUT-ONLY, tanpa pull-up internal -> WAJIB resistor 10k ke 3.3V
#define HR_PIN_BTN1       32
#define HR_PIN_BTN2       35
#define HR_PIN_BTN3       34

// Semua tombol dianggap aktif-LOW (tekan = tersambung ke GND).
#define HR_BTN_ACTIVE_LOW 1

// ------------------------------------------------------------
//  Layar TFT 480x320 - SPI langsung (tanpa TFT_eSPI).
//  Pin & urutan init SAMA dengan AB_test_fullscreen.ino (terbukti jalan).
//  Tidak ada MISO (layar hanya ditulis, tidak dibaca).
// ------------------------------------------------------------
#define HR_PIN_TFT_CS     5
#define HR_PIN_TFT_DC     2
#define HR_PIN_TFT_RST    4
#define HR_PIN_TFT_SCLK   18
#define HR_PIN_TFT_MOSI   23

#define HR_TFT_W          480
#define HR_TFT_H          320
#define HR_TFT_MADCTL     0x28          // landscape, terbukti benar di tes
#define HR_TFT_SPI_HZ     20000000UL

// ============================================================
//  3. BUZZER
// ============================================================
// 1 = buzzer aktif (modul dengan driver sendiri, cukup HIGH/LOW)
// 0 = buzzer pasif (perlu frekuensi, pakai LEDC tone)
#define HR_BUZZER_IS_ACTIVE   1
#define HR_BUZZER_TONE_HZ     2700   // dipakai kalau buzzer pasif
#define HR_BUZZER_LEDC_CH     0

// ============================================================
//  4. AMBANG BATAS (DEFAULT)
// ============================================================
// Nilai default ini dipakai hanya saat ESP32 belum pernah menerima
// konfigurasi dari backend. Setelah menerima konfigurasi, nilainya
// disimpan permanen di NVS melalui Preferences.
//
// Dashboard memakai rentang aman MIN..MAX. Nilai di luar rentang
// dianggap ALARM dan mengaktifkan buzzer.
#define HR_DEFAULT_TEMP_MIN   -40.0f
#define HR_DEFAULT_TEMP_MAX    32.0f
#define HR_DEFAULT_HUM_MIN     25.0f
#define HR_DEFAULT_HUM_MAX     75.0f
#define HR_DEFAULT_GAS_MIN_PCT  0.0f
#define HR_DEFAULT_GAS_MAX_PCT 48.84f   // ~2000/4095*100

// Batas lama tetap didefinisikan sebagai referensi kalibrasi saja.
// Evaluasi alarm runtime TIDAK lagi menggunakan nilai ini.
#define HR_TEMP_WARN      27.0f
#define HR_TEMP_ALARM     32.0f
#define HR_HUM_HI_WARN    65.0f
#define HR_HUM_HI_ALARM   75.0f
#define HR_HUM_LO_WARN    35.0f
#define HR_HUM_LO_ALARM   25.0f
#define HR_GAS_WARN       1200
#define HR_GAS_ALARM      2000

// Nilai dianggap "beneran" kalau sudah lewat batas selama X ms.
// Fungsinya seperti orang yang tidak langsung panik saat dengar bunyi
// aneh sekali - dia tunggu dulu beberapa detik apakah terus berbunyi.
#define HR_LEVEL_DEBOUNCE_MS  5000UL

// ============================================================
//  5. INTERVAL WAKTU
// ============================================================
#define HR_DHT_INTERVAL_MS        2000UL    // DHT22 minimal 2 detik
#define HR_MQ2_INTERVAL_MS        500UL
#define HR_DISPLAY_INTERVAL_MS    500UL
#define HR_PUBLISH_INTERVAL_MS    10000UL   // kirim rutin tiap 10 detik
#define HR_MQ2_WARMUP_MS          30000UL   // heater MQ2 perlu pemanasan

// ============================================================
//  6. WIFI
// ============================================================
#define HR_WIFI_SSID      "NAMA WIFI"
#define HR_WIFI_PASS      "PASSWORD WIFI"
#define HR_WIFI_RETRY_MS  15000UL

// ============================================================
//  7. MQTT (broker lokal di jaringan kantor)
// ============================================================
#define HR_MQTT_HOST      "192.168.x.x"   // GANTI: IP broker Mosquitto kantor
#define HR_MQTT_PORT      1883
#define HR_MQTT_USER      ""               // kosongkan kalau broker tanpa auth
#define HR_MQTT_PASS      ""
#define HR_MQTT_RETRY_MS  5000UL

// Struktur topic:
//   <base>/telemetry  -> data sensor (JSON)
//   <base>/status     -> "online" / "offline" (retained + Last Will)
//   <base>/event      -> perubahan status alarm
//   <base>/cmd        -> perintah masuk dari backend
#define HR_MQTT_BASE      "serverroom/" HR_DEVICE_ID
#define HR_TOPIC_TELE     HR_MQTT_BASE "/telemetry"
#define HR_TOPIC_STATUS   HR_MQTT_BASE "/status"
#define HR_TOPIC_EVENT    HR_MQTT_BASE "/event"
#define HR_TOPIC_CMD      HR_MQTT_BASE "/cmd"

// Kalau MQTT putus, data disimpan dulu di RAM sebanyak ini,
// lalu dikirim menyusul begitu tersambung lagi.
#define HR_BUFFER_SLOTS   40

// ============================================================
//  8. DEBUG
// ============================================================
#define HR_SERIAL_BAUD    115200
#define HR_DEBUG          1

#if HR_DEBUG
  #define HR_LOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define HR_LOG(...)   do { } while (0)
#endif

// ============================================================
//  TIPE DATA BERSAMA
// ============================================================
enum HrLevel {
  HR_LEVEL_OK    = 0,
  HR_LEVEL_WARN  = 1,
  HR_LEVEL_ALARM = 2
};

struct HrReading {
  float    tempC;
  float    humPct;
  int      gasRaw;
  float    gasPct;
  bool     tempValid;
  bool     humValid;
  bool     gasValid;
  HrLevel  tempLevel;
  HrLevel  humLevel;
  HrLevel  gasLevel;
  HrLevel  overall;
  uint32_t stampMs;
};

const char* hrLevelName(HrLevel lv);

#endif // HR_CONFIG_H
