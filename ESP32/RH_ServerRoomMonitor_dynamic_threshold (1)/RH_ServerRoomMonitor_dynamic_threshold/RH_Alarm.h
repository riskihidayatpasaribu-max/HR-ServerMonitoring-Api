/*
 * RH_Alarm.h
 * Pengatur buzzer, tanpa delay() sama sekali.
 *
 * Pola bunyi:
 *   OK    : diam
 *   WARN  : bip pendek tiap 3 detik
 *   ALARM : bip cepat terus-menerus
 */
#ifndef HR_ALARM_H
#define HR_ALARM_H

#include "RH_Config.h"

class HrAlarm {
public:
  void begin();
  void setLevel(HrLevel lv);
  void update();

  void mute();                  // bisukan sampai kondisi normal lagi
  void unmute();
  bool isMuted() const { return _muted; }
  bool isSounding() const { return _on; }

  void beepOnce(uint16_t ms = 60);   // umpan balik saat tombol ditekan

private:
  HrLevel  _level = HR_LEVEL_OK;
  bool     _muted = false;
  bool     _on    = false;
  uint32_t _phaseAt = 0;
  uint32_t _oneShotUntil = 0;

  void output(bool on);
};

#endif // HR_ALARM_H
