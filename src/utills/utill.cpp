#include "util.h"

Util::Util()
{
    //constractor
    m_ssid = "HUAWEI-B525-B3AC";
    m_password = "@bismillah?";
}
Util::~Util(){
    
}

void Util::blink_led(int pin,char opt){
   if(opt == 'H'){
    digitalWrite(pin,HIGH);
    delay(200);
    digitalWrite(pin,LOW);
    delay(200);
   }if(opt == 'L') {
    digitalWrite(pin,LOW);
   }
}
//red
void Util::rgb(int pin, char opt) {
    if(opt == 'H'){
        digitalWrite(pin,HIGH);
    }else if(opt == 'L') {
        digitalWrite(pin,LOW);
    }else {
        //log error
        Serial.print("Invalid Option");
    }
}
//connecting to wifi
void Util::connectToWifi(byte bpin, byte rpin,byte gpin) {
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
//disconnect to wifi
void Util::disconnectWifi(byte redpin,byte greenpin) {
    WiFi.disconnect();
    rgb(greenpin, 'L'); //green LED off
    rgb(redpin, 'H'); //red on
}
// connecting to mqtt
void Util::connectToMqtt() {
    //
    mqtt.setClient(wificlient);
    mqtt.setServer(mqtt_server,port);
    if(!mqtt.connected()) {
        Serial.println("Connecting to mqt..");
        if(mqtt.connect("ESP32Client")){
            Serial.println("Connected to mqtt");
        }else {
            Serial.print("faild to connect to mqtt er:");
            Serial.println(mqtt.state());
        }
    }
} 
//subscrbing
void Util::subscribeToATopic() {
    mqtt.setClient(wificlient);
    mqtt.setServer(mqtt_server,port);
    mqtt.setCallback([this](char * topic, byte * paylod, unsigned int length){
        handleIncomingMsg(topic,paylod,length);
    });
    if(!mqtt.connected()) {
        connectToMqtt();
        mqtt.subscribe("iot/fan/state"); 
    }else {
        //mqtt.subscribe("iot/fan/state");
    }
     
}
//handle incoming messages
void Util::handleIncomingMsg(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.print("Message: ");
    Serial.println(message);

    if (String(topic) == "iot/fan/state") {
        if (message == "on") {
            // Turn ON the fan
            digitalWrite(22, HIGH);  
            Serial.println("Fan turned ON");
        } else if (message == "off") {
            digitalWrite(22, LOW);  
            Serial.println("Fan turned OFF");
        }
    }
}
//Publish DHT reading
void Util::publisheDHTReadings(float temp, float hum) {
    //temp
    if(!mqtt.connected()) {
        connectToMqtt();
    }
    StaticJsonDocument<100> doc;
    doc["temp"] = temp;
    doc["hum"] = hum;
    char payload[100];
    serializeJson(doc,payload);

    mqtt.loop(); //keep the connection alive
    mqtt.publish("iot/temp",payload);
}