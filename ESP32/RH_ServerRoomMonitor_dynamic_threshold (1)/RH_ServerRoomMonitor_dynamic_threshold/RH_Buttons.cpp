#include "RH_Buttons.h"

void HrButtons::begin() {
  // GPIO32 punya pull-up internal, jadi kita pakai.
  pinMode(HR_PIN_BTN1, INPUT_PULLUP);

  // GPIO34 & GPIO35 INPUT-ONLY dan TIDAK punya pull-up internal.
  // Kalau tidak dipasang resistor 10k ke 3.3V di luar, kakinya
  // "mengambang" dan ESP32 akan membaca tombol tertekan sendiri
  // karena noise listrik.
  pinMode(HR_PIN_BTN2, INPUT);
  pinMode(HR_PIN_BTN3, INPUT);

  HR_LOG("[BTN] Pin %d/%d/%d siap. INGAT: pin %d & %d butuh pull-up 10k eksternal.\n",
         HR_PIN_BTN1, HR_PIN_BTN2, HR_PIN_BTN3, HR_PIN_BTN2, HR_PIN_BTN3);
}

bool HrButtons::readRaw(uint8_t index) const {
  int v = digitalRead(_pin[index]);
#if HR_BTN_ACTIVE_LOW
  return (v == LOW);
#else
  return (v == HIGH);
#endif
}

void HrButtons::update() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < COUNT; i++) {
    bool raw = readRaw(i);

    if (raw != _lastRaw[i]) {
      _lastRaw[i]    = raw;
      _lastChange[i] = now;
    }

    // Terima perubahan hanya kalau sudah tenang selama DEBOUNCE_MS.
    if ((now - _lastChange[i]) >= DEBOUNCE_MS && raw != _stable[i]) {
      _stable[i] = raw;

      if (raw) {
        _downAt[i]   = now;
        _longSent[i] = false;
      } else {
        if (!_longSent[i] && (now - _downAt[i]) < LONG_MS) {
          _event[i] = HR_BTN_SHORT;
        }
      }
    }

    // Tahan lama: kejadian dikirim saat masih ditekan.
    if (_stable[i] && !_longSent[i] && (now - _downAt[i]) >= LONG_MS) {
      _longSent[i] = true;
      _event[i]    = HR_BTN_LONG;
    }
  }
}

HrBtnEvent HrButtons::take(uint8_t index) {
  if (index >= COUNT) return HR_BTN_NONE;
  HrBtnEvent e = _event[index];
  _event[index] = HR_BTN_NONE;
  return e;
}

bool HrButtons::isDown(uint8_t index) const {
  if (index >= COUNT) return false;
  return _stable[index];
}
