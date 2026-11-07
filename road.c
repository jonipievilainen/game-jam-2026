#include "road.h"

#define CURVE_SEGMENTS 8

int64_t road_segment_at(int64_t z) {
	int64_t segment = z / ROAD_SEGMENT_LENGTH;
	if (z % ROAD_SEGMENT_LENGTH < 0)
		segment--;
	return segment;
}

static int curve_anchor(int64_t index, uint32_t seed) {
	uint64_t bits = (uint64_t) index;
	uint32_t value = (uint32_t) bits ^ (uint32_t) (bits >> 32) ^ seed;

	if (index == 0 || index == 1)
		return 0;
	value ^= value >> 16;
	value *= 0x7feb352dU;
	value ^= value >> 15;
	value *= 0x846ca68bU;
	value ^= value >> 16;
	return (int) (value % 481) - 240;
}

int road_center(int64_t segment, uint32_t seed) {
	int64_t block = segment / CURVE_SEGMENTS;
	int step = (int) (segment % CURVE_SEGMENTS);
	if (step < 0) {
		block--;
		step += CURVE_SEGMENTS;
	}

	int t = step * 32;
	int blend = t * t * (768 - 2 * t) / 65536;
	int a = curve_anchor(block, seed);
	int b = curve_anchor(block + 1, seed);
	return a + (b - a) * blend / 256;
}
