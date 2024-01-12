#pragma once

#include <Arduino.h>
#include <cmath>

#include "common_types.h"

float celsiusTo(const float celsius, const DegreesUnit unit);
void formatSize(unsigned long bytes, char* result, int resultSize);
