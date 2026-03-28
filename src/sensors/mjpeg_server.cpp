#include "mjpeg_server.h"

MjpegServer::MjpegServer(uint16_t port) : _server(port) {
    _minFrameInterval = 100;      // Default: 10 FPS (100ms between frames)
    _rateLimitEnabled = true;
}

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
<img src="/mjpeg"><br><small>Stream active (10 FPS max)</small></body></html>
        )=====";
        _server.send(200, "text/html", html);
    });

    // MJPEG stream with rate limiting
    _server.on("/mjpeg", HTTP_GET, [this]() {
        WiFiClient client = _server.client();
        
        // Optimize client for stability
        client.setNoDelay(true);
        client.setTimeout(5000);
        
        // Send headers
        client.write("HTTP/1.1 200 OK\r\n"
                     "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                     "Cache-Control: no-cache\r\n"
                     "Connection: close\r\n\r\n");

        Serial.println("[MJPEG] New client connected - Rate limited to 10 FPS");

        // Create dedicated task for this client
        xTaskCreatePinnedToCore(
            streamTask,
            "MJPEG_Task",
            4096,
            new WiFiClient(client),
            1,      // Lower priority to give WiFi/MQTT more time
            nullptr,
            1       // Run on Core 1 (Core 0 for WiFi/MQTT)
        );
    });

    _server.begin();
    Serial.printf("[MJPEG] Stream ready → http://%s/ (Rate limited to %d FPS)\n", 
                  WiFi.localIP().toString().c_str(), 
                  1000 / _minFrameInterval);
    return true;
}

void MjpegServer::handleClient() {
    _server.handleClient();  // Only accepts new connections - fast and non-blocking
}

void MjpegServer::setMaxFPS(uint8_t fps) {
    if (fps > 0 && fps <= 30) {
        _minFrameInterval = 1000 / fps;
        Serial.printf("[MJPEG] Max FPS set to %d (interval: %d ms)\n", fps, _minFrameInterval);
    }
}

// Runs in its own task with rate limiting
void MjpegServer::streamTask(void* pvParameters) {
    WiFiClient* client = static_cast<WiFiClient*>(pvParameters);
    unsigned long lastFrameTime = 0;
    int frameCount = 0;
    unsigned long startTime = millis();
    int consecutiveErrors = 0;
    
    Serial.println("[MJPEG] Stream task started for client");
    
    while (client->connected()) {
        unsigned long now = millis();
        
        // RATE LIMITING - Critical for ESP32 stability
        if (now - lastFrameTime >= 100) {  // 10 FPS max
            camera_fb_t* fb = esp_camera_fb_get();
            
            if (!fb) {
                consecutiveErrors++;
                if (consecutiveErrors > 10) {
                    Serial.println("[MJPEG] Too many frame errors, disconnecting client");
                    break;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            
            consecutiveErrors = 0;
            
            // Send frame header
            if (!client->connected()) {
                esp_camera_fb_return(fb);
                break;
            }
            
            int written = client->printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
            if (written <= 0) {
                esp_camera_fb_return(fb);
                break;
            }
            
            // Send frame data in chunks to prevent blocking
            size_t remaining = fb->len;
            size_t offset = 0;
            const size_t CHUNK_SIZE = 1024;  // Send in 1KB chunks
            
            while (remaining > 0 && client->connected()) {
                size_t toSend = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
                written = client->write(fb->buf + offset, toSend);
                if (written <= 0) {
                    break;
                }
                offset += written;
                remaining -= written;
                vTaskDelay(1 / portTICK_PERIOD_MS);  // Small delay between chunks
            }
            
            if (client->connected()) {
                client->write("\r\n", 2);
            }
            
            esp_camera_fb_return(fb);
            
            lastFrameTime = now;
            frameCount++;
            
            // Log frame rate occasionally
            if (frameCount >= 30) {
                unsigned long elapsed = millis() - startTime;
                if (elapsed > 0) {
                    float fps = (frameCount * 1000.0) / elapsed;
                    if (fps > 15) {
                        Serial.printf("[MJPEG] WARNING: High FPS: %.1f (may cause instability)\n", fps);
                    }
                }
                frameCount = 0;
                startTime = millis();
            }
            
            // Give other tasks time to run
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } else {
            // Not time to send frame, yield CPU
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }

    client->stop();
    delete client;
    Serial.println("[MJPEG] Client disconnected");
    vTaskDelete(NULL);
}