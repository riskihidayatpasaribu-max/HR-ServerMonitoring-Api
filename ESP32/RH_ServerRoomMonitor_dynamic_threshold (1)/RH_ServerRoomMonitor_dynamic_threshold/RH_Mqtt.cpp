#include "RH_Mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient   s_net;
static PubSubClient s_mqtt(s_net);
static void (*s_cmdCb)(const char*, const char*) = nullptr;

static void s_onMessage(char* topic, byte* payload, unsigned int len) {
  static char buf[256];
  unsigned int n = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  HR_LOG("[MQTT] Masuk %s : %s\n", topic, buf);
  if (s_cmdCb) s_cmdCb(topic, buf);
}

void HrMqtt::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // hemat daya dimatikan supaya MQTT stabil
  WiFi.setAutoReconnect(true);
  WiFi.begin(HR_WIFI_SSID, HR_WIFI_PASS);
  _lastWifiTry = millis();

  s_mqtt.setServer(HR_MQTT_HOST, HR_MQTT_PORT);
  s_mqtt.setBufferSize(512);
  s_mqtt.setKeepAlive(30);
  s_mqtt.setSocketTimeout(5);
  s_mqtt.setCallback(s_onMessage);

  HR_LOG("[NET] Menyambung ke SSID \"%s\", broker %s:%d\n",
         HR_WIFI_SSID, HR_MQTT_HOST, (int)HR_MQTT_PORT);
}

void HrMqtt::setCommandHandler(void (*cb)(const char*, const char*)) {
  s_cmdCb = cb;
}

bool HrMqtt::wifiOk() const { return WiFi.status() == WL_CONNECTED; }
bool HrMqtt::mqttOk()       { return s_mqtt.connected(); }
int  HrMqtt::rssi()   const { return wifiOk() ? WiFi.RSSI() : 0; }

void HrMqtt::ensureWifi() {
  if (wifiOk()) return;

  uint32_t now = millis();
  if (now - _lastWifiTry < HR_WIFI_RETRY_MS) return;
  _lastWifiTry = now;

  HR_LOG("[NET] WiFi putus, coba sambung ulang\n");
  WiFi.disconnect();
  WiFi.begin(HR_WIFI_SSID, HR_WIFI_PASS);
}

void HrMqtt::ensureMqtt() {
  if (!wifiOk() || s_mqtt.connected()) return;

  uint32_t now = millis();
  if (now - _lastMqttTry < HR_MQTT_RETRY_MS) return;
  _lastMqttTry = now;

  const char* user = (strlen(HR_MQTT_USER) > 0) ? HR_MQTT_USER : nullptr;
  const char* pass = (strlen(HR_MQTT_PASS) > 0) ? HR_MQTT_PASS : nullptr;

  // Last Will: kalau ESP32 mati mendadak, broker sendiri yang
  // mengabarkan "offline" ke backend.
  bool ok = s_mqtt.connect(HR_DEVICE_ID, user, pass,
                           HR_TOPIC_STATUS, 0, true, "offline");

  if (ok) {
    HR_LOG("[MQTT] Tersambung ke broker\n");
    s_mqtt.publish(HR_TOPIC_STATUS, "online", true);
    s_mqtt.subscribe(HR_TOPIC_CMD);
    flushBuffer();
  } else {
    HR_LOG("[MQTT] Gagal, state=%d\n", s_mqtt.state());
  }
}

void HrMqtt::update() {
  ensureWifi();
  ensureMqtt();

  bool up = s_mqtt.connected();
  if (up) s_mqtt.loop();

  if (up && !_wasConnected) flushBuffer();
  _wasConnected = up;
}

void HrMqtt::bufferPush(const HrReading& r) {
  _buf[_head].r       = r;
  _buf[_head].stampMs = millis();
  _head = (uint16_t)((_head + 1) % HR_BUFFER_SLOTS);

  if (_count < HR_BUFFER_SLOTS) {
    _count++;
  } else {
    // Buffer penuh: data paling tua ditimpa. Ini disengaja -
    // data terbaru lebih berguna daripada data 10 menit lalu.
    _overflowed = true;
  }
}

void HrMqtt::publishReading(const HrReading& r) {
  if (s_mqtt.connected()) {
    if (sendTelemetry(r, 0)) return;
  }
  bufferPush(r);
  HR_LOG("[MQTT] Offline, data ditampung (%u menunggu%s)\n",
         _count, _overflowed ? ", ada yang tertimpa" : "");
}

void HrMqtt::flushBuffer() {
  if (_count == 0) return;
  HR_LOG("[MQTT] Mengirim %u data tertunda\n", _count);

  uint16_t start = (uint16_t)((_head + HR_BUFFER_SLOTS - _count) % HR_BUFFER_SLOTS);
  uint32_t now = millis();
  uint16_t sent = 0;

  for (uint16_t i = 0; i < _count; i++) {
    uint16_t idx = (uint16_t)((start + i) % HR_BUFFER_SLOTS);
    uint32_t age = now - _buf[idx].stampMs;

    if (!sendTelemetry(_buf[idx].r, age)) break;   // putus lagi, berhenti
    sent++;
    s_mqtt.loop();
  }

  _count = (uint16_t)(_count - sent);
  if (_count == 0) _overflowed = false;
}

size_t HrMqtt::buildJson(char* out, size_t cap, const HrReading& r,
                         uint32_t ageMs) {
  char tempStr[12] = "null";
  char humStr[12]  = "null";
  char gasPctStr[12] = "null";
  char gasRawStr[12] = "null";

  if (r.tempValid) snprintf(tempStr, sizeof(tempStr), "%.1f", r.tempC);
  if (r.humValid)  snprintf(humStr,  sizeof(humStr),  "%.1f", r.humPct);
  if (r.gasValid) {
    snprintf(gasPctStr, sizeof(gasPctStr), "%.1f", r.gasPct);
    snprintf(gasRawStr, sizeof(gasRawStr), "%d",   r.gasRaw);
  }

  return (size_t)snprintf(
    out, cap,
    "{"
      "\"device_id\":\"%s\","
      "\"location\":\"%s\","
      "\"fw\":\"%s\","
      "\"uptime_s\":%lu,"
      "\"age_ms\":%lu,"
      "\"temp_c\":%s,"
      "\"hum_pct\":%s,"
      "\"gas_raw\":%s,"
      "\"gas_pct\":%s,"
      "\"level\":{\"temp\":\"%s\",\"hum\":\"%s\",\"gas\":\"%s\"},"
      "\"status\":\"%s\","
      "\"rssi\":%d"
    "}",
    HR_DEVICE_ID, HR_LOCATION, HR_FW_VERSION,
    (unsigned long)(millis() / 1000UL),
    (unsigned long)ageMs,
    tempStr, humStr, gasRawStr, gasPctStr,
    hrLevelName(r.tempLevel), hrLevelName(r.humLevel), hrLevelName(r.gasLevel),
    hrLevelName(r.overall),
    rssi()
  );
}

bool HrMqtt::sendTelemetry(const HrReading& r, uint32_t ageMs) {
  char payload[448];
  size_t n = buildJson(payload, sizeof(payload), r, ageMs);
  if (n == 0 || n >= sizeof(payload)) {
    HR_LOG("[MQTT] JSON terlalu panjang, dibatalkan\n");
    return false;
  }
  return s_mqtt.publish(HR_TOPIC_TELE, payload);
}

void HrMqtt::publishEvent(const char* what, HrLevel from, HrLevel to) {
  if (!s_mqtt.connected()) return;

  char payload[224];
  snprintf(payload, sizeof(payload),
           "{\"device_id\":\"%s\",\"what\":\"%s\",\"from\":\"%s\","
           "\"to\":\"%s\",\"uptime_s\":%lu}",
           HR_DEVICE_ID, what, hrLevelName(from), hrLevelName(to),
           (unsigned long)(millis() / 1000UL));

  s_mqtt.publish(HR_TOPIC_EVENT, payload);
}
