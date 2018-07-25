/*
* This is a simple implementation of the Happy Bubbles type devices on an ESP32
* Using a combination of code from Andreas Spiess https://github.com/SensorsIot/Bluetooth-BLE-on-Arduino-IDE/tree/master/BLE_Proximity_Sensor
* and RiRoman https://github.com/RiRomain/Happy_Bubble_ESP32_Node
* The above implementations had problems with the WiFi DHCP server not renewing the lease on connect
* and failing after a day or two. The solution was to turn one radio off manually whilst the other
* was active. I have had this code running for 3 weeks without failure.
*/

#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
// #include <ESP8266WebServer.h>
//#include <ESP8266HTTPUpdateServer.h>

#include "settings.h"

const char* mqtt_server = (char*)settings.broker.c_str();
long lastReconnectAttempt = 0;

int scanDuration = 15; //In seconds

WiFiClient espClient;
PubSubClient client(espClient);

WiFiServer server(80);

String httpUpdateResponse;

unsigned long entry;

void printDeviceInfo(BLEAdvertisedDevice advertisedDevice) {
  DEBUG_PRINT("---------------- Found new device  ------------------");
  DEBUG_PRINTF("Advertised Device: %s \n", advertisedDevice.toString().c_str());

  if (advertisedDevice.haveRSSI()) {
    DEBUG_PRINT("RSSI             : "); Serial.println(advertisedDevice.getRSSI());
 }
}

void sendToMqtt(BLEAdvertisedDevice advertisedDevice) {
  int rssi = advertisedDevice.getRSSI();
  String name = String(advertisedDevice.getName().c_str());
  //String UUID = String(advertisedDevice.getServiceUUID().toString().c_str());
  BLEAddress address = advertisedDevice.getAddress();
  String macTransformed = String(address.toString().c_str());
  //cc : 40 : 80 : ba   : 9f    :  7a
  //01 2 34 5 67 8 910 11 1213 14 1516
  macTransformed.remove(14, 1);
  macTransformed.remove(11, 1);
  macTransformed.remove(8, 1);
  macTransformed.remove(5, 1);
  macTransformed.remove(2, 1);

  if (name.length() == 0) {
    name = "unknown";
  }


  /*
    {
    "hostname": "living-room",
    "mac": "001060AA36F8",
    "rssi": -94,
    "is_scan_response": "0",
    "type": "3",
    "data": "0201061aff4c000215e2c56db5dffb48d2b060d0f5a71096e000680068c5"
    }
  */
  StaticJsonBuffer<500> JSONbuffer;
  JsonObject& JSONencoder = JSONbuffer.createObject();

  float distance = calculateDistance(rssi);

  JSONencoder["id"] = macTransformed.c_str();
  JSONencoder["name"] = name;
  //JSONencoder["id"] = UUID;
  JSONencoder["distance"] = distance;


  char JSONmessageBuffer[500];
  JSONencoder.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));

  //happy-bubbles/ble/<the-hostname>/raw/<the-bluetooth-MAC-address
  String publishTopic = (char*)settings.pubTopic.c_str();// + macTransformed.c_str();
  if (client.publish(publishTopic.c_str(), JSONmessageBuffer) == true) { //TODO base_topic + mac_address
    DEBUG_PRINT("Success sending message to topic: "); Serial.println(publishTopic);
    DEBUG_PRINT("Message: "); Serial.println(JSONmessageBuffer);
  } else {
    DEBUG_PRINT("Error sending message: "); Serial.println(publishTopic);
    DEBUG_PRINT("Message: "); Serial.println(JSONmessageBuffer);
  }
}

void publishResults(BLEScanResults scanResults) {

  delay(10);
  btStop();
  // Connect to WiFi
  DEBUG_PRINTLN("Connecting to ");
  DEBUG_PRINT(settings.ssid);
  //WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid.c_str(), settings.psk.c_str());
  entry = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - entry >= 15000) esp_restart();
    delay(500);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("");
  DEBUG_PRINT("WiFi connceted, IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  client.setServer(settings.broker.c_str(), 1883);
  //client.setCallback();
  DEBUG_PRINTLN("Connect to MQTT server...");
  while(!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    if (client.connect("ESP32Client")) {
      DEBUG_PRINTLN("connected");
      for (int i = 0; i < scanResults.getCount(); i++) {
        sendToMqtt(scanResults.getDevice(i));
      }
    } else {
      DEBUG_PRINT("failed rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN( "try again in 5 seconds");
      delay(500);
    }
  }
  for (int i = 0; i > 10; i++) {
    client.loop();
    delay(100);
  }

  client.disconnect();
  delay(100);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStart();
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      printDeviceInfo(advertisedDevice);
      //sendToMqtt(advertisedDevice);
    }
};

void setup() {
  // put your setup code here, to run once:
  DEBUG_BEGIN(115200);

  //WiFi.begin(ssid, password);
 //setupWiFi();

 //client.setServer(mqtt_server, 1883);
 //lastReconnectAttempt = 0;

  BLEDevice::init("");
  DEBUG_PRINTLN("Finished setup");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    DEBUG_PRINT("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("ESP8266Client")) {
    if (client.connect("ESP32Client")) {
      DEBUG_PRINTLN("connected");
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

float calculateDistance(int rssi) {

  int txPower = -60; //hard coded power value. Usually ranges between -59 to -65

  //double n = 2.0; // ranges 2-4

  if (rssi == 0) {
    return -1.0;
  }

 float distance = pow(10, ((double)(txPower - rssi) / 20));
 return distance;

}

void loop() {

  BLEScan* pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(scanDuration);
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Total devices found: "); Serial.println(foundDevices.getCount());
  DEBUG_PRINTLN("Scan done!");

  if (foundDevices.getCount() > 0) {
    DEBUG_PRINTLN("Publishing Results..");
    publishResults(foundDevices);
    DEBUG_PRINTLN("Publish Done.");
  }

}
