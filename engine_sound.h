#pragma once

#include <stdint.h>

void engine_sound_init(void);
void engine_sound_update(int speed, int throttle, uint32_t tick);
