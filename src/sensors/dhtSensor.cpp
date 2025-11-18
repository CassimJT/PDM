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
        if(!isnan(temp)){
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
        if(!isnan(humidity)){
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
void DHTSensor::updateDht(){
     if(m_dht) {
        m_temp = m_dht->readTemperature();
        m_humidity = m_dht->readHumidity();

        if(isnan(m_temp) || isnan(m_humidity)) {
          Serial.println("Invalid Humidity");
          return;
        }
    }
}