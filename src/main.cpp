#include <Arduino.h>
#include "utills/util.h" // Ensure this path is correct
#include "sensors/rtp_server.h"
#include <HardwareSerial.h>


// Definitions
#define red 0
#define green 15
#define blue 12
#define txPin 32
#define rxPin 33
int _delay = 1000;
Util util;
Rtp_server server;
TaskHandle_t videoTaskHandle = NULL;

void startCamera();

// Use custom HardwareSerial instance
HardwareSerial hardwareSerial(2);

void setup() {
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);
  util.connectToWifi(blue,red,green);
  Serial.begin(9600); 
  hardwareSerial.begin(9600, SERIAL_8N1, rxPin, txPin); 
  //startCamera();
  delay(2000);
  Serial.println("Master ready");

}
//main loop
void loop() {
 if(hardwareSerial.available()) {
    String data = hardwareSerial.readStringUntil('\n');
    data.trim();
    Serial.println(data);
 }
 
 delay(1000);
}

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