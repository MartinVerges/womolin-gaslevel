/**
 *
 * Gas Scale
 * https://github.com/MartinVerges/rv-smart-gas-scale
 *
 * (c) 2022 Martin Verges
 *
**/

#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LITTLEFS.h>
#include "ble.h"

extern bool enableWifi;
extern bool enableBle;
extern bool enableMqtt;
extern bool enableDac;

void APIRegisterRoutes() {
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
    if (preferences.putBool("enableBle", jsonBuffer["enableble"].as<boolean>())) {
      if (enableBle) stopBleServer();
      enableBle = jsonBuffer["enableble"].as<boolean>();
      if (enableBle) createBleServer(hostName);
      yield();
    }
    if (preferences.putBool("enableWifi", jsonBuffer["enablewifi"].as<boolean>())) {
      enableWifi = jsonBuffer["enablewifi"].as<boolean>();
    }
    if (preferences.putBool("enableDac", jsonBuffer["enabledac"].as<boolean>())) {
      enableDac = jsonBuffer["enabledac"].as<boolean>();
    }

    // MQTT Settings
    preferences.putUInt("mqttPort", jsonBuffer["mqttport"].as<uint16_t>());
    preferences.putString("mqttHost", jsonBuffer["mqtthost"].as<String>());
    preferences.putString("mqttTopic", jsonBuffer["mqtttopic"].as<String>());
    preferences.putString("mqttUser", jsonBuffer["mqttuser"].as<String>());
    preferences.putString("mqttPass", jsonBuffer["mqttpass"].as<String>());
    if (preferences.putBool("enableMqtt", jsonBuffer["enablemqtt"].as<boolean>())) {
      if (enableMqtt) Mqtt.disconnect(true);
      enableMqtt = jsonBuffer["enablemqtt"].as<boolean>();
      if (enableMqtt) {
        Mqtt.prepare();
        Mqtt.connect();
      }
    }
    
    request->send(200, "application/json", "{\"message\":\"New hostname stored in NVS, reboot required!\"}");
  });

  webServer.on("/api/setup/empty", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    if (!request->hasParam("scale")) return request->send(400, "text/plain", "Missing parameter scale");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "text/plain", "Bad request, value outside available scales");

    // Calibration - empty scale
    LevelManagers[scale-1]->emptyScale();
    request->send(200, "application/json", "{\"message\":\"Empty scale calibration done!\"}");
  });

  webServer.on("/api/setup/weight", HTTP_POST, [&](AsyncWebServerRequest * request){}, NULL,
    [&](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
      
    if (!request->hasParam("scale")) return request->send(400, "text/plain", "Missing parameter scale");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "text/plain", "Bad request, value outside available scales");

    // Calibration - reference weight
    DynamicJsonDocument jsonBuffer(64);
    deserializeJson(jsonBuffer, (const char*)data);
    if (!jsonBuffer["weight"].is<int>()) return request->send(422, "text/plain", "Invalid data");
    LevelManagers[scale-1]->setupWeight(jsonBuffer["weight"]);
    request->send(200, "application/json", "{\"message\":\"Setup completed\"}");
  });

  webServer.on("/api/setup/empty", HTTP_POST, [&](AsyncWebServerRequest *request) {
    if (!request->hasParam("scale")) return request->send(400, "text/plain", "Missing parameter scale");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "text/plain", "Bad request, value outside available scales");

    // Calibration - empty scale
    LevelManagers[scale-1]->emptyScale();
    request->send(200, "application/json", "{\"message\":\"Begin calibration success\"}");
  });

  webServer.on("/api/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<1024> doc;

      doc["hostname"] = hostName;
      doc["enablewifi"] = enableWifi;
      doc["enableble"] = enableBle;
      doc["enabledac"] = enableDac;

      // MQTT
      doc["enablemqtt"] = enableMqtt;
      doc["mqttport"] = preferences.getUInt("mqttPort", 1883);
      doc["mqtthost"] = preferences.getString("mqttHost", "");
      doc["mqtttopic"] = preferences.getString("mqttTopic", "");
      doc["mqttuser"] = preferences.getString("mqttUser", "");
      doc["mqttpass"] = preferences.getString("mqttPass", "");

      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(415, "text/plain", "Unsupported Media Type");
  });

  webServer.on("/api/rawvalue", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->hasParam("scale")) return request->send(400, "text/plain", "Missing parameter scale");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "text/plain", "Bad request, value outside available scales");

    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<16> doc;
      doc["raw"] = LevelManagers[scale-1]->getSensorMedianValue(true);
      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(200, "text/plain", (String)LevelManagers[scale-1]->getSensorMedianValue(true));
  });

  webServer.on("/api/level/current/all", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    StaticJsonDocument<512> doc;
    for (uint8_t i=0; i < LEVELMANAGERS; i++) {
      doc[String("level")+String(i)] = LevelManagers[i]->getCalculatedPercentage(true);
    }
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });

  webServer.on("/api/level/current", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if (!request->hasParam("scale")) return request->send(400, "text/plain", "Missing parameter scale");    
    uint8_t scale = request->getParam("scale")->value().toInt();
    if (scale > LEVELMANAGERS or scale < 1) return request->send(400, "text/plain", "Bad request, value outside available scales");

    if (request->contentType() == "application/json") {
      String output;
      StaticJsonDocument<16> doc;
      doc["levelPercent"] = LevelManagers[scale-1]->getCalculatedPercentage(true);
      serializeJson(doc, output);
      request->send(200, "application/json", output);
    } else request->send(200, "text/plain", (String)LevelManagers[scale-1]->getCalculatedPercentage(true));
  });

  webServer.on("/api/esp/heap", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  webServer.on("/api/esp/cores", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getChipCores()));
  });

  webServer.on("/api/esp/freq", HTTP_GET, [&](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getCpuFreqMHz()));
  });

  webServer.on("/api/num/levels", HTTP_GET, [&](AsyncWebServerRequest *request) {
    String output;
    DynamicJsonDocument json(256);
    json["num"] = LEVELMANAGERS;
    serializeJson(json, output);
    request->send(200, "application/json", output);
  });

  webServer.serveStatic("/", LITTLEFS, "/").setDefaultFile("index.html");
  
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("[WEB] Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
  });
  webServer.addHandler(&events);

  webServer.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      if (request->contentType() == "application/json") {
        request->send(404, "application/json", "{\"message\":\"Not found\"}");
      } else request->send(404, "text/plain", "Not found");
    }
  });
}
