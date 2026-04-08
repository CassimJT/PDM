#include "util.h"

// =============================
// NEW FLAGS FOR POWER CONTROL
// =============================
volatile bool mqttPowerRequested = false;
volatile bool mqttPowerState = false;

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
    const char* brokers[] = {"192.168.8.130", "192.168.8.149"};
    const int brokerCount = 2;

    mqtt.setClient(wificlient);

    mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->handleIncomingMsg(topic, payload, length);
    });

    if (!mqtt.connected()) {
        Serial.println("Connecting to MQTT with failover...");

        bool connected = false;
        for (int i = 0; i < brokerCount; i++) {
            mqtt.setServer(brokers[i], port);

            String willTopic = "plantdoctor/device/" + String(DEVICE_ID) + "/availability";
            const char* willPayload = "offline";
            boolean willRetain = true;
            uint8_t willQoS = 1;

            if (mqtt.connect(DEVICE_ID,
                             "plantdoctor",
                             "device",
                             willTopic.c_str(),
                             willQoS,
                             willRetain,
                             willPayload)) {
                Serial.print("Connected to MQTT broker: ");
                Serial.println(brokers[i]);
                m_mqttConnected = true;
                connected = true;
                publishAvailability(true);
                publishDiscovery();
                subscribeToTopics();
                break;
            } else {
                Serial.print("Failed to connect to broker: ");
                Serial.print(brokers[i]);
                Serial.print(" - error: ");
                Serial.println(mqtt.state());
            }
        }

        if (!connected) {
            Serial.println("MQTT connection failed on all brokers.");
            m_mqttConnected = false;
        }
    }
}

// Disconnect MQTT gracefully
void Util::disconnectFromMqtt() {
    if (mqtt.connected()) {
        Serial.println("Util: Performing graceful MQTT shutdown...");
        publishAvailability(false);
        unsigned long publishStart = millis();
        while (millis() - publishStart < 50) {
            mqtt.loop();
            delay(1);
        }
        mqtt.disconnect();
        Serial.println("Util: MQTT disconnected gracefully");
        m_mqttConnected = false;
    }
}

// Subscribe to PNDDevice topics including power topic
void Util::subscribeToTopics() {
    if (mqtt.connected()) {
        String cmdTopic = "plantdoctor/device/" + String(DEVICE_ID) + "/command";
        mqtt.subscribe(cmdTopic.c_str());
        Serial.print("Subscribed to: ");
        Serial.println(cmdTopic);

        // NEW POWER TOPIC
        mqtt.subscribe(MQTT_POWER);
        Serial.print("Subscribed to power topic: ");
        Serial.println(MQTT_POWER);
    }
}

// Handle incoming messages from MQTT including power commands
void Util::handleIncomingMsg(char* topic, byte* payload, unsigned int length) {

    // =============================
    // HANDLE POWER TOPIC
    // =============================
    if (String(topic) == MQTT_POWER) {
        String msg;
        for (unsigned int i=0; i<length; i++) msg += (char)payload[i];
        msg.toLowerCase();

        if(msg == "on") {
            mqttPowerState = true;
            mqttPowerRequested = true;
            publishStatus("on");
        } else if(msg == "off") {
            mqttPowerState = false;
            mqttPowerRequested = true;
            publishStatus("off");
        }
        return;
    }

    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    
    String message;
    for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
    
    Serial.print("Message: ");
    Serial.println(message);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        return;
    }
    
    const char* command = doc["command"] | "";
    if (strlen(command) > 0) processCommand(command, doc);
}

// Process structured commands
void Util::processCommand(const char* command, JsonDocument& doc) {
    Serial.print("Processing command: ");
    Serial.println(command);
    
    if (strcmp(command, "power") == 0) {
        bool powerOn = doc["value"] | false;
        if (powerOn) turnFunOn();
        else turnFunOff();

        mqttPowerState = powerOn;
        mqttPowerRequested = true;

        publishStatus(powerOn ? "on" : "off");
        
    } else if (strcmp(command, "get_status") == 0) {
        publishStatus(mqtt.connected() ? "connected" : "disconnected");
        
    } else if (strcmp(command, "get_sensors") == 0) {
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

// --- THE REST OF YOUR ORIGINAL METHODS ---

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

void Util::publishAvailability(bool online) {
    if (!mqtt.connected()) return;
    
    const char* status = online ? "online" : "offline";
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/availability").c_str(), 
                 status, true);
    Serial.print("Availability published: ");
    Serial.println(status);
}

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
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/sensors").c_str(), payload);
    Serial.print("Sensors published: ");
    Serial.println(payload);
}

void Util::publishStatus(const char* state) {
    if (!mqtt.connected()) return;
    
    StaticJsonDocument<128> doc;
    doc["state"] = state;
    doc["timestamp"] = millis();
    
    char payload[128];
    serializeJson(doc, payload);
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/status").c_str(), payload);
}

void Util::publishError(const char* error) {
    if (!mqtt.connected()) return;
    
    StaticJsonDocument<128> doc;
    doc["error"] = error;
    doc["timestamp"] = millis();
    
    char payload[128];
    serializeJson(doc, payload);
    
    mqtt.publish(("plantdoctor/device/" + String(DEVICE_ID) + "/error").c_str(), payload);
}

void Util::publisheDHTReadings(float temp, float hum) {
    publishSensors(temp, hum);
}

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
        if (statuscode == 200) Serial.println("Payload sent");
        else {
            Serial.print("Error: ");
            Serial.println(statuscode);
        }
        http.end();
    } else Serial.println("Not connected to any network");
}

void Util::attachSerial(HardwareSerial &serial) {
    _serial = &serial;
}

void Util::turnBuzzerOn(int pin, char sig) {
    pinMode(pin, OUTPUT);
    if(sig == 'H') digitalWrite(pin, HIGH);
    else if(sig == 'L') digitalWrite(pin, LOW);
    else Serial.print("Invalid Option");
}

void Util::turnFunOn() {
    if (_serial) {
        _serial->println("funOn");
        Serial.println("Fan ON");
    }
}

void Util::turnFunOff() {
    if (_serial) {
        _serial->println("funOff");
        Serial.println("Fan OFF");
    }    
}

PubSubClient& Util::getMqttp() {
    return mqtt;
}