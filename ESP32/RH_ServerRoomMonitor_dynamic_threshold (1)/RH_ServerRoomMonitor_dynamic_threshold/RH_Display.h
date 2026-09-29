/*
 * RH_Display.h
 * Dashboard di TFT 3.5" (480x320 landscape) via driver RH_Tft (SPI langsung).
 *
 * Prinsip: gambar rangka SEKALI saja, lalu hanya angka yang berubah
 * yang ditimpa ulang. Kalau seluruh layar digambar ulang tiap detik,
 * hasilnya berkedip dan lambat.
 */
#ifndef HR_DISPLAY_H
#define HR_DISPLAY_H

#include "RH_Config.h"

struct HrNetInfo {
  bool     wifiOk;
  bool     mqttOk;
  int      rssi;
  uint16_t pending;
  bool     muted;
  uint32_t warmupSec;
  String   ip;
};

class HrDisplay {
public:
  void begin();
  void splash(const char* line1, const char* line2);

  void update(const HrReading& r, const HrNetInfo& n);
  void forceRedraw() { _needStatic = true; }

  void setPage(uint8_t p);
  void nextPage();
  uint8_t page() const { return _page; }

  static const uint8_t PAGE_COUNT = 2;   // 0 = dashboard, 1 = info jaringan

private:
  uint8_t _page = 0;
  bool    _needStatic = true;

  // Nilai terakhir yang tergambar, supaya tahu mana yang perlu ditimpa.
  float    _lastTemp = -999.0f;
  float    _lastHum  = -999.0f;
  int      _lastGas  = -1;
  HrLevel  _lastTempLv = (HrLevel)255;
  HrLevel  _lastHumLv  = (HrLevel)255;
  HrLevel  _lastGasLv  = (HrLevel)255;
  HrLevel  _lastOverall = (HrLevel)255;
  bool     _lastWifi = false;
  bool     _lastMqtt = false;
  bool     _lastMuted = false;
  uint16_t _lastPending = 0xFFFF;
  uint32_t _lastUptimeS = 0xFFFFFFFF;
  uint32_t _lastWarm = 0xFFFFFFFF;
  bool     _hdrDrawn = false;     // indikator WIFI/MQTT di header sudah tergambar?

  // Cache baris halaman jaringan: hanya baris yang berubah yang digambar ulang.
  static const uint8_t NET_LINES = 8;
  char     _netCache[NET_LINES][48];
  uint16_t _netCol[NET_LINES];

  void drawDashboardStatic();
  void drawDashboard(const HrReading& r, const HrNetInfo& n);
  void drawHeaderStatic(const char* title);
  void drawBrandFooter();
  void drawNetPageStatic();
  void drawNetPage(const HrReading& r, const HrNetInfo& n);

  void drawCardValue(int16_t x, int16_t y, int16_t w, const char* value,
                     const char* unit, HrLevel lv, bool valid = true);
  void drawHeader(const HrNetInfo& n);
  void drawStatusBar(HrLevel lv, bool muted, uint32_t warmupSec);
  void drawFooter(const HrNetInfo& n);

  static uint16_t levelColor(HrLevel lv);
  void resetCache();
};

#endif // HR_DISPLAY_H
