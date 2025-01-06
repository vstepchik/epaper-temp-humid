#include <Arduino.h>
#include <cstdint>
#include <esp32-hal-timer.h>
#include <Adafruit_Si7021.h>
#include <WiFi.h>
#include "esp_attr.h"
#include <MorseCodeMachine.h>

#include "time.h"
#include "display_controller.h"
#include "common_types.h"
#include "esp32-hal.h"
#include "credentials.h"
#include "settings.h"
#include "stats_collector.h"
#include "RTClib.h"

// RUNTIME STATE
RTC_DATA_ATTR bool initial = true;
RTC_DATA_ATTR bool repaintRequested = true;
RTC_DATA_ATTR bool timeSynced = false;
RTC_DATA_ATTR struct tm timeinfo;
RTC_DATA_ATTR uint32_t wakeupCounter = 0;
RTC_DATA_ATTR uint64_t lastAlarmAtSec = 0;
RTC_DATA_ATTR uint64_t lastSensorReadoutAtSec = 0;

static Adafruit_Si7021 sensor = Adafruit_Si7021();
static RTC_DATA_ATTR DisplayController display(initial);
static RTC_DATA_ATTR StatsCollector<uint16_t> statsCollector(initial);
static RTC_DS3231 rtc;


bool wasClick = false;
DisplayRenderPayload displayPayload;

char buf[128];

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
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint8_t attempts = WIFI_CONNECT_ATTEMPTS;
  while (WiFi.status() != WL_CONNECTED) {
    attempts -= 1;
    if (attempts == 0) {
      return false;
    }
    yield();
    delay(WIFI_CONNECT_ATTEMPT_INTERVAL_MS - 40);
    if (BLINK_LED) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(40);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_0, NTP_SERVER_1, NTP_SERVER_2);
  delay(500);
  getLocalTime(&timeinfo);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  return true;
}

void delayMorse() {
  delay(BUZZ_LENGTH_MS);
}

void dotMorse() {
  tone(BUZZER_PIN, BUZZ_PITCH_HZ);
  delayMorse();
  noTone(BUZZER_PIN);
}

void dashMorse() {
  tone(BUZZER_PIN, BUZZ_PITCH_HZ);
  delayMorse();
  delayMorse();
  delayMorse();
  noTone(BUZZER_PIN);
}

void makeAlertSound(const char msg[]) {
  gpio_hold_dis(BUZZER_PIN);
  sendMorse(msg, delayMorse, dotMorse, dashMorse);
  gpio_hold_en(BUZZER_PIN);
}

void gracefulSleep(const unsigned long wakeupTimeMicroseconds) {
  Serial.println("..setting buzzer pin low");
  digitalWrite(BUZZER_PIN, LOW);
  gpio_hold_en(BUZZER_PIN);
  gpio_deep_sleep_hold_en(); // make sure the buzzer pin is down during deep sleep
  Serial.println("..computing sleep time");
  auto sleepInterval = MICROSECONDS_PER_MILLISECOND * WAKEUP_INTERVAL_MS;
  // subtract time spent turned on to keep interval and not delay between wakeups
  auto wakeupAfterMicroseconds = constrain(sleepInterval - wakeupTimeMicroseconds, MICROSECONDS_PER_MILLISECOND * 100, sleepInterval);
  Serial.println("(now really sleep)\n\n\n\n");
  Serial.flush();
  delay(1); // without this the program is reset by watchdog during sleep for some reason - couldn't figure out why
  esp_deep_sleep(wakeupAfterMicroseconds); 
  Serial.println("Eh? Should not happen!");
}

void setup() {
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  unsigned long wakeupTime = micros();

  Serial.begin(115200);
  Serial.print(F("Wakeup!!!!!! #"));
  Serial.println(++wakeupCounter);
  Serial.print(F("compiled: "));
  Serial.print(__DATE__);
  Serial.print(F(" "));
  Serial.println(__TIME__);

  wasClick = false;
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      wasClick = true; 
      if (BLINK_LED) {
        digitalWrite(LED_BUILTIN, HIGH); delay(200);
        digitalWrite(LED_BUILTIN, LOW);  delay(50);
      }
  }
  Serial.print(F("Was click: "));
  Serial.println(wasClick);
  if (wasClick) {
    repaintRequested = true;
  }
  // Blink once for wakeup
  if (BLINK_LED) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (!setupInterrupts()) return;
  Serial.println(F("Interrupts set."));

  // ### TIME
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    if (!timeSynced) {
      timeSynced = syncTime();
    }
    if(!getLocalTime(&timeinfo)) {
      snprintf(buf, sizeof(buf), "Failed to get time :(");
      display.debug_print(buf);
      return;
    }

    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  DateTime dt_now = rtc.now();
  Serial.print("\nRTC Time: ");
  Serial.print(dt_now.year(), DEC);
  Serial.print('/');
  Serial.print(dt_now.month(), DEC);
  Serial.print('/');
  Serial.print(dt_now.day(), DEC);
  Serial.print(' ');
  Serial.print(dt_now.hour(), DEC);
  Serial.print(':');
  Serial.print(dt_now.minute(), DEC);
  Serial.print(':');
  Serial.print(dt_now.second(), DEC);
  Serial.println();
  Serial.print("RTC Temperature: ");
  Serial.print(rtc.getTemperature());
  Serial.println(" C\n");

  // ### SENSOR

  UpdateFlags updateFlags = UpdateFlags::NONE;
  DateTime lastSensorReadoutAt = DateTime(SECONDS_FROM_1970_TO_2000 + lastSensorReadoutAtSec);
  if ((dt_now - lastSensorReadoutAt).totalseconds() >= SENSOR_READ_INTERVAL_SEC) {
    if (sensor.begin()) {
      Serial.println("Reading sensor...");
      lastSensorReadoutAtSec = dt_now.secondstime();
      sensor.heater(false); // preserve battery
      updateFlags = statsCollector.collect(sensor.readTemperature(), sensor.readHumidity());
    } else {
      Serial.println("Sensor failure!");
      snprintf(buf, sizeof(buf), "Sensor no begin :(");
      display.debug_print(buf);
    }
  } else {
    Serial.println("Sensor - skip");
  }

  if (repaintRequested || (uint16_t) updateFlags) {
    Serial.println("Repainting");
    float t, h;
    statsCollector.currentReadingMedian(&t, &h);

    displayPayload.degreesUnit = CELSIUS;
    displayPayload.timeinfo = dt_now;
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

    statsCollector.getHistoryChartDataT(displayPayload.historyChartT);
    statsCollector.getHistoryChartDataH(displayPayload.historyChartH);

    DisplayController::DrawFlags flags = DisplayController::DrawFlags::SD_CARD | DisplayController::DrawFlags::BATTERY | DisplayController::DrawFlags::TIME;
    if (isFlagSet(updateFlags, UpdateFlags::CURRENT_READING)) flags |= DisplayController::DrawFlags::CURRENT_READINGS | DisplayController::DrawFlags::GAUGES;
    if (isFlagSet(updateFlags, UpdateFlags::STATS_DAY | UpdateFlags::STATS_WEEK | UpdateFlags::STATS_MONTH)) flags |= DisplayController::DrawFlags::STATISTICS;
    if (isFlagSet(updateFlags, UpdateFlags::HISTORY_HOUR | UpdateFlags::HISTORY_DAY | UpdateFlags::HISTORY_WEEK | UpdateFlags::HISTORY_MONTH | UpdateFlags::HISTORY_YEAR)) flags |= DisplayController::DrawFlags::HISTORY_GRAPH;

    // todo: try lowering frequency here to save power
    display.repaint(flags, &displayPayload);
    repaintRequested = false;
  } else {
    Serial.println("Repaint - skip");
  }

  DateTime lastAlarmAt = DateTime(SECONDS_FROM_1970_TO_2000 + lastAlarmAtSec);
  if ((dt_now - lastAlarmAt).totalseconds() >= ALARM_INTERVAL_SEC) {
    Serial.println("Making alarm sound");
    lastAlarmAtSec = dt_now.secondstime();
    if (displayPayload.humidityAlert == ALERT_DANGER) {
      makeAlertSound("HUM");
    }
    if (displayPayload.temperatureAlert == ALERT_DANGER) {
      makeAlertSound("TMP");
    }
    if (displayPayload.batteryLevel <= ALERT_BAT_LOW) {
      makeAlertSound("BAT");
    }
  } else {
    Serial.println("Alarm sound - skip");
  }

  // statsCollector.printDebug();

  Serial.print("Going to bed.. (total wakeup time ");
  Serial.print(micros() - wakeupTime);
  Serial.println("us) z-z-z-z\n");
  // Serial.flush();
  initial = false;

  // blink before sleep
  if (BLINK_LED) {
    digitalWrite(LED_BUILTIN, HIGH); delay(5);
    digitalWrite(LED_BUILTIN, LOW);  delay(50);
    digitalWrite(LED_BUILTIN, HIGH); delay(5);
    digitalWrite(LED_BUILTIN, LOW);
  }

  // will reset from setup() after wakeup
  gracefulSleep(wakeupTime);
}

void loop() {
  gracefulSleep(0);
}
