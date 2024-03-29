/**
 * @file main.cpp
 * @author Martin Verges <martin@verges.cc>
 * @brief Gas scale to MQTT with WiFi, BLE, and more
 * @version 0.1
 * @date 2022-07-09
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#if !(defined(AUTO_FW_VERSION))
  #define AUTO_FW_VERSION "v0.0.0-00000000"
#endif
#if !(defined(AUTO_FW_DATE))
  #define AUTO_FW_DATE "2023-01-01"
#endif
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

#include "log.h"

#include <Arduino.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

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

#define BMP_SDA 21
#define BMP_SCL 22

#include "global.h"
#include "api-routes.h"
#include "ble.h"
#include "dac.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
Adafruit_BMP085_Unified bmp180 = Adafruit_BMP085_Unified(10085);

bool bmp180_found = false;

#include <Adafruit_BMP280.h>
bool bmp280_found = false;
Adafruit_BMP280 bmp280; // use I2C interface
Adafruit_Sensor *bmp280_temp = bmp280.getTemperatureSensor();
Adafruit_Sensor *bmp280_pressure = bmp280.getPressureSensor();

/*
// Experimental to connect the weight module via 4 cables intead of 5
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire(32); // 32, 27, or 23
DallasTemperature sensors(&oneWire);
*/
WebSerialClass WebSerial;

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

    LOG_INFO_LN(F("[POWER] Sleeping..."));
    esp_deep_sleep_start();
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : 
      LOG_INFO_LN(F("[POWER] Wakeup caused by external signal using RTC_IO"));
      button1.pressed = true;
    break;
    case ESP_SLEEP_WAKEUP_EXT1 : LOG_INFO_LN(F("[POWER] Wakeup caused by external signal using RTC_CNTL")); break;
    case ESP_SLEEP_WAKEUP_TIMER : 
      LOG_INFO_LN(F("[POWER] Wakeup caused by timer"));
      uint64_t timeNow, timeDiff;
      timeNow = rtc_time_slowclk_to_us(rtc_time_get(), esp_clk_slowclk_cal_get());
      timeDiff = timeNow - sleepTime;
      printf("Now: %" PRIu64 "ms, Duration: %" PRIu64 "ms\n", timeNow / 1000, timeDiff / 1000);
      delay(2000);
    break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : LOG_INFO_LN(F("[POWER] Wakeup caused by touchpad")); break;
    case ESP_SLEEP_WAKEUP_ULP : LOG_INFO_LN(F("[POWER] Wakeup caused by ULP program")); break;
    default : LOG_INFO_F("[POWER] Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void initWifiAndServices() {
  if (enableOtaWebUpdate) {
    otaWebUpdater.setFirmware(AUTO_FW_DATE, AUTO_FW_VERSION);
    otaWebUpdater.startBackgroundTask();
    otaWebUpdater.attachWebServer(&webServer);
  }

  // Load well known Wifi AP credentials from NVS
  WifiManager.startBackgroundTask();
  WifiManager.attachWebServer(&webServer);
  WifiManager.fallbackToSoftAp(preferences.getBool("enableSoftAp", true));

  WebSerial.begin(&webServer);

  APIRegisterRoutes();
  webServer.begin();
  LOG_INFO_LN(F("[WEB] HTTP server started"));

  if (enableWifi) {
    LOG_INFO_LN(F("[MDNS] Starting mDNS Service!"));
    MDNS.begin(hostname.c_str());
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ota", "udp", 3232);
    LOG_INFO_F("[MDNS] You should be able now to open http://%s.local/ in your browser.\n", hostname);
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
  else LOG_INFO_LN(F("[MQTT] Publish to MQTT is disabled."));
}

void setup() {
//  pinMode(23, OUTPUT);
//  digitalWrite(23, LOW);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  LOG_INFO_LN(F("\n\n==== starting ESP32 setup() ===="));
  LOG_INFO_F("Firmware build date: %s %s\n", __DATE__, __TIME__);
  LOG_INFO_F("Firmware Version: %s (%s)\n", AUTO_FW_VERSION, AUTO_FW_DATE);

  print_wakeup_reason();
  LOG_INFO_F("[SETUP] Configure ESP32 to sleep for every %d Seconds\n", TIME_TO_SLEEP);

  LOG_INFO_F("[GPIO] Configuration of GPIO %d as INPUT_PULLUP ... ", button1.PIN);
  pinMode(button1.PIN, INPUT_PULLUP);
  attachInterrupt(button1.PIN, ISR_button1, FALLING);
  LOG_INFO_LN(F("done"));

  if (!LittleFS.begin(true)) {
    LOG_INFO_LN(F("[FS] An Error has occurred while mounting LittleFS"));
    // Reduce power consumption while having issues with NVS
    // This won't fix the problem, a check of the sensor log is required
    deepsleepForSeconds(5);
  }
  if (!preferences.begin(NVS_NAMESPACE)) preferences.clear();
  LOG_INFO_LN(F("[LITTLEFS] initialized"));

  bmp180_found = bmp180.begin(BMP085_MODE_ULTRAHIGHRES);
  if (!bmp180_found) {
    LOG_INFO_LN(F("[BMP180] Chip not found, trying BMP280 next"));
    bmp280_found = bmp280.begin(BMP280_ADDRESS_ALT);
    if (bmp280_found) {
      /* Default settings from datasheet. */
      bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                          Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                          Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                          Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                          Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
      bmp280_temp->printSensorDetails();
    } else {
     LOG_INFO_LN(F("[BMP280] Chip not found, disabling temperature and pressure"));
    }
  }

  for (uint8_t i=0; i < LEVELMANAGERS; i++) {
    LevelManagers[i]->begin(String(NVS_NAMESPACE) + String("s") + String(i));
  }
  
  // Load Settings from NVS
  hostname = preferences.getString("hostname");
  if (hostname.isEmpty()) {
    hostname = "gaslevel";
    preferences.putString("hostname", hostname);
  }
  enableWifi = preferences.getBool("enableWifi", true);
  enableBle = preferences.getBool("enableBle", false);
  enableDac = preferences.getBool("enableDac", false);
  enableMqtt = preferences.getBool("enableMqtt", false);
  enableOtaWebUpdate = preferences.getBool("otaWebEnabled", enableOtaWebUpdate);

  if (!preferences.getString("otaWebUrl").isEmpty()) {
    otaWebUpdater.setBaseUrl(preferences.getString("otaWebUrl"));
  }

  if (enableWifi) initWifiAndServices();
  else LOG_INFO_LN(F("[WIFI] Not starting WiFi!"));

  if (enableBle) createBleServer(hostname);
  else LOG_INFO_LN(F("[BLE] Bluetooth low energy is disabled."));
  
  String otaPassword = preferences.getString("otaPassword");
  if (otaPassword.isEmpty()) {
    otaPassword = String((uint32_t)ESP.getEfuseMac());
    preferences.putString("otaPassword", otaPassword);
  }
  otaWebUpdater.setOtaPassword(otaPassword);
  LOG_INFO_F("[OTA] Password set to '%s'\n", otaPassword);
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
    LOG_INFO_LN(F("[EVENT] Button pressed!"));
    button1.pressed = false;
    if (enableWifi) {
      // bringt up a SoftAP instead of beeing a client
      WifiManager.runSoftAP();
    } else {
      initWifiAndServices();
    }
    // softReset();
  }
  
  // Do not continue regular operation as long as a OTA is running
  // Reason: Background workload can cause upgrade issues that we want to avoid!
  if (otaWebUpdater.otaIsRunning) return sleepOrDelay();

  if (runtime() - Timing.lastServiceCheck > Timing.serviceInterval) {
    Timing.lastServiceCheck = runtime();
    // Check if all the services work
    if (enableWifi && WiFi.status() == WL_CONNECTED && WiFi.getMode() & WIFI_MODE_STA) {
      if (enableMqtt && !Mqtt.isConnected()) Mqtt.connect();
    }
  }

  // Update values from HX711
  for (uint8_t i=0; i < LEVELMANAGERS; i++) {
    // LevelManagers[i]->initHX711();
    LevelManagers[i]->loop();
  }

  // run regular operation
  if (runtime() - Timing.lastStatusUpdate > Timing.statusUpdateInterval) {
    Timing.lastStatusUpdate = runtime();

    String jsonOutput;
    DynamicJsonDocument jsonDoc(1024);
    JsonArray jsonArray = jsonDoc.to<JsonArray>();

    float pressure = 0.f;
    float temperature = 0.f;
    if (bmp180_found) {
      sensors_event_t event;
      bmp180.getEvent(&event);
      bmp180.getTemperature(&temperature);
      pressure = event.pressure;

    } else if (bmp280_found) {
      sensors_event_t temp_event, pressure_event;
      bmp280_pressure->getEvent(&pressure_event);
      bmp280_temp->getEvent(&temp_event);
      pressure = pressure_event.pressure;
      temperature = temp_event.temperature;
      
    }
/*
    digitalWrite(23, LOW);
    sensors.begin();
    sensors.requestTemperatures(); 
    float temperatureC = sensors.getTempCByIndex(0);
    Serial.print(temperatureC);
    Serial.println("ºC");
    digitalWrite(23, HIGH);
    delay(100);
*/
    for (uint8_t i=0; i < LEVELMANAGERS; i++) {
      JsonObject jsonNestedObject = jsonArray.createNestedObject();
      // LevelManagers[i]->initHX711();

      jsonNestedObject["id"] = i;
      jsonNestedObject["airPressure"] = pressure;
      jsonNestedObject["temperature"] = temperature;
      jsonNestedObject["sensorValue"] = LevelManagers[i]->getLastMedian();

      if (enableMqtt && Mqtt.isReady()) {
        Mqtt.client.publish((Mqtt.mqttTopic + "/airPressure").c_str(), String(pressure).c_str(), true);
        Mqtt.client.publish((Mqtt.mqttTopic + "/temperature").c_str(), String(temperature).c_str(), true);
      }

      if (LevelManagers[i]->isConfigured()) {
        jsonNestedObject["level"] = LevelManagers[i]->getLevel();
        jsonNestedObject["gasWeight"] = LevelManagers[i]->getGasWeight();

        if (enableDac) dacValue(i+1, LevelManagers[i]->getLevel());
        if (enableBle) updateBleCharacteristic(LevelManagers[i]->getLevel());  // FIXME: need to manage multiple levels
        if (enableMqtt && Mqtt.isReady()) {
          Mqtt.client.publish((Mqtt.mqttTopic + "/level" + String(i+1)).c_str(), String(LevelManagers[i]->getLevel()).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/sensorValue" + String(i+1)).c_str(), String(LevelManagers[i]->getLastMedian()).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/gasWeight" + String(i+1)).c_str(), String(LevelManagers[i]->getGasWeight()).c_str(), true);
        }
        LOG_INFO_F("[SENSOR] %d. sensor level is %d%% (raw sensor value = %d)\n",
          i+1, LevelManagers[i]->getLevel(), LevelManagers[i]->getLastMedian()
        );
      } else {
        if (enableDac) dacValue(i+1, 0);
        if (enableBle) updateBleCharacteristic(0);  // FIXME
        if (enableMqtt && Mqtt.isReady()) {
          Mqtt.client.publish((Mqtt.mqttTopic + "/level" + String(i+1)).c_str(), "0", true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/sensorValue" + String(i+1)).c_str(), String(LevelManagers[i]->getLastMedian()).c_str(), true);
          Mqtt.client.publish((Mqtt.mqttTopic + "/gasWeight" + String(i+1)).c_str(), "0", true);
        }
        LOG_INFO_F("[SENSOR] %d. Sensor is not configured, please run the setup! (raw sensor value %d)\n",
          i+1, LevelManagers[i]->getLastMedian()
        );
      }
    }

    serializeJsonPretty(jsonArray, jsonOutput);
    events.send(jsonOutput.c_str(), "status", millis());
    //LOG_INFO_LN(jsonOutput);
  }
  sleepOrDelay();
}
