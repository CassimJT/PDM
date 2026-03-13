#include <Arduino.h>
#include "utills/util.h"
#include "sensors/dhtSensor.h"
#include "sensors/camera_manager.h"
#include "sensors/mjpeg_server.h"
#include <HardwareSerial.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

#define RED     0
#define GREEN   15
#define BLUE    12
#define TX_PIN  1
#define RX_PIN  3
#define DHT_PIN 32
#define DHT_TYPE DHT11
#define BUZZER_PIN 14
#define IR_PIN   33  

#define IR_BUTTON_1 0xFF30CF
#define IR_BUTTON_0 0xFF6897

HardwareSerial slaveSerial(2);  
Util           util;
DHTSensor      dht;
CameraManager  camera;
MjpegServer    webServer(80);    

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 5000;   
unsigned long lastIRPress = 0;
const unsigned long IR_DEBOUNCE = 300;
bool systemRunning = false;  
bool buzzerState = false;

IRrecv irReceiver(IR_PIN);
decode_results irResults;

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RED,   OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE,  OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(BLUE, LOW);

    util.connectToWifi(BLUE, RED, GREEN);

    if (WiFi.status() == WL_CONNECTED) {
        camera.begin();
        webServer.begin();
        slaveSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
        util.attachSerial(slaveSerial);
        pinMode(DHT_PIN, INPUT);
        dht.initDHT(DHT_PIN, DHT_TYPE);
        delay(2000);
    }
    
    irReceiver.enableIRIn();
    digitalWrite(GREEN, LOW);
    delay(2000);
    digitalWrite(BLUE, HIGH);
    systemRunning = false;
}

void startLoopFunctions() {
    if (!systemRunning) {
        systemRunning = true;
        lastDHTRead = millis();
        digitalWrite(RED, LOW);
        digitalWrite(BLUE, LOW);
        digitalWrite(GREEN, HIGH);
        tone(BUZZER_PIN, 4000);
        delay(100);
        noTone(BUZZER_PIN);
    }
}

void stopLoopFunctions() {
    if (systemRunning) {
        systemRunning = false;
        if (buzzerState) {
            noTone(BUZZER_PIN);
            digitalWrite(BUZZER_PIN, LOW);
            buzzerState = false;
        }
        digitalWrite(GREEN, LOW);
         digitalWrite(BLUE, LOW);
        digitalWrite(RED, HIGH);
        tone(BUZZER_PIN, 4000);
        delay(50);
        noTone(BUZZER_PIN);
        delay(50);
        tone(BUZZER_PIN, 4000);
        delay(50);
        noTone(BUZZER_PIN);
    }
}

void loop() {
    if (irReceiver.decode(&irResults)) {
        if (irResults.value != 0xFFFFFFFFFFFFFFFF) {
            if (millis() - lastIRPress > IR_DEBOUNCE) {
                if (irResults.value == IR_BUTTON_1) {
                    startLoopFunctions();
                } else if (irResults.value == IR_BUTTON_0) {
                    stopLoopFunctions();
                }
                lastIRPress = millis();
            }
        }
        irReceiver.resume();
    }

    webServer.handleClient();

    if (systemRunning) {
        if (millis() - lastDHTRead >= DHT_INTERVAL) {
            dht.updateDht();
            float temp = dht.getTemp();
            float hum  = dht.getHumidity();
            if (!isnan(temp) && !isnan(hum)) {
                util.publisheDHTReadings(temp, hum);
            }
            lastDHTRead = millis();
        }
        if (util.getMqttp().connected()) {
            util.getMqttp().loop();
        }
    }
    
    delay(10);
}