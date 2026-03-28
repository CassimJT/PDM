#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

class MjpegServer {
private:
    WebServer _server;
    unsigned long _minFrameInterval;  // Minimum time between frames (ms)
    bool _rateLimitEnabled;

    static void streamTask(void* pvParameters);

public:
    MjpegServer(uint16_t port = 80);
    bool begin();
    void handleClient();
    void setMaxFPS(uint8_t fps);
};