#pragma once

// Pins
#define LED_BUILTIN GPIO_NUM_19
#define BUZZER_PIN GPIO_NUM_14
#define BATTERY_ADC_PIN GPIO_NUM_35
#define ONBOARD_BUTTON_PIN GPIO_NUM_39

// Constants
#define DAY_PER_MONTH 30
#define HOUR_PER_DAY 24
#define MIN_PER_HOUR 60
#define SEC_PER_MIN 60
#define MICROSECONDS_PER_SECOND 1000000
#define MICROSECONDS_PER_MILLISECOND 1000

#define BATT_EMPTY 1860 // value at 3.25V
/*
3.20v = 1828
3.25v = 1860
3.30v = 1895
3.35v = 1927
3.40v = 1960
3.45v = 1990
3.50v = 2019
3.55v = 2055
3.60v = 2086
3.65v = 2116
3.70v = 2147
3.75v = 2176
3.80v = 2207
3.85v = 2239
3.90v = 2270
3.95v = 2299
4.00v = 2330
4.05v = 2361
4.10v = 2390
4.15v = 2420
4.20v = 2450
*/
#define BATT_FULL 2330 // value at 4.00V


#define PX_PER_1H 30
#define PX_PER_23H 46
#define PX_PER_6D 36
#define PX_PER_23D 69
#define PX_PER_11M 48
#define CHART_LEN_PX PX_PER_1H + PX_PER_23H + PX_PER_6D + PX_PER_23D + PX_PER_11M


// Settings
#define WIFI_CONNECT_ATTEMPTS 20
#define WIFI_CONNECT_ATTEMPT_INTERVAL_MS 250 // should be > 50
#define NTP_SERVER_0 "pool.ntp.org"
#define NTP_SERVER_1 "time.google.com"
#define NTP_SERVER_2 "time.cloudflare.com"
#define GMT_OFFSET_SEC 3600
#define DAYLIGHT_OFFSET_SEC 3600

#define BLINK_LED false
#define WAKEUP_INTERVAL_MS 12000 // energy drain <--> timekeeping accuracy tradeoff
#define SENSOR_READ_INTERVAL_SEC 40 // 3read/2min
#define N_UPDATES_BETWEEN_FULL_REPAINTS 20

#define ALARM_INTERVAL_SEC 3*60*60+5 // 3h5s for small drift
#define BUZZ_LENGTH_MS 100
#define BUZZ_PITCH_HZ 4000 // ~3700-4000 resonance
#define ALERT_BAT_LOW 0.15


// Computed
#define CURRENT_READING_MEDIAN_FILTER_SIZE 60/PX_PER_1H*60/SENSOR_READ_INTERVAL_SEC
#define READS_BUFFER_SIZE MIN_PER_HOUR/PX_PER_1H*SEC_PER_MIN/SENSOR_READ_INTERVAL_SEC // 3 reads buffer
