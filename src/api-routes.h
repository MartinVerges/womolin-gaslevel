/**
 * @file api-routes.h
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
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include "ble.h"
#include <Update.h>
#include <esp_ota_ops.h>

extern bool enableWifi;
extern bool enableBle;
extern bool enableMqtt;
extern bool enableDac;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

void APIRegisterRoutes() {
  webServer.on("/api/firmware/info", HTTP_GET, [&](AsyncWebServerRequest *request) {
    auto data = esp_ota_get_running_partition();
    String output;
    DynamicJsonDocument doc(256);
    doc["partition_type"] = data->type;
    doc["partition_subtype"] = data->subtype;
    doc["address"] = data->address;
    doc["size"] = data->size;
    doc["label"] = data->label;
    doc["encrypted"] = data->encrypted;
    doc["firmware_version"] = AUTO_FW_VERSION;
    doc["firmware_date"] = AUTO_FW_DATE;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  events.onConnect([&](AsyncEventSourceClient *client){
    if(client->lastId()){
      LOG_INFO_F("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("connected", NULL, millis(), 1000);
  });
  webServer.addHandler(&events);

  webServer.on("/api/reset", HTTP_POST, [&](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    request->send(200, "application/json", "{\"message\":\"Resetting the sensor!\"}");
    request->send(response);
    yield();
    delay(250);
    ESP.restart();
  });

  webServer.on("/api/config", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    DynamicJsonDocument jsonBuffer(1024);
    deserializeJson(jsonBuffer, (const char*)data);

    if (preferences.begin(NVS_NAMESPACE)) {
      String hostname = jsonBuffer["hostname"].as<String>();
      if (!hostname || hostname.length() < 3 || hostname.length() > 32) {
        // TODO: Add better checks according to RFC hostnames
        request->send(422, "application/json", "{\"message\":\"Invalid hostname!\"}");
        return;
      } else {
        preferences.putString("hostname", hostname);
      }

      if (preferences.putBool("enableWifi", jsonBuffer["enableWifi"].as<boolean>())) {
        enableWifi = jsonBuffer["enableWifi"].as<boolean>();
      }
      if (preferences.putBool("enableSoftAp", jsonBuffer["enableSoftAp"].as<boolean>())) {
        WifiManager.fallbackToSoftAp(jsonBuffer["enableSoftAp"].as<boolean>());
      }

      if (preferences.putBool("enableBle", jsonBuffer["enableBle"].as<boolean>())) {
        if (enableBle) stopBleServer();
        enableBle = jsonBuffer["enableBle"].as<boolean>();
        if (enableBle) createBleServer(hostname);
        yield();
      }
      if (preferences.putBool("enableDac", jsonBuffer["enableDac"].as<boolean>())) {
        enableDac = jsonBuffer["enableDac"].as<boolean>();
      }

      preferences.putString("otaPassword", jsonBuffer["otaPassword"].as<String>());
      preferences.putBool("otaWebEnabled", jsonBuffer["otaWebEnabled"].as<boolean>());
      if (preferences.putString("otaWebUrl", jsonBuffer["otaWebUrl"].as<String>())) {
        otaWebUpdater.setBaseUrl(jsonBuffer["otaWebUrl"].as<String>());
      }  

      // MQTT Settings
      preferences.putUInt("mqttPort", jsonBuffer["mqttPort"].as<uint16_t>());
      preferences.putString("mqttHost", jsonBuffer["mqttHost"].as<String>());
      preferences.putString("mqttTopic", jsonBuffer["mqttTopic"].as<String>());
      preferences.putString("mqttUser", jsonBuffer["mqttUser"].as<String>());
      preferences.putString("mqttPass", jsonBuffer["mqttPass"].as<String>());
      if (preferences.putBool("enableMqtt", jsonBuffer["enableMqtt"].as<boolean>())) {
        if (enableMqtt) Mqtt.disconnect();
        enableMqtt = jsonBuffer["enableMqtt"].as<boolean>();
        if (enableMqtt) {
          Mqtt.prepare(
            jsonBuffer["mqttHost"].as<String>(),
            jsonBuffer["mqttPort"].as<uint16_t>(),
            jsonBuffer["mqttTopic"].as<String>(),
            jsonBuffer["mqttUser"].as<String>(),
            jsonBuffer["mqttPass"].as<String>()
          );
          Mqtt.connect();
        }
      }
    }
    preferences.end();

    request->send(200, "application/json", "{\"message\":\"New configuration stored in NVS, reboot required!\"}");
  });

  webServer.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      DynamicJsonDocument doc(1024);

      if (preferences.begin(NVS_NAMESPACE, true)) {
        doc["hostname"] = hostname;
        doc["enableWifi"] = enableWifi;
        doc["enableSoftAp"] = WifiManager.getFallbackState();
        doc["enableBle"] = enableBle;
        doc["enableDac"] = enableDac;

        doc["otaPassword"] = preferences.getString("otaPassword");

        // MQTT
        doc["enableMqtt"] = enableMqtt;
        doc["mqttPort"] = preferences.getUInt("mqttPort", 1883);
        doc["mqttHost"] = preferences.getString("mqttHost", "");
        doc["mqttTopic"] = preferences.getString("mqttTopic", "");
        doc["mqttUser"] = preferences.getString("mqttUser", "");
        doc["mqttPass"] = preferences.getString("mqttPass", "");
      }
      preferences.end();

      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(415, "text/plain", "Unsupported Media Type");
  });

  webServer.on("/api/scale/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    if (request->contentType() == "application/json") {
      request->send(200, "application/json", LevelManagers[scale-1]->getJsonConfig());
    } else request->send(415, "text/plain", "Unsupported Media Type");
  });

  webServer.on("/api/scale/config", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {

    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    bool success = LevelManagers[scale-1]->putJsonConfig(String((const char*)data));
    if (success) request->send(200, "application/json", "{\"message\":\"Loaded new scale config!\"}");
    else return request->send(422, "application/json", "{\"message\":\"Invalid data or unable to write to NVS\"}");
  });

  webServer.on("/api/calibrate/empty", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    // Calibration - empty scale
    LevelManagers[scale-1]->emptyScale();
    request->send(200, "application/json", "{\"message\":\"Empty scale calibration done!\"}");
  });

  webServer.on("/api/calibrate/weight", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    // Calibration - reference weight
    DynamicJsonDocument jsonBuffer(128);
    deserializeJson(jsonBuffer, (const char*)data);
    if (!jsonBuffer["weight"].is<int>()) return request->send(422, "application/json", "{\"message\":\"Invalid data\"}");
    LevelManagers[scale-1]->applyCalibrateWeight(jsonBuffer["weight"].as<int>());
    request->send(200, "application/json", "{\"message\":\"Setup completed\"}");
  });

  webServer.on("/api/calibrate/bottleweight", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    // Calibration - reference weight
    DynamicJsonDocument jsonBuffer(128);
    deserializeJson(jsonBuffer, (const char*)data);

    if (!jsonBuffer["emptyWeightGramms"].is<int>() || !jsonBuffer["fullWeightGramms"].is<int>()) {
      return request->send(422, "application/json", "{\"message\":\"Invalid data\"}");
    }

    int empty = jsonBuffer["emptyWeightGramms"].as<int>();
    int full = jsonBuffer["fullWeightGramms"].as<int>();
    if (empty <= 0) return request->send(422, "application/json", "{\"message\":\"Invalid data: emptyWeightGramms\"}");
    else if (full <= 0 ) return request->send(422, "application/json", "{\"message\":\"Invalid data: fullWeightGramms\"}");
    else {
      if (LevelManagers[scale-1]->setBottleWeight(empty, full)) {
        request->send(200, "application/json", "{\"message\":\"New bottle weight configured\"}");
      } else {
        request->send(500, "application/json", "{\"message\":\"Error while writing configuration to NVS.\"}");
      }
    }
  });

  webServer.on("/api/calibrate/bottleweight", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->hasParam("scale")) return request->send(400, "application/json", "{\"message\":\"Missing parameter scale\"}");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "application/json", "{\"message\":\"Bad request, value outside available scales\"}");

    String output;
    DynamicJsonDocument doc(256);

    doc["emptyWeightGramms"] = LevelManagers[scale-1]->getBottleEmptyWeight();
    doc["fullWeightGramms"] = LevelManagers[scale-1]->getBottleFullWeight();

    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/level/current/all", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    DynamicJsonDocument jsonDoc(1024);

    for (uint8_t i=0; i < LEVELMANAGERS; i++) {
        jsonDoc[i]["id"] = i;
        jsonDoc[i]["level"] = LevelManagers[i]->getLevel();
        jsonDoc[i]["gasWeight"] = LevelManagers[i]->getGasWeight();
        jsonDoc[i]["sensorValue"] = LevelManagers[i]->getLastMedian();
    }

    serializeJson(jsonDoc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/level/num", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    DynamicJsonDocument json(256);
    json["num"] = LEVELMANAGERS;
    serializeJson(json, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/partition/switch", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    auto next = esp_ota_get_next_update_partition(NULL);
    auto error = esp_ota_set_boot_partition(next);
    if (error == ESP_OK) {
      request->send(200, "application/json", "{\"message\":\"New partition ready for boot\"}");
    } else {
      request->send(500, "application/json", "{\"message\":\"Error switching boot partition\"}");
    }
  });

  webServer.on("/api/esp", HTTP_GET, [&](AsyncWebServerRequest * request) {
    String output;
    DynamicJsonDocument json(2048);

    JsonObject booting = json.createNestedObject("booting");
    booting["rebootReason"] = esp_reset_reason();
    booting["partitionCount"] = esp_ota_get_app_partition_count();

    auto partition = esp_ota_get_boot_partition();
    JsonObject bootPartition = json.createNestedObject("bootPartition");
    bootPartition["address"] = partition->address;
    bootPartition["size"] = partition->size;
    bootPartition["label"] = partition->label;
    bootPartition["encrypted"] = partition->encrypted;
    switch (partition->type) {
      case ESP_PARTITION_TYPE_APP:  bootPartition["type"] = "app"; break;
      case ESP_PARTITION_TYPE_DATA: bootPartition["type"] = "data"; break;
      default: bootPartition["type"] = "any";
    }
    bootPartition["subtype"] = partition->subtype;

    partition = esp_ota_get_running_partition();
    JsonObject runningPartition = json.createNestedObject("runningPartition");
    runningPartition["address"] = partition->address;
    runningPartition["size"] = partition->size;
    runningPartition["label"] = partition->label;
    runningPartition["encrypted"] = partition->encrypted;
    switch (partition->type) {
      case ESP_PARTITION_TYPE_APP:  runningPartition["type"] = "app"; break;
      case ESP_PARTITION_TYPE_DATA: runningPartition["type"] = "data"; break;
      default: runningPartition["type"] = "any";
    }
    runningPartition["subtype"] = partition->subtype;

    JsonObject build = json.createNestedObject("build");
    build["date"] = __DATE__;
    build["time"] = __TIME__;

    JsonObject ram = json.createNestedObject("ram");
    ram["heapSize"] = ESP.getHeapSize();
    ram["freeHeap"] = ESP.getFreeHeap();
    ram["usagePercent"] = (float)ESP.getFreeHeap() / (float)ESP.getHeapSize() * 100.f;
    ram["minFreeHeap"] = ESP.getMinFreeHeap();
    ram["maxAllocHeap"] = ESP.getMaxAllocHeap();

    JsonObject spi = json.createNestedObject("spi");
    spi["psramSize"] = ESP.getPsramSize();
    spi["freePsram"] = ESP.getFreePsram();
    spi["minFreePsram"] = ESP.getMinFreePsram();
    spi["maxAllocPsram"] = ESP.getMaxAllocPsram();

    JsonObject chip = json.createNestedObject("chip");
    chip["revision"] = ESP.getChipRevision();
    chip["model"] = ESP.getChipModel();
    chip["cores"] = ESP.getChipCores();
    chip["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    chip["cycleCount"] = ESP.getCycleCount();
    chip["sdkVersion"] = ESP.getSdkVersion();
    chip["efuseMac"] = ESP.getEfuseMac();
    chip["temperature"] = (temprature_sens_read() - 32) / 1.8;

    JsonObject flash = json.createNestedObject("flash");
    flash["flashChipSize"] = ESP.getFlashChipSize();
    flash["flashChipRealSize"] = spi_flash_get_chip_size();
    flash["flashChipSpeedMHz"] = ESP.getFlashChipSpeed() / 1000000;
    flash["flashChipMode"] = ESP.getFlashChipMode();
    flash["sdkVersion"] = ESP.getFlashChipSize();

    JsonObject sketch = json.createNestedObject("sketch");
    sketch["size"] = ESP.getSketchSize();
    sketch["maxSize"] = ESP.getFreeSketchSpace();
    sketch["usagePercent"] = (float)ESP.getSketchSize() / (float)ESP.getFreeSketchSpace() * 100.f;
    sketch["md5"] = ESP.getSketchMD5();

    JsonObject fs = json.createNestedObject("filesystem");
    fs["type"] = F("LittleFS");
    fs["totalBytes"] = LittleFS.totalBytes();
    fs["usedBytes"] = LittleFS.usedBytes();
    fs["usagePercent"] = (float)LittleFS.usedBytes() / (float)LittleFS.totalBytes() * 100.f;
    
    serializeJson(json, output);
    request->send(200, "application/json", output);
  });

  File tmp = LittleFS.open("/index.html");
  time_t cr = tmp.getLastWrite();
  tmp.close();
  struct tm * timeinfo = gmtime(&cr);

  webServer.serveStatic("/", LittleFS, "/")
    .setCacheControl("max-age=86400")
    .setLastModified(timeinfo)
    .setDefaultFile("index.html");


  webServer.onNotFound([&](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html");
      response->setCode(200);
      request->send(response);
      /*    
      if (request->contentType() == "application/json") {
        request->send(404, "application/json", "{\"message\":\"Not found\"}");
      } else request->send(404, "text/plain", "Not found");
      */
    }
  });
}
