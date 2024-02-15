#pragma once 

#define WIFI_SSID "IoT"
#define WIFI_PWD "IoT"

#define HTTP_LISTEN_PORT    80
#define HTTP_USERNAME       "admin"
#define HTTP_PASSWORD       "admin"
#if defined(ESP8266)
    #define SERIALSPEED         460800
#elif defined(ESP32)
    #define SERIALSPEED         460800
#endif

