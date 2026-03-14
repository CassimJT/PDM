#include "dhtSensor.h"

DHTSensor::DHTSensor():
m_humidity(0.0),
m_temp(0.0)
{
    //constructor
    m_dht = nullptr;  // Initialize to nullptr
}

DHTSensor::~DHTSensor()
{
    //destructor
    if (m_dht) {
        delete m_dht;
    }
}

//initialize dhtSensor
void DHTSensor::initDHT(byte dhtpin, int dhtType) {
    if(!m_dht) {
        m_dht = new DHT(dhtpin, dhtType);
        m_dht->begin();
        Serial.println("DHT sensor initialized");
    } else {
        Serial.println("DHT already initialized");
    }
}

//update both temp and humidity at once
void DHTSensor::updateDht(){
    if(!m_dht) {
        Serial.println("DHT not initialized!");
        return;
    }
    
    // Read both sensors
    float temp = m_dht->readTemperature();
    float hum = m_dht->readHumidity();
    
    // Check each reading individually
    bool tempValid = !isnan(temp);
    bool humValid = !isnan(hum);
    
    // Update only valid readings
    if (tempValid) {
        m_temp = temp;
    } else {
        Serial.println("Warning: Invalid temperature reading");
    }
    
    if (humValid) {
        m_humidity = hum;
    } else {
        Serial.println("Warning: Invalid humidity reading");
    }
    
    // Print readings if at least one is valid
    if (tempValid || humValid) {
        Serial.print("DHT Readings - Temp: ");
        if (tempValid) {
            Serial.print(m_temp);
            Serial.print("C");
        } else {
            Serial.print("INVALID");
        }
        
        Serial.print(", Hum: ");
        if (humValid) {
            Serial.print(m_humidity);
            Serial.print("%");
        } else {
            Serial.print("INVALID");
        }
        Serial.println();
    } else {
        Serial.println("Both temperature and humidity readings are invalid!");
    }
}

//update only temperature
void DHTSensor::updateTemp(){
    if(!m_dht) {
        Serial.println("DHT not initialized!");
        return;
    }
    
    float temp = m_dht->readTemperature();
    if(!isnan(temp)){
        m_temp = temp;
        Serial.print("Temperature updated: ");
        Serial.print(m_temp);
        Serial.println("C");
    } else {
        Serial.println("Invalid Temperature reading");
    }
}

//return the temperature Value
float DHTSensor::getTemp() const {
    return m_temp;
}

// update only humidity 
void DHTSensor::updateHumidity() {
    if(!m_dht) {
        Serial.println("DHT not initialized!");
        return;
    }
    
    float humidity = m_dht->readHumidity();
    if(!isnan(humidity)){
        m_humidity = humidity;
        Serial.print("Humidity updated: ");
        Serial.print(m_humidity);
        Serial.println("%");
    } else {
        Serial.println("Invalid Humidity reading");
    }
}

//return the humidity value
float DHTSensor::getHumidity() const {
    return m_humidity;
}