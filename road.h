#pragma once

#include <stdint.h>

#define ROAD_SEGMENT_LENGTH 64
#define ROAD_HALF_WIDTH 260

int64_t road_segment_at(int64_t z);
int road_center(int64_t segment, uint32_t seed);
