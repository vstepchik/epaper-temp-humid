#pragma once

#include "time.h"
#include <cstdint>
#include <sys/types.h>
#include "settings.h"

enum DegreesUnit {
    CELSIUS, FARENHEIT,
};

enum AlertLevel {
    ALERT_NONE, ALERT_WARNING, ALERT_DANGER,
};

template <typename T>
struct MeasurementStatistics {
    T average;
    T median;
    T max;
    T min;
};

struct DisplayRenderPayload {
    float chartYAxisLowTempCelsiusBound = 10.0;
    float chartYAxisHighTempCelsiusBound = 30.0;
    float chartYAxisLowHumidityBound = 0.0;
    float chartYAxisHighHumidityBound = 100.0;
    float gaugeTempCelsiusCenter = 20.0;
    float gaugeHumidityCenter = 70.0;

    uint64_t sdCardVolumeBytes = 0; // 0 = no sd card
    uint64_t sdCardOccupiedBytes = 0;
    struct tm timeinfo;
    float batteryLevel = 0.0;

    DegreesUnit degreesUnit = CELSIUS;
    float currentTemperatureCelsius = 0.0;
    float currentHumidity = 0.0;
    AlertLevel temperatureAlert = ALERT_NONE;
    AlertLevel humidityAlert = ALERT_NONE;
    MeasurementStatistics<float> statsT1D;
    MeasurementStatistics<float> statsT1W;
    MeasurementStatistics<float> statsT1M;
    MeasurementStatistics<float> statsH1D;
    MeasurementStatistics<float> statsH1W;
    MeasurementStatistics<float> statsH1M;

    float historyChartT[CHART_LEN_PX];
    float historyChartH[CHART_LEN_PX];
};
