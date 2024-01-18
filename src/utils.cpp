#include "utils.h"

float celsiusTo(const float celsius, const DegreesUnit unit) {
    switch (unit) {
        case CELSIUS:
        return celsius;
        case FARENHEIT:
        return celsius * 9/5 - 32;
    }
    return NAN;
}

void formatSize(uint64_t bytes, char* result, uint8_t resultSize) {
    static const char* suffixes = " KMGT";
    uint8_t suffixIndex = 0;
    double size = bytes;

    while (size >= 1024 && suffixIndex < 4) {
        size /= 1024.0;
        suffixIndex++;
    }

    if (suffixIndex == 0) {
        snprintf(result, resultSize, "%lu", (unsigned long)size);
    } else if (size >= 100) {
        snprintf(result, resultSize, "%lu%cb", (unsigned long)size, suffixes[suffixIndex]);
    } else if (size >= 10) {
        snprintf(result, resultSize, "%.1f%cb", size, suffixes[suffixIndex]);
    } else {
        snprintf(result, resultSize, "%.2f%cb", size, suffixes[suffixIndex]);
    }
}
