#pragma once

#include <Arduino.h>
#include <GxEPD2.h>
#include <GxEPD2_BW.h>
#include <type_traits>
#include "time.h"
#include "common_types.h"
#include "utils.h"
#include "bitmaps.h"

#include "fnt_04b03b.h"
#include "tiniest_num42.h"
#include "fnt_big_digits.h"
#include <Fonts/TomThumb.h>

class DisplayController {
public:
  enum class DrawFlags : uint16_t {
    NONE             = 0,
    FULL             = 1 << 0, // 1
    SD_CARD          = 1 << 1, // 2
    BATTERY          = 1 << 2, // 4
    TIME             = 1 << 3, // 8
    GAUGES           = 1 << 4, // 16
    CURRENT_READINGS = 1 << 5, // 32
    STATISTICS       = 1 << 6, // 64
    HISTORY_GRAPH    = 1 << 7, // 128
  };

  DisplayController(bool initial);
  void debug_print(char* txt);
  void repaint(const DrawFlags drawFlags, DisplayRenderPayload* data);

private:
  GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;

  uint32_t repaintCounter;

  void drawBackground(DisplayRenderPayload* data);

  void drawStatusBar(DisplayRenderPayload* data);

  void drawGauges(DegreesUnit tempUnit, float tempMiddlePointCelsius, float humidityMiddlePointValue, float currentTempConverted, float currentHumidity);

  void drawCurrentReadings(DisplayRenderPayload* data, const float currentTemp, const char unitSymbol);

  void drawAllStats(DisplayRenderPayload* data);

  template<typename StatsConversion>
  void drawStats(unsigned char x, unsigned char y, MeasurementStatistics<float> stats, StatsConversion conversion);

  void drawHistoryGraph(DisplayRenderPayload* data, const char unitSymbol);
};

inline DisplayController::DrawFlags operator|(DisplayController::DrawFlags a, DisplayController::DrawFlags b) {
  return static_cast<DisplayController::DrawFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline DisplayController::DrawFlags operator&(DisplayController::DrawFlags a, DisplayController::DrawFlags b) {
  return static_cast<DisplayController::DrawFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline DisplayController::DrawFlags operator^(DisplayController::DrawFlags a, DisplayController::DrawFlags b) {
  return static_cast<DisplayController::DrawFlags>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}

inline DisplayController::DrawFlags operator~(DisplayController::DrawFlags a) {
  return static_cast<DisplayController::DrawFlags>(~static_cast<uint16_t>(a));
}

inline bool isFlagSet(DisplayController::DrawFlags flags, DisplayController::DrawFlags flagToCheck) {
  return (flags & flagToCheck) == flagToCheck;
}
