#ifndef HR_THRESHOLDS_H
#define HR_THRESHOLDS_H

#include <Arduino.h>

struct HrThresholds {
  float temperatureMin;
  float temperatureMax;
  float humidityMin;
  float humidityMax;
  float gasMinPct;
  float gasMaxPct;
};

#endif // HR_THRESHOLDS_H
