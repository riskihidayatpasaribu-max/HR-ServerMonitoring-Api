/*
 * RH_Tft.h
 * Driver layar 480x320 via SPI langsung, diturunkan dari AB_test_fullscreen.ino.
 *
 * Kenapa bukan TFT_eSPI lagi:
 *   Urutan init di tes full screen (COLMOD 0x55, MADCTL 0x28, CS=5, DC=2,
 *   RST=4) sudah terbukti benar di panelmu. Driver ini memakai persis
 *   urutan itu, lalu menambahkan Adafruit_GFX supaya teks & bentuk bisa
 *   digambar. Tidak perlu lagi menimpa User_Setup.h di folder library.
 *
 * Library yang dibutuhkan: "Adafruit GFX Library" (Library Manager).
 */
#ifndef RH_TFT_H
#define RH_TFT_H

#include <Adafruit_GFX.h>
#include "RH_Config.h"

class RhTft : public Adafruit_GFX {
public:
  RhTft();

  void begin();

  // ---- Override Adafruit_GFX supaya bentuk digambar cepat ----
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;

  // Tulis blok piksel RGB565 (urutan byte host) dalam SATU jendela.
  // 'stride' = lebar buffer sumber dalam piksel.
  void blit16(int16_t x, int16_t y, int16_t w, int16_t h,
              const uint16_t* src, int16_t stride);

private:
  void writeCommand(uint8_t cmd);
  void writeData(uint8_t data);
  void hardwareReset();
  void setWindow(int x0, int y0, int x1, int y1);
  void streamColor(uint16_t color, uint32_t count);
};

#endif // RH_TFT_H
