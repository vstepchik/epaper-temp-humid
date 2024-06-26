#pragma once

#include <Arduino.h>
#include <RingBuf.h>
#include <cstdint>
#include <AceSorting.h>

#include "HardwareSerial.h"
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

inline UpdateFlags operator|(UpdateFlags a, UpdateFlags b) {
  return static_cast<UpdateFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline UpdateFlags& operator|=(UpdateFlags& a, UpdateFlags b) {
    a = a | b;
    return a;
}

inline UpdateFlags operator&(UpdateFlags a, UpdateFlags b) {
  return static_cast<UpdateFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline UpdateFlags operator^(UpdateFlags a, UpdateFlags b) {
  return static_cast<UpdateFlags>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}

inline UpdateFlags operator~(UpdateFlags a) {
  return static_cast<UpdateFlags>(~static_cast<uint16_t>(a));
}

inline bool isFlagSet(UpdateFlags flags, UpdateFlags flagToCheck) {
  return (flags & flagToCheck) == flagToCheck;
}

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
MeasurementStatistics<T> calculateStatistics(RingBuf<T, S>& buffer, long onlyOldestNEntries = S) {
    MeasurementStatistics<T> stats = {0, 0, 0, 0};
    if (buffer.isEmpty()) {
        return stats;
    }

    uint16_t bufferSize = std::min(static_cast<uint16_t>(buffer.size()), static_cast<uint16_t>(onlyOldestNEntries));
    T tempArray[bufferSize];  // Adjusted size for median calculation
    long sum = 0;
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

    // Set avg, max and min
    stats.average = sum / bufferSize;
    stats.max = maxValue;
    stats.min = minValue;

    // Calculate median
    ace_sorting::shellSortKnuth(tempArray, bufferSize);
    uint16_t middle = bufferSize / 2;
    if (bufferSize % 2 != 0) {
        stats.median = tempArray[middle];
    } else {
        stats.median = (tempArray[middle - 1] + tempArray[middle]) / 2;
    }
    return stats;
}

template <typename compact_t>
class StatsCollector {
public:
  StatsCollector(bool initial) : state([]() -> bool { return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED; }) {
    if (initial) {
      // prepare to push as soon as the previous buffer is full
      state.timeSinceLastHourBufPush = hourBufPushInterval - 1;
      state.timeSinceLastDayBufPush = dayBufPushInterval - 1;
      state.timeSinceLastWeekBufPush = weekBufPushInterval - 1;
      state.timeSinceLastMonthBufPush = monthBufPushInterval - 1;
      state.timeSinceLastYearBufPush = yearBufPushInterval - 1;
    }
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

    Serial.println("---");
    Serial.print("Tday:  "); Serial.print("A="); Serial.print("M="); Serial.print("^="); Serial.println("v=");

    time = micros() - time;
    Serial.print("StatsCollector::printDebug took "); Serial.print(time); Serial.println(" microseconds.");
    Serial.println("==== End Stats Collector Debug Info ====");
  }

  UpdateFlags collect(float temperature, float humidity) {
    state.currentReadingBufT.pushOverwrite(pack<compact_t>(temperature));
    state.currentReadingBufH.pushOverwrite(pack<compact_t>(humidity));
    auto prevTemp = state.statsTempCurrent;
    auto prevHumidity = state.statsHumidityCurrent;
    state.statsTempCurrent = state.currentReadingBufT[state.currentReadingBufT.size() - 1];
    state.statsHumidityCurrent = state.currentReadingBufH[state.currentReadingBufH.size() - 1];
    UpdateFlags updateFlags = UpdateFlags::NONE;
    if (state.statsTempCurrent != prevTemp || state.statsHumidityCurrent != prevHumidity) {
      updateFlags |= UpdateFlags::CURRENT_READING;
    }

    time_t now;
    time(&now);
    time_t elapsedTimeSec = now - state.lastCollectedAtUnixTimeSec;
    state.lastCollectedAtUnixTimeSec = now;

    // push readings only if the previous buffers are full
    state.timeSinceLastHourBufPush += elapsedTimeSec * state.currentReadingBufT.isFull();
    state.timeSinceLastDayBufPush += elapsedTimeSec * state.hourBufT.isFull();
    state.timeSinceLastWeekBufPush += elapsedTimeSec * state.dayBufT.isFull();
    state.timeSinceLastMonthBufPush += elapsedTimeSec * state.weekBufT.isFull();
    state.timeSinceLastYearBufPush += elapsedTimeSec * state.monthBufT.isFull();

    if (state.timeSinceLastHourBufPush >= hourBufPushInterval) {
      state.timeSinceLastHourBufPush -= hourBufPushInterval;
      state.hourBufT.pushOverwrite(calculateStatistics<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE>(state.currentReadingBufT).median);
      state.hourBufH.pushOverwrite(calculateStatistics<compact_t, CURRENT_READING_MEDIAN_FILTER_SIZE>(state.currentReadingBufH).median);
      updateFlags |= UpdateFlags::HISTORY_HOUR;
    }

    if (state.timeSinceLastDayBufPush >= dayBufPushInterval) {
      state.timeSinceLastDayBufPush -= dayBufPushInterval;
      state.dayBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_1H>(state.hourBufT, 30/2).median);
      state.dayBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_1H>(state.hourBufH, 30/2).median);
      updateFlags |= UpdateFlags::HISTORY_DAY;
    }

    if (state.timeSinceLastWeekBufPush >= weekBufPushInterval) {
      state.timeSinceLastWeekBufPush -= weekBufPushInterval;
      state.weekBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_23H>(state.dayBufT, 4*60/30).median);
      state.weekBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_23H>(state.dayBufH, 4*60/30).median);
      updateFlags |= UpdateFlags::HISTORY_WEEK;
    }

    if (state.timeSinceLastMonthBufPush >= monthBufPushInterval) {
      state.timeSinceLastMonthBufPush -= monthBufPushInterval;
      state.monthBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_6D>(state.weekBufT, 8/4).median);
      state.monthBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_6D>(state.weekBufH, 8/4).median);
      updateFlags |= UpdateFlags::HISTORY_MONTH;
    }

    if (state.timeSinceLastYearBufPush >= yearBufPushInterval) {
      state.timeSinceLastYearBufPush -= yearBufPushInterval;
      state.yearBufT.pushOverwrite(calculateStatistics<compact_t, PX_PER_23D>(state.monthBufT, 7*24/8).median);
      state.yearBufH.pushOverwrite(calculateStatistics<compact_t, PX_PER_23D>(state.monthBufH, 7*24/8).median);
      updateFlags |= UpdateFlags::HISTORY_YEAR;
    }

    // update d/w/m statistics every hour
    if (isFlagSet(updateFlags, UpdateFlags::HISTORY_HOUR)) {
      auto hourT = calculateStatistics<compact_t, PX_PER_1H>(state.hourBufT).median;
      auto hourH = calculateStatistics<compact_t, PX_PER_1H>(state.hourBufH).median;
      
      RingBuf<compact_t, PX_PER_23H+1> fullDay;
      fullDay.pushOverwrite(hourT);
      for (int i = 0; i < state.dayBufT.size(); ++i) { fullDay.pushOverwrite(state.dayBufT[i]); }
      state.statsTemp1D = calculateStatistics<compact_t, PX_PER_23H+1>(fullDay);
      fullDay.clear();
      fullDay.pushOverwrite(hourH);
      for (int i = 0; i < state.dayBufH.size(); ++i) { fullDay.pushOverwrite(state.dayBufH[i]); }
      state.statsHumidity1D = calculateStatistics<compact_t, PX_PER_23H+1>(fullDay);
      updateFlags |= UpdateFlags::STATS_DAY;

      RingBuf<compact_t, PX_PER_6D+1> fullWeek;
      fullWeek.pushOverwrite(state.statsTemp1D.median);
      for (int i = 0; i < state.weekBufT.size(); ++i) { fullWeek.pushOverwrite(state.weekBufT[i]); }
      state.statsTemp1W = calculateStatistics<compact_t, PX_PER_6D+1>(fullWeek);
      fullWeek.clear();
      fullWeek.pushOverwrite(state.statsHumidity1D.median);
      for (int i = 0; i < state.weekBufH.size(); ++i) { fullWeek.pushOverwrite(state.weekBufH[i]); }
      state.statsHumidity1W = calculateStatistics<compact_t, PX_PER_6D+1>(fullWeek);
      updateFlags |= UpdateFlags::STATS_WEEK;

      RingBuf<compact_t, PX_PER_23D+1> fullMonth;
      fullMonth.pushOverwrite(state.statsTemp1W.median);
      for (int i = 0; i < state.monthBufT.size(); ++i) { fullMonth.pushOverwrite(state.monthBufT[i]); }
      state.statsTemp1M = calculateStatistics<compact_t, PX_PER_23D+1>(fullMonth);
      fullMonth.clear();
      fullMonth.pushOverwrite(state.statsHumidity1W.median);
      for (int i = 0; i < state.monthBufH.size(); ++i) { fullMonth.pushOverwrite(state.monthBufH[i]); }
      state.statsHumidity1M = calculateStatistics<compact_t, PX_PER_23D+1>(fullMonth);
      updateFlags |= UpdateFlags::STATS_MONTH;
    }

    return updateFlags;
  }

  void currentReadingMedian(float* temp, float* humidity) {
    *temp = unpack(state.statsTempCurrent);
    *humidity = unpack(state.statsHumidityCurrent);
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

  void getHistoryChartDataT(float (&values)[CHART_LEN_PX]) {
    int v = 0, b = state.hourBufT.size();
    while (b > 0) values[v++] = unpack(state.hourBufT[--b]);
    b = state.dayBufT.size();
    while (b > 0) values[v++] = unpack(state.dayBufT[--b]);
    b = state.weekBufT.size();
    while (b > 0) values[v++] = unpack(state.weekBufT[--b]);
    b = state.monthBufT.size();
    while (b > 0) values[v++] = unpack(state.monthBufT[--b]);
    b = state.yearBufT.size();
    while (b > 0) values[v++] = unpack(state.yearBufT[--b]);
    while (v < CHART_LEN_PX) values[v++] = NAN;
  }

  void getHistoryChartDataH(float (&values)[CHART_LEN_PX]) {
    int v = 0, b = state.hourBufH.size();
    while (b > 0) values[v++] = unpack(state.hourBufH[--b]);
    b = state.dayBufH.size();
    while (b > 0) values[v++] = unpack(state.dayBufH[--b]);
    b = state.weekBufH.size();
    while (b > 0) values[v++] = unpack(state.weekBufH[--b]);
    b = state.monthBufH.size();
    while (b > 0) values[v++] = unpack(state.monthBufH[--b]);
    b = state.yearBufH.size();
    while (b > 0) values[v++] = unpack(state.yearBufH[--b]);
    while (v < CHART_LEN_PX) values[v++] = NAN;
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

    compact_t statsTempCurrent;
    MeasurementStatistics<compact_t> statsTemp1D;
    MeasurementStatistics<compact_t> statsTemp1W;
    MeasurementStatistics<compact_t> statsTemp1M;
    compact_t statsHumidityCurrent;
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
      yearBufH(initHelper) {}
  };

  State state;
  static const uint32_t hourBufPushInterval = 60/PX_PER_1H*60; // 2m/px, full buf = 1h
  static const uint32_t dayBufPushInterval = 60/(PX_PER_23H/(24-1))*60; // 30m/px, full buf = 23h
  static const uint32_t weekBufPushInterval = (7-1)*24/PX_PER_6D*60*60; // 4h/px, full buf = 6d
  static const uint32_t monthBufPushInterval = (24-1)*24/PX_PER_23D*60*60; // 8h/px, full buf = 23d
  static const uint32_t yearBufPushInterval = ((float)(12-1)*30)/PX_PER_11M*60*60*24; // 7d/px, full buf = 11M

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
