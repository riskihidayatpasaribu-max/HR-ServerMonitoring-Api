/*
 * RH_Sensors.h
 * Baca DHT22 + MQ2 dan evaluasi berdasarkan threshold yang dikirim backend.
 */
#ifndef HR_SENSORS_H
#define HR_SENSORS_H

#include "RH_Config.h"

class HrSensors {
public:
  void begin();
  bool update();
  const HrReading& reading() const { return _r; }

  bool warmingUp() const;
  uint32_t warmupRemainSec() const;

  // Threshold runtime yang dapat diubah backend melalui MQTT.
  void setThresholds(const HrThresholds& thresholds, bool persist = true);
  const HrThresholds& thresholds() const { return _thresholds; }

private:
  HrReading _r = {};
  HrThresholds _thresholds = {
    HR_DEFAULT_TEMP_MIN,
    HR_DEFAULT_TEMP_MAX,
    HR_DEFAULT_HUM_MIN,
    HR_DEFAULT_HUM_MAX,
    HR_DEFAULT_GAS_MIN_PCT,
    HR_DEFAULT_GAS_MAX_PCT
  };

  uint32_t _lastDht = 0;
  uint32_t _lastMq2 = 0;
  uint32_t _bootMs  = 0;

  static const uint8_t MQ2_AVG_N = 10;
  uint16_t _mq2Buf[MQ2_AVG_N] = {0};
  uint8_t  _mq2Idx = 0;
  bool     _mq2Filled = false;
  uint8_t  _dhtFailCount = 0;

  HrLevel _tempCand = HR_LEVEL_OK; uint32_t _tempCandSince = 0;
  HrLevel _humCand  = HR_LEVEL_OK; uint32_t _humCandSince  = 0;
  HrLevel _gasCand  = HR_LEVEL_OK; uint32_t _gasCandSince  = 0;

  void readDht();
  void readMq2();
  void evaluateLevels();
  void loadThresholds();

  static HrLevel holdLevel(HrLevel raw, HrLevel& cand, uint32_t& since,
                           HrLevel current);
  HrLevel tempLevelOf(float c) const;
  HrLevel humLevelOf(float h) const;
  HrLevel gasLevelOf(float pct) const;
};

#endif // HR_SENSORS_H
