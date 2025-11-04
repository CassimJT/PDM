#include "rtp_server.h"

Rtp_server::Rtp_server(/* args */){}

Rtp_server::~Rtp_server(){}

//camera initializations
void Rtp_server::initialiseCamera(){
    //...
    configCamera();
}
//Camera configarations
void Rtp_server::configCamera() {
    //...
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 13;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
    }
}
//Start the camera
void Rtp_server::startCameraServer(){
    //..
    rtspServer.setCredentials(rtspUser, rtspPassword);
    if (rtspServer.init(RTSPServer::VIDEO_ONLY, 8554)) {
        Serial.println("RTSP server started successfully");
    } else {
        Serial.println("Failed to start RTSP server");
    }
    
}
// get frame quality
void Rtp_server::getFrameQuality() {
    //...
    sensor_t * s = esp_camera_sensor_get(); 
    quality = s->status.quality; 
    Serial.printf("Camera Quality is: %d\n", quality);
}
void Rtp_server::sendVideo(void* pvParameters) {
    while (true) { 
    // Send frame via RTP
    if(rtspServer.readyToSendFrame()) { 
      camera_fb_t* fb = esp_camera_fb_get();
      rtspServer.sendRTSPFrame(fb->buf, fb->len, quality, fb->width, fb->height);
      esp_camera_fb_return(fb);
    }
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}