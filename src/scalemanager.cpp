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
  setGPIOs(dout, pd_sck, 128);
  initHX711();
}

SCALEMANAGER::SCALEMANAGER(uint8_t dout, uint8_t pd_sck, uint8_t gain) {
  setGPIOs(dout, pd_sck, gain);
  initHX711();
}

void SCALEMANAGER::setGPIOs(uint8_t dout, uint8_t pd_sck, uint8_t gain) {
  DOUT = dout;
  PD_SCK = pd_sck;
  GAIN = gain;
  Serial.printf("setGPIOs(dout = %d, pd_sck = %d, gain = %d)\n", DOUT, PD_SCK, GAIN);
}

void SCALEMANAGER::initHX711() {
  hx711.begin(DOUT, PD_SCK, GAIN);
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
    SCALE = hx711.get_scale();
    OFFSET = hx711.get_offset();
    preferences.clear();
    preferences.putDouble("scale", hx711.get_scale());
    preferences.putULong("offset", hx711.get_offset());
    preferences.putUInt("emptyWeight", emptyWeightGramms);
    preferences.putUInt("fullWeight", fullWeightGramms);

    LOG_INFO_F("[SCALE] Stored scale (%.8f) and offset (%d) in NVS\n",
      preferences.getDouble("scale"),
      preferences.getULong("offset")
    );
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
    doc["offset"] = hx711.get_offset();
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
     || jsonBuffer["offset"].isNull()
     || jsonBuffer["emptyWeight"].isNull()
     || jsonBuffer["fullWeight"].isNull()) {
      LOG_INFO_LN("At least one field is missing. Check the scale, offset, emptyWeight, fullWeight field!");
      return false;
    }

    SCALE = jsonBuffer["scale"].as<double>();
    hx711.set_scale(SCALE);
    OFFSET = jsonBuffer["offset"].as<uint32_t>();
    hx711.set_offset(OFFSET);
    LOG_INFO_F("[SCALE] Successfully set data. Scale = %.8f with offset %d\n", SCALE, OFFSET);

    emptyWeightGramms = jsonBuffer["emptyWeight"].as<uint32_t>();
    fullWeightGramms = jsonBuffer["fullWeight"].as<uint32_t>();
    LOG_INFO_F("[SCALE] Bottle configuration: Empty = %dg Full = %dg\n", emptyWeightGramms, fullWeightGramms);

    return writeToNVS();
}


bool SCALEMANAGER::isConfigured() {
  //return true;
  return SCALE != 1.f && OFFSET != 0;
}

void SCALEMANAGER::begin(String nvs) {
  NVS = nvs;
  if (!preferences.begin(NVS.c_str(), false)) {
    LOG_INFO_LN(F("[SCALE] Error opening NVS Namespace, giving up..."));
  } else {
    SCALE = preferences.getDouble("scale", 1.f);
    OFFSET = preferences.getULong("offset", 0);
    LOG_INFO_F("[SCALE] Successfully recovered data. Scale = %.8f with offset %d\n", SCALE, OFFSET);
    emptyWeightGramms = preferences.getUInt("emptyWeight", 5500); // 11Kg alu bottle weights 5.5Kg empty
    fullWeightGramms = preferences.getUInt("fullWeight", 16500);  // 5.5Kg alu bottle plus 11Kg gas
    LOG_INFO_F("[SCALE] Bottle configuration: Empty = %dg Full = %dg\n", emptyWeightGramms, fullWeightGramms);
    hx711.set_scale(SCALE);
    hx711.set_offset(OFFSET);
    preferences.end();
  }
}

uint32_t SCALEMANAGER::getSensorMedianValue(bool cached) {
  if (cached) return lastMedian;
  if (hx711.wait_ready_retry(100, 5)) {
    //lastMedian = (int)floor(hx711.get_median_value(10) / 1000);
    lastMedian = hx711.get_units(10);
    // LOG_INFO_F("getSensorMedianValue(cached = %s) returned lastMedian = %d\n", cached ? "true" : "false", lastMedian);
    if (lastMedian == UINT64_MAX) {
      LOG_INFO_LN(F("[SCALE] Detected UINT64_MAX value, ignoring!"));
      lastMedian = 0;
    }
    return lastMedian;
  } else {
    LOG_INFO_LN(F("[SCALE] Unable to communicate with the HX711 modul."));
    return -1;
  }
}

uint8_t SCALEMANAGER::calculateLevel() {
  if (isConfigured() && lastMedian > fullWeightGramms*10) {
    LOG_INFO_F("[SCALE] Abnormal reading from hx711 detected (lastMedian = %d), possibly incorrect. Set to 0 to prevent underflow issues.\n",
      lastMedian = 0
    );
  }
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
  SCALE = 1.f;
  OFFSET = 0;
  hx711.set_scale(SCALE);
  hx711.tare();
  LOG_INFO_F("[SCALE] Resetting scale to %.8f with new offset set to %d\n",
    hx711.get_scale(),
    hx711.get_offset()
  );
}

bool SCALEMANAGER::applyCalibrateWeight(uint32_t weight) {
  SCALE = hx711.get_units(10) / weight;
  hx711.set_scale(SCALE);
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
