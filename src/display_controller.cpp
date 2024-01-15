#include "Arduino.h"
#include "display_controller.h"

unsigned long ET = 0;
const char y04b = Font_04b03b.yAdvance / 2 + 1;

DisplayController::DisplayController() : display(GxEPD2_213_B74(5, 17, 16, 4)) {
    display.init();
    display.setRotation(3);
    display.setFullWindow();
    display.setTextColor(GxEPD_BLACK);
    display.setTextWrap(false);

    // delay(5);
    // display.firstPage();
    // do {
    //     display.fillScreen(GxEPD_WHITE);
    //     display.drawLine(0, 0, display.width(), display.height(), GxEPD_BLACK);
    //     display.drawLine(display.width(), 0, 0, display.height(), GxEPD_BLACK);
    // } while (display.nextPage());
    // display.hibernate();
}

void DisplayController::debug_print(char* txt) {
    unsigned long T1 = 0, T2 = 0;
    
    display.setFullWindow();
    display.firstPage();
    do {
        T1 = micros();
        display.fillScreen(GxEPD_WHITE);

        display.setFont(&Font_04b03b);
        display.setCursor(0, y04b);
        display.print(txt);

        T2 = micros();
        ET = T2 - T1;
        // display draw time
        display.setFont(&Font_04b03b);
        display.setCursor(0, display.height());
        display.print(ET);
    } while (display.nextPage());

    display.hibernate();
}

void DisplayController::full_repaint(DisplayRenderPayload* data) {
    unsigned long T1 = 0, T2 = 0;
    const float currentTemp = celsiusTo(data->currentTemperatureCelsius, data->degreesUnit);
    const char unitSymbol = data->degreesUnit == CELSIUS ? 'C' : 'F';
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];
    
    display.setFullWindow();
    // display.setPartialWindow(1, 1, 100, 100);
    display.firstPage();
    do {
        T1 = micros();
        display.fillScreen(GxEPD_WHITE);
        drawStatusBar(data);

        // gauges
        drawGauges(
            data->degreesUnit,
            data->gaugeTempCelsiusCenter,
            data->gaugeHumidityCenter, 
            currentTemp, 
            data->currentHumidity
        );

        drawCurrentValues(data, currentTemp, unitSymbol);

        // statistics
        display.drawRect(146, 17, 104, 39, GxEPD_BLACK);
        display.fillRect(143, 13, 16, 15, GxEPD_WHITE);
        display.drawInvertedBitmap(143, 13, bmp_stats_hint, 16, 15, GxEPD_BLACK);
        // statistics - grid lines
        display.drawFastHLine(159, 27, 90, GxEPD_BLACK);
        display.drawFastHLine(147, 41, 102, GxEPD_BLACK);
        display.drawFastVLine(153, 28, 27, GxEPD_BLACK);
        display.drawFastVLine(185, 18, 37, GxEPD_BLACK);
        display.drawFastVLine(217, 18, 37, GxEPD_BLACK);
        for (unsigned char i = 0; i < 3; ++i) {
            display.drawPixel(169 + i*32, 28, GxEPD_BLACK);
            display.drawPixel(169 + i*32, 40, GxEPD_BLACK);
            display.drawPixel(169 + i*32, 42, GxEPD_BLACK);
            display.drawPixel(169 + i*32, 54, GxEPD_BLACK);
        }
        // statistics - labels
        display.setFont(&Font_04b03b);
        display.setCursor(148, 32 + y04b);
        display.print(unitSymbol);
        display.setCursor(148, 46 + y04b);
        display.print(F("H"));
        display.setCursor(163, 20 + y04b);
        display.print(F("DAY"));
        display.setCursor(191, 20 + y04b);
        display.print(F("WEEK"));
        display.setCursor(221, 20 + y04b);
        display.print(F("MONTH"));

        // stats
        auto tempConversion = [&data](float input) -> float { return celsiusTo(input, data->degreesUnit); };
        auto humConversion = [](float input) -> float { return input; };
        drawStats(154, 28, data->statsT1D, tempConversion); // todo: conversion to kelvins
        drawStats(186, 28, data->statsT1W, tempConversion);
        drawStats(218, 28, data->statsT1M, tempConversion);
        drawStats(154, 42, data->statsH1D, humConversion);
        drawStats(186, 42, data->statsH1W, humConversion);
        drawStats(218, 42, data->statsH1M, humConversion);

        drawHistoryGraph(data, unitSymbol);

        T2 = micros();
        ET = T2 - T1;
        // display draw time
        display.setFont(&Font_04b03b);
        display.setCursor(0, y04b);
        display.print(ET);
    } while (display.nextPage());
    display.hibernate();
}

void DisplayController::drawGauges(
    DegreesUnit tempUnit, 
    float tempMiddlePointCelsius, 
    float humidityMiddlePointValue, 
    float currentTempConverted, 
    float currentHumidity
) {
    // gauges
    display.drawInvertedBitmap(2, 19, bmp_gauge_t_bg, 101, 16, GxEPD_BLACK);
    display.drawInvertedBitmap(2, 37, bmp_gauge_h_bg, 101, 16, GxEPD_BLACK);
    // gauges - labels
    display.setFont(&Font_04b03b);
    char buf[3];
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
    unsigned char xPositions[3] = {0, 47, 96};
    for (unsigned char i = 0; i < 3; ++i) {
        snprintf(buf, sizeof(buf), "%.0f", tempValues[i]);
        display.setCursor(xPositions[i], 13 + y04b);
        display.print(buf);
        snprintf(buf, sizeof(buf), "%.0f", humValues[i]);
        display.setCursor(xPositions[i], 54 + y04b);
        display.print(buf);
    }
    // gauges - arrows
    float tempArrX = (currentTempConverted - tempValues[0]) / (tempValues[2] - tempValues[0]) * 100.0;
    tempArrX = constrain(tempArrX, 1, 99);
    display.fillRect(tempArrX, 20, 5, 9, GxEPD_WHITE);
    display.drawInvertedBitmap(tempArrX, 20, bmp_arrow_thin_dn, 5, 9, GxEPD_BLACK);

    float humArrX = map(currentHumidity, humValues[0], humValues[2], 0, 100);
    humArrX = constrain(humArrX, 1, 99);
    display.fillRect(humArrX, 43, 5, 9, GxEPD_WHITE);
    display.drawInvertedBitmap(humArrX, 43, bmp_arrow_thin_up, 5, 9, GxEPD_BLACK);
    // gauges - patch overlaps
    display.fillRect(0, 19, 2, 16, GxEPD_WHITE); display.drawFastVLine(2, 19, 16, GxEPD_BLACK);
    display.fillRect(0, 37, 2, 16, GxEPD_WHITE); display.drawFastVLine(2, 37, 16, GxEPD_BLACK);
    display.fillRect(103, 19, 2, 16, GxEPD_WHITE); display.drawFastVLine(102, 19, 16, GxEPD_BLACK);
    display.fillRect(103, 37, 2, 16, GxEPD_WHITE); display.drawFastVLine(102, 37, 16, GxEPD_BLACK);
}

template<typename StatsConversion>
void DisplayController::drawStats(unsigned char x, unsigned char y, MeasurementStatistics stats, StatsConversion conversion) {
    display.setFont(&TomThumb);
    const unsigned char adv = TomThumb.yAdvance;
    display.setCursor(x + 1, y + adv);
    display.print(conversion(stats.average), 1);

    display.setCursor(x + 1, y + adv + 6);
    display.print(conversion(stats.median), 1);

    display.setCursor(x + 17, y + adv);
    display.print(conversion(stats.max), 1);

    display.setCursor(x + 17, y + adv + 6);
    display.print(conversion(stats.min), 1);
}

void DisplayController::drawStatusBar(DisplayRenderPayload* data) {
    // sd card
    if (data->sdCardVolumeBytes > 0) {
        display.drawInvertedBitmap(67, 0, bmp_sd_card_icon, 4, 5, GxEPD_BLACK);
        display.setFont(&Font_04b03b);
        char occupiedBuf[7], volumeBuf[7], buf[sizeof(occupiedBuf) + sizeof(volumeBuf)];
        formatSize(data->sdCardOccupiedBytes, occupiedBuf, sizeof(occupiedBuf));
        formatSize(data->sdCardVolumeBytes, volumeBuf, sizeof(volumeBuf));
        snprintf(buf, sizeof(buf), "%s/%s", occupiedBuf, volumeBuf);
        // display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor(72, y04b);
        display.print(buf);
    }

    // time
    display.setFont(&Font_04b03b);
    char buf[19];
    strftime(buf, sizeof(buf), "%Y-%m-%d - %H:%M", &data->timeinfo);
    display.setCursor(152, y04b);
    display.print(buf);

    // battery
    display.drawInvertedBitmap(236, 0, bmp_bat_full, 14, 5, GxEPD_BLACK);
    int8_t bat_pixels_empty = (1.0 - data->batteryLevel) * 11;
    display.fillRect(248-bat_pixels_empty, 1, bat_pixels_empty, 3, GxEPD_WHITE);
    if (bat_pixels_empty > 0) display.drawPixel(247-bat_pixels_empty, 1, GxEPD_BLACK);

    display.setFont(&tiniest_num42);
    display.setCursor(242, 10);
    display.print(data->batteryLevel * (float)100, 0);
}

void DisplayController::paint_time(tm* timeinfo) {
    display.setFont(&Font_04b03b);
    char buf[19];
    strftime(buf, sizeof(buf), "%Y-%m-%d - %H:%M", timeinfo);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setPartialWindow(152, 0, tbw, tbh);
    display.firstPage();
    do {
        display.fillRect(0, 0, tbw, tbh, GxEPD_WHITE);
        display.setCursor(0, y04b);
        display.print(buf);
    } while (display.nextPage());
}

void DisplayController::drawCurrentValues(DisplayRenderPayload* data, const float currentTemp, const char unitSymbol) {
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];
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
}

void DisplayController::drawHistoryGraph(DisplayRenderPayload* data, const char unitSymbol) {
    const unsigned char graph_stops_count = 6;
    const unsigned char graph_stops[graph_stops_count] = {3, 51, 120, 156, 202, 232};
    int16_t tbx, tby; uint16_t tbw, tbh;
    char buf[5];
    // graph - lines
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
        display.setCursor(graph_stops[i] - 3, 117 + y04b);
        display.print(graph_stop_labels[i]);
    }
    // graph - labels - values
    snprintf(buf, sizeof(buf), "%.0f", celsiusTo(data->chartYAxisHighTempCelsiusBound, data->degreesUnit));
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 69-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 69);
    display.print(buf);
    snprintf(buf, sizeof(buf), "%.0f", celsiusTo(data->chartYAxisLowTempCelsiusBound, data->degreesUnit));
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 89-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 89);
    display.print(buf);
    snprintf(buf, sizeof(buf), "%.0f", data->chartYAxisHighHumidityBound);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 98-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 98);
    display.print(buf);
    snprintf(buf, sizeof(buf), "%.0f", data->chartYAxisLowHumidityBound);
    display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(235, 118-y04b, tbw-2, tbh, GxEPD_WHITE);
    display.setCursor(236, 118);
    display.print(buf);

    // graph - values
    if (data->sdCardVolumeBytes == 0) {
        display.drawFastVLine(120, 68, 18, GxEPD_BLACK);
        display.drawFastVLine(120, 96, 18, GxEPD_BLACK);
        display.drawInvertedBitmap(40, 72, bmp_no_sd_card, 48, 10, GxEPD_BLACK);
        display.drawInvertedBitmap(40, 100, bmp_no_sd_card, 48, 10, GxEPD_BLACK);
    }
    for (uint8_t i = 0; i < 112; i++) {
        uint8_t val = sin((float) i / 20) * 5.0 + 11;
        display.drawFastVLine(i + 121, 86 - val, val, GxEPD_BLACK);

        val = cos((float) i / 20) * 4.0 + 9;
        display.drawFastVLine(i + 121, 114 - val, val, GxEPD_BLACK);
    }
}
