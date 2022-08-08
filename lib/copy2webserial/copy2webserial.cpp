/**
 * @file copy2webserial.cpp
 * @author Martin Verges <martin@veges.cc>
 * @version 0.1
 * @date 2022-08-07
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#include <copy2webserial.h>

void SerialCopyBgTask(void* param) {
  yield();
  delay(1000); // wait a short time until everything is setup before executing the loop forever
  yield();

  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  Copy2WebSerialClass * copy2WebSerial = (Copy2WebSerialClass *)param;

  for(;;) {
    yield();
    if (Serial.available()) {
      copy2WebSerial->write(Serial.read());
    }
    yield();
    vTaskDelay(xDelay);
  }
}

Copy2WebSerialClass::Copy2WebSerialClass() {
  xTaskCreate(
    SerialCopyBgTask,
    "serialCopy",
    4000,           // Stack size in words
    this,           // Task input parameter
    0,              // Priority of the task
    &bgTaskHandle   // Task handle.
  );
}

Copy2WebSerialClass::~Copy2WebSerialClass() {
  vTaskDelete(bgTaskHandle);
}

void Copy2WebSerialClass::begin(AsyncWebServer *server, const char* url) {
  webServer = server;

  webSocket = new AsyncWebSocket("/api/webserial");
  webSocket->onEvent([&](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) -> void {
    if(type == WS_EVT_CONNECT){
      #if defined(DEBUG)
        Serial.println(F("[WEBSERIAL] Client connection received"));
      #endif
    } else if(type == WS_EVT_DISCONNECT){
      #if defined(DEBUG)
        Serial.println(F("[WEBSERIAL] Client disconnected"));
      #endif
    } else if(type == WS_EVT_DATA){
      #if defined(DEBUG)
        Serial.println(F("[WEBSERIAL] Received Websocket Data"));
      #endif
    }
  });
  webServer->addHandler(webSocket);

  #if defined(WEBSERIAL_DEBUG)
    Serial.println(F("[WEBSERIAL] Attached AsyncWebServer along with Websockets"));
  #endif
}

void Copy2WebSerialClass::write(int c) {
  if (webServer != nullptr) webSocket->textAll(String(c));
}
