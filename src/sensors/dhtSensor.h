#ifndef DHTSENSOR_H
#define DHRSENSOR_H

#include <esp32-hal-gpio.h>
#include <WiFi.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

class DHTSensor
{
private:
    /* data */

public:
    DHTSensor(/* args */);
    ~DHTSensor();

        void initDHT(byte dhtpin, int dhtType);

        float getTemp()const;
        void updateTemp();

        void updateHumidity();
        float getHumidity()const;

        void updateDhtFromSerial();
       
private:
    float m_temp;
    float m_humidity;
    DHT *m_dht = nullptr;
};



#endif