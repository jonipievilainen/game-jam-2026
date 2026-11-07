#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../road.h"

int main(void) {
	const uint32_t seed = 1234567;
	int differs = 0;

	assert(road_segment_at(0) == 0);
	assert(road_segment_at(63) == 0);
	assert(road_segment_at(64) == 1);
	assert(road_segment_at(-1) == -1);
	assert(road_segment_at(-64) == -1);
	assert(road_segment_at(-65) == -2);
	assert(road_segment_at(INT64_C(1000000000000)) == INT64_C(15625000000));
	assert(road_segment_at(INT64_C(-1000000000000)) == INT64_C(-15625000000));

	for (int64_t i = -500000; i < 500000; i++) {
		int current = road_center(i, seed);
		int next = road_center(i + 1, seed);
		assert(current >= -240 && current <= 240);
		assert(next - current <= 90 && next - current >= -90);
		assert(current == road_center(i, seed));
		if (current != road_center(i, seed + 1))
			differs = 1;
	}
	assert(differs);
	for (int64_t i = INT64_C(15625000000); i < INT64_C(15625001000); i++) {
		int current = road_center(i, seed);
		int next = road_center(i + 1, seed);
		assert(current >= -240 && current <= 240);
		assert(next - current <= 90 && next - current >= -90);
	}
	puts("Road continuity, bounds, seed, reverse and long-distance tests passed.");
	return 0;
}
