/**
 * @file scalemanager.cpp
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#include "log.h"

#include <Arduino.h>
#include <ArduinoJson.h>
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

SCALEMANAGER::SCALEMANAGER(uint8_t dout, uint8_t pd_sck) {  
  hx711.begin(dout, pd_sck, 64);
  hx711.set_gain(128);
}

SCALEMANAGER::SCALEMANAGER(uint8_t dout, uint8_t pd_sck, uint8_t gain) {  
  hx711.begin(dout, pd_sck, 64);
  if (gain == 128 or gain == 64 or gain == 32) hx711.set_gain(gain);
  else hx711.set_gain(128);
}

SCALEMANAGER::~SCALEMANAGER() {
}

uint64_t SCALEMANAGER::runtime() {
  return rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000;
}

void SCALEMANAGER::loop() {
  if (runtime() - timing.lastSensorRead >= timing.sensorIntervalMs) {
    timing.lastSensorRead = runtime();
    getSensorMedianValue(false); // update lastMedian
    if (isConfigured()) {
      calculateLevel();
    }
  }
}

bool SCALEMANAGER::writeToNVS() {
  if (preferences.begin(NVS.c_str(), false)) {
    preferences.clear();
    preferences.putDouble("scale", hx711.get_scale());
    preferences.putULong64("tare", hx711.get_offset());
    preferences.putUInt("emptyWeight", emptyWeightGramms);
    preferences.putUInt("fullWeight", fullWeightGramms);
    preferences.end();
    return true;
  } else {
    LOG_INFO_LN(F("[SCALE] Unable to write data to NVS, giving up..."));
    return false;
  }
}

String SCALEMANAGER::getJsonConfig() {
    String output;
    DynamicJsonDocument doc(256);

    doc["scale"] = hx711.get_scale();
    doc["tare"] = hx711.get_offset();
    doc["emptyWeight"] = emptyWeightGramms;
    doc["fullWeight"] = fullWeightGramms;

    serializeJsonPretty(doc, output);
    return output;
}

// Write the config running environment and to NVS
bool SCALEMANAGER::putJsonConfig(String newCfg) {
    DynamicJsonDocument jsonBuffer(1024);
    DeserializationError error = deserializeJson(jsonBuffer, newCfg);

    if (error) {
      LOG_INFO("deserializeJson() failed: ");
      LOG_INFO_LN(error.f_str());
      return false;
    }

    if (jsonBuffer["scale"].isNull()
     || jsonBuffer["tare"].isNull()
     || jsonBuffer["emptyWeight"].isNull()
     || jsonBuffer["fullWeight"].isNull()) {
      LOG_INFO_LN("At least one field is missing. Check the scale, tare, emptyWeight, fullWeight field!");
      return false;
    }

    scale = jsonBuffer["scale"].as<double>();
    hx711.set_scale(scale);
    tare = jsonBuffer["tare"].as<uint64_t>();
    hx711.set_offset(tare);
    LOG_INFO_F("[SCALE] Successfully set data. Scale = %f with offset %ld\n", scale, tare);

    emptyWeightGramms = jsonBuffer["emptyWeight"].as<uint32_t>();
    fullWeightGramms = jsonBuffer["fullWeight"].as<uint32_t>();
    LOG_INFO_F("[SCALE] Bottle configuration: Empty = %dg Full = %dg\n", emptyWeightGramms, fullWeightGramms);

    return writeToNVS();
}


bool SCALEMANAGER::isConfigured() {
  //return true;
  return scale != 1.f && tare != 0;
}

void SCALEMANAGER::begin(String nvs) {
  NVS = nvs;
  if (!preferences.begin(NVS.c_str(), false)) {
    LOG_INFO_LN(F("[SCALE] Error opening NVS Namespace, giving up..."));
  } else {
    scale = preferences.getDouble("scale", 1.f);
    tare = preferences.getULong64("tare", 0);
    LOG_INFO_F("[SCALE] Successfully recovered data. Scale = %f with offset %ld\n", scale, tare);
    emptyWeightGramms = preferences.getUInt("emptyWeight", 5500); // 11Kg alu bottle by default is 5Kg empty
    fullWeightGramms = preferences.getUInt("fullWeight", 16500);  // 11Kg alu bottle by default
    LOG_INFO_F("[SCALE] Bottle configuration: Empty = %dg Full = %dg\n", emptyWeightGramms, fullWeightGramms);
    hx711.set_scale(scale);
    hx711.set_offset(tare);
    preferences.end();
  }
}

uint32_t SCALEMANAGER::getSensorMedianValue(bool cached) {
  if (cached) return lastMedian;
  if (hx711.wait_ready_retry(100, 5)) {
    //lastMedian = (int)floor(hx711.get_median_value(10) / 1000);
    lastMedian = hx711.get_units(10);
    return lastMedian;
  } else {
    LOG_INFO_LN(F("[SCALE] Unable to communicate with the HX711 modul."));
    return -1;
  }
}

uint8_t SCALEMANAGER::calculateLevel() {
  currentGasWeightGramms = (lastMedian > emptyWeightGramms) ? lastMedian - emptyWeightGramms : 0;
  uint32_t maxGasWeight = fullWeightGramms - emptyWeightGramms;
  uint32_t pct = (uint32_t)((float)currentGasWeightGramms / (float)maxGasWeight * 100.f);
  // LOG_INFO_F("DEBUG - %d | %d | %d | %d | %d | %d \n", lastMedian, pct, currentGasWeight, maxGasWeight, fullWeightGramms, emptyWeightGramms);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  level = pct;
  return level;
}

void SCALEMANAGER::emptyScale() {
  LOG_INFO_LN(F("[SCALE] Resetting scale"));
  hx711.set_scale();
  LOG_INFO_LN(F("[SCALE] Resetting tare"));
  hx711.tare();
}

bool SCALEMANAGER::applyCalibrateWeight(uint32_t weight) {
  hx711.set_scale(hx711.get_units(10) / weight);
  return writeToNVS();
}

bool SCALEMANAGER::setBottleWeight(uint32_t newEmptyWeightGramms, uint32_t newFullWeightGramms) {
  emptyWeightGramms = newEmptyWeightGramms;
  fullWeightGramms = newFullWeightGramms;
  bool ret = writeToNVS();
  if (ret) {
    LOG_INFO_F("[SCALE] New bottle weight configured. Empty = %d, Full = %d\n", newEmptyWeightGramms, newFullWeightGramms);
    return true;
  }
  LOG_INFO_F("[SCALE] Unable to configure the new Bottle weight! Given data empty = %d, full = %d\n", newEmptyWeightGramms, newFullWeightGramms);
  return false;
}

uint32_t SCALEMANAGER::getBottleEmptyWeight() { return (uint32_t)emptyWeightGramms; }
uint32_t SCALEMANAGER::getBottleFullWeight() { return (uint32_t)fullWeightGramms; }
