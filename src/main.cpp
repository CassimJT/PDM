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

// Buzzer timing constants (milliseconds)
#define BUZZER_SHORT_BEEP 100
#define BUZZER_LONG_BEEP  500
#define BUZZER_PAUSE      100

HardwareSerial slaveSerial(2);  
Util           util;
DHTSensor      dht;
CameraManager  camera;
MjpegServer    webServer(80);    

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 5000;   
unsigned long lastIRPress = 0;
const unsigned long IR_DEBOUNCE = 300;
unsigned long lastBuzzerChange = 0;
const unsigned long BUZZER_DEBOUNCE = 50;  // Prevent rapid buzzer changes
bool systemRunning = false;  
bool buzzerActive = false;
unsigned long buzzerEndTime = 0;  // For timed buzzer operation

IRrecv irReceiver(IR_PIN);
decode_results irResults;

// Function to turn buzzer on/off with debouncing
void setBuzzer(bool state) {
    if (state == buzzerActive) {
        return; // No change needed
    }
    
    if (millis() - lastBuzzerChange < BUZZER_DEBOUNCE) {
        return; // Debounce buzzer changes
    }
    
    digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
    buzzerActive = state;
    lastBuzzerChange = millis();
    
    // If turning on, set when it should automatically turn off
    if (state) {
        buzzerEndTime = millis() + BUZZER_LONG_BEEP; // Default to long beep
    }
}

// Function to beep for a specific duration
void beep(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerEndTime = millis() + duration;
    lastBuzzerChange = millis();
}

// Function for pattern beeps
void playBeepPattern(int count, int beepDuration, int pauseDuration) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(beepDuration);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < count - 1) {
            delay(pauseDuration);
        }
    }
    buzzerActive = false;
    buzzerEndTime = 0;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RED,   OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE,  OUTPUT);
    pinMode(DHT_PIN, INPUT);
    // Initialize buzzer pin
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);  // Start with buzzer off
    
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(BLUE, LOW);

    util.connectToWifi(BLUE, RED, GREEN);

    if (WiFi.status() == WL_CONNECTED) {
        camera.begin();
        webServer.begin();
        slaveSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
        util.attachSerial(slaveSerial);
        dht.initDHT(DHT_PIN, DHT_TYPE);
        delay(2000);
        
        // Successful initialization - single long beep
        playBeepPattern(1, BUZZER_LONG_BEEP, 0);
    } else {
        // WiFi connection failed - three short beeps
        playBeepPattern(3, BUZZER_SHORT_BEEP, BUZZER_SHORT_BEEP);
    }
    
    irReceiver.enableIRIn();
    digitalWrite(GREEN, LOW);
    delay(2000);
    digitalWrite(BLUE, HIGH);
    systemRunning = false;
}

// UPDATED: startLoopFunctions with MQTT connection check
void startLoopFunctions() {
    if (!systemRunning) {
        Serial.println("Starting system...");
        
        // Ensure MQTT is connected before proceeding
        if (!util.getMqttp().connected()) {
            Serial.println("MQTT not connected, attempting to reconnect...");
            util.connectToMqtt();
        }
        
        // Check if MQTT is now connected
        if (util.getMqttp().connected()) {
            systemRunning = true;
            lastDHTRead = millis();
            
            // Update LEDs
            digitalWrite(RED, LOW);
            digitalWrite(BLUE, LOW);
            digitalWrite(GREEN, HIGH);
            
            // This ensures UI gets updated immediately
            util.publishAvailability(true);
            
            // Start beep - two short beeps
            playBeepPattern(2, BUZZER_SHORT_BEEP, BUZZER_PAUSE);
            
            Serial.println("System started successfully");
        } else {
            Serial.println("Failed to start: MQTT not connected");
            // Flash red LED to indicate error
            for (int i = 0; i < 3; i++) {
                digitalWrite(RED, HIGH);
                delay(100);
                digitalWrite(RED, LOW);
                delay(100);
            }
        }
    }
}

// UPDATED: stopLoopFunctions with graceful MQTT disconnect
void stopLoopFunctions() {
    if (systemRunning) {
        Serial.println("Stopping system - initiating graceful shutdown...");
        
        // Stop system flag first
        systemRunning = false;
        
        // Turn off hardware immediately
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
        buzzerEndTime = 0;
        
        // Update LEDs
        digitalWrite(GREEN, LOW);
        digitalWrite(BLUE, LOW);
        digitalWrite(RED, HIGH);
        
        // Gracefully disconnect from MQTT (this publishes offline status)
        util.disconnectFromMqtt();
        
        // Short beep to indicate stop
        digitalWrite(BUZZER_PIN, HIGH);
        delay(BUZZER_SHORT_BEEP);
        digitalWrite(BUZZER_PIN, LOW);
        
        Serial.println("System stopped gracefully");
    }
}

void loop() {
    // Auto-turn off buzzer after duration
    if (buzzerActive && buzzerEndTime > 0 && millis() >= buzzerEndTime) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
        buzzerEndTime = 0;
    }

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
                Serial.println(temp);
                Serial.println(hum);
                util.publisheDHTReadings(temp, hum);
                
                // Alert if temperature or humidity is out of range
                if (temp > 35.0 || hum > 80.0) {
                    // Quick alert beep without blocking
                    if (!buzzerActive) {
                        beep(BUZZER_SHORT_BEEP);
                    }
                }
            }
            lastDHTRead = millis();
        }
        
        // MQTT loop - only if connected
        if (util.getMqttp().connected()) {
            util.getMqttp().loop();
        } else {
            // Try to reconnect if system is running but MQTT disconnected
            static unsigned long lastMqttReconnect = 0;
            if (millis() - lastMqttReconnect > 10000) { // Try every 10 seconds
                lastMqttReconnect = millis();
                Serial.println("System running but MQTT disconnected, attempting reconnect...");
                util.connectToMqtt();
            }
        }
    }
    
    delay(10);
}