#include <Arduino.h>
#include "utills/util.h"
#include "sensors/dhtSensor.h"
#include "sensors/camera_manager.h"
#include "sensors/mjpeg_server.h"
#include <HardwareSerial.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

// =============================
// PINS
// =============================
#define RED     0
#define GREEN   15
#define BLUE    12
#define TX_PIN  1
#define RX_PIN  3
#define DHT_PIN 32
#define DHT_TYPE DHT11
#define BUZZER_PIN 14
#define IR_PIN   33  
#define POWER_BTN_PIN 2  

// IR buttons
#define IR_BUTTON_1 0xFF30CF
#define IR_BUTTON_0 0xFF6897

// Buzzer timing
#define BUZZER_SHORT_BEEP 100
#define BUZZER_LONG_BEEP  500
#define BUZZER_PAUSE      100

// Timing intervals
const unsigned long DHT_INTERVAL = 5000;
const unsigned long HEARTBEAT_INTERVAL = 30000;
const unsigned long MQTT_RECONNECT_INTERVAL = 10000;
const unsigned long IR_DEBOUNCE = 300;
const unsigned long BUZZER_DEBOUNCE = 50;
const unsigned long BTN_DEBOUNCE = 300;

// =============================
// OBJECTS
// =============================
HardwareSerial slaveSerial(2);  
Util util;
DHTSensor dht;
CameraManager camera;
MjpegServer webServer(80);    
IRrecv irReceiver(IR_PIN);
decode_results irResults;

// =============================
// STATE
// =============================
bool systemRunning = false;
bool buzzerActive = false;
bool lastBtnState = HIGH;

unsigned long lastDHTRead = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastIRPress = 0;
unsigned long lastBuzzerChange = 0;
unsigned long buzzerEndTime = 0;
unsigned long lastBtnPress = 0;

// =============================
// BUZZER CONTROL
// =============================
void setBuzzer(bool state) {
    if (state == buzzerActive) return;
    if (millis() - lastBuzzerChange < BUZZER_DEBOUNCE) return;
    digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
    buzzerActive = state;
    lastBuzzerChange = millis();
    if (state) buzzerEndTime = millis() + BUZZER_LONG_BEEP;
}

void beep(int duration) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerEndTime = millis() + duration;
    lastBuzzerChange = millis();
}

void playBeepPattern(int count, int beepDuration, int pauseDuration) {
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(beepDuration);
        digitalWrite(BUZZER_PIN, LOW);
        if (i < count - 1) delay(pauseDuration);
    }
    buzzerActive = false;
    buzzerEndTime = 0;
}

// =============================
// SETUP
// =============================
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RED, OUTPUT);
    pinMode(GREEN, OUTPUT);
    pinMode(BLUE, OUTPUT);
    pinMode(DHT_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(POWER_BTN_PIN, INPUT_PULLUP);  // <-- power button

    digitalWrite(RED, LOW);
    digitalWrite(GREEN, LOW);
    digitalWrite(BLUE, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    util.connectToWifi(BLUE, RED, GREEN);

    if (WiFi.status() == WL_CONNECTED) {
        camera.begin();
        webServer.begin();
        webServer.setMaxFPS(30);
        slaveSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
        util.attachSerial(slaveSerial);

        dht.initDHT(DHT_PIN, DHT_TYPE);

        delay(2000);
        playBeepPattern(1, BUZZER_LONG_BEEP, 0);
    } else {
        playBeepPattern(3, BUZZER_SHORT_BEEP, BUZZER_SHORT_BEEP);
    }

    irReceiver.enableIRIn();
    digitalWrite(GREEN, LOW);
    delay(2000);
    digitalWrite(BLUE, HIGH);
}

// =============================
// START/STOP SYSTEM FUNCTIONS
// =============================
void startLoopFunctions() {
    if (systemRunning) return;

    Serial.println("Starting system...");
    if (!util.getMqttp().connected())
        util.connectToMqtt();

    if (util.getMqttp().connected()) {
        systemRunning = true;
        lastDHTRead = millis();
        lastHeartbeat = millis();

        digitalWrite(RED, LOW);
        digitalWrite(BLUE, LOW);
        digitalWrite(GREEN, HIGH);

        util.publishAvailability(true);
        playBeepPattern(2, BUZZER_SHORT_BEEP, BUZZER_PAUSE);
        Serial.println("System started");
    } else {
        Serial.println("MQTT connection failed");
        for (int i = 0; i < 3; i++) {
            digitalWrite(RED, HIGH);
            delay(100);
            digitalWrite(RED, LOW);
            delay(100);
        }
    }
}

void stopLoopFunctions() {
    if (!systemRunning) return;
    Serial.println("Stopping system...");

    systemRunning = false;
    digitalWrite(GREEN, LOW);
    digitalWrite(BLUE, LOW);
    digitalWrite(RED, HIGH);

    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    buzzerEndTime = 0;

    util.publishAvailability(false);
    beep(BUZZER_SHORT_BEEP);
}

// =============================
// LOOP
// =============================
void loop() {
    // =============================
    // BUZZER TIMER
    // =============================
    if (buzzerActive && buzzerEndTime > 0 && millis() >= buzzerEndTime) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
        buzzerEndTime = 0;
    }

    // =============================
    // IR REMOTE
    // =============================
    if (irReceiver.decode(&irResults)) {
        if (irResults.value != 0xFFFFFFFFFFFFFFFF) {
            if (millis() - lastIRPress > IR_DEBOUNCE) {
                if (irResults.value == IR_BUTTON_1)
                    startLoopFunctions();
                else if (irResults.value == IR_BUTTON_0)
                    stopLoopFunctions();
                lastIRPress = millis();
            }
        }
        irReceiver.resume();
    }

    // =============================
    // PHYSICAL POWER BUTTON
    // =============================
    bool btnState = digitalRead(POWER_BTN_PIN);
    if (btnState == LOW && lastBtnState == HIGH && millis() - lastBtnPress > BTN_DEBOUNCE) {
        if (systemRunning)
            stopLoopFunctions();
        else
            startLoopFunctions();
        lastBtnPress = millis();
    }
    lastBtnState = btnState;

    // =============================
    // MQTT POWER CONTROL
    // =============================
    if (mqttPowerRequested) {
        mqttPowerRequested = false; // reset flag
        if (mqttPowerState)
            startLoopFunctions();
        else
            stopLoopFunctions();
    }

    // =============================
    // CAMERA SERVER
    // =============================
    webServer.handleClient();

    // =============================
    // SENSOR LOOP
    // =============================
    if (systemRunning && millis() - lastDHTRead >= DHT_INTERVAL) {
        dht.updateDht();
        float temp = dht.getTemp();
        float hum  = dht.getHumidity();
        if (!isnan(temp) && !isnan(hum)) {
            util.publisheDHTReadings(temp, hum);
            if (temp > 35.0 || hum > 80.0) {
                if (!buzzerActive) beep(BUZZER_SHORT_BEEP);
            }
        }
        lastDHTRead = millis();
    }

    // =============================
    // MQTT LOOP (ALWAYS RUN)
    // =============================
    if (util.getMqttp().connected()) {
        util.getMqttp().loop();
    } else {
        if (millis() - lastMqttReconnect > MQTT_RECONNECT_INTERVAL) {
            lastMqttReconnect = millis();
            Serial.println("MQTT reconnect attempt...");
            util.connectToMqtt();
        }
    }

    delay(50);
}