/**
 * @file copy2webserial.h
 * @author Martin Verges <martin@veges.cc>
 * @version 0.1
 * @date 2022-08-07
 * 
 * @copyright Copyright (c) 2022 by the author alone
 *            https://gitlab.womolin.de/martin.verges/gaslevel
 * 
 * License: CC BY-NC-SA 4.0
 */

#ifndef Copy2WebSerial_h
#define Copy2WebSerial_h

#include <ESPAsyncWebServer.h>

void SerialCopyBgTask(void* param);

class Copy2WebSerialClass {
    public:
        Copy2WebSerialClass();
        virtual ~Copy2WebSerialClass();

        void begin(AsyncWebServer *server, const char* url = "/api/webserial");
        void write(int c);

    private:
        AsyncWebSocket * webSocket;
        AsyncWebServer * webServer = nullptr;
        TaskHandle_t bgTaskHandle;
};

#endif // Copy2WebSerial_h
