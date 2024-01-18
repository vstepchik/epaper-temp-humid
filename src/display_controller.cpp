#include "Arduino.h"
#include "common_types.h"
#include "display_controller.h"
#include "esp32-hal.h"
#include "settings.h"
#include <cstdint>

static const char y04b = Font_04b03b.yAdvance / 2 + 1;

DisplayController::DisplayController(bool initial) : display(GxEPD2_213_B74(5, 17, 16, 4)) {
    display.init(0, initial);
    display.setRotation(3);
    display.setFullWindow();
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);
}

void DisplayController::debug_print(char* txt) {
    display.setFullWindow();
    display.firstPage();
    do {
        unsigned long timestamp = micros();
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&Font_04b03b);
        display.setCursor(0, y04b);
        display.print(txt);

        // display draw time
        display.setFont(&Font_04b03b);
        display.setCursor(0, display.height());
        display.print(micros() - timestamp);
    } while (display.nextPage());

    display.hibernate();
}

void DisplayController::repaint(const DrawFlags drawFlags, DisplayRenderPayload* data) {
    const float currentTemp = celsiusTo(data->currentTemperatureCelsius, data->degreesUnit);
    const char unitSymbol = data->degreesUnit == CELSIUS ? 'C' : 'F';
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];

    unsigned long timestampFullRepaint = micros();

    if (isFlagSet(drawFlags, DrawFlags::FULL)) repaintCounter = 0;
    if (repaintCounter++ % N_UPDATES_BETWEEN_FULL_REPAINTS == 0 || isFlagSet(drawFlags, DrawFlags::FULL)) {
        display.setFullWindow();
    } else {
        struct Rect { 
            uint8_t x, y, w, h; 
            inline Rect operator+=(const Rect other) {
                return Rect {
                    x = min(this->x, other.x),
                    y = min(this->y, other.y),
                    w = max(this->x + this->w, other.x + other.w) - min(this->x, other.x),
                    h = max(this->y + this->h, other.y + other.h) - min(this->y, other.y),
                };
            }
        };
        // 0  11  140  56
        Rect drawArea = Rect { .x=255, .y=255, .w=0, .h=0 };
        if (isFlagSet(drawFlags, DrawFlags::SD_CARD)) drawArea += Rect { .x=67, .y=0, .w=72, .h=5 }; // (0, 0, 256, 5)
        if (isFlagSet(drawFlags, DrawFlags::BATTERY)) drawArea += Rect { .x=0, .y=0, .w=36, .h=5 }; // (0, 0, 256, 5)
        if (isFlagSet(drawFlags, DrawFlags::TIME)) drawArea += Rect { .x=170, .y=0, .w=72, .h=5 }; // (0, 0, 256, 5)
        if (isFlagSet(drawFlags, DrawFlags::GAUGES)) drawArea += Rect { .x=0, .y=13, .w=105, .h=51 }; // (0, 13, 105, 51)
        if (isFlagSet(drawFlags, DrawFlags::CURRENT_READINGS)) drawArea += Rect { .x=106, .y=11, .w=34, .h=56 }; // (106, 11, 34, 56)
        if (isFlagSet(drawFlags, DrawFlags::STATISTICS)) drawArea += Rect { .x=143, .y=13, .w=107, .h=43 }; // (143, 13, 107, 43)
        if (isFlagSet(drawFlags, DrawFlags::HISTORY_GRAPH)) drawArea += Rect { .x=0, .y=64, .w=255, .h=58 }; // (0, 64, 250, 58)
        display.setPartialWindow(drawArea.x, drawArea.y, drawArea.w, drawArea.h);
    }

    display.firstPage();
    do {
        drawStatusBar(data);
        drawGauges(
            data->degreesUnit,
            data->gaugeTempCelsiusCenter,
            data->gaugeHumidityCenter, 
            currentTemp, 
            data->currentHumidity
        );
        drawCurrentReadings(data, currentTemp, unitSymbol);
        drawAllStats(data);
        drawHistoryGraph(data, unitSymbol);
    } while (display.nextPage());
    display.hibernate();

    Serial.print("Repaint full time: ");
    Serial.print((micros() - timestampFullRepaint) / 1000.0f, 1);
    Serial.println("ms");
}

void DisplayController::drawGauges(
    DegreesUnit tempUnit, 
    float tempMiddlePointCelsius, 
    float humidityMiddlePointValue, 
    float currentTempConverted, 
    float currentHumidity
) {
    const float minTemp = celsiusTo(tempMiddlePointCelsius - 12, tempUnit);
    const float maxTemp = celsiusTo(tempMiddlePointCelsius + 12, tempUnit);
    const float minHum = humidityMiddlePointValue - 25;
    const float maxHum = humidityMiddlePointValue + 25;
    const float tempArrX = constrain((currentTempConverted - minTemp) / (maxTemp - minTemp) * 100.0, 1, 99);
    const float humArrX = constrain((currentHumidity - minHum) / (maxHum - minHum) * 100.0, 1, 99);

    unsigned long timestamp = micros();
    // gauges - labels
    display.setFont(&Font_04b03b);
    float tempValues[3] = {
        celsiusTo(tempMiddlePointCelsius - 12, tempUnit),
        celsiusTo(tempMiddlePointCelsius, tempUnit),
        celsiusTo(tempMiddlePointCelsius + 12, tempUnit),
    };
    float humValues[3] = {
        humidityMiddlePointValue - 25,
        humidityMiddlePointValue,
        humidityMiddlePointValue + 25,
    };
    char buf[5];
    unsigned char xPositions[3] = {0, 47, 96};
    for (unsigned char i = 0; i < 3; ++i) {
        snprintf(buf, sizeof(buf), "%.0f", tempValues[i]);
        display.setCursor(xPositions[i], 13 + y04b);
        display.print(buf);
        snprintf(buf, sizeof(buf), "%.0f", humValues[i]);
        display.setCursor(xPositions[i], 54 + y04b);
        display.print(buf);
    }
    // gauges
    display.drawInvertedBitmap(2, 19, bmp_gauge_t_bg, 101, 16, GxEPD_BLACK);
    display.drawInvertedBitmap(2, 37, bmp_gauge_h_bg, 101, 16, GxEPD_BLACK);
    // gauges - arrows
    // gauges - arrows - temperature
    display.fillRect(tempArrX, 20, 5, 9, GxEPD_WHITE);
    display.drawInvertedBitmap(tempArrX, 20, bmp_arrow_thin_dn, 5, 9, GxEPD_BLACK);
    // gauges - arrows - humidity
    display.fillRect(humArrX, 43, 5, 9, GxEPD_WHITE);
    display.drawInvertedBitmap(humArrX, 43, bmp_arrow_thin_up, 5, 9, GxEPD_BLACK);
    // gauges - arrows - patch overlaps
    display.fillRect(0, 19, 2, 16, GxEPD_WHITE); display.drawFastVLine(2, 19, 16, GxEPD_BLACK);
    display.fillRect(0, 37, 2, 16, GxEPD_WHITE); display.drawFastVLine(2, 37, 16, GxEPD_BLACK);
    display.fillRect(103, 19, 2, 16, GxEPD_WHITE); display.drawFastVLine(102, 19, 16, GxEPD_BLACK);
    display.fillRect(103, 37, 2, 16, GxEPD_WHITE); display.drawFastVLine(102, 37, 16, GxEPD_BLACK);
    Serial.print("Repaint - gauges: ");
    Serial.println(micros() - timestamp);
}

void DisplayController::drawStatusBar(DisplayRenderPayload* data) {
    int16_t tbx, tby; uint16_t tbw, tbh;
    char occupiedBuf[7], volumeBuf[7], buf[20];
    display.setFont(&Font_04b03b);
    // sd card
    // sd card - icon
    display.drawInvertedBitmap(67, 0, bmp_sd_card_icon, 4, 5, GxEPD_BLACK);
    // sd card - label
    if (data->sdCardVolumeBytes > 0) {
        formatSize(data->sdCardOccupiedBytes, occupiedBuf, sizeof(occupiedBuf));
        formatSize(data->sdCardVolumeBytes, volumeBuf, sizeof(volumeBuf));
        snprintf(buf, sizeof(buf), "%s/%s", occupiedBuf, volumeBuf);
    } else {
        snprintf(buf, sizeof(buf), "[N/A]");
    }
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);

    unsigned long timestamp = micros();
    display.setCursor(72, y04b);
    display.print(buf);
    Serial.print("Repaint - sd card: ");
    Serial.println(micros() - timestamp);

    // time
    strftime(buf, sizeof(buf), "%Y-%m-%d - %H:%M", &data->timeinfo);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    timestamp = micros();
    display.setCursor(170, y04b);
    display.print(buf);
    Serial.print("Repaint - time: ");
    Serial.println(micros() - timestamp);

    // battery - picture
    const uint8_t batX = 0;
    const uint8_t batY = 0;
    timestamp = micros();
    display.drawInvertedBitmap(batX, batY, bmp_bat_full, 14, 5, GxEPD_BLACK);
    int8_t bat_pixels_empty = (1.0 - data->batteryLevel) * 11;
    display.fillRect(batX+12-bat_pixels_empty, batY+1, bat_pixels_empty, 3, GxEPD_WHITE);
    if (bat_pixels_empty > 0) display.drawPixel(batX-1-bat_pixels_empty, batY+1, GxEPD_BLACK);
    // battery - label
    display.setCursor(batX+16, batY+y04b);
    snprintf(buf, sizeof(buf), "%.0f%%", data->batteryLevel * (float)100);
    display.print(buf);
    Serial.print("Repaint - battery: ");
    Serial.println(micros() - timestamp);
}

void DisplayController::drawAllStats(DisplayRenderPayload* data) {
    // stats
    const uint8_t statX = 143, statY = 13;
    auto tempConversion = [&data](float input) -> float { return celsiusTo(input, data->degreesUnit); };
    auto humConversion = [](float input) -> float { return input; };
    unsigned long timestamp = micros();

    // statistics
    display.drawRect(statX+3, statY+4, 104, 39, GxEPD_BLACK);
    display.fillRect(statX, statY, 16, 15, GxEPD_WHITE);
    display.drawInvertedBitmap(statX, statY, bmp_stats_hint, 16, 15, GxEPD_BLACK);
    // statistics - grid lines
    display.drawFastHLine(statX+16, statY+14, 90, GxEPD_BLACK);
    display.drawFastHLine(statX+4, statY+28, 102, GxEPD_BLACK);
    display.drawFastVLine(statX+10, statY+15, 27, GxEPD_BLACK);
    display.drawFastVLine(statX+42, statY+5, 37, GxEPD_BLACK);
    display.drawFastVLine(statX+74, statY+5, 37, GxEPD_BLACK);
    for (unsigned char i = 0; i < 3; ++i) {
        display.drawPixel(statX+26 + i*32, 28, GxEPD_BLACK);
        display.drawPixel(statX+26 + i*32, 40, GxEPD_BLACK);
        display.drawPixel(statX+26 + i*32, 42, GxEPD_BLACK);
        display.drawPixel(statX+26 + i*32, 54, GxEPD_BLACK);
    }
    // statistics - labels
    display.setFont(&Font_04b03b);
    display.setCursor(statX+5, statY+19 + y04b);
    display.print(data->degreesUnit == CELSIUS ? 'C' : 'F');
    display.setCursor(statX+5, statY+33 + y04b);
    display.print(F("H"));
    display.setCursor(statX+20, statY+7 + y04b);
    display.print(F("DAY"));
    display.setCursor(statX+48, statY+7 + y04b);
    display.print(F("WEEK"));
    display.setCursor(statX+78, statY+7 + y04b);
    display.print(F("MONTH"));

    // statistics - individual values
    display.setFont(&TomThumb);
    drawStats(statX+11+32*0, statY+15+14*0, data->statsT1D, tempConversion);
    drawStats(statX+11+32*1, statY+15+14*0, data->statsT1W, tempConversion);
    drawStats(statX+11+32*2, statY+15+14*0, data->statsT1M, tempConversion);
    drawStats(statX+11+32*0, statY+15+14*1, data->statsH1D, humConversion);
    drawStats(statX+11+32*1, statY+15+14*1, data->statsH1W, humConversion);
    drawStats(statX+11+32*2, statY+15+14*1, data->statsH1M, humConversion);
    Serial.print("Repaint - all stats: ");
    Serial.println(micros() - timestamp);
}

template<typename StatsConversion>
void DisplayController::drawStats(unsigned char x, unsigned char y, MeasurementStatistics stats, StatsConversion conversion) {
    const uint8_t adv = TomThumb.yAdvance;
    display.setCursor(x + 1, y + adv);
    display.print(conversion(stats.average), 1);

    display.setCursor(x + 1, y + adv + 6);
    display.print(conversion(stats.median), 1);

    display.setCursor(x + 17, y + adv);
    display.print(conversion(stats.max), 1);

    display.setCursor(x + 17, y + adv + 6);
    display.print(conversion(stats.min), 1);
}

void DisplayController::drawCurrentReadings(DisplayRenderPayload* data, const float currentTemp, const char unitSymbol) {
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];
    unsigned long timestamp = micros();
    // current values
    display.setFont(&big_digits);
    snprintf(buf, sizeof(buf), "%.1f", currentTemp);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(132 - tbw, 24 + big_digits.yAdvance);
    display.print(buf);
    snprintf(buf, sizeof(buf), "%.1f", data->currentHumidity);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(132 - tbw, 39 + big_digits.yAdvance);
    display.print(buf);

    display.drawCircle(134, 25, 1, GxEPD_BLACK);
    display.setFont(&Font_04b03b);
    display.setCursor(134, 33);
    display.print(unitSymbol);
    display.drawLine(134, 47, 138, 43, GxEPD_BLACK);
    display.drawRect(134, 43, 2, 2, GxEPD_BLACK);
    display.drawRect(137, 46, 2, 2, GxEPD_BLACK);
    // alerts
    switch (data->temperatureAlert) {
        case ALERT_DANGER:
            display.drawInvertedBitmap(118, 11, bmp_danger, 9, 11, GxEPD_BLACK);
            break;
        case ALERT_WARNING:
            display.drawInvertedBitmap(117, 11, bmp_warning, 11, 11, GxEPD_BLACK);
            break;
        case ALERT_NONE:
        default:
            break;
    };
    switch (data->humidityAlert) {
        case ALERT_DANGER:
            display.drawInvertedBitmap(118, 50, bmp_danger, 9, 11, GxEPD_BLACK);
            break;
        case ALERT_WARNING:
            display.drawInvertedBitmap(117, 50, bmp_warning, 11, 11, GxEPD_BLACK);
            break;
        case ALERT_NONE:
        default:
            break;
    };

    Serial.print("Repaint - current values: ");
    Serial.println(micros() - timestamp);
}

void DisplayController::drawHistoryGraph(DisplayRenderPayload* data, const char unitSymbol) {
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];
    unsigned long timestamp = micros();
    // graph - lines
    const unsigned char graph_stops_count = 6;
    const unsigned char graph_stops[graph_stops_count] = {3, 51, 120, 156, 202, 232};
    // graph - lines - horizontal
    display.drawFastHLine(0, 87, 250, GxEPD_BLACK);
    display.drawFastHLine(0, 115, 250, GxEPD_BLACK);
    for (unsigned char i = 4; i < 233; i += 4) {
        display.drawPixel(i, 66, GxEPD_BLACK);
        display.drawPixel(i, 94, GxEPD_BLACK);
    }
    display.drawFastHLine(233, 67, 17, GxEPD_BLACK);
    display.drawFastHLine(233, 95, 17, GxEPD_BLACK);
    // graph - lines - vertical
    display.drawFastVLine(3, 66, 20, GxEPD_BLACK);
    display.drawFastVLine(3, 94, 20, GxEPD_BLACK);
    display.drawFastVLine(233, 69, 17, GxEPD_BLACK);
    display.drawFastVLine(233, 97, 17, GxEPD_BLACK);
    for (unsigned char i = 0; i < 18; i += 3) {
        display.drawPixel(3, 68 + i, GxEPD_WHITE);
        display.drawPixel(3, 96 + i, GxEPD_WHITE);
        display.drawPixel(233, 68 + i, GxEPD_WHITE);
        display.drawPixel(233, 96 + i, GxEPD_WHITE);
    }
    for (unsigned char i : graph_stops) {
        display.drawPixel(i, 87, GxEPD_WHITE);
        display.drawPixel(i, 115, GxEPD_WHITE);
        display.drawPixel(i, 116, GxEPD_BLACK);
        display.drawPixel(i, 117, GxEPD_BLACK);
        for (unsigned char j = 0; j < 6; j += 2) {
            display.drawPixel(i, 88 + j, GxEPD_BLACK);
        }
    }
    // graph - icons
    display.drawInvertedBitmap(237, 73, bmp_t_icon, 5, 8, GxEPD_BLACK);
    display.setFont(&Font_04b03b);
    display.setCursor(243, 81);
    display.print(unitSymbol);
    display.drawCircle(244, 73, 1, GxEPD_BLACK);
    display.drawInvertedBitmap(237, 101, bmp_h_icon, 10, 8, GxEPD_BLACK);
    // graph - labels
    display.setFont(&Font_04b03b);
    const char* graph_stop_labels[graph_stops_count] = {"t-1Y", "t-1m", "t-1w", "t-1d", "t-1h", "t-0"};
    for (unsigned char i = 0; i < graph_stops_count; ++i) {
        display.setCursor(graph_stops[i] - 3 - i*1.5, 117 + y04b);
        display.print(graph_stop_labels[i]);
    }
    // graph - labels - values
    // graph - labels - values - temp high
    snprintf(buf, sizeof(buf), "%.0f", celsiusTo(data->chartYAxisHighTempCelsiusBound, data->degreesUnit));
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 69-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 69);
    display.print(buf);
    // graph - labels - values - temp low
    snprintf(buf, sizeof(buf), "%.0f", celsiusTo(data->chartYAxisLowTempCelsiusBound, data->degreesUnit));
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 89-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 89);
    display.print(buf);
    // graph - labels - values - humidity high
    snprintf(buf, sizeof(buf), "%.0f", data->chartYAxisHighHumidityBound);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 98-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 98);
    display.print(buf);
    // graph - labels - values - humidity low
    snprintf(buf, sizeof(buf), "%.0f", data->chartYAxisLowHumidityBound);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 118-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 118);
    display.print(buf);
    // graph - values
    for (uint8_t i = 0; i < 112; i++) {
        uint8_t val = sin((float) i / 20) * 5.0 + 11;
        display.drawFastVLine(i + 121, 86 - val, val, GxEPD_BLACK);

        val = cos((float) i / 20) * 4.0 + 9;
        display.drawFastVLine(i + 121, 114 - val, val, GxEPD_BLACK);
    }
    Serial.print("Repaint - graph values: ");
    Serial.println(micros() - timestamp);
}
