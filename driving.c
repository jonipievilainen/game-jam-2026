#include "driving.h"

#define ACCELERATION 16
#define BRAKING 48
#define COASTING 8

int driving_speed_step(int speed, int throttle) {
	if (throttle > 0) {
		if (speed < 0) {
			speed += BRAKING;
			return speed > 0 ? 0 : speed;
		}
		speed += ACCELERATION;
		return speed > DRIVING_FORWARD_MAX ? DRIVING_FORWARD_MAX : speed;
	}
	if (throttle < 0) {
		if (speed > 0) {
			speed -= BRAKING;
			return speed < 0 ? 0 : speed;
		}
		speed -= ACCELERATION;
		return speed < -DRIVING_REVERSE_MAX ? -DRIVING_REVERSE_MAX : speed;
	}
	if (speed > 0)
		return speed > COASTING ? speed - COASTING : 0;
	if (speed < 0)
		return speed < -COASTING ? speed + COASTING : 0;
	return 0;
}

int driving_steering_step(int speed, int steer) {
	return steer * speed * 5 / (2 * DRIVING_SPEED_SCALE);
}
