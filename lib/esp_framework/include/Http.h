// Http.h

#ifndef _HTTP_h
#define _HTTP_h

#include "Arduino.h"
#include "Common.h"

class Http
{
private:
    static bool isBegin;
    static void handleRoot();
#ifndef DISABLE_MQTT
    static void handleMqtt();
#ifndef DISABLE_MQTT_DISCOVERY
    static void handleDiscovery();
#endif
#endif
    static void handleHttp();
    static void handledhcp();
    static void handleScanWifi();
    static void handleWifi();
    static void handleOperate();
    static void handleNotFound();
    static void handleModuleSetting();
    static void handleOTA();
    static void handleGetStatus();
    static void handleUpdate();
    static void handleUpdateUpload();
#ifdef USE_UFILESYS
    static void handleFileDo();
    static void handleUploadFile();
    static void handleUploadFileUpload();
#endif
    static bool checkAuth();

public:
    static WebServer *server;
    static void init();
    static void stop();
    static void loop();
    static bool callModule(uint8_t function);
    static bool captivePortal();

    static void OTA(String url);
};

#endif
