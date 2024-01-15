#include <Arduino.h>
#include <cstdint>
#include <esp32-hal-timer.h>
#include <Adafruit_Si7021.h>
#include <WiFi.h>

#include "time.h"
#include "display_controller.h"
#include "common_types.h"
#include "esp32-hal.h"
#include "credentials.h"
#include "settings.h"


// RUNTIME STATE
RTC_DATA_ATTR bool repaintRequested = true;
RTC_DATA_ATTR bool timeSynced = false;
RTC_DATA_ATTR struct tm timeinfo;
RTC_DATA_ATTR int64_t lastWakedAt = 0;

Adafruit_Si7021 sensor = Adafruit_Si7021();
RTC_DATA_ATTR DisplayController display;

bool wasClick = false;
DisplayRenderPayload displayPayload;

char buf[1024];

float batteryAdcToFullness(uint16_t adcValue) {
    // Clamping the voltage values to the battery's min and max voltages
    if (adcValue >= BATT_FULL) {
        return 1.0f;
    } else if (adcValue <= BATT_EMPTY) {
        return 0.0f;
    }

    // Mapping the voltage to a percentage
    // This is a simple linear approximation. You might want to use a more complex function
    // for a more accurate mapping.
    return (float)(adcValue - BATT_EMPTY) / (BATT_FULL - BATT_EMPTY);
}

AlertLevel calcHumidityAlert(float humidity) {
  if (humidity >= 62.0 && humidity <= 73.0) return ALERT_NONE;
  if (humidity >= 60.0 && humidity <= 75.0) return ALERT_WARNING;
  return ALERT_DANGER;
}

AlertLevel calcTemperatureAlert(float tempCelsius) {
  if (tempCelsius >= 19.0 && tempCelsius <= 22.0) return ALERT_NONE;
  if (tempCelsius >= 17.0 && tempCelsius <= 24.0) return ALERT_WARNING;
  return ALERT_DANGER;
}

bool syncTime() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t attempts = WIFI_CONNECT_ATTEMPTS;
  while (WiFi.status() != WL_CONNECTED) {
    attempts -= 1;
    if (attempts == 0) {
      return false;
    }
    delay(WIFI_CONNECT_ATTEMPT_INTERVAL_MS - 40);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(40);
    digitalWrite(LED_BUILTIN, LOW);
  }
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  delay(500);
  getLocalTime(&timeinfo);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);

  // Blink once for wakeup
  digitalWrite(LED_BUILTIN, HIGH);
  delay(20);
  digitalWrite(LED_BUILTIN, LOW);

  switch(esp_sleep_get_wakeup_cause()){
    case ESP_SLEEP_WAKEUP_EXT0 : wasClick = true; break;
    case ESP_SLEEP_WAKEUP_EXT1 : break;
    case ESP_SLEEP_WAKEUP_TIMER : break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : break;
    case ESP_SLEEP_WAKEUP_ULP : break;
    default : break;
  }


  if (!timeSynced) {
    // timeSynced = syncTime();
  }
  if(!getLocalTime(&timeinfo)) {
    snprintf(buf, sizeof(buf), "Failed to get time :(");
  }

  int64_t now = esp_timer_get_time();
  uint32_t microsSinceLastWakeup = now - lastWakedAt;
  lastWakedAt = now;
  if (wasClick) {
    repaintRequested = true;
  }

  switch (esp_sleep_enable_ext0_wakeup(GPIO_NUM_39, LOW)) {
    case ESP_OK:
      break;
    case ESP_ERR_INVALID_ARG:
      snprintf(buf, sizeof(buf), "The selected GPIO is not an RTC GPIO, or the mode is invalid");
      display.debug_print(buf);
      return;
    case ESP_ERR_INVALID_STATE:
      snprintf(buf, sizeof(buf), "Wakeup triggers conflict");
      display.debug_print(buf);
      return;
  };

  if (!sensor.begin()) {
    snprintf(buf, sizeof(buf), "Sensor no begin :(");
    display.debug_print(buf);
    return;
  } else {
    sensor.heater(false);
  }

  displayPayload.degreesUnit = CELSIUS;

  if (repaintRequested) {
    displayPayload.timeinfo = timeinfo;
    displayPayload.currentTemperatureCelsius = sensor.readTemperature();
    displayPayload.temperatureAlert = calcTemperatureAlert(displayPayload.currentTemperatureCelsius);
    displayPayload.currentHumidity = sensor.readHumidity();
    displayPayload.humidityAlert = calcHumidityAlert(displayPayload.currentHumidity);
    displayPayload.batteryLevel = batteryAdcToFullness(analogRead(BATTERY_ADC_PIN));
    // displayPayload.sdCardVolumeBytes = (uint64_t) 1000 * 1000 * 1000 * 32;
    // displayPayload.sdCardOccupiedBytes = (uint64_t) 1024 * 1024 * 2 * displayPayload.currentTemperatureCelsius;

    displayPayload.statsT1D = {
      .average = displayPayload.currentTemperatureCelsius,
      .median = displayPayload.currentTemperatureCelsius,
      .max = displayPayload.currentTemperatureCelsius,
      .min = displayPayload.currentTemperatureCelsius,
    };
    displayPayload.statsT1W = displayPayload.statsT1D;
    displayPayload.statsT1M = displayPayload.statsT1D;
    displayPayload.statsH1D = {
      .average = displayPayload.currentHumidity,
      .median = displayPayload.currentHumidity,
      .max = displayPayload.currentHumidity,
      .min = displayPayload.currentHumidity,
    };
    displayPayload.statsH1W = displayPayload.statsH1D;
    displayPayload.statsH1M = displayPayload.statsH1D;
    // end debug

    display.full_repaint(&displayPayload);
    repaintRequested = false;
  }
  display.paint_time(&displayPayload.timeinfo);

  digitalWrite(LED_BUILTIN, HIGH); delay(5);
  digitalWrite(LED_BUILTIN, LOW);  delay(50);
  digitalWrite(LED_BUILTIN, HIGH); delay(5);
  digitalWrite(LED_BUILTIN, LOW);  delay(50);

  esp_deep_sleep(MICROSECONDS_PER_MILLISECOND * WAKEUP_INTERVAL_MS);
}

void loop() { 
  // never executed
}
