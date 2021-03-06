/**
 * @file scalemanager.cpp
 * @author Martin Verges <martin@veges.cc>
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include "scalemanager.h"
#include <soc/rtc.h>
extern "C" {
  #if ESP_ARDUINO_VERSION_MAJOR >= 2
    #include <esp32/clk.h>
  #else
    #include <esp_clk.h>
  #endif
}

// Store some history in the RTC RAM that survives deep sleep
#define MAX_RTC_HISTORY 100                        // store some data points to provide short term history
typedef struct {
    int currentLevel;
    int sensorValue;
    uint64_t timestamp;
} sensorReadings;
RTC_DATA_ATTR sensorReadings readingHistory[MAX_RTC_HISTORY];
RTC_DATA_ATTR int readingHistoryCount = 0;

SCALEMANAGER::SCALEMANAGER(uint8_t dout, uint8_t pd_sck) {  
  hx711.begin(dout, pd_sck, 64);
  hx711.set_gain(128);
}

SCALEMANAGER::~SCALEMANAGER() {
}

uint64_t SCALEMANAGER::runtime() {
  return rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000;
}

bool SCALEMANAGER::writeToNVS() {
  if (preferences.begin(NVS.c_str(), false)) {
    preferences.clear();
    preferences.putFloat("scale", hx711.get_scale());
    preferences.putLong("tare", hx711.get_offset());
    preferences.putFloat("emptyWeight", emptyWeightGramms);
    preferences.putFloat("fullWeight", fullWeightGramms);
    preferences.end();
    return true;
  } else {
    Serial.println(F("[SCALE] Unable to write data to NVS, giving up..."));
    return false;
  }
}

bool SCALEMANAGER::isConfigured() {
  //return true;
  return scale != 1.f && tare != 0;
}

void SCALEMANAGER::begin(String nvs) {
  NVS = nvs;
  if (!preferences.begin(NVS.c_str(), false)) {
    Serial.println(F("[SCALE] Error opening NVS Namespace, giving up..."));
  } else {
    scale = preferences.getFloat("scale", 1.f);
    tare = preferences.getLong("tare", 0);
    emptyWeightGramms = preferences.getUInt("", 5500); // 11Kg bottle by default is 5Kg empty
    fullWeightGramms = preferences.getUInt("", 16500); // 11Kg bottle by default
    Serial.printf("[SCALE] Successfully recovered data. Scale = %f with offset %ld\n", scale, tare);
    hx711.set_scale(scale);
    hx711.set_offset(tare);
    preferences.end();
  }
}

int SCALEMANAGER::getSensorMedianValue(bool cached) {
  if (cached) return lastMedian;
  if (hx711.wait_ready_retry(100, 5)) {
    //lastMedian = (int)floor(hx711.get_median_value(10) / 1000);
    lastMedian = hx711.get_units(10);
    return lastMedian;
  } else {
    Serial.println(F("[SCALE] Unable to communicate with the HX711 modul."));
    return -1;
  }
}

int SCALEMANAGER::getCalculatedPercentage(bool cached) {
  int val = lastMedian;
  if (!cached && val == 0) {
    val = getSensorMedianValue(false);
  } else if (!cached) {
    val = (val * 10 + getSensorMedianValue(false)) / 11;
  }
  u_int32_t currentGasWeight = val - emptyWeightGramms;
  u_int32_t maxGasWeight = fullWeightGramms - emptyWeightGramms;
  int pct = (int)((float)currentGasWeight / (float)maxGasWeight * 100.f);
  if (pct < 0) return 0;
  if (pct > 100) return 100;
  return pct;
}

void SCALEMANAGER::emptyScale() {
  Serial.println(F("[SCALE] Resetting scale"));
  hx711.set_scale();
  Serial.println(F("[SCALE] Resetting tare"));
  hx711.tare();
}

bool SCALEMANAGER::applyCalibrateWeight(int weight) {
  hx711.set_scale(hx711.get_units(10) / weight);
  return writeToNVS();
}

bool SCALEMANAGER::setBottleWeight(u_int32_t newEmptyWeightGramms, u_int32_t newFullWeightGramms) {
  emptyWeightGramms = newEmptyWeightGramms;
  fullWeightGramms = newFullWeightGramms;
  bool ret = writeToNVS();
  if (ret) {
    Serial.printf("[SCALE] New bottle weight configured. Empty = %d, Full = %d\n", newEmptyWeightGramms, newFullWeightGramms);
    return true;
  }
  Serial.printf("[SCALE] Unable to configure the new Bottle weight! Given data empty = %d, full = %d\n", newEmptyWeightGramms, newFullWeightGramms);
  return false;
}

int SCALEMANAGER::getBottleEmptyWeight() { return (int)emptyWeightGramms; }
int SCALEMANAGER::getBottleFullWeight() { return (int)fullWeightGramms; }
