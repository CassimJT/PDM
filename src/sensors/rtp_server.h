#ifndef RTP_SERVER_H
#define RTP_SERVER_H

#include <ESP32-RTSPServer.h>
#include <Arduino.h>
#include "WiFi.h"
#include "esp_camera.h"
#include "esp_http_server.h"

class Rtp_server {
public:
    Rtp_server();
    ~Rtp_server();

    void initialiseCamera();
    void startCameraServer();
    void getFrameQuality();
    void sendVideo(void* pvParameters);

private:
    void configCamera();

    // RTSP members
    const char *rtspUser = "";
    const char *rtspPassword = "";
    RTSPServer rtspServer;
    TaskHandle_t videoTaskHandle = NULL; 
    int quality;

    // GPIO config (all private, fixed)
    static const int XCLK_GPIO_NUM  = 21;
    static const int SIOD_GPIO_NUM  = 26;
    static const int SIOC_GPIO_NUM  = 27;

    static const int Y9_GPIO_NUM    = 35;
    static const int Y8_GPIO_NUM    = 34;
    static const int Y7_GPIO_NUM    = 39;
    static const int Y6_GPIO_NUM    = 36;
    static const int Y5_GPIO_NUM    = 19;
    static const int Y4_GPIO_NUM    = 18;
    static const int Y3_GPIO_NUM    = 5;
    static const int Y2_GPIO_NUM    = 4;

    static const int VSYNC_GPIO_NUM = 25;
    static const int HREF_GPIO_NUM  = 23;
    static const int PCLK_GPIO_NUM  = 22;

    static const int PWDN_GPIO_NUM  = -1;
    static const int RESET_GPIO_NUM = -1;
};

#endif
