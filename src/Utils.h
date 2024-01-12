#pragma once
#include <Arduino.h>
#include "common_types.h"

float celsiusTo(const float celsius, const DegreesUnit unit);
String formatSize(const unsigned long bytes);
void formatSize(unsigned long bytes, char* result, int resultSize);