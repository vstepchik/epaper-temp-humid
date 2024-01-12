#pragma once

#include <Arduino.h>
#include <cmath>

#include "common_types.h"

float celsiusTo(const float celsius, const DegreesUnit unit);
void formatSize(uint64_t bytes, char* result, uint8_t resultSize);
