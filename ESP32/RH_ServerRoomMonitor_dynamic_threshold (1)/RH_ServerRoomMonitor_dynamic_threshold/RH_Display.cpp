#include "RH_Display.h"
#include "RH_Tft.h"
#include "RH_Logos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

static RhTft tft;

// ---- Kanvas teks (off-screen) ----
// Teks dirender dulu ke kanvas kecil, lalu dikirim ke layar dalam SATU
// blok. Jauh lebih cepat & tanpa kedip dibanding menggambar piksel satu-satu.
#define CV_W  480
#define CV_H  48
static GFXcanvas16* cv = nullptr;

// ---- Font (semua BOLD: goresan tebal jauh lebih terbaca di layar kecil) ----
#define F_SMALL   (&FreeSansBold9pt7b)
#define F_MED     (&FreeSansBold12pt7b)
#define F_LARGE   (&FreeSansBold18pt7b)
#define F_NUM     (&FreeSansBold24pt7b)
#define F_MONO    (&FreeMonoBold12pt7b)

// ---- Palet warna ----
// Rasio kontras (WCAG) sudah dicek; target teks >= 4.5:1.
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define C_BLACK     RGB(0, 0, 0)
#define C_WHITE     RGB(255, 255, 255)
#define C_BG        RGB(12, 14, 18)      // HARUS sama dgn latar logo AE di tools/RH_png2rgb565.py
#define C_CARD      RGB(30, 36, 46)
#define C_LINE      RGB(80, 92, 110)
#define C_TEXT      C_WHITE
#define C_SUB       RGB(215, 220, 228)   // teks sekunder (11:1 di kartu)

// Warna merek, diambil dari logo masing-masing.
#define C_RSUP_BLUE RGB(0, 0, 240)       // biru logo RSUP  (9.3:1 di putih)
#define C_AE_CYAN   RGB(48, 208, 228)    // cyan logo AE    (10.4:1 di latar gelap)

// Angka & garis indikator di atas kartu gelap.
#define C_NODATA    RGB(150, 158, 170)   // tidak ada data (6:1 di kartu)
#define LV_OK       RGB(60, 255, 110)
#define LV_WARN     RGB(255, 176, 0)
#define LV_ALARM    RGB(255, 90, 90)

// Latar bar status (teks putih, kecuali WARN: teks hitam).
#define ST_OK       RGB(0, 110, 50)
#define ST_WARN     RGB(255, 176, 0)
#define ST_ALARM    RGB(200, 20, 20)
#define ST_WARMUP   RGB(0, 90, 170)

// Header putih (agar biru logo RSUP kontras).
#define C_HDR_LABEL RGB(25, 35, 60)
#define C_DOT_OK    RGB(0, 160, 60)
#define C_DOT_BAD   RGB(215, 30, 30)

// ---- Tata letak 480 x 320 ----
#define LAY_MARGIN     8
#define LAY_HEAD_H     48            // area putih
#define LAY_HEAD_BAR   3             // garis biru di bawah header
#define LAY_CARD_Y     58
#define LAY_CARD_H     138
#define LAY_CARD_W     149
#define LAY_CARD_X0    8
#define LAY_CARD_X1    (LAY_CARD_X0 + LAY_CARD_W + 8)
#define LAY_CARD_X2    (LAY_CARD_X1 + LAY_CARD_W + 8)
#define LAY_STAT_Y     204
#define LAY_STAT_H     52
#define LAY_FOOT_LINE  262
#define LAY_LOGO_AE_X  (480 - LAY_MARGIN - RH_LOGO_AE_W)
#define LAY_LOGO_AE_Y  273

enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };

// Gambar teks di dalam kotak (x,y,w,h) dengan latar 'bg'.
// Vertikal selalu rata tengah berdasarkan tinggi huruf kapital, jadi
// baris dengan/tanpa huruf berekor (g, y, p) tetap sejajar.
static void drawText(int16_t x, int16_t y, int16_t w, int16_t h,
                     const char* s, const GFXfont* font,
                     uint16_t fg, uint16_t bg, uint8_t align) {
  if (!cv || !cv->getBuffer()) return;
  if (w > CV_W) w = CV_W;
  if (h > CV_H) h = CV_H;

  cv->fillRect(0, 0, w, h, bg);
  cv->setFont(font);
  cv->setTextSize(1);
  cv->setTextWrap(false);
  cv->setTextColor(fg);

  int16_t  x1, y1, sx, sy, gx, gy;
  uint16_t tw, th, sw, sh, gw, gh;
  cv->getTextBounds("H",     0, 0, &x1, &y1, &tw, &th);   // tinggi kapital
  cv->getTextBounds("gjpqy", 0, 0, &gx, &gy, &gw, &gh);   // huruf berekor
  cv->getTextBounds(s,       0, 0, &sx, &sy, &sw, &sh);   // lebar teks

  const int16_t pad = 4;
  int16_t cx;
  switch (align) {
    case AL_CENTER: cx = (w - (int16_t)sw) / 2 - sx;       break;
    case AL_RIGHT:  cx = w - pad - (int16_t)sw - sx;       break;
    default:        cx = pad;                              break;
  }

  // Baseline: rata tengah berdasarkan huruf kapital. Kalau ekor huruf
  // (g, y, p ...) akan keluar dari kotak, baseline dinaikkan secukupnya.
  int16_t desc = gy + (int16_t)gh;                 // ekor di bawah baseline
  if (desc < 0) desc = 0;
  int16_t cy = (h - (int16_t)th) / 2 - y1;
  if (cy + desc > h) cy = h - desc;
  if (cy < -y1) cy = -y1;                          // jangan sampai kapital terpotong atas

  cv->setCursor(cx, cy);
  cv->print(s);
  tft.blit16(x, y, w, h, cv->getBuffer(), CV_W);
}

uint16_t HrDisplay::levelColor(HrLevel lv) {
  switch (lv) {
    case HR_LEVEL_WARN:  return LV_WARN;
    case HR_LEVEL_ALARM: return LV_ALARM;
    default:             return LV_OK;
  }
}

void HrDisplay::resetCache() {
  _lastTemp = -999.0f;
  _lastHum  = -999.0f;
  _lastGas  = -1;
  _lastTempLv = _lastHumLv = _lastGasLv = _lastOverall = (HrLevel)255;
  _lastPending = 0xFFFF;
  _lastUptimeS = 0xFFFFFFFF;
  _lastWarm    = 0xFFFFFFFF;
  _hdrDrawn    = false;
  _lastWifi = _lastMqtt = _lastMuted = false;
  for (uint8_t i = 0; i < NET_LINES; i++) {
    _netCache[i][0] = '\0';
    _netCol[i] = 0;
  }
}

void HrDisplay::begin() {
  tft.begin();
  cv = new GFXcanvas16(CV_W, CV_H);
  if (!cv || !cv->getBuffer()) {
    HR_LOG("[TFT ] GAGAL alokasi kanvas teks (%d byte)\n", CV_W * CV_H * 2);
  }
  tft.fillScreen(C_BG);
  _needStatic = true;
  resetCache();
  HR_LOG("[TFT ] %dx%d siap (SPI langsung)\n", tft.width(), tft.height());
}

void HrDisplay::splash(const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  drawText(0, 108, 480, 44, line1, F_LARGE, C_TEXT, C_BG, AL_CENTER);
  drawText(0, 160, 480, 30, line2, F_SMALL, C_SUB,  C_BG, AL_CENTER);
  drawBrandFooter();
  _needStatic = true;
}

void HrDisplay::setPage(uint8_t p) {
  if (p >= PAGE_COUNT) p = 0;
  if (p == _page) return;
  _page = p;
  _needStatic = true;
  resetCache();
}

void HrDisplay::nextPage() {
  setPage((uint8_t)((_page + 1) % PAGE_COUNT));
}

void HrDisplay::update(const HrReading& r, const HrNetInfo& n) {
  if (_page == 0) {
    if (_needStatic) { drawDashboardStatic(); _needStatic = false; resetCache(); }
    drawDashboard(r, n);
  } else {
    if (_needStatic) { drawNetPageStatic(); _needStatic = false; resetCache(); }
    drawNetPage(r, n);
  }
}

// ------------------------------------------------------------
//  BAGIAN BERSAMA : header putih (logo RSUP) & footer (watermark AE)
// ------------------------------------------------------------
void HrDisplay::drawHeaderStatic(const char* title) {
  tft.fillRect(0, 0, 480, LAY_HEAD_H, C_WHITE);
  tft.fillRect(0, LAY_HEAD_H, 480, LAY_HEAD_BAR, C_RSUP_BLUE);

  // Logo PT kiri-atas
  tft.blit16(4, 3, RH_LOGO_RSUP_W, RH_LOGO_RSUP_H, RH_LOGO_RSUP, RH_LOGO_RSUP_W);

  // Teks warna biru logo
  drawText(54, 2,  240, 26, title, F_MED, C_RSUP_BLUE, C_WHITE, AL_LEFT);
  drawText(54, 28, 420, 19, "PT Riau Sakti United Plantations - Industry",
           F_SMALL, C_RSUP_BLUE, C_WHITE, AL_LEFT);
}

void HrDisplay::drawBrandFooter() {
  tft.fillRect(0, LAY_FOOT_LINE, 480, 320 - LAY_FOOT_LINE, C_BG);
  tft.drawFastHLine(LAY_MARGIN, LAY_FOOT_LINE, 480 - 2 * LAY_MARGIN, C_LINE);

  // Watermark tim kanan-bawah: tulisan cyan mengikuti warna logo
  tft.blit16(LAY_LOGO_AE_X, LAY_LOGO_AE_Y, RH_LOGO_AE_W, RH_LOGO_AE_H,
             RH_LOGO_AE, RH_LOGO_AE_W);
  drawText(LAY_LOGO_AE_X - 144, 271, 140, 20, "AUTOMATION", F_SMALL, C_AE_CYAN, C_BG, AL_RIGHT);
  drawText(LAY_LOGO_AE_X - 144, 290, 140, 20, "ENGINEER",   F_SMALL, C_AE_CYAN, C_BG, AL_RIGHT);
}

// ------------------------------------------------------------
//  HALAMAN 0 : DASHBOARD
// ------------------------------------------------------------
void HrDisplay::drawDashboardStatic() {
  tft.fillScreen(C_BG);
  drawHeaderStatic(HR_LOCATION);

  // Tiga kartu
  const int16_t xs[3]   = { LAY_CARD_X0, LAY_CARD_X1, LAY_CARD_X2 };
  const char*   caps[3] = { "SUHU", "KELEMBAPAN", "GAS / ASAP" };

  for (uint8_t i = 0; i < 3; i++) {
    tft.fillRoundRect(xs[i], LAY_CARD_Y, LAY_CARD_W, LAY_CARD_H, 8, C_CARD);
    tft.drawRoundRect(xs[i], LAY_CARD_Y, LAY_CARD_W, LAY_CARD_H, 8, C_LINE);
    drawText(xs[i] + 4, LAY_CARD_Y + 6, LAY_CARD_W - 8, 26,
             caps[i], F_SMALL, C_SUB, C_CARD, AL_CENTER);
  }

  // Bingkai status
  tft.drawRoundRect(LAY_CARD_X0, LAY_STAT_Y,
                    480 - 2 * LAY_CARD_X0, LAY_STAT_H, 8, C_LINE);

  drawBrandFooter();
}

void HrDisplay::drawCardValue(int16_t x, int16_t y, int16_t w,
                              const char* value, const char* unit,
                              HrLevel lv, bool valid) {
  // Sensor gagal -> abu-abu netral, supaya "--.-" tidak tampak seperti status OK.
  uint16_t col = valid ? levelColor(lv) : C_NODATA;

  drawText(x + 4, y + 34, w - 8, 48, value, F_NUM,   col,   C_CARD, AL_CENTER);
  drawText(x + 4, y + 86, w - 8, 24, unit,  F_SMALL, C_SUB, C_CARD, AL_CENTER);

  // Garis indikator tebal di bawah kartu.
  tft.fillRect(x + 12, y + LAY_CARD_H - 16, w - 24, 5, col);
}

void HrDisplay::drawHeader(const HrNetInfo& n) {
  // Dua titik indikator di header putih: WiFi dan MQTT.
  uint16_t cw = n.wifiOk ? C_DOT_OK : C_DOT_BAD;
  uint16_t cm = n.mqttOk ? C_DOT_OK : C_DOT_BAD;

  tft.fillRect(298, 2, 180, 26, C_WHITE);

  drawText(302, 4, 66, 22, "WIFI", F_SMALL, C_HDR_LABEL, C_WHITE, AL_RIGHT);
  tft.fillCircle(380, 15, 7, cw);

  drawText(388, 4, 66, 22, "MQTT", F_SMALL, C_HDR_LABEL, C_WHITE, AL_RIGHT);
  tft.fillCircle(468, 15, 7, cm);
}

void HrDisplay::drawStatusBar(HrLevel lv, bool muted, uint32_t warmupSec) {
  uint16_t fill, fg = C_WHITE;
  const char* txt;

  if (warmupSec > 0) {
    fill = ST_WARMUP;
    txt = "PEMANASAN SENSOR GAS";
  } else {
    switch (lv) {
      case HR_LEVEL_ALARM: fill = ST_ALARM; txt = "ALARM - PERIKSA SEGERA"; break;
      case HR_LEVEL_WARN:  fill = ST_WARN;  txt = "PERINGATAN"; fg = C_BLACK; break;
      default:             fill = ST_OK;    txt = "KONDISI NORMAL";         break;
    }
  }

  tft.fillRoundRect(LAY_CARD_X0 + 2, LAY_STAT_Y + 2,
                    480 - 2 * LAY_CARD_X0 - 4, LAY_STAT_H - 4, 8, fill);

  char sub[48];
  if (warmupSec > 0) {
    snprintf(sub, sizeof(sub), "tunggu %lu detik", (unsigned long)warmupSec);
  } else if (muted) {
    snprintf(sub, sizeof(sub), "buzzer dibisukan");
  } else {
    sub[0] = '\0';
  }

  const int16_t bx = LAY_CARD_X0 + 16;
  const int16_t bw = 480 - 2 * bx;
  if (sub[0]) {
    drawText(bx, LAY_STAT_Y + 4,  bw, 28, txt, F_MED,   fg, fill, AL_CENTER);
    drawText(bx, LAY_STAT_Y + 30, bw, 20, sub, F_SMALL, fg, fill, AL_CENTER);
  } else {
    drawText(bx, LAY_STAT_Y + 10, bw, 32, txt, F_MED,   fg, fill, AL_CENTER);
  }
}

void HrDisplay::drawFooter(const HrNetInfo& n) {
  char l1[48], l2[48];
  uint32_t up = millis() / 1000UL;
  snprintf(l1, sizeof(l1), "IP %s", n.ip.c_str());
  snprintf(l2, sizeof(l2), "%d dBm | buf %u | up %luh%lum",
           n.rssi, n.pending,
           (unsigned long)(up / 3600UL), (unsigned long)((up % 3600UL) / 60UL));

  drawText(LAY_MARGIN, 268, 262, 24, l1, F_SMALL, C_SUB, C_BG, AL_LEFT);
  drawText(LAY_MARGIN, 292, 262, 24, l2, F_SMALL, C_SUB, C_BG, AL_LEFT);
}

void HrDisplay::drawDashboard(const HrReading& r, const HrNetInfo& n) {
  char buf[16];

  // --- Suhu ---
  if (fabsf(r.tempC - _lastTemp) >= 0.1f || r.tempLevel != _lastTempLv
      || !r.tempValid) {
    if (r.tempValid) snprintf(buf, sizeof(buf), "%.1f", r.tempC);
    else             snprintf(buf, sizeof(buf), "--.-");
    drawCardValue(LAY_CARD_X0, LAY_CARD_Y, LAY_CARD_W, buf, "derajat C",
                  r.tempLevel, r.tempValid);
    _lastTemp   = r.tempC;
    _lastTempLv = r.tempLevel;
  }

  // --- Kelembapan ---
  if (fabsf(r.humPct - _lastHum) >= 0.1f || r.humLevel != _lastHumLv
      || !r.humValid) {
    if (r.humValid) snprintf(buf, sizeof(buf), "%.1f", r.humPct);
    else            snprintf(buf, sizeof(buf), "--.-");
    drawCardValue(LAY_CARD_X1, LAY_CARD_Y, LAY_CARD_W, buf, "persen RH",
                  r.humLevel, r.humValid);
    _lastHum   = r.humPct;
    _lastHumLv = r.humLevel;
  }

  // --- Gas ---
  if (abs(r.gasRaw - _lastGas) >= 10 || r.gasLevel != _lastGasLv
      || !r.gasValid) {
    if (r.gasValid) snprintf(buf, sizeof(buf), "%d", r.gasRaw);
    else            snprintf(buf, sizeof(buf), "----");
    drawCardValue(LAY_CARD_X2, LAY_CARD_Y, LAY_CARD_W, buf, "nilai ADC",
                  r.gasLevel, r.gasValid);
    _lastGas   = r.gasRaw;
    _lastGasLv = r.gasLevel;
  }

  if (!_hdrDrawn || n.wifiOk != _lastWifi || n.mqttOk != _lastMqtt) {
    drawHeader(n);
    _lastWifi = n.wifiOk;
    _lastMqtt = n.mqttOk;
    _hdrDrawn = true;
  }

  // Bar status hanya digambar ulang kalau isinya berubah
  // (termasuk hitung mundur pemanasan yang berubah tiap detik).
  if (r.overall != _lastOverall || n.muted != _lastMuted
      || n.warmupSec != _lastWarm) {
    drawStatusBar(r.overall, n.muted, n.warmupSec);
    _lastOverall = r.overall;
    _lastMuted   = n.muted;
    _lastWarm    = n.warmupSec;
  }

  uint32_t up = millis() / 1000UL;
  if (up != _lastUptimeS || n.pending != _lastPending) {
    drawFooter(n);
    _lastUptimeS = up;
    _lastPending = n.pending;
  }
}

// ------------------------------------------------------------
//  HALAMAN 1 : INFO JARINGAN / DIAGNOSA
// ------------------------------------------------------------
void HrDisplay::drawNetPageStatic() {
  tft.fillScreen(C_BG);
  drawHeaderStatic("Info Jaringan");
  drawBrandFooter();
}

void HrDisplay::drawNetPage(const HrReading& r, const HrNetInfo& n) {
  (void)r;

  // Indikator WiFi/MQTT di header juga tampil di halaman ini.
  if (!_hdrDrawn || n.wifiOk != _lastWifi || n.mqttOk != _lastMqtt) {
    drawHeader(n);
    _lastWifi = n.wifiOk;
    _lastMqtt = n.mqttOk;
    _hdrDrawn = true;
  }

  char line[NET_LINES][48];
  uint16_t col[NET_LINES];
  for (uint8_t i = 0; i < NET_LINES; i++) col[i] = C_TEXT;

  snprintf(line[0], sizeof(line[0]), "Device   : %s", HR_DEVICE_ID);
  snprintf(line[1], sizeof(line[1]), "Firmware : %s", HR_FW_VERSION);
  snprintf(line[2], sizeof(line[2]), "SSID     : %s", HR_WIFI_SSID);
  snprintf(line[3], sizeof(line[3]), "IP       : %s", n.ip.c_str());
  snprintf(line[4], sizeof(line[4]), "RSSI     : %d dBm", n.rssi);
  snprintf(line[5], sizeof(line[5]), "Broker   : %s:%d", HR_MQTT_HOST, (int)HR_MQTT_PORT);
  snprintf(line[6], sizeof(line[6]), "MQTT     : %s", n.mqttOk ? "tersambung" : "putus");
  col[6] = n.mqttOk ? LV_OK : LV_ALARM;
  snprintf(line[7], sizeof(line[7]), "Buffer   : %u data menunggu", n.pending);

  int16_t y = 56;
  const int16_t dy = 25;
  for (uint8_t i = 0; i < NET_LINES; i++, y += dy) {
    if (strcmp(line[i], _netCache[i]) == 0 && col[i] == _netCol[i]) continue;
    drawText(LAY_MARGIN, y, 464, dy, line[i], F_MONO, col[i], C_BG, AL_LEFT);
    strncpy(_netCache[i], line[i], sizeof(_netCache[i]));
    _netCache[i][sizeof(_netCache[i]) - 1] = '\0';
    _netCol[i] = col[i];
  }
}
