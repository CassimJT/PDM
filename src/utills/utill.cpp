#include "util.h"

Util::Util()
{
    // Constructor
    m_ssid = "HUAWEI-B525-B3AC";
    m_password = "@bismillah?";
    m_mqttConnected = false;
    m_lastReconnectAttempt = 0;
    
    // Initialize MQTT with larger buffer for JSON
    mqtt.setBufferSize(512);
}

Util::~Util(){
    
}

void Util::blink_led(int pin, char opt){
    if(opt == 'H'){
        digitalWrite(pin, HIGH);
        delay(200);
        digitalWrite(pin, LOW);
        delay(200);
    }
    if(opt == 'L') {
        digitalWrite(pin, LOW);
    }
}

// RGB LED control
void Util::rgb(int pin, char opt) {
    if(opt == 'H'){
        digitalWrite(pin, HIGH);
    } else if(opt == 'L') {
        digitalWrite(pin, LOW);
    } else {
        Serial.print("Invalid Option");
    }
}

// Connecting to WiFi
void Util::connectToWifi(byte bpin, byte rpin, byte gpin) {
    Serial.println("Connecting to wifi ....");
    WiFi.mode(WIFI_STA);
    WiFi.begin(m_ssid, m_password);

    int start_attempt_time = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start_attempt_time < TIME_OUT) {
        Serial.print(".");
        blink_led(bpin, 'H');
        delay(500);
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("");
        Serial.println("Failed to connect.");
        blink_led(bpin, 'L');
        rgb(rpin, 'H'); // red on
        Serial.print("WiFi Status Code: ");
        Serial.println(WiFi.status());
        return;
    }
    
    Serial.println("Connected!");
    rgb(gpin, 'H'); // green on
    Serial.println(WiFi.localIP());
}

bool Util::isWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// Disconnect WiFi
void Util::disconnectWifi(byte redpin, byte greenpin) {
    WiFi.disconnect();
    rgb(greenpin, 'L'); // green LED off
    rgb(redpin, 'H');   // red on
}

// Connecting to MQTT with Last Will and Testament
void Util::connectToMqtt() {
    mqtt.setClient(wificlient);
    mqtt.setServer(mqtt_server, port);
    
    // Set callback before connecting
    mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->handleIncomingMsg(topic, payload, length);
    });
    
    if (!mqtt.connected()) {
        Serial.println("Connecting to MQTT...");
        
        // Setup Last Will and Testament
        String willTopic = "plantdoctor/device/" + String(DEVICE_ID) + "/availability";
        const char* willPayload = "offline";
        boolean willRetain = true;  // Retain so new clients see last state
        uint8_t willQoS = 1;        // QoS 1 ensures delivery
        
        // Connect with LWT
        if (mqtt.connect(DEVICE_ID, 
                        "plantdoctor",    // username (optional)
                        "device",         // password (optional)
                        willTopic.c_str(), // will topic
                        willQoS,          // will QoS
                        willRetain,       // will retain
                        willPayload)) {   // will message
                        
            Serial.println("Connected to MQTT with LWT");
            m_mqttConnected = true;
            publishAvailability(true);
            publishDiscovery();
            subscribeToTopics();
            
        } else {
            Serial.print("Failed to connect to MQTT error: ");
            Serial.println(mqtt.state());
            m_mqttConnected = false;
        }
    }
}

// Updated disconnectFromMqtt with graceful shutdown
void Util::disconnectFromMqtt() {
    if (mqtt.connected()) {
        Serial.println("Util: Performing graceful MQTT shutdown...");
        
        // Publish offline status (retained message)
        publishAvailability(false);
        
        // Small non-blocking delay to ensure publish is sent
        unsigned long publishStart = millis();
        while (millis() - publishStart < 50) {
            mqtt.loop();  // Process MQTT traffic
            delay(1);
        }
        
        // Disconnect MQTT properly (sends DISCONNECT packet)
        mqtt.disconnect();
        Serial.println("Util: MQTT disconnected gracefully");
        
        m_mqttConnected = false;
    }
}

// Subscribe to PNDDevice topics
void Util::subscribeToTopics() {
    if (mqtt.connected()) {
        // Subscribe to command topic for this specific device
        String cmdTopic = "plantdoctor/device/" + String(DEVICE_ID) + "/command";
        mqtt.subscribe(cmdTopic.c_str());
        Serial.print("Subscribed to: ");
        Serial.println(cmdTopic);
    }
}

// Handle incoming messages from Qt PNDDevice
void Util::handleIncomingMsg(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    
    // Convert payload to string
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("Message: ");
    Serial.println(message);
    
    // Parse JSON command
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Extract command
    const char* command = doc["command"] | "";
    
    if (strlen(command) > 0) {
        processCommand(command, doc);
    }
}

// Process structured commands from PNDDevice
void Util::processCommand(const char* command, JsonDocument& doc) {
    Serial.print("Processing command: ");
    Serial.println(command);
    
    if (strcmp(command, "power") == 0) {
        bool powerOn = doc["value"] | false;
        if (powerOn) {
            turnFunOn();
        } else {
            turnFunOff();
        }
        publishStatus(powerOn ? "on" : "off");
        
    } else if (strcmp(command, "get_status") == 0) {
        publishStatus(mqtt.connected() ? "connected" : "disconnected");
        
    } else if (strcmp(command, "get_sensors") == 0) {
        // This will be handled by the main loop when it reads sensors
        Serial.println("Sensor request received");
        
    } else if (strcmp(command, "configure") == 0) {
        if (doc.containsKey("config")) {
            JsonObject config = doc["config"];
            if (config.containsKey("interval")) {
                Serial.print("Setting interval to: ");
                Serial.println(config["interval"].as<int>());
            }
            publishStatus("configured");
        }
    }
}

// Publish discovery message
void Util::publishDiscovery() {
    if (!mqtt.connected()) return;
    
    StaticJsonDocument<256> doc;
    doc["device_id"] = DEVICE_ID;
    doc["type"] = DEVICE_TYPE;
    doc["capabilities"] = "sensor,actuator";
    
    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["temperature"] = true;
    sensors["humidity"] = true;
    
    JsonObject actuators = doc.createNestedObject("actuators");
    actuators["fan"] = true;
    actuators["buzzer"] = true;
    
    char payload[256];
    serializeJson(doc, payload);
    
    mqtt.publish("plantdoctor/discovery", payload);
    Serial.println("Discovery published");
}

// Publish availability (online/offline)
void Util::publishAvailability(bool online) {
    if (!mqtt.connected()) return;
    
    const char* status = online ? "online" : "offline";
    // Publish with retain=true so new subscribers get last known state
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/availability").c_str(), 
                 status, 
                 true);  // retained
    Serial.print("Availability published: ");
    Serial.println(status);
}

// Publish sensor data
void Util::publishSensors(float temp, float hum) {
    if (!mqtt.connected()) {
        unsigned long now = millis();
        if (now - m_lastReconnectAttempt > 5000) {
            m_lastReconnectAttempt = now;
            connectToMqtt();
        }
        if (!mqtt.connected()) return;
    }
    
    StaticJsonDocument<128> doc;
    doc["temperature"] = temp;
    doc["humidity"] = hum;
    doc["timestamp"] = millis();
    
    char payload[128];
    serializeJson(doc, payload);
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/sensors").c_str(), 
                 payload);
    Serial.print("Sensors published: ");
    Serial.println(payload);
}

// Publish device status
void Util::publishStatus(const char* state) {
    if (!mqtt.connected()) return;
    
    StaticJsonDocument<128> doc;
    doc["state"] = state;
    doc["timestamp"] = millis();
    
    char payload[128];
    serializeJson(doc, payload);
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/status").c_str(), 
                 payload);
}

// Publish error message
void Util::publishError(const char* error) {
    if (!mqtt.connected()) return;
    
    StaticJsonDocument<128> doc;
    doc["error"] = error;
    doc["timestamp"] = millis();
    
    char payload[128];
    serializeJson(doc, payload);
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/error").c_str(), 
                 payload);
}

// Legacy method wrapper
void Util::publisheDHTReadings(float temp, float hum) {
    publishSensors(temp, hum);
}

// Notify user via API
void Util::notifyUser(String msg) {
    if (WiFi.status() == WL_CONNECTED) {
        http.begin("http://192.168.8.116:3000/api/iot/msg");  
        http.addHeader("Content-Type", "application/json");
        
        StaticJsonDocument<200> json;
        json["msg"] = msg;
        json["device"] = DEVICE_ID;

        String payload;
        serializeJson(json, payload);
        
        int statuscode = http.POST(payload);
        if (statuscode == 200) {
            Serial.println("Payload sent");
        } else {
            Serial.print("Error: ");
            Serial.println(statuscode);
        }
        http.end();
    } else {
        Serial.println("Not connected to any network");
    }
}

// Attach serial
void Util::attachSerial(HardwareSerial &serial) {
    _serial = &serial;
}

// Turn on the buzzer
void Util::turnBuzzerOn(int pin, char sig) {
    pinMode(pin, OUTPUT);
    if(sig == 'H'){
        digitalWrite(pin, HIGH);
    } else if(sig == 'L') {
        digitalWrite(pin, LOW);
    } else {
        Serial.print("Invalid Option");
    }
    digitalWrite(pin,sig);
}

// Turn on the fan
void Util::turnFunOn() {
    if (_serial) {
        _serial->println("funOn");
        Serial.println("Fan ON");
    }
}

// Turn off the fan
void Util::turnFunOff() {
    if (_serial) {
        _serial->println("funOff");
        Serial.println("Fan OFF");
    }    
}

// Get MQTT client reference
PubSubClient& Util::getMqttp() {
    return mqtt;
}