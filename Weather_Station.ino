#include "Adafruit_SHT31.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <esp_adc_cal.h>

#include "properties.h"

#define BATTERY_MULTISAMPLING 5
#define VOLTAGE_MAX 4200  // in mV
#define VOLTAGE_MIN 3300  // in mV

// Upload Code:
// Make S2 boards into Device Firmware Upgrade (DFU) mode.
// * Hold on Button 0
// * Press Button Reset
// * Release Button 0 when you hear the prompt tone on UBS reconnection

const int oneWireBus = 9;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

Adafruit_SHT31 sht;

long startTime = 0;
int batteryVoltage = 0;
int batteryPercentage = 0;

void setup() {
  if (initialiseSystem()) {
    if (startWiFi()) {
      if (ota) {
        runOta();
      }
      if (!test) {
        process();
      }
    }
    if (!test) {
      sleep();
    }
  } else {
    emergencySleep();
  }
}

void loop() {
  Serial.println("Test mode is on...");
  process();
  delay(5000);
}

boolean initialiseSystem() {
  startTime = millis();

  pinMode(LED_BUILTIN, OUTPUT);
  if (led) {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  Serial.begin(115200);
  if (waitForSerial) {
    while (!Serial)
      ;
  }
  Serial.println("\nInitializing...");

  readBattery();
  if (batteryPercentage < minBatteryPercentage) {
    return false;
  }

  sht = Adafruit_SHT31();
  sht.begin(0x44);
  sensors.begin();
  delay(100);

  return true;
}

void readBattery() {
  adc1_config_width(ADC_WIDTH_BIT_13);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_13, 1100, &adc_chars);

  uint32_t dOut = 0;
  for (int i = 0; i < BATTERY_MULTISAMPLING; i++) {
    dOut += adc1_get_raw(ADC1_CHANNEL_4);
  }
  dOut = dOut / BATTERY_MULTISAMPLING;

  Serial.println("dOut: " + String(dOut));
  uint32_t vOut = esp_adc_cal_raw_to_voltage(dOut, &adc_chars);
  Serial.println("vOut: " + String(vOut) + "mV");
  uint32_t vBat = vOut * 2;
  Serial.println("vBat: " + String(vBat) + "mV");

  int percentage = 100 * (vBat - VOLTAGE_MIN) / (VOLTAGE_MAX - VOLTAGE_MIN);
  if (percentage < 0) {
    percentage = 0;
  } else if (percentage > 100) {
    percentage = 100;
  }
  Serial.println("pBat: " + String(percentage) + "%");

  batteryVoltage = vBat;
  batteryPercentage = percentage;
}

boolean startWiFi() {
  Serial.println("Connecting to: " + String(ssid));
  WiFi.mode(WIFI_STA);
  if (staticIP) {
    WiFi.config(localIP, gateway, subnet, primaryDNS, secondaryDNS);
  }
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    delay(10);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("WiFi connection *** FAILED ***");
    return false;
  }
}

void stopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi switched off");
}

void sleep() {
  stopWiFi();

  // convert s into us
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);

  Serial.println("Awake for " + String((millis() - startTime) / 1000.0, 3) + "s");
  Serial.println("Entering " + String(sleepDuration) + "s of sleep time");
  Serial.println("Starting deep-sleep period...");

  digitalWrite(LED_BUILTIN, LOW);
  delay(10);

  esp_deep_sleep_start();
}

void emergencySleep() {
  // convert h into us
  esp_sleep_enable_timer_wakeup(emergencySleepDuration * 3600000000ULL);

  Serial.println("Starting emergency deep-sleep period...");

  digitalWrite(LED_BUILTIN, LOW);
  delay(10);

  esp_deep_sleep_start();
}

void process() {

  // the chip ID is essentially its MAC address
  char chipId[23];
  snprintf(chipId, 23, "%llX", ESP.getEfuseMac());

  if (test) {
    // refresh needed for test
    readBattery();
  }

  String payload = measurement + ",id=" + String(chipId) + ","
                   + tags
                   + " fwVersion=" + String(fwVersion)
                   + ",batteryVoltage=" + String(batteryVoltage)
                   + ",batteryPercentage=" + String(batteryPercentage);

  // get sensor data
  float tInt = sht.readTemperature();
  if (!isnan(tInt)) {
    Serial.println("Temperature Internal (C): " + String(tInt));
    payload += ",temperatureInt=" + String(tInt, 2);
  } else {
    Serial.println("Failed to read internal temperature");
  }

  float h = sht.readHumidity();
  if (!isnan(h)) {
    Serial.println("Humidity (%): " + String(h));
    payload += ",humidity=" + String(h, 2);
  } else {
    Serial.println("Failed to read humidity");
  }

  sensors.requestTemperatures();
  float tExt = sensors.getTempCByIndex(0);
  if (!isnan(tExt)) {
    Serial.println("Temperature External (C): " + String(tExt));
    payload += ",temperatureExt=" + String(tExt, 2);
  } else {
    Serial.println("Failed to read external temperature");
  }

  // send sensor data
  String postUrl = apiRoot + "/api/v2/write?org=" + organization + "&bucket=" + bucket + "&precision=s";

  Serial.print("Request: ");
  Serial.println(postUrl);
  Serial.print("Payload: ");
  Serial.println(payload);

  WiFiClient client;
  HTTPClient http;
  http.begin(client, postUrl);
  http.setConnectTimeout(timeout);
  http.addHeader("Authorization", "Token " + token);
  int httpCode = http.POST(payload);
  http.end();
  client.stop();

  Serial.print("HTTP Status: ");
  Serial.println(httpCode);
  Serial.println("");
}

void runOta() {
  Serial.println("Checking for firmware updates.");

  HTTPClient httpClient;
  httpClient.begin(otaUrl + "version.txt");
  int httpCode = httpClient.GET();
  if (httpCode == 200) {
    int newFWVersion = httpClient.getString().toInt();

    Serial.print("Current firmware version: ");
    Serial.println(fwVersion);
    Serial.print("Available firmware version: ");
    Serial.println(newFWVersion);

    if (newFWVersion > fwVersion) {
      Serial.println("Newer version available, updating...");

      httpUpdate.onStart(otaStarted);
      httpUpdate.onEnd(otaFinished);
      httpUpdate.onProgress(otaProgress);

      WiFiClient wiFiClient;
      t_httpUpdate_return status = httpUpdate.update(wiFiClient, otaUrl + "image.bin");
      switch (status) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("OTA failed, error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("No OTA update");
          break;
        case HTTP_UPDATE_OK:
          Serial.println("OTA was successfull.");
          break;
      }
    } else {
      Serial.println("Already on latest version");
    }
  } else {
    Serial.print("Firmware version check failed, got HTTP response code ");
    Serial.println(httpCode);
  }

  httpClient.end();
}

void otaStarted() {
  Serial.println("OTA update process started");
}

void otaFinished() {
  Serial.println("OTA update process finished");
}

void otaProgress(int cur, int total) {
  Serial.printf("OTA update process at %d of %d bytes...\n", cur, total);
}