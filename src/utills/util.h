#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <esp32-hal-gpio.h>
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
class Util
{
private:
    /* data */
    public:
        Util();
        ~Util();
        //blink the led
        void blink_led(int pin,char opt);
        void rgb(int pin,char opt);
        void connectToWifi(byte bpin, byte rpin,byte gpin);
        void disconnectWifi(byte redpin,byte greenpin);
        void connectToMqtt();
        void subscribeToATopic();
        void handleIncomingMsg(char* topic, byte* payload, unsigned int length);

       
    private:
        String m_ssid;
        String m_password;
        const int TIME_OUT = 30000;
        HTTPClient http;
        WiFiClient wificlient;
        PubSubClient mqtt;
        const char* mqtt_server = "192.168.8.130";
        const int port = 1883;

};

#endif