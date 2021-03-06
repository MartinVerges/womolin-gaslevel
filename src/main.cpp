/**
 * @file main.cpp
 * @author Martin Verges <martin@veges.cc>
 * @brief Gas scale to MQTT with WiFi, BLE, and more
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#if !(defined(ESP32))
  #error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#define uS_TO_S_FACTOR   1000000           // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP    10                 // WakeUp interval

// Fix an issue with the HX711 library on ESP32
#if !(defined(ARDUINO_ARCH_ESP32))
  #define ARDUINO_ARCH_ESP32 true
#endif
#undef USE_LittleFS
#define USE_LittleFS true

#include <Arduino.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Power Management
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <soc/rtc.h>
extern "C" {
  #if ESP_ARDUINO_VERSION_MAJOR >= 2
    #include <esp32/clk.h>
  #else
    #include <esp_clk.h>
  #endif
}

#include "global.h"
#include "api-routes.h"
#include "ble.h"
#include "dac.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
Adafruit_BMP085_Unified bmp180 = Adafruit_BMP085_Unified(10085);
bool bmp180_found = false;

void IRAM_ATTR ISR_button1() {
  button1.pressed = true;
}

void deepsleepForSeconds(int seconds) {
    esp_sleep_enable_timer_wakeup(seconds * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

// Check if a feature is enabled, that prevents the
// deep sleep mode of our ESP32 chip.
void sleepOrDelay() {
  if (enableWifi || enableBle || enableMqtt) {
    yield();
    delay(50);
  } else {
    // We can save a lot of power by going into deepsleep
    // Thid disables WIFI and everything.
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    sleepTime = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
    rtc_gpio_pullup_en(button1.PIN);
    rtc_gpio_pulldown_dis(button1.PIN);
    esp_sleep_enable_ext0_wakeup(button1.PIN, 0);

    Serial.println(F("[POWER] Sleeping..."));
    esp_deep_sleep_start();
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : 
      Serial.println(F("[POWER] Wakeup caused by external signal using RTC_IO"));
      button1.pressed = true;
    break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println(F("[POWER] Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : 
      Serial.println(F("[POWER] Wakeup caused by timer"));
      uint64_t timeNow, timeDiff;
      timeNow = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      timeDiff = timeNow - sleepTime;
      printf("Now: %" PRIu64 "ms, Duration: %" PRIu64 "ms\n", timeNow / 1000, timeDiff / 1000);
      delay(2000);
    break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println(F("[POWER] Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println(F("[POWER] Wakeup caused by ULP program")); break;
    default : Serial.printf("[POWER] Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void initWifiAndServices() {
  // Load well known Wifi AP credentials from NVS
  WifiManager.startBackgroundTask();
  WifiManager.attachWebServer(&webServer);
  WifiManager.fallbackToSoftAp(preferences.getBool("enableSoftAp", true));

  APIRegisterRoutes();
  webServer.begin();
  Serial.println(F("[WEB] HTTP server started"));

  if (enableWifi) {
    Serial.println(F("[MDNS] Starting mDNS Service!"));
    MDNS.begin(hostName.c_str());
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ota", "udp", 3232);
    Serial.printf("[MDNS] You should be able now to open http://%s.local/ in your browser.\n", hostName);
  }

  if (enableMqtt) {
    Mqtt.prepare(
      preferences.getString("mqttHost", "localhost"),
      preferences.getUInt("mqttPort", 1883),
      preferences.getString("mqttTopic", "verges/gaslevel"),
      preferences.getString("mqttUser", ""),
      preferences.getString("mqttPass", "")
    );
  }
  else Serial.println(F("[MQTT] Publish to MQTT is disabled."));
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println(F("\n\n==== starting ESP32 setup() ===="));
  Serial.printf("Firmware build date: %s %s\n", __DATE__, __TIME__);

  print_wakeup_reason();
  Serial.printf("[SETUP] Configure ESP32 to sleep for every %d Seconds\n", TIME_TO_SLEEP);

  Serial.printf("[GPIO] Configuration of GPIO %d as INPUT_PULLUP ... ", button1.PIN);
  pinMode(button1.PIN, INPUT_PULLUP);
  attachInterrupt(button1.PIN, ISR_button1, FALLING);
  Serial.println(F("done"));

  if (!LittleFS.begin(true)) {
    Serial.println(F("[FS] An Error has occurred while mounting LittleFS"));
    // Reduce power consumption while having issues with NVS
    // This won't fix the problem, a check of the sensor log is required
    deepsleepForSeconds(5);
  }
  if (!preferences.begin(NVS_NAMESPACE)) preferences.clear();
  Serial.println(F("[LITTLEFS] initialized"));

  bmp180_found = bmp180.begin(BMP085_MODE_ULTRAHIGHRES);
  if (!bmp180_found) Serial.println(F("[BMP180] Chip not found, disabling temperature and pressure"));

  for (uint8_t i=0; i < LEVELMANAGERS; i++) {
    LevelManagers[i]->begin(String(NVS_NAMESPACE) + String("s") + String(i));
  }
  
  // Load Settings from NVS
  hostName = preferences.getString("hostName");
  if (hostName.isEmpty()) {
    hostName = "gaslevel";
    preferences.putString("hostName", hostName);
  }
  enableWifi = preferences.getBool("enableWifi", true);
  enableBle = preferences.getBool("enableBle", false);
  enableDac = preferences.getBool("enableDac", false);
  enableMqtt = preferences.getBool("enableMqtt", false);

  if (enableWifi) initWifiAndServices();
  else Serial.println(F("[WIFI] Not starting WiFi!"));

  if (enableBle) createBleServer(hostName);
  else Serial.println(F("[BLE] Bluetooth low energy is disabled."));
  
  String otaPassword = preferences.getString("otaPassword");
  if (otaPassword.isEmpty()) {
    otaPassword = String((uint32_t)ESP.getEfuseMac());
    preferences.putString("otaPassword", otaPassword);
  }
  Serial.printf("[OTA] Password set to '%s'\n", otaPassword);
  ArduinoOTA
    .setHostname(hostName.c_str())
    .setPassword(otaPassword.c_str())
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else {
        type = "filesystem";
        LittleFS.end();
      }
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  preferences.end();

  for (uint8_t i=0; i < LEVELMANAGERS; i++) {
    if (!LevelManagers[i]->isConfigured()) {
      // we need to bring up WiFi to provide a convenient setup routine
      enableWifi = true;
    }
  }
}

// Soft reset the ESP to start with setup() again, but without loosing RTC_DATA as it would be with ESP.reset()
void softReset() {
  if (enableWifi) {
    webServer.end();
    MDNS.end();
    Mqtt.disconnect();
    WifiManager.stopWifi();
  }
  esp_sleep_enable_timer_wakeup(1);
  esp_deep_sleep_start();
}

void loop() {
  if (button1.pressed) {
    Serial.println(F("[EVENT] Button pressed!"));
    button1.pressed = false;
    if (enableWifi) {
      // bringt up a SoftAP instead of beeing a client
      WifiManager.runSoftAP();
    } else {
      initWifiAndServices();
    }
    // softReset();
  }

  if (runtime() - Timing.lastServiceCheck > Timing.serviceInterval) {
    Timing.lastServiceCheck = runtime();
    // Check if all the services work
    if (enableWifi && WiFi.status() == WL_CONNECTED && WiFi.getMode() & WIFI_MODE_STA) {
      if (enableMqtt && !Mqtt.isConnected()) Mqtt.connect();
    }
  }
 
  // run regular operation
  if (runtime() - Timing.lastSensorRead > Timing.sensorInterval) {
    Timing.lastSensorRead = runtime();

    String jsonOutput;
    //StaticJsonDocument<1024> jsonDoc;
    DynamicJsonDocument jsonDoc(1024);
    JsonArray jsonArray = jsonDoc.to<JsonArray>();

    sensors_event_t event;
    float temperature = 0.f;
    if (bmp180_found) {
      bmp180.getEvent(&event);
      bmp180.getTemperature(&temperature);
    } else {
      event.pressure = 0;
    }
    for (uint8_t i=0; i < LEVELMANAGERS; i++) {
      int sensorValue = LevelManagers[i]->getSensorMedianValue();

      JsonObject jsonNestedObject = jsonArray.createNestedObject();
      jsonNestedObject["id"] = i;
      jsonNestedObject["airPressure"] = event.pressure;
      jsonNestedObject["temperature"] = temperature;
      jsonNestedObject["sensorValue"] = sensorValue;

      if (LevelManagers[i]->isConfigured()) {
        uint8_t level = LevelManagers[i]->getCalculatedPercentage(true);
        jsonNestedObject["level"] = level;

        String ident = String("level") + String(i);
        if (enableDac) dacValue(i+1, level);
        if (enableBle) updateBleCharacteristic(level);  // FIXME: need to manage multiple levels
        if (enableMqtt && Mqtt.isReady()) {
          Mqtt.client.publish((Mqtt.mqttTopic + "/level" + String(i+1)).c_str(), String(level).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/sensorValue" + String(i+1)).c_str(), String(sensorValue).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/airPressure" + String(i+1)).c_str(), String(event.pressure).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/temperature" + String(i+1)).c_str(), String(temperature).c_str(), true);
        }

        Serial.printf("[SENSOR] Current level of %d. sensor is %d (raw %d)\n",
          i+1, level, LevelManagers[i]->getSensorMedianValue(true)
        );
      } else {
        if (enableDac) dacValue(i+1, 0);
        if (enableBle) updateBleCharacteristic(0);  // FIXME
        Serial.printf("[SENSOR] Sensor %d not configured, please run the setup! (Raw sensor value %d)\n",
          i+1, (int)LevelManagers[i]->getSensorMedianValue(true)
        );
      }
    }

    serializeJsonPretty(jsonArray, jsonOutput);
    events.send(jsonOutput.c_str(), "status", millis());
    //Serial.println(jsonOutput);
  }
  sleepOrDelay();
}
