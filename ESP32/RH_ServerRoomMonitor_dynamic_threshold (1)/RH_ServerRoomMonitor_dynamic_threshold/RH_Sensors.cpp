#include "RH_Sensors.h"
#include <DHT.h>
#include <Preferences.h>

static DHT s_dht(HR_PIN_DHT22, DHT22);
static Preferences s_prefs;

const char* hrLevelName(HrLevel lv) {
  switch (lv) {
    case HR_LEVEL_OK:    return "ok";
    case HR_LEVEL_WARN:  return "warning";
    case HR_LEVEL_ALARM: return "alarm";
  }
  return "unknown";
}

void HrSensors::begin() {
  s_dht.begin();
  analogReadResolution(12);
  analogSetPinAttenuation(HR_PIN_MQ2_AO, ADC_11db);

  loadThresholds();

  _bootMs = millis();
  _r.tempValid = false;
  _r.humValid  = false;
  _r.gasValid  = false;
  _r.overall   = HR_LEVEL_OK;

  HR_LOG("[SENS] DHT22 pin %d, MQ2 pin %d siap\n", HR_PIN_DHT22, HR_PIN_MQ2_AO);
  HR_LOG("[THR ] T %.1f..%.1f | H %.1f..%.1f | G %.1f..%.1f %%\n",
         _thresholds.temperatureMin, _thresholds.temperatureMax,
         _thresholds.humidityMin, _thresholds.humidityMax,
         _thresholds.gasMinPct, _thresholds.gasMaxPct);
}

void HrSensors::loadThresholds() {
  s_prefs.begin("threshold", true);

  _thresholds.temperatureMin = s_prefs.getFloat("tmin", HR_DEFAULT_TEMP_MIN);
  _thresholds.temperatureMax = s_prefs.getFloat("tmax", HR_DEFAULT_TEMP_MAX);
  _thresholds.humidityMin    = s_prefs.getFloat("hmin", HR_DEFAULT_HUM_MIN);
  _thresholds.humidityMax    = s_prefs.getFloat("hmax", HR_DEFAULT_HUM_MAX);
  _thresholds.gasMinPct      = s_prefs.getFloat("gmin", HR_DEFAULT_GAS_MIN_PCT);
  _thresholds.gasMaxPct      = s_prefs.getFloat("gmax", HR_DEFAULT_GAS_MAX_PCT);

  s_prefs.end();
}

void HrSensors::setThresholds(const HrThresholds& thresholds, bool persist) {
  if (thresholds.temperatureMin >= thresholds.temperatureMax ||
      thresholds.humidityMin >= thresholds.humidityMax ||
      thresholds.gasMinPct >= thresholds.gasMaxPct) {
    HR_LOG("[THR ] Konfigurasi ditolak: MIN harus < MAX\n");
    return;
  }

  _thresholds = thresholds;

  // Paksa evaluasi ulang segera setelah threshold berubah.
  _tempCand = HR_LEVEL_OK;
  _humCand  = HR_LEVEL_OK;
  _gasCand  = HR_LEVEL_OK;
  _tempCandSince = _humCandSince = _gasCandSince = millis();

  if (persist) {
    s_prefs.begin("threshold", false);
    s_prefs.putFloat("tmin", _thresholds.temperatureMin);
    s_prefs.putFloat("tmax", _thresholds.temperatureMax);
    s_prefs.putFloat("hmin", _thresholds.humidityMin);
    s_prefs.putFloat("hmax", _thresholds.humidityMax);
    s_prefs.putFloat("gmin", _thresholds.gasMinPct);
    s_prefs.putFloat("gmax", _thresholds.gasMaxPct);
    s_prefs.end();
  }

  evaluateLevels();

  HR_LOG("[THR ] Applied: T %.1f..%.1f | H %.1f..%.1f | G %.1f..%.1f %%\n",
         _thresholds.temperatureMin, _thresholds.temperatureMax,
         _thresholds.humidityMin, _thresholds.humidityMax,
         _thresholds.gasMinPct, _thresholds.gasMaxPct);
}

bool HrSensors::warmingUp() const {
  return (millis() - _bootMs) < HR_MQ2_WARMUP_MS;
}

uint32_t HrSensors::warmupRemainSec() const {
  uint32_t el = millis() - _bootMs;
  if (el >= HR_MQ2_WARMUP_MS) return 0;
  return (HR_MQ2_WARMUP_MS - el + 999UL) / 1000UL;
}

bool HrSensors::update() {
  bool fresh = false;
  uint32_t now = millis();

  if (now - _lastMq2 >= HR_MQ2_INTERVAL_MS) {
    _lastMq2 = now;
    readMq2();
    fresh = true;
  }

  if (now - _lastDht >= HR_DHT_INTERVAL_MS) {
    _lastDht = now;
    readDht();
    fresh = true;
  }

  if (fresh) {
    evaluateLevels();
    _r.stampMs = now;
  }
  return fresh;
}

void HrSensors::readDht() {
  float t = s_dht.readTemperature();
  float h = s_dht.readHumidity();

  bool tOk = !isnan(t) && t > -40.0f && t < 85.0f;
  bool hOk = !isnan(h) && h >= 0.0f && h <= 100.0f;

  if (tOk && hOk) {
    _r.tempC = t;
    _r.humPct = h;
    _r.tempValid = true;
    _r.humValid = true;
    _dhtFailCount = 0;
  } else {
    if (_dhtFailCount < 250) _dhtFailCount++;
    if (_dhtFailCount >= 5) {
      _r.tempValid = false;
      _r.humValid = false;
      HR_LOG("[SENS] DHT22 gagal %u kali berturut-turut\n", _dhtFailCount);
    }
  }
}

void HrSensors::readMq2() {
  int raw = analogRead(HR_PIN_MQ2_AO);
  if (raw < 0) raw = 0;
  if (raw > 4095) raw = 4095;

  _mq2Buf[_mq2Idx] = (uint16_t)raw;
  _mq2Idx = (uint8_t)((_mq2Idx + 1) % MQ2_AVG_N);
  if (_mq2Idx == 0) _mq2Filled = true;

  uint8_t n = _mq2Filled ? MQ2_AVG_N : _mq2Idx;
  if (n == 0) n = 1;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += _mq2Buf[i];

  _r.gasRaw = (int)(sum / n);
  _r.gasPct = (_r.gasRaw * 100.0f) / 4095.0f;
  _r.gasValid = !warmingUp();
}

HrLevel HrSensors::tempLevelOf(float c) const {
  return (c >= _thresholds.temperatureMin && c <= _thresholds.temperatureMax)
      ? HR_LEVEL_OK : HR_LEVEL_ALARM;
}

HrLevel HrSensors::humLevelOf(float h) const {
  return (h >= _thresholds.humidityMin && h <= _thresholds.humidityMax)
      ? HR_LEVEL_OK : HR_LEVEL_ALARM;
}

HrLevel HrSensors::gasLevelOf(float pct) const {
  return (pct >= _thresholds.gasMinPct && pct <= _thresholds.gasMaxPct)
      ? HR_LEVEL_OK : HR_LEVEL_ALARM;
}

HrLevel HrSensors::holdLevel(HrLevel raw, HrLevel& cand, uint32_t& since,
                             HrLevel current) {
  uint32_t now = millis();

  if (raw > current) {
    cand = raw;
    since = now;
    return raw;
  }
  if (raw == current) {
    cand = raw;
    since = now;
    return current;
  }
  if (raw != cand) {
    cand = raw;
    since = now;
    return current;
  }
  if (now - since >= HR_LEVEL_DEBOUNCE_MS) return raw;
  return current;
}

void HrSensors::evaluateLevels() {
  HrLevel t = _r.tempValid ? tempLevelOf(_r.tempC) : HR_LEVEL_OK;
  HrLevel h = _r.humValid ? humLevelOf(_r.humPct) : HR_LEVEL_OK;
  HrLevel g = _r.gasValid ? gasLevelOf(_r.gasPct) : HR_LEVEL_OK;

  _r.tempLevel = holdLevel(t, _tempCand, _tempCandSince, _r.tempLevel);
  _r.humLevel = holdLevel(h, _humCand, _humCandSince, _r.humLevel);
  _r.gasLevel = holdLevel(g, _gasCand, _gasCandSince, _r.gasLevel);

  HrLevel worst = _r.tempLevel;
  if (_r.humLevel > worst) worst = _r.humLevel;
  if (_r.gasLevel > worst) worst = _r.gasLevel;
  _r.overall = worst;
}
