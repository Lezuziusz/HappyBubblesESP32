//#include <EEPROM.h>

#ifndef SETTINGS_H
#define SETTINGS_H

#define FIRMWARE_VERSION "0.0.1 alpha"


#define DEVICE_NAME "ESP8266WiFi"
#define WIFI_AP_NAME DEVICE_NAME

class Settings {

  public:
    Settings() {};

  String ssid = "";
  String psk = "";
  String name;
  String broker = "192.168.x.x";
  String pubTopic = "happy-bubbles/presence/ha";
};

Settings settings;


#endif

  /* Serial debugging support */
  #define SERIAL_DEBUG
    #ifdef SERIAL_DEBUG
      #define DEBUG_BEGIN(x)   { delay(500); Serial.begin(x); }
      #define DEBUG_PRINT(x)   Serial.print(x)
      #define DEBUG_PRINTLN(x) Serial.println(x)
      #define DEBUG_PRINTF(x,y) Serial.printf(x,y)
      //#define DEBUG_PRINTLN(x,y) Serial.println(x,y)
    #else
      #define DEBUG_BEGIN(x)   {}
      #define DEBUG_PRINT(x)   {}
      #define DEBUG_PRINTLN(x) {}
      #define DEBUG_PRINTF(x,y) {}
    #endif
