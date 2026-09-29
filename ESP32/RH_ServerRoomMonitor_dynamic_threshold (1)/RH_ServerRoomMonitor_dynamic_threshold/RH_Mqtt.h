/*
 * RH_Mqtt.h
 * WiFi + MQTT, semuanya non-blocking (tidak pernah menahan loop).
 *
 * Kalau jaringan putus, pembacaan disimpan dulu di RAM
 * (seperti ruang tunggu), lalu dikirim menyusul begitu tersambung.
 */
#ifndef HR_MQTT_H
#define HR_MQTT_H

#include "RH_Config.h"

class HrMqtt {
public:
  void begin();
  void update();                       // panggil tiap loop

  // Masukkan pembacaan ke antrian kirim.
  // Kalau MQTT tersambung -> langsung kirim.
  // Kalau tidak -> disimpan di buffer.
  void publishReading(const HrReading& r);

  // Kirim catatan perubahan status alarm.
  void publishEvent(const char* what, HrLevel from, HrLevel to);

  bool wifiOk() const;
  bool mqttOk();
  uint16_t pending() const { return _count; }
  int rssi() const;

  // Perintah dari backend. Isi fungsinya di .ino kalau sudah dibutuhkan.
  void setCommandHandler(void (*cb)(const char* topic, const char* payload));

private:
  struct Slot { HrReading r; uint32_t stampMs; };

  Slot     _buf[HR_BUFFER_SLOTS];
  uint16_t _head = 0;          // posisi tulis berikutnya
  uint16_t _count = 0;         // berapa yang menunggu
  bool     _overflowed = false;

  uint32_t _lastWifiTry = 0;
  uint32_t _lastMqttTry = 0;
  bool     _wasConnected = false;

  void ensureWifi();
  void ensureMqtt();
  void flushBuffer();
  void bufferPush(const HrReading& r);

  bool sendTelemetry(const HrReading& r, uint32_t ageMs);
  size_t buildJson(char* out, size_t cap, const HrReading& r, uint32_t ageMs);
};

#endif // HR_MQTT_H
