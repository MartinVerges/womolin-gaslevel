/**
 *
 * Gas Scale
 * https://github.com/MartinVerges/rv-smart-gas-scale
 *
 * (c) 2022 Martin Verges
 *
**/

#include <Arduino.h>
#include "scalemanager.h"
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Preferences.h>
#define SPIFFS LITTLEFS 
#include <LITTLEFS.h>
#include "MQTTclient.h"

#define webserverPort 80                    // Start the Webserver on this port
#define NVS_NAMESPACE "gas-scale"          // Preferences.h namespace to store settings

RTC_DATA_ATTR struct timing_t {
  // Wifi interval in loop()
  uint64_t lastWifiCheck = 0;               // last millis() from WifiCheck
  const unsigned int wifiInterval = 30000;  // Interval in ms to execute code

  // Check Services like MQTT, ...
  uint64_t lastServiceCheck = 0;               // last millis() from ServiceCheck
  const unsigned int serviceInterval = 10000;  // Interval in ms to execute code

  // Sensor data in loop()
  uint64_t lastSensorRead = 0;              // last millis() from Sensor read
  const unsigned int sensorInterval = 1000; // Interval in ms to execute code

  // Setup executing in loop()
  uint64_t lastSetupRead = 0;               // last millis() from Setup run
  const unsigned int setupInterval = 15 * 60 * 1000 / 255;   // Interval in ms to execute code
} Timing;

RTC_DATA_ATTR bool startWifiConfigPortal = false; // Start the config portal on setup() (default set by wakeup funct.)
RTC_DATA_ATTR uint64_t sleepTime = 0;             // Time that the esp32 slept

struct used_pins {
  uint8_t dout;
  uint8_t pd_sck;
};

SCALEMANAGER LevelManager1;
SCALEMANAGER LevelManager2;
#define LEVELMANAGERS 2
SCALEMANAGER * LevelManagers[LEVELMANAGERS] = {
  &LevelManager1,
  &LevelManager2
};
used_pins GPIOSETTINGS[LEVELMANAGERS] = {
  {32,27},
  {16,17}
};

struct Button {
  const gpio_num_t PIN;
  bool pressed;
};
Button button1 = {GPIO_NUM_4, false};       // Run the setup (use a RTC GPIO)

String hostName;
uint32_t blePin;
DNSServer dnsServer;
AsyncWebServer webServer(webserverPort);
AsyncEventSource events("/events");
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
