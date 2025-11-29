#include <Arduino.h>
#include "utills/util.h"
#include "sensors/dhtSensor.h"
#include "sensors/camera_manager.h"
#include "sensors/mjpeg_server.h"
#include <HardwareSerial.h>

// --------------------------------------------------
// Pin definitions
#define RED     0
#define GREEN   15
#define BLUE    12
#define TX_PIN  1
#define RX_PIN  3
#define DHT_PIN 32
#define DHT_TYPE DHT11

// --------------------------------------------------
// Global objects
HardwareSerial slaveSerial(2);   // UART2 to your Arduino slave
Util           util;
DHTSensor      dht;
CameraManager  camera;
MjpegServer    webServer(80);    // ← change to 81 if port 80 is blocked

// --------------------------------------------------
// Timing
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 5000;   // 5 seconds

// --------------------------------------------------
void setup() {
    Serial.begin(115200);

    // LED pins
    pinMode(RED,   OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE,  OUTPUT);

    // --------------------------------------------------
    // 1. Connect to WiFi
    util.connectToWifi(BLUE, RED, GREEN);

    // --------------------------------------------------
    // 2. Start camera
    if (!camera.begin()) {
        Serial.println("Camera init FAILED!");
        while (true) {
            digitalWrite(RED, HIGH);
            delay(200);
            digitalWrite(RED, LOW);
            delay(200);
        }
    }
    Serial.println("Camera OK");

    // --------------------------------------------------
    // 3. Start MJPEG web server
    webServer.begin();                     // This prints the URL
    Serial.println("Web server started");

    // --------------------------------------------------
    // 4. UART to Arduino slave
    slaveSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    util.attachSerial(slaveSerial);

    // --------------------------------------------------
    // 5. DHT11 sensor
    pinMode(DHT_PIN, INPUT);
    dht.initDHT(DHT_PIN, DHT_TYPE);
    delay(2000);

    Serial.println("=== ESP32-CAM MASTER READY ===");
    Serial.print("Video stream → http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
}

void loop() {
    // --------------------------------------------------
    // 1. Handle MJPEG clients (non-blocking, very fast)
    webServer.handleClient();

    // --------------------------------------------------
    // 2. Read DHT11 every 5 seconds and publish via MQTT
    if (millis() - lastDHTRead >= DHT_INTERVAL) {
        dht.updateDht();
        float temp = dht.getTemp();
        float hum  = dht.getHumidity();

        if (!isnan(temp) && !isnan(hum)) {
            util.publisheDHTReadings(temp, hum);
        } else {
            Serial.println("DHT read failed");
        }
        lastDHTRead = millis();
    }

    // --------------------------------------------------
    // 3. Keep MQTT alive
    if (util.getMqttp().connected()) {
        util.getMqttp().loop();
    }

  
}