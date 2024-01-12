#include <Arduino.h>
#include <esp32-hal-timer.h>
#include <ace_button/AceButton.h>
#include <Adafruit_Si7021.h>

#include "display_controller.h"
#include "common_types.h"
#include "settings.h"

using namespace ace_button;

AceButton button(ONBOARD_BUTTON_PIN);
Adafruit_Si7021 sensor = Adafruit_Si7021();
DisplayController display;

bool repaintRequested = true;
hw_timer_t * timer = NULL;  // Pointer to the hardware timer
DisplayRenderPayload displayPayload;

void handleEvent(AceButton *, uint8_t, uint8_t);
void IRAM_ATTR onTimer();

char buf[256];

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

void setup() {
  randomSeed(1337);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ONBOARD_BUTTON_PIN, INPUT_PULLUP);
  button.setEventHandler(handleEvent);

  // Initialize the hardware timer
  timer = timerBegin(0, 80, true);  // Timer 0, prescaler 80, count up

  // Attach the timer to the interrupt function and set it for 1 minute (60,000,000 microseconds)
  timerAttachInterrupt(timer, &onTimer, true);
  // timerAlarmWrite(timer, 1000000 * SENSOR_READ_INTERVAL_SEC, true);  // Set time in microseconds
  timerAlarmWrite(timer, 1000000 * 120, true);  // Set time in microseconds
  timerAlarmEnable(timer);  // Enable the timer

  if (!sensor.begin()) {
    snprintf(buf, sizeof(buf), "No begin :(");
  } else {
    sensor.heater(false);
    snprintf(buf, sizeof(buf), "Sensor Rev(%hhu)\nSerial #%08X%08X\nT: %.2f\nH: %.2f", sensor.getRevision(), sensor.sernum_a, sensor.sernum_b, sensor.readTemperature(), sensor.readHumidity());
  }

  displayPayload.degreesUnit = CELSIUS;
}

void loop() { 
  button.check(); 
  if (repaintRequested) {
    // debug
    displayPayload.currentTemperatureCelsius = sensor.readTemperature();
    displayPayload.temperatureAlert = calcTemperatureAlert(displayPayload.currentTemperatureCelsius);
    displayPayload.currentHumidity = sensor.readHumidity();
    displayPayload.humidityAlert = calcHumidityAlert(displayPayload.currentHumidity);
    displayPayload.batteryLevel = batteryAdcToFullness(analogRead(BATTERY_ADC_PIN));
    displayPayload.sdCardVolumeBytes = 98227358105L;
    displayPayload.sdCardOccupiedBytes = 56231604L;

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
    // snprintf(buf, sizeof(buf), "Sensor Rev(%hhu)\nSerial #%08X%08X\nT: %.2f\nH: %.2f", sensor.getRevision(), sensor.sernum_a, sensor.sernum_b, sensor.readTemperature(), sensor.readHumidity());
    // display.debug_print(buf);

    repaintRequested = false;
  }
}

void  handleEvent(AceButton *, uint8_t eventType, uint8_t) {
  switch (eventType) {
  case AceButton::kEventPressed:
    // digitalWrite(LED_BUILTIN, HIGH);
    repaintRequested = true;
    break;
  case AceButton::kEventReleased:
    // digitalWrite(LED_BUILTIN, LOW);
    break;
  default:
    break;
  }
}

void IRAM_ATTR onTimer() {
    repaintRequested = true;
}