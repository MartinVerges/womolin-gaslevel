/**
 * @file global.h
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

RTC_DATA_ATTR struct timing_t {
  // Check Services like MQTT, ...
  uint64_t lastServiceCheck = 0;               // last millis() from ServiceCheck
  const unsigned int serviceInterval = 30000;  // Interval in ms to execute code

  // Sensor data in loop()
  uint64_t lastSensorRead = 0;              // last millis() from Sensor read
  const unsigned int sensorInterval = 5000; // Interval in ms to execute code
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
Preferences preferences;

MQTTclient Mqtt;

String getMacFromBT(String spacer = "") {
  String output = "";
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);
  for (int i = 0; i < 6; i++) {
    char m[3];
    sprintf(m, "%02X", mac[i]);
    output += m;
    if (i < 5) output += spacer;
  }
  return output;
}
