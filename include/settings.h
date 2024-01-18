#pragma once

// Pins
#define LED_BUILTIN GPIO_NUM_19
#define BATTERY_ADC_PIN GPIO_NUM_35
#define ONBOARD_BUTTON_PIN GPIO_NUM_39

// Constants
#define DAY_PER_MONTH 30
#define HOUR_PER_DAY 24
#define MIN_PER_HOUR 60
#define SEC_PER_MIN 60
#define MICROSECONDS_PER_SECOND 1000000
#define MICROSECONDS_PER_MILLISECOND 1000

#define BATT_EMPTY 1828 // value at 3.2V
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
#define BATT_FULL 2450 // value at 4.2V


/*
single measurement = 4b float humidity + 4b float temperature = 8 bytes

T-0..T-1h  = 60m/30px    = 2m/px  = 8 bytes * 30px = 240 bytes
T-1h..T-1d = 23h/46px    = 30m/px = 8 bytes * 46px = 368 bytes
T-1d..T-1w = 6*24h/36px  = 4h/px  = 8 bytes * 36px = 288 bytes
T-1w..T-1m = 23*24h/69px = 8h/px  = 8 bytes * 69px = 552 bytes
T-1m..T-1y = 336d/48px   = 7d/px  = 8 bytes * 48px = 384 bytes
-- TOTAL ------------------------------------------ 1832 bytes
*/
#define READING_SIZE_BYTES sizeof(float)*2
#define PX_PER_1H 30
#define PX_PER_23H 46
#define PX_PER_6D 36
#define PX_PER_23D 69
#define PX_PER_11M 48
#define GRAPH_DATA_SIZE_BYTES READING_SIZE_BYTES*(PX_PER_1H+PX_PER_23H+PX_PER_6D+PX_PER_23D+PX_PER_11M)


// Settings
#define WIFI_CONNECT_ATTEMPTS 20
#define WIFI_CONNECT_ATTEMPT_INTERVAL_MS 500 // should be > 50
#define SYNC_TIME_OVER_WIFI_EVERY_MINUTES 60*8
#define NTP_SERVER_0 "pool.ntp.org"
#define NTP_SERVER_1 "time.google.com"
#define NTP_SERVER_2 "time.cloudflare.com"
#define GMT_OFFSET_SEC 3600
#define DAYLIGHT_OFFSET_SEC 3600

#define WAKEUP_INTERVAL_MS 2000 // energy drain <--> timekeeping accuracy tradeoff
#define SENSOR_READ_INTERVAL_SEC 12 // 10 reads/2min
#define N_UPDATES_BETWEEN_FULL_REPAINTS 10

#define BUZZ_LENGTH_MS 10
#define BUZZ_PITCH_HZ 2000


// Computed
#define READS_BUFFER_SIZE MIN_PER_HOUR/PX_PER_1H*SEC_PER_MIN/SENSOR_READ_INTERVAL_SEC // 10 reads buffer
