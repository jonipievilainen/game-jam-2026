#include <assert.h>
#include <stdio.h>

#include "../driving.h"

int main(void) {
	int speed = 0;
	for (int i = 0; i < 128; i++) {
		int next = driving_speed_step(speed, 1);
		assert(next > speed);
		speed = next;
	}
	assert(speed == DRIVING_FORWARD_MAX);
	assert(driving_speed_step(speed, 1) == DRIVING_FORWARD_MAX);
	assert(driving_steering_step(speed, 1) == 20);
	assert(driving_steering_step(speed, -1) == -20);

	while (speed > 0) {
		int next = driving_speed_step(speed, -1);
		assert(next >= 0 && next < speed);
		speed = next;
	}
	for (int i = 0; i < 64; i++)
		speed = driving_speed_step(speed, -1);
	assert(speed == -DRIVING_REVERSE_MAX);
	assert(driving_speed_step(speed, -1) == -DRIVING_REVERSE_MAX);
	assert(driving_steering_step(speed, 1) < 0);

	while (speed < 0) {
		int next = driving_speed_step(speed, 1);
		assert(next <= 0 && next > speed);
		speed = next;
	}
	assert(driving_steering_step(speed, 1) == 0);
	assert(driving_speed_step(3, 0) == 0);
	assert(driving_speed_step(-3, 0) == 0);
	assert(driving_speed_step(100, 0) < 100);
	assert(driving_speed_step(-100, 0) > -100);
	puts("Acceleration, limits, braking, coasting and steering tests passed.");
	return 0;
}
