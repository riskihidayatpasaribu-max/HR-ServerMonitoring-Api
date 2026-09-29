/*
 * RH_Buttons.h
 * Lapisan pembacaan 3 tombol panel: debounce + deteksi tekan singkat / tahan.
 *
 * Fungsi tiap tombol BELUM ditentukan. Modul ini hanya menyediakan
 * "kejadian"-nya. Cara pakainya lihat bagian hrHandleButtons()
 * di RH_ServerRoomMonitor.ino - di situ tinggal diisi nanti.
 */
#ifndef HR_BUTTONS_H
#define HR_BUTTONS_H

#include "RH_Config.h"

enum HrBtnEvent {
  HR_BTN_NONE = 0,
  HR_BTN_SHORT,     // tekan lalu lepas (< 800 ms)
  HR_BTN_LONG       // ditahan >= 800 ms
};

class HrButtons {
public:
  void begin();
  void update();

  // index: 0 = GPIO32, 1 = GPIO35, 2 = GPIO34
  // Kejadian hanya bisa diambil satu kali (auto-clear setelah dibaca).
  HrBtnEvent take(uint8_t index);

  bool isDown(uint8_t index) const;

  static const uint8_t COUNT = 3;

private:
  static const uint32_t DEBOUNCE_MS = 35;
  static const uint32_t LONG_MS     = 800;

  uint8_t    _pin[COUNT]      = { HR_PIN_BTN1, HR_PIN_BTN2, HR_PIN_BTN3 };
  bool       _stable[COUNT]   = { false, false, false };  // true = tertekan
  bool       _lastRaw[COUNT]  = { false, false, false };
  uint32_t   _lastChange[COUNT] = { 0, 0, 0 };
  uint32_t   _downAt[COUNT]   = { 0, 0, 0 };
  bool       _longSent[COUNT] = { false, false, false };
  HrBtnEvent _event[COUNT]    = { HR_BTN_NONE, HR_BTN_NONE, HR_BTN_NONE };

  bool readRaw(uint8_t index) const;
};

#endif // HR_BUTTONS_H
