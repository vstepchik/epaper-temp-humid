#pragma once

#include "Arduino.h"
#include <GxEPD2_BW.h>
#include "common_types.h"
#include "Utils.h"

#include "fnt_04b03b.h"
#include "tiniest_num42.h"
#include "fnt_big_digits.h"
#include <Fonts/TomThumb.h>

class DisplayController {
public:
  DisplayController();
  void debug_print(char* txt);
  void full_repaint(DisplayRenderPayload* data);

private:
  GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display;
  void drawStatusBar(DisplayRenderPayload* data);
  void drawGauges(DegreesUnit tempUnit, float tempMiddlePointCelsius, float humidityMiddlePointValue, float currentTempConverted, float currentHumidity);
  void drawCurrentValues(DisplayRenderPayload* data, const float currentTemp, const char unitSymbol);
  void drawStats(unsigned char x, unsigned char y, MeasurementStatistics stats);
  void drawHistoryGraph(DisplayRenderPayload* data, const char unitSymbol);
};
