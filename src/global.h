/**
 * @file global.h
 * @author Martin Verges <martin@verges.cc>
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#include <Arduino.h>
#include "scalemanager.h"
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Preferences.h>
#define SPIFFS LittleFS 
#include <LittleFS.h>
#include "MQTTclient.h"
#include "wifimanager.h"

#define webserverPort 80                    // Start the Webserver on this port
#define NVS_NAMESPACE "gaslevel"            // Preferences.h namespace to store settings

#include <SPI.h>
#include <Wire.h>

bool otaRunning = false;

RTC_DATA_ATTR struct timing_t {
  // Check Services like MQTT, ...
  uint64_t lastServiceCheck = 0;               // last millis() from ServiceCheck
  const unsigned int serviceInterval = 30000;  // Interval in ms to execute code

  // Sensor data in loop()
  uint64_t lastStatusUpdate = 0;                  // last millis() from Status report
  const unsigned int statusUpdateInterval = 5000; // Interval in ms to execute code
} Timing;

RTC_DATA_ATTR uint64_t sleepTime = 0;             // Time that the esp32 slept

SCALEMANAGER LevelManager1(32,27);
SCALEMANAGER LevelManager2(16,17);
#define LEVELMANAGERS 2
SCALEMANAGER * LevelManagers[LEVELMANAGERS] = {
  &LevelManager1,
  &LevelManager2
};

WIFIMANAGER WifiManager;
bool enableWifi = true;                     // Enable Wifi, disable to reduce power consumtion, stored in NVS

struct Button {
  const gpio_num_t PIN;
  bool pressed;
};
Button button1 = {GPIO_NUM_4, false};       // Run the setup (use a RTC GPIO)

String hostName;
uint32_t blePin;
DNSServer dnsServer;
AsyncWebServer webServer(webserverPort);
AsyncEventSource events("/api/events");
Preferences preferences;

MQTTclient Mqtt;

uint64_t runtime() {
  return rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get()) / 1000;
}

String uint64ToString(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c +='0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}
