/**
 * @file api-routes.h
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
    StaticJsonDocument<16> doc;
    doc["partition_type"] = data->type;
    doc["partition_subtype"] = data->subtype;
    doc["address"] = data->address;
    doc["size"] = data->size;
    doc["label"] = data->label;
    doc["encrypted"] = data->encrypted;
    serializeJson(doc, output);
    request->send(500, "application/json", output);
  });

  webServer.on("/api/update/upload", HTTP_POST,
    [&](AsyncWebServerRequest *request) { },
    [&](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {

    String otaPassword = "";
    if (preferences.begin(NVS_NAMESPACE, true)) {
      otaPassword = preferences.getString("otaPassword");
      preferences.end();

      if (otaPassword.length()) {
        if(!request->authenticate("ota", otaPassword.c_str())) {
          return request->send(401, "application/json", "{\"message\":\"Invalid OTA password provided!\"}");
        }
      } else Serial.println(F("[OTA] No password configured, no authentication requested!"));
    } else Serial.println(F("[OTA] Unable to load password from NVS."));

    if (!index) {
      Serial.print(F("[OTA] Begin firmware update with filename: "));
      Serial.println(filename);
      // if filename includes spiffs|littlefs, update the spiffs|littlefs partition
      int cmd = (filename.indexOf("spiffs") > -1 || filename.indexOf("littlefs") > -1) ? U_SPIFFS : U_FLASH;
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
        Serial.print(F("[OTA] Error: "));
        Update.printError(Serial);
        request->send(500, "application/json", "{\"message\":\"Unable to begin firmware update!\"}");
      }
    }

    if (Update.write(data, len) != len) {
      Serial.print(F("[OTA] Error: "));
      Update.printError(Serial);
      request->send(500, "application/json", "{\"message\":\"Unable to write firmware update data!\"}");
    }

    if (final) {
      if (!Update.end(true)) {
        String output;
        StaticJsonDocument<16> doc;
        doc["message"] = "Update error";
        doc["error"] = Update.errorString();
        serializeJson(doc, output);
        request->send(500, "application/json", output);

        Serial.println("[OTA] Error when calling calling Update.end().");
        Update.printError(Serial);
      } else {
        Serial.println("[OTA] Firmware update successful.");
        request->send(200, "application/json", "{\"message\":\"Please wait while the device reboots!\"}");
        yield();
        delay(250);

        Serial.println("[OTA] Update complete, rebooting now!");
        Serial.flush();
        ESP.restart();
      }
    }
  });

  events.onConnect([&](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
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
        preferences.putString("hostName", hostname);
      }

      if (preferences.putBool("enableWifi", jsonBuffer["enablewifi"].as<boolean>())) {
        enableWifi = jsonBuffer["enablewifi"].as<boolean>();
      }
      if (preferences.putBool("enableSoftAp", jsonBuffer["enablesoftap"].as<boolean>())) {
        WifiManager.fallbackToSoftAp(jsonBuffer["enablesoftap"].as<boolean>());
      }

      if (preferences.putBool("enableBle", jsonBuffer["enableble"].as<boolean>())) {
        if (enableBle) stopBleServer();
        enableBle = jsonBuffer["enableble"].as<boolean>();
        if (enableBle) createBleServer(hostName);
        yield();
      }
      if (preferences.putBool("enableDac", jsonBuffer["enabledac"].as<boolean>())) {
        enableDac = jsonBuffer["enabledac"].as<boolean>();
      }

      preferences.putString("otaPassword", jsonBuffer["otaPassword"].as<String>());

      // MQTT Settings
      preferences.putUInt("mqttPort", jsonBuffer["mqttport"].as<uint16_t>());
      preferences.putString("mqttHost", jsonBuffer["mqtthost"].as<String>());
      preferences.putString("mqttTopic", jsonBuffer["mqtttopic"].as<String>());
      preferences.putString("mqttUser", jsonBuffer["mqttuser"].as<String>());
      preferences.putString("mqttPass", jsonBuffer["mqttpass"].as<String>());
      if (preferences.putBool("enableMqtt", jsonBuffer["enablemqtt"].as<boolean>())) {
        if (enableMqtt) Mqtt.disconnect();
        enableMqtt = jsonBuffer["enablemqtt"].as<boolean>();
        if (enableMqtt) {
          Mqtt.prepare(
            jsonBuffer["mqtthost"].as<String>(),
            jsonBuffer["mqttport"].as<uint16_t>(),
            jsonBuffer["mqtttopic"].as<String>(),
            jsonBuffer["mqttuser"].as<String>(),
            jsonBuffer["mqttpass"].as<String>()
          );
          Mqtt.connect();
        }
      }
    }
    preferences.end();

    request->send(200, "application/json", "{\"message\":\"New hostname stored in NVS, reboot required!\"}");
  });

  webServer.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<1024> doc;

      if (preferences.begin(NVS_NAMESPACE, true)) {
        doc["hostname"] = hostName;
        doc["enablewifi"] = enableWifi;
        doc["enablesoftap"] = WifiManager.getFallbackState();
        doc["enableble"] = enableBle;
        doc["enabledac"] = enableDac;

        doc["otaPassword"] = preferences.getString("otaPassword");

        // MQTT
        doc["enablemqtt"] = enableMqtt;
        doc["mqttport"] = preferences.getUInt("mqttPort", 1883);
        doc["mqtthost"] = preferences.getString("mqttHost", "");
        doc["mqtttopic"] = preferences.getString("mqttTopic", "");
        doc["mqttuser"] = preferences.getString("mqttUser", "");
        doc["mqttpass"] = preferences.getString("mqttPass", "");
      }
      preferences.end();

      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(415, "text/plain", "Unsupported Media Type");
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
    DynamicJsonDocument jsonBuffer(64);
    deserializeJson(jsonBuffer, (const char*)data);
    if (!jsonBuffer["weight"].is<int>()) return request->send(422, "application/json", "{\"message\":\"Invalid data\"}");
    LevelManagers[scale-1]->applyCalibrateWeight(jsonBuffer["weight"]);
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
    StaticJsonDocument<256> doc;

    doc["emptyWeightGramms"] = LevelManagers[scale-1]->getBottleEmptyWeight();
    doc["fullWeightGramms"] = LevelManagers[scale-1]->getBottleFullWeight();

    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/level/current/all", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    StaticJsonDocument<512> doc;
    JsonArray array = doc.to<JsonArray>();

    for (uint8_t i=0; i < LEVELMANAGERS; i++) {
      array.add(LevelManagers[i]->getCalculatedPercentage(true));
    }
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/level/num", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    DynamicJsonDocument json(256);
    json["num"] = LEVELMANAGERS;
    serializeJson(json, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/esp", HTTP_GET, [&](AsyncWebServerRequest * request) {
    String output;
    DynamicJsonDocument json(2048);

    json["rebootReason"] = esp_reset_reason();

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
//  char buffer [80];
//  strftime(buffer, 80, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);

  webServer.serveStatic("/", LittleFS, "/")
    .setCacheControl("max-age=86400")
    .setLastModified(timeinfo)
    .setDefaultFile("index.html");

  webServer.onNotFound([&](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      if (request->contentType() == "application/json") {
        request->send(404, "application/json", "{\"message\":\"Not found\"}");
      } else request->send(404, "text/plain", "Not found");
    }
  });
}