// mjpeg_server.cpp
#include "mjpeg_server.h"

MjpegServer::MjpegServer(uint16_t port) : _server(port) {}

bool MjpegServer::begin() {
    if (!esp_camera_sensor_get()) {
        Serial.println("[MJPEG] Camera not initialized!");
        return false;
    }

    // Root page
    _server.on("/", HTTP_GET, [this]() {
        String html = R"=====(
<!DOCTYPE html><html><head><title>ESP32-CAM</title>
<style>body{margin:0;background:#111;color:#fff;text-align:center;font-family:Arial;}
img{max-width:100%;height:auto;}</style></head>
<body><h1>ESP32-CAM Live Stream</h1>
<img src="/mjpeg"><br><small>Stream active</small></body></html>
        )=====";
        _server.send(200, "text/html", html);
    });

    // MJPEG stream — launches a task per client
    _server.on("/mjpeg", HTTP_GET, [this]() {
        WiFiClient client = _server.client();
        client.write("HTTP/1.1 200 OK\r\n"
                     "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Connection: close\r\n\r\n");

        // Create a dedicated task for this client — never blocks main loop!
        xTaskCreatePinnedToCore(
            streamTask,
            "MJPEG_Task",
            4096,
            new WiFiClient(client),  // heap allocated copy
            1,
            nullptr,
            1  // Run on Core 1 → Core 0 free for WiFi + MQTT
        );
    });

    _server.begin();
    Serial.printf("[MJPEG] Stream ready → http://%s/\n", WiFi.localIP().toString().c_str());
    return true;
}

void MjpegServer::handleClient() {
    _server.handleClient();  // Only accepts new connections — super fast
}

// Runs in its own task — never blocks anything
void MjpegServer::streamTask(void* pvParameters) {
    WiFiClient* client = static_cast<WiFiClient*>(pvParameters);

    while (client->connected()) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            delay(10);
            continue;
        }

        client->printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client->write(fb->buf, fb->len);
        client->write("\r\n", 2);

        esp_camera_fb_return(fb);
        vTaskDelay(1);  // Let MQTT & WiFi breathe
    }

    client->stop();
    delete client;
    Serial.println("[MJPEG] Client disconnected");
    vTaskDelete(NULL);
}