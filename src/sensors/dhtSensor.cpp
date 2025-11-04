#include "dhtSensor.h"

DHTSensor::DHTSensor():
m_humidity(0.0),
m_temp(0.0)
{
    //constractor
}

DHTSensor::~DHTSensor()
{
    //distractor
    delete m_dht;
}
//initialize dhtSensor
void DHTSensor::initDHT(byte dhtpin, int dhtType) {
    if(!m_dht) {
        m_dht = new DHT(dhtpin,dhtType);
        m_dht->begin();
    }else {
        Serial.println("Failed to Initialize DHT11");
    }

}
//update m_temp
void DHTSensor::updateTemp(){
    if(m_dht){
        float temp = m_dht->readTemperature();
        if(!isnan(m_temp)){
             m_temp = temp;
        }else {
             Serial.println("Invalid Temperature");
        }
    }
}
//return the temperature Value
float DHTSensor::getTemp() const {
    return m_temp;
}
// update the m_humidity 
void DHTSensor::updateHumidity() {
    if(m_dht) {
        float humidity = m_dht->readHumidity();
        if(!isnan(m_humidity)){
            m_humidity = humidity;
            
        }else{
            Serial.println("Invalid Humidity");
        }
    }

}
//return the humdity value
float DHTSensor::getHumidity()const {
    return m_humidity;
}
//update temp from Arduino via UART
void DHTSensor::updateDhtFromSerial(){
    if(Serial.available()) {
        String data = Serial.readStringUntil('\n');
        data.trim();
        int commaIndex = data.indexOf(',');
        if(commaIndex > 0) {
            String tempString = data.substring(0,commaIndex);
            String humString = data.substring(commaIndex + 1);

            float temp = tempString.toFloat();
            float hum = humString.toFloat();
            if(!isnan(temp) && !isnan(hum)) {
                m_temp = temp;
                m_humidity = hum;
            }else {
                Serial.println("Invalid temp or humidity");
            }
        }else {
            Serial.println("Invalide formate");
        }
    }
}