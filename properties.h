// global settings
const int sleepDuration          = 300;                            // sleep duration in seconds
const int emergencySleepDuration = 24;                             // sleep duration in hours in case of low battery
const int minBatteryPercentage   = 20;                             // min battery percentage before emergency sleep is activated and normal usage is stopped
const boolean led                = true;                           // turn LED on while active
const boolean test               = false;                          // no sleep in test mode
const boolean waitForSerial      = false;                          // will wait for serial before executing code

// wifi settings
const char* ssid                 = "SSID";                     // wifi SSID
const char* password             = "PASSWORD";              // wifi password
const int wifiTimeout            = 10000;                          // timeout for trying to establish Wifi connection, in milliseconds
const boolean staticIP           = true;                           // if true the following IPAddress are used to configure WiFi, else DHCP will be used
const IPAddress                    localIP(192, 168, 1, 15);        // static IP, also set this in your router to avoid conflicts
const IPAddress                    gateway(192, 168, 1, 1);        // gateway, should be your routers IP
const IPAddress                    subnet(255, 255, 255, 0);       // subnet
const IPAddress                    primaryDNS(1, 1, 1, 1);         // use cloudflare as primary DNS resolver
const IPAddress                    secondaryDNS(8, 8, 8, 8);       // use google as fallback

// OTA update settings
const boolean ota                = true;                           // turn OTA on or off. note: once turned off any new FW must be flashed manually
const int fwVersion              = 4;                              // firmware version, don't forget to increase this!
const String otaUrl              = "http://192.168.1.17:90/esp/weather-station-garden/"; // url where a version.txt and image.bin file are expected

// influxdb2 settings
const String apiRoot             = "http://192.168.1.17:9096";
const String organization        = "home";
const String bucket              = "weather";
const String token               = "TOKEN`";
const String measurement         = "station";
const String tags                = "location=Garden";
const uint16_t timeout           = 2000;                           // in mili seconds
