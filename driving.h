#pragma once

#define DRIVING_SPEED_SCALE 256
#define DRIVING_FORWARD_MAX (8 * DRIVING_SPEED_SCALE)
#define DRIVING_REVERSE_MAX (4 * DRIVING_SPEED_SCALE)

int driving_speed_step(int speed, int throttle);
int driving_steering_step(int speed, int steer);
