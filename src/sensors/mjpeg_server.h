// mjpeg_server.h
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"

class MjpegServer {
private:
    WebServer _server;

    static void streamTask(void* pvParameters);

public:
    MjpegServer(uint16_t port = 80);
    bool begin();
    void handleClient();
};