#include <Arduino.h>
#include <cstdint>
#include <esp32-hal-timer.h>
#include <Adafruit_Si7021.h>
#include <WiFi.h>
#include "esp_heap_caps.h"

#include "GxEPD2.h"
#include "fnt_04b03b.h"
#include "fnt_big_digits.h"
#include "time.h"
#include "display_controller.h"
#include "common_types.h"
#include "esp32-hal.h"
#include "credentials.h"
#include "settings.h"
#include "stats_collector.h"


// RUNTIME STATE
RTC_DATA_ATTR bool initial = true;
RTC_DATA_ATTR bool repaintRequested = true;
RTC_DATA_ATTR bool timeSynced = false;
RTC_DATA_ATTR struct tm timeinfo;
RTC_DATA_ATTR uint32_t wakeupCounter = 0;
static RTC_DATA_ATTR RingBuf<float, 5> testRtcBuf;

static Adafruit_Si7021 sensor = Adafruit_Si7021();
static RTC_DATA_ATTR DisplayController display(initial);
static RTC_DATA_ATTR StatsCollector<uint16_t> statsCollector;


bool wasClick = false;
DisplayRenderPayload displayPayload;

char buf[1024];

inline float batteryAdcToFullness(uint16_t adcValue) {
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

inline AlertLevel calcHumidityAlert(float humidity) {
  if (humidity >= 62.0 && humidity <= 73.0) return ALERT_NONE;
  if (humidity >= 60.0 && humidity <= 75.0) return ALERT_WARNING;
  return ALERT_DANGER;
}

inline AlertLevel calcTemperatureAlert(float tempCelsius) {
  if (tempCelsius >= 19.0 && tempCelsius <= 22.0) return ALERT_NONE;
  if (tempCelsius >= 17.0 && tempCelsius <= 24.0) return ALERT_WARNING;
  return ALERT_DANGER;
}

inline bool setupInterrupts() {
  const uint16_t code = esp_sleep_enable_ext0_wakeup(ONBOARD_BUTTON_PIN, LOW);
  switch (code) {
    case ESP_OK:
      return true;
    case ESP_ERR_INVALID_ARG:
      snprintf(buf, sizeof(buf), "Failed to set button wakeup trigger: the selected GPIO is not an RTC GPIO, or the mode is invalid");
      break;
    case ESP_ERR_INVALID_STATE:
      snprintf(buf, sizeof(buf), "Failed to set button wakeup trigger: wakeup triggers conflict");
      break;
    default:
      snprintf(buf, sizeof(buf), "Failed to set button wakeup trigger: code %i", code);
      break;
  };
  display.debug_print(buf);
  return false;
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
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_0, NTP_SERVER_1, NTP_SERVER_2);
  delay(500);
  getLocalTime(&timeinfo);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);
  unsigned long wakeupTime = micros();

  Serial.begin(115200);
  Serial.print("Wakeup!!!!!! #");
  Serial.println(++wakeupCounter);

  wasClick = false;
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      wasClick = true; 
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW);  delay(50);
  }
  Serial.print("Was click: ");
  Serial.println(wasClick);
  if (wasClick) {
    repaintRequested = true;
  }
  Serial.print("Repaint requested: ");
  Serial.println(repaintRequested);
  // Blink once for wakeup
  digitalWrite(LED_BUILTIN, HIGH);
  delay(20);
  digitalWrite(LED_BUILTIN, LOW);

  if (!setupInterrupts()) return;
  Serial.println("Interrupts set.");

  // if (!timeSynced) {
    // timeSynced = syncTime();
  // }
  // if(!getLocalTime(&timeinfo)) {
    // Serial.println("Failed to get time :(");
  //   snprintf(buf, sizeof(buf), "Failed to get time :(");
  //   display.debug_print(buf);
  //   return;
  // }


  if (sensor.begin()) {
    sensor.heater(false);
  } else {
    snprintf(buf, sizeof(buf), "Sensor no begin :(");
    display.debug_print(buf);
    return;
  }

  Serial.print("sizeof(display) = ");
  Serial.print(sizeof(display));
  Serial.println();

  Serial.print("sizeof(sensor) = ");
  Serial.print(sizeof(sensor));
  Serial.println();
  
  UpdateFlags statsUpdateFlags = statsCollector.collect(sensor.readTemperature(), sensor.readHumidity());
  Serial.print("sizeof(statsCollector) = ");
  Serial.print(sizeof(statsCollector));
  Serial.println();

  if (repaintRequested || (uint16_t) statsUpdateFlags) {
    Serial.println("Repainting");
    float t, h;
    statsCollector.currentReadingMedian(&t, &h);

    displayPayload.degreesUnit = CELSIUS;
    displayPayload.timeinfo = timeinfo;
    displayPayload.currentTemperatureCelsius = t;
    displayPayload.temperatureAlert = calcTemperatureAlert(displayPayload.currentTemperatureCelsius);
    displayPayload.currentHumidity = h;
    displayPayload.humidityAlert = calcHumidityAlert(displayPayload.currentHumidity);
    displayPayload.batteryLevel = batteryAdcToFullness(analogRead(BATTERY_ADC_PIN));

    displayPayload.statsT1D = statsCollector.statsTemp1D();
    displayPayload.statsT1W = statsCollector.statsTemp1W();
    displayPayload.statsT1M = statsCollector.statsTemp1M();
    displayPayload.statsH1D = statsCollector.statsHumidity1D();
    displayPayload.statsH1W = statsCollector.statsHumidity1W();
    displayPayload.statsH1M = statsCollector.statsHumidity1M();

    display.repaint(DisplayController::DrawFlags::CURRENT_READINGS | DisplayController::DrawFlags::GAUGES, &displayPayload);
    // repaintRequested = false;
  }

  statsCollector.printDebug();
  // heap_caps_print_heap_info(MALLOC_CAP_8BIT);

  Serial.print("Going to bed.. (total wakeup time ");
  Serial.print(micros() - wakeupTime);
  Serial.println("us) z-z-z-z\n\n");
  Serial.flush();
  initial = false;
}

void loop() {
  // blink before sleep
  digitalWrite(LED_BUILTIN, HIGH); delay(5);
  digitalWrite(LED_BUILTIN, LOW);  delay(50);
  digitalWrite(LED_BUILTIN, HIGH); delay(5);
  digitalWrite(LED_BUILTIN, LOW);  delay(50);

  // will reset from setup() after wakeup
  esp_deep_sleep(MICROSECONDS_PER_MILLISECOND * WAKEUP_INTERVAL_MS);
}
