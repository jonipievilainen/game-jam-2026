#include <stdint.h>
#include <psxgpu.h>

#include "wheel_texture.h"

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 64

static uint32_t pixels[TEXTURE_WIDTH * TEXTURE_HEIGHT / 2];

static uint16_t rgb15(int red, int green, int blue) {
	return (uint16_t) (red | (green << 5) | (blue << 10));
}

void init_wheel_texture(void) {
	RECT area = { 640, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT };

	for (int y = 0; y < TEXTURE_HEIGHT; y++) {
		for (int x = 0; x < TEXTURE_WIDTH; x += 2) {
			uint16_t color[2];
			for (int i = 0; i < 2; i++) {
				int px = x + i;
				if (px < 64) {
					int dx = px - 32;
					int dy = y - 32;
					int radius = dx * dx + dy * dy;
					if (radius < 36)
						color[i] = rgb15(12, 13, 14);
					else if (radius < 300) {
						int spoke = (dx > -3 && dx < 3) ||
							(dy > -3 && dy < 3) ||
							(dx - dy > -3 && dx - dy < 3) ||
							(dx + dy > -3 && dx + dy < 3);
						color[i] = spoke ? rgb15(26, 27, 28) : rgb15(11, 12, 13);
					} else if (radius < 380)
						color[i] = rgb15(23, 24, 25);
					else if (radius < 760)
						color[i] = rgb15(5, 6, 7);
					else
						color[i] = rgb15(3, 4, 5);
				} else {
					int tread = ((px - 64 + y / 3) % 12 < 3) ||
						(y > 13 && y < 17);
					color[i] = tread ? rgb15(3, 4, 5) : rgb15(7, 8, 9);
				}
			}
			pixels[(y * TEXTURE_WIDTH + x) / 2] =
				(uint32_t) color[0] | ((uint32_t) color[1] << 16);
		}
	}

	LoadImage(&area, pixels);
	DrawSync(0);
}
