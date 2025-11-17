#include <Arduino.h>
#include "utills/util.h" 
#include "sensors/rtp_server.h"
#include "sensors/dhtSensor.h"
#include <HardwareSerial.h>


//--------------------------------------------------
#define red 0
#define green 15
#define blue 12
#define txPin 1
#define rxPin 3
#define DHTPIN 32      
#define DHTTYPE DHT11
int _delay = 1000;

//----------------------------------------------
unsigned long lastmills = 0;
unsigned long debounceDelay = 60;
unsigned long lastDebounceTime;
const unsigned long sendInterval = 5000;


//-------- definitions --------------------------
void startCamera();
void readTempAndHumidity();

//-------------------------------------------------
HardwareSerial hardwareSerial(2);
Util util;
Rtp_server server;
DHTSensor dht;
TaskHandle_t videoTaskHandle = NULL;

//---------------Setup -------------------------
void setup() {
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);
  util.connectToWifi(blue,red,green);
  Serial.begin(9600); 
  hardwareSerial.begin(9600, SERIAL_8N1, rxPin, txPin); 
  util.attachSerial(hardwareSerial);
  dht.initDHT(DHTPIN,DHTTYPE);
  //startCamera();
  delay(2000);
  Serial.println("Master ready");

}

//**************** Main**************** */
void loop() {
  //util.turnBuzzerOn();
  readTempAndHumidity();
   if(util.getMqttp().connected()) {
      util.getMqttp().loop();
   }
   delay(1000);
}
/******************End main */


//---------------camera -----------
void startCamera() {
  server.initialiseCamera();
  server.getFrameQuality();
  server.startCameraServer();

  xTaskCreatePinnedToCore(
    [](void* param) { static_cast<Rtp_server*>(param)->sendVideo(nullptr); },
    "VideoTask",
    1024 * 5,
    &server,
    9,
    &videoTaskHandle,
    1 // Core 1
  );
}

//----------------read temp----------
void readTempAndHumidity() {
    dht.updateDht();
    float temp = dht.getTemp();
    float hum = dht.getHumidity();

    if (millis() - lastmills >= sendInterval) {
        util.publisheDHTReadings(temp, hum);
        lastmills = millis();
    }
}
