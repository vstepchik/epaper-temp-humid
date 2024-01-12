#include "Utils.h"
#include <cmath>

float celsiusTo(const float celsius, const DegreesUnit unit) {
    switch (unit) {
        case CELSIUS:
        return celsius;
        case FARENHEIT:
        return celsius * 9/5 - 32;
    }
    return NAN;
}

void formatSize(unsigned long bytes, char* result, int resultSize) {
    const char* suffixes = "KMGT";
    int suffixIndex = 0;
    double size = bytes;

    while (size >= 1024 && suffixIndex < 4) {
        size /= 1024.0;
        suffixIndex++;
    }

    if (suffixIndex == 0) {
        snprintf(result, resultSize, "%lu", (unsigned long)size);
    } else if (size >= 100) {
        snprintf(result, resultSize, "%lu%s", (unsigned long)size, suffixes + suffixIndex - 1);
    } else if (size >= 10) {
        snprintf(result, resultSize, "%.1f%s", size, suffixes + suffixIndex - 1);
    } else {
        snprintf(result, resultSize, "%.2f%s", size, suffixes + suffixIndex - 1);
    }
}

// 982273581056