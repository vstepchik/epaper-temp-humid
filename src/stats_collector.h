#pragma once

#include <Arduino.h>
#include <RingBuf.h>
#include <cstdint>
#include <AceSorting.h>

#include "common_types.h"
#include "esp32-hal.h"
#include "settings.h"


enum class UpdateFlags : uint16_t {
    NONE             = 0,
    CURRENT_READING  = 1 << 0, // 1
    STATS_DAY        = 1 << 1, // 2
    STATS_WEEK       = 1 << 2, // 4
    STATS_MONTH      = 1 << 3, // 8
    HISTORY_HOUR     = 1 << 4, // 16
    HISTORY_DAY      = 1 << 5, // 32
    HISTORY_WEEK     = 1 << 6, // 64
    HISTORY_MONTH    = 1 << 7, // 128
    HISTORY_YEAR     = 1 << 8, // 256
};

template <typename compact_t>
static inline compact_t pack(float value) {
  return (compact_t) (value * 100.0);
}

template <typename compact_t>
static inline float unpack(compact_t value) {
  return (float) value / 100.0;
}

template <typename compact_t>
static inline MeasurementStatistics<float> unpack(MeasurementStatistics<compact_t> value) {
  return MeasurementStatistics<float> {
    unpack(value.average),
    unpack(value.median),
    unpack(value.max),
    unpack(value.min),
  };
}

template<typename T, long S>
MeasurementStatistics<T> calculateStatistics(RingBuf<T, S>& buffer) {
    MeasurementStatistics<T> stats = {0, 0, 0, 0};
    if (buffer.isEmpty()) {
        return stats;
    }

    uint16_t bufferSize = buffer.size();
    T tempArray[bufferSize];  // For median calculation
    T sum = 0;
    T maxValue = buffer[0];
    T minValue = buffer[0];

    // Iterating through the buffer to calculate sum, max, and min
    for (uint16_t i = 0; i < bufferSize; ++i) {
        T currentValue = buffer[i];
        sum += currentValue;
        if (currentValue > maxValue) {
            maxValue = currentValue;
        }
        if (currentValue < minValue) {
            minValue = currentValue;
        }
        tempArray[i] = currentValue;  // Copying data for median calculation
    }

    // Calculate average
    stats.average = sum / bufferSize;

    // Calculate median
    ace_sorting::shellSortKnuth(tempArray, bufferSize);
    uint16_t middle = bufferSize / 2;
    if (bufferSize % 2 != 0) {
        stats.median = tempArray[middle];
    } else {
        stats.median = (tempArray[middle - 1] + tempArray[middle]) / 2;
    }

    // Set max and min
    stats.max = maxValue;
    stats.min = minValue;

    return stats;
}

template <typename compact_t>
class StatsCollector {
public:
  StatsCollector() : state([]() -> bool { return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED; }) {
  }

  void printDebug() {
    auto time = micros();

    Serial.println("====== Stats Collector Debug Info ======");
    Serial.print("lastCollectedAtUnixTimeSec: "); Serial.println(state.lastCollectedAtUnixTimeSec);
    printDebug<CURRENT_READING_MEDIAN_FILTER_SIZE>("currentReadingBufT", state.currentReadingBufT);
    printDebug<CURRENT_READING_MEDIAN_FILTER_SIZE>("currentReadingBufH", state.currentReadingBufH);
    printDebug<PX_PER_1H>("hourBufT", state.hourBufT);
    printDebug<PX_PER_1H>("hourBufH", state.hourBufH);
    printDebug<PX_PER_23H>("dayBufT", state.dayBufT);
    printDebug<PX_PER_23H>("dayBufH", state.dayBufH);
    printDebug<PX_PER_6D> ("weekBufT", state.weekBufT);
    printDebug<PX_PER_6D> ("weekBufH", state.weekBufH);
    printDebug<PX_PER_23D>("monthBufT", state.monthBufT);
    printDebug<PX_PER_23D>("monthBufH", state.monthBufH);
    printDebug<PX_PER_11M>("yearBufT", state.yearBufT);
    printDebug<PX_PER_11M>("yearBufH", state.yearBufH);

    time = micros() - time;
    Serial.print("StatsCollector::printDebug took "); Serial.print(time); Serial.println(" microseconds.");
    Serial.println("==== End Stats Collector Debug Info ====");
  }

  UpdateFlags collect(float temperature, float humidity) {
    state.currentReadingBufT.pushOverwrite(pack<compact_t>(temperature));
    state.currentReadingBufH.pushOverwrite(pack<compact_t>(humidity));
    
    time_t now;
    time(&now);
    time_t elapsedTimeSec = now - state.lastCollectedAtUnixTimeSec;
    state.lastCollectedAtUnixTimeSec = now;

    state.timeSinceLastHourBufPush += elapsedTimeSec;
    state.timeSinceLastDayBufPush += elapsedTimeSec;
    state.timeSinceLastWeekBufPush += elapsedTimeSec;
    state.timeSinceLastMonthBufPush += elapsedTimeSec;
    state.timeSinceLastYearBufPush += elapsedTimeSec;

    bool dayStatsUpdated = false;
    bool weekStatsUpdated = false;
    bool monthStatsUpdated = false;

    static const uint32_t hourBufPushInterval = 60/PX_PER_1H*60;
    if (state.timeSinceLastHourBufPush >= hourBufPushInterval) {
      state.timeSinceLastHourBufPush -= hourBufPushInterval;
      state.hourBufT.pushOverwrite(calculateStatistics<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE>(state.currentReadingBufT).median);
      state.hourBufH.pushOverwrite(calculateStatistics<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE>(state.currentReadingBufH).median);
      dayStatsUpdated = true;
    }

    static const uint32_t dayBufPushInterval = 60/(PX_PER_23H/(24-1))*60;
    if (state.timeSinceLastDayBufPush >= dayBufPushInterval) {
      state.timeSinceLastDayBufPush -= dayBufPushInterval;
      state.dayBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_1H>(state.hourBufT).median);
      state.dayBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_1H>(state.hourBufH).median);
      dayStatsUpdated = true;
    }

    static const uint32_t weekBufPushInterval = (7-1)*24/PX_PER_6D*60*60;
    if (state.timeSinceLastWeekBufPush >= weekBufPushInterval) {
      state.timeSinceLastWeekBufPush -= weekBufPushInterval;
      state.weekBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_23H>(state.dayBufT).median);
      state.weekBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_23H>(state.dayBufT).median);
      weekStatsUpdated = true;
    }

    static const uint32_t monthBufPushInterval = (24-1)*24/PX_PER_23D*60*60;
    if (state.timeSinceLastMonthBufPush >= monthBufPushInterval) {
      state.timeSinceLastMonthBufPush -= monthBufPushInterval;
      state.monthBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_6D>(state.weekBufT).median);
      state.monthBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_6D>(state.weekBufH).median);
      monthStatsUpdated = true;
    }

    static const uint32_t yearBufPushInterval = ((float)(12-1)*30)/PX_PER_11M*60*60*24;
    if (state.timeSinceLastYearBufPush >= yearBufPushInterval) {
      state.timeSinceLastYearBufPush -= yearBufPushInterval;
      state.yearBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_23D>(state.monthBufT).median);
      state.yearBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_23D>(state.monthBufH).median);
    }

    if (dayStatsUpdated) {

    }

    if (weekStatsUpdated) {

    }

    if (monthStatsUpdated) {

    }

    return UpdateFlags::CURRENT_READING;
  }

  void currentReadingMedian(float* temp, float* humidity) {
    *temp = unpack(state.statsTempCurrent.median);
    *humidity = unpack(state.statsHumidityCurrent.median);
  }

  MeasurementStatistics<float> statsTemp1D() {
    return unpack(state.statsTemp1D);
  }

  MeasurementStatistics<float> statsTemp1W() {
    return unpack(state.statsTemp1W);
  }

  MeasurementStatistics<float> statsTemp1M() {
    return unpack(state.statsTemp1M);
  }

  MeasurementStatistics<float> statsHumidity1D() {
    return unpack(state.statsHumidity1D);
  }

  MeasurementStatistics<float> statsHumidity1W() {
    return unpack(state.statsHumidity1W);
  }

  MeasurementStatistics<float> statsHumidity1M() {
    return unpack(state.statsHumidity1M);
  }

private:
  struct State {
    time_t lastCollectedAtUnixTimeSec;
    time_t timeSinceLastHourBufPush;
    time_t timeSinceLastDayBufPush;
    time_t timeSinceLastWeekBufPush;
    time_t timeSinceLastMonthBufPush;
    time_t timeSinceLastYearBufPush;

    RingBuf<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE> currentReadingBufT;
    RingBuf<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE> currentReadingBufH;
    RingBuf<compact_t, PX_PER_1H>  hourBufT;
    RingBuf<compact_t, PX_PER_1H>  hourBufH;
    RingBuf<compact_t, PX_PER_23H> dayBufT;
    RingBuf<compact_t, PX_PER_23H> dayBufH;
    RingBuf<compact_t, PX_PER_6D>  weekBufT;
    RingBuf<compact_t, PX_PER_6D>  weekBufH;
    RingBuf<compact_t, PX_PER_23D> monthBufT;
    RingBuf<compact_t, PX_PER_23D> monthBufH;
    RingBuf<compact_t, PX_PER_11M> yearBufT;
    RingBuf<compact_t, PX_PER_11M> yearBufH;

    MeasurementStatistics<compact_t> statsTempCurrent;
    MeasurementStatistics<compact_t> statsTemp1D;
    MeasurementStatistics<compact_t> statsTemp1W;
    MeasurementStatistics<compact_t> statsTemp1M;
    MeasurementStatistics<compact_t> statsHumidityCurrent;
    MeasurementStatistics<compact_t> statsHumidity1D;
    MeasurementStatistics<compact_t> statsHumidity1W;
    MeasurementStatistics<compact_t> statsHumidity1M;

    State(bool (*initHelper)(void)) 
    : currentReadingBufT(initHelper),
      currentReadingBufH(initHelper),
      hourBufT(initHelper),
      hourBufH(initHelper),
      dayBufT(initHelper),
      dayBufH(initHelper),
      weekBufT(initHelper),
      weekBufH(initHelper),
      monthBufT(initHelper),
      monthBufH(initHelper),
      yearBufT(initHelper),
      yearBufH(initHelper) {
        // Constructor body, if needed
    }
  };

  State state;

  template<size_t S>
  void printDebug(const char* name, RingBuf<compact_t, S>& buf) {
    Serial.print(name); 
    Serial.print(": ["); 
    for (uint8_t i = 0; i < buf.size(); ++i) { 
      Serial.print(buf[i]); 
      i+1 < buf.size() && Serial.print(", "); 
    } Serial.print("] ("); 
    Serial.print(buf.size()); 
    Serial.print(" / "); 
    Serial.print(buf.maxSize());
    Serial.println(")"); 
  }
};
