#include "RH_Alarm.h"

void HrAlarm::begin() {
#if HR_BUZZER_IS_ACTIVE
  pinMode(HR_PIN_BUZZER, OUTPUT);
  digitalWrite(HR_PIN_BUZZER, LOW);
#else
  ledcSetup(HR_BUZZER_LEDC_CH, HR_BUZZER_TONE_HZ, 8);
  ledcAttachPin(HR_PIN_BUZZER, HR_BUZZER_LEDC_CH);
  ledcWrite(HR_BUZZER_LEDC_CH, 0);
#endif
  _on = false;
  HR_LOG("[ALRM] Buzzer pin %d siap\n", HR_PIN_BUZZER);
}

void HrAlarm::output(bool on) {
  if (on == _on) return;
  _on = on;
#if HR_BUZZER_IS_ACTIVE
  digitalWrite(HR_PIN_BUZZER, on ? HIGH : LOW);
#else
  ledcWriteTone(HR_BUZZER_LEDC_CH, on ? HR_BUZZER_TONE_HZ : 0);
#endif
}

void HrAlarm::setLevel(HrLevel lv) {
  if (lv == _level) return;

  // Naik level = bahaya bertambah, bisu otomatis dibuka supaya
  // tidak ada kondisi baru yang diam-diam terlewat.
  if (lv > _level) _muted = false;

  // Kembali normal = bisu direset juga.
  if (lv == HR_LEVEL_OK) _muted = false;

  _level   = lv;
  _phaseAt = millis();
}

void HrAlarm::mute()   { _muted = true; output(false); }
void HrAlarm::unmute() { _muted = false; }

void HrAlarm::beepOnce(uint16_t ms) {
  _oneShotUntil = millis() + ms;
}

void HrAlarm::update() {
  uint32_t now = millis();

  // Bip umpan balik tombol menang sesaat atas pola apa pun.
  if (_oneShotUntil != 0) {
    if ((int32_t)(now - _oneShotUntil) < 0) {
      output(true);
      return;
    }
    _oneShotUntil = 0;
    output(false);
  }

  if (_muted || _level == HR_LEVEL_OK) {
    output(false);
    return;
  }

  uint32_t onMs, periodMs;
  if (_level == HR_LEVEL_ALARM) {
    onMs = 250; periodMs = 500;      // bip cepat
  } else {
    onMs = 120; periodMs = 3000;     // bip pendek tiap 3 detik
  }

  uint32_t phase = (now - _phaseAt) % periodMs;
  output(phase < onMs);
}
