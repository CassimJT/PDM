#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <esp32-hal-gpio.h>
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Device identity - change this for each device
#define DEVICE_ID "esp32_001"
#define DEVICE_TYPE "sensor_fan"

// MQTT Topics matching PNDTopics
#define MQTT_ROOT "plantdoctor"
#define MQTT_DISCOVERY "plantdoctor/discovery"
#define MQTT_AVAILABILITY "plantdoctor/device/" DEVICE_ID "/availability"
#define MQTT_SENSORS "plantdoctor/device/" DEVICE_ID "/sensors"
#define MQTT_STATUS "plantdoctor/device/" DEVICE_ID "/status"
#define MQTT_COMMAND "plantdoctor/device/" DEVICE_ID "/command"
#define MQTT_ERROR "plantdoctor/device/" DEVICE_ID "/error"

class Util
{
private:
    /* data */
public:
    Util();
    ~Util();
    
    // LED Control
    void blink_led(int pin, char opt);
    void rgb(int pin, char opt);
    
    // WiFi Connection
    void connectToWifi(byte bpin, byte rpin, byte gpin);
    void disconnectWifi(byte redpin, byte greenpin);
    bool isWifiConnected();
    
    // MQTT Connection (Updated for PNDDevice)
    void connectToMqtt();
    void disconnectFromMqtt();
    void subscribeToTopics();
    void handleIncomingMsg(char* topic, byte* payload, unsigned int length);
    
    // PNDDevice Compatible Methods
    void publishDiscovery();
    void publishAvailability(bool online);
    void publishSensors(float temp, float hum);
    void publishStatus(const char* state);
    void publishError(const char* error);
    
    // Legacy methods (wrapped for compatibility)
    void publisheDHTReadings(float temp, float hum);  
    void notifyUser(String msg);
    
    // Hardware Control
    void attachSerial(HardwareSerial &serial);
    void turnBuzzerOn(int pin, char sig);
    void turnFunOn();
    void turnFunOff();
    
    // Getters
    PubSubClient& getMqttp();
    String getDeviceId() { return String(DEVICE_ID); }

private:
    String m_ssid;
    String m_password;
    const int TIME_OUT = 30000;
    HTTPClient http;
    WiFiClient wificlient;
    PubSubClient mqtt;
    const char* mqtt_server = "192.168.8.130"; 
    const int port = 1883;
    
    // Device state
    bool m_mqttConnected;
    unsigned long m_lastReconnectAttempt;
    
    HardwareSerial* _serial = nullptr;
    
    // Helper methods
    void processCommand(const char* command, JsonDocument& doc);
};

#endif