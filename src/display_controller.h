#pragma once

#include <Arduino.h>
#include <GxEPD2.h>
#include <GxEPD2_BW.h>
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
  DisplayController(bool initial);
  void debug_print(char* txt);
  void repaint(bool fullRepaint, DisplayRenderPayload* data);

private:
  GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;

  uint32_t repaintCounter;

  void drawBackground(DisplayRenderPayload* data);

  void drawStatusBar(DisplayRenderPayload* data);

  void drawGauges(DegreesUnit tempUnit, float tempMiddlePointCelsius, float humidityMiddlePointValue, float currentTempConverted, float currentHumidity);

  void drawCurrentValues(DisplayRenderPayload* data, const float currentTemp, const char unitSymbol);

  void drawAllStats(DisplayRenderPayload* data);

  template<typename StatsConversion>
  void drawStats(unsigned char x, unsigned char y, MeasurementStatistics stats, StatsConversion conversion);

  void drawHistoryGraph(DisplayRenderPayload* data, const char unitSymbol);
};
