#include "RH_Tft.h"
#include <SPI.h>

// Buffer kerja untuk mengirim banyak piksel sekaligus.
static uint8_t s_chunk[1024];               // 512 piksel

RhTft::RhTft() : Adafruit_GFX(HR_TFT_W, HR_TFT_H) {}

// ------------------------------------------------------------
//  Level rendah - sama dengan tes full screen
// ------------------------------------------------------------
void RhTft::writeCommand(uint8_t cmd) {
  digitalWrite(HR_PIN_TFT_DC, LOW);
  digitalWrite(HR_PIN_TFT_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(HR_PIN_TFT_CS, HIGH);
}

void RhTft::writeData(uint8_t data) {
  digitalWrite(HR_PIN_TFT_DC, HIGH);
  digitalWrite(HR_PIN_TFT_CS, LOW);
  SPI.transfer(data);
  digitalWrite(HR_PIN_TFT_CS, HIGH);
}

void RhTft::hardwareReset() {
  digitalWrite(HR_PIN_TFT_RST, HIGH); delay(50);
  digitalWrite(HR_PIN_TFT_RST, LOW);  delay(50);
  digitalWrite(HR_PIN_TFT_RST, HIGH); delay(150);
}

void RhTft::setWindow(int x0, int y0, int x1, int y1) {
  writeCommand(0x2A);
  writeData(x0 >> 8); writeData(x0 & 0xFF);
  writeData(x1 >> 8); writeData(x1 & 0xFF);
  writeCommand(0x2B);
  writeData(y0 >> 8); writeData(y0 & 0xFF);
  writeData(y1 >> 8); writeData(y1 & 0xFF);
  writeCommand(0x2C);
}

void RhTft::begin() {
  pinMode(HR_PIN_TFT_CS, OUTPUT);
  pinMode(HR_PIN_TFT_DC, OUTPUT);
  pinMode(HR_PIN_TFT_RST, OUTPUT);
  digitalWrite(HR_PIN_TFT_CS, HIGH);

  SPI.begin(HR_PIN_TFT_SCLK, -1, HR_PIN_TFT_MOSI, HR_PIN_TFT_CS);
  SPI.beginTransaction(SPISettings(HR_TFT_SPI_HZ, MSBFIRST, SPI_MODE0));

  hardwareReset();
  writeCommand(0x01); delay(150);          // software reset
  writeCommand(0x11); delay(150);          // sleep out
  writeCommand(0x3A); writeData(0x55);     // 16 bit/piksel
  writeCommand(0x36); writeData(HR_TFT_MADCTL);
  writeCommand(0x29); delay(50);           // display on

  fillScreen(0x0000);
}

// ------------------------------------------------------------
//  Kirim 'count' piksel berwarna sama ke jendela yang sudah diset
// ------------------------------------------------------------
void RhTft::streamColor(uint16_t color, uint32_t count) {
  const uint8_t hi = color >> 8, lo = color & 0xFF;
  const uint32_t chunkPx = sizeof(s_chunk) / 2;

  uint32_t first = (count < chunkPx) ? count : chunkPx;
  for (uint32_t i = 0; i < first; i++) {
    s_chunk[2 * i]     = hi;
    s_chunk[2 * i + 1] = lo;
  }

  digitalWrite(HR_PIN_TFT_DC, HIGH);
  digitalWrite(HR_PIN_TFT_CS, LOW);
  while (count) {
    uint32_t n = (count < chunkPx) ? count : chunkPx;
    SPI.writeBytes(s_chunk, n * 2);
    count -= n;
  }
  digitalWrite(HR_PIN_TFT_CS, HIGH);
}

// ------------------------------------------------------------
//  Override Adafruit_GFX
// ------------------------------------------------------------
void RhTft::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > HR_TFT_W) w = HR_TFT_W - x;
  if (y + h > HR_TFT_H) h = HR_TFT_H - y;
  if (w <= 0 || h <= 0) return;

  setWindow(x, y, x + w - 1, y + h - 1);
  streamColor(color, (uint32_t)w * (uint32_t)h);
}

void RhTft::fillScreen(uint16_t color) {
  fillRect(0, 0, HR_TFT_W, HR_TFT_H, color);
}

void RhTft::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || y < 0 || x >= HR_TFT_W || y >= HR_TFT_H) return;
  setWindow(x, y, x, y);
  streamColor(color, 1);
}

void RhTft::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  fillRect(x, y, w, 1, color);
}

void RhTft::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  fillRect(x, y, 1, h, color);
}

// ------------------------------------------------------------
//  Blok piksel dari buffer (dipakai untuk teks yang dirender off-screen)
// ------------------------------------------------------------
void RhTft::blit16(int16_t x, int16_t y, int16_t w, int16_t h,
                   const uint16_t* src, int16_t stride) {
  if (!src || w <= 0 || h <= 0) return;
  if (x < 0 || y < 0 || x + w > HR_TFT_W || y + h > HR_TFT_H) return;
  if ((uint32_t)w * 2 > sizeof(s_chunk)) return;   // 1 baris harus muat di buffer

  setWindow(x, y, x + w - 1, y + h - 1);

  digitalWrite(HR_PIN_TFT_DC, HIGH);
  digitalWrite(HR_PIN_TFT_CS, LOW);
  for (int16_t row = 0; row < h; row++) {
    const uint16_t* p = src + (uint32_t)row * stride;
    for (int16_t i = 0; i < w; i++) {         // RGB565 -> byte big-endian
      s_chunk[2 * i]     = p[i] >> 8;
      s_chunk[2 * i + 1] = p[i] & 0xFF;
    }
    SPI.writeBytes(s_chunk, (uint32_t)w * 2);
  }
  digitalWrite(HR_PIN_TFT_CS, HIGH);
}
