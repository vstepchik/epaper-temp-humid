#pragma once

#include <cstdint>
#include <sys/types.h>

enum DegreesUnit {
    CELSIUS, FARENHEIT,
};

enum AlertLevel {
    ALERT_NONE, ALERT_WARNING, ALERT_DANGER,
};

struct SensorReading {
    float tempCelsius;
    float humidity;
};

struct MeasurementStatistics {
    float average;
    float median;
    float max;
    float min;
};

struct DisplayRenderPayload {
    float chartYAxisLowTempCelsiusBound = -10.0;
    float chartYAxisHighTempCelsiusBound = 30.0;
    float chartYAxisLowHumidityBound = 0.0;
    float chartYAxisHighHumidityBound = 100.0;
    float gaugeTempCelsiusCenter = 20.0;
    float gaugeHumidityCenter = 70.0;

    uint64_t sdCardVolumeBytes = 0; // 0 = no sd card
    uint64_t sdCardOccupiedBytes = 0;
    uint32_t utcTimeSecEpoch = 0;
    float batteryLevel = 0.0;

    DegreesUnit degreesUnit = CELSIUS;
    float currentTemperatureCelsius = 0.0;
    float currentHumidity = 0.0;
    AlertLevel temperatureAlert = ALERT_NONE;
    AlertLevel humidityAlert = ALERT_NONE;
    MeasurementStatistics statsT1D;
    MeasurementStatistics statsT1W;
    MeasurementStatistics statsT1M;
    MeasurementStatistics statsH1D;
    MeasurementStatistics statsH1W;
    MeasurementStatistics statsH1M;
    // todo: add references to arrays for chart data
};
