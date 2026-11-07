#include <stdint.h>
#include <psxgpu.h>

#include "body_texture.h"

#define WIDTH 128
#define HEIGHT 96

static uint32_t pixels[WIDTH * HEIGHT / 2];

static uint16_t rgb15(int r, int g, int b) {
	return (uint16_t) (r | (g << 5) | (b << 10));
}

static uint16_t body_pixel(int x, int y) {
	int panel = y / 32;
	int row = y % 32;
	uint16_t paint = rgb15(6, 14, 23);

	if (row < 16) {
		int window = panel < 2
			? x >= 9 && x < 119
			: x >= 36 && x < 101 && (x < 65 || x > 69);
		if (window && row >= 3 && row < 14) {
			if (row < 5 || (x + row * 3) % 46 < 3)
				return rgb15(14, 21, 25);
			return rgb15(5, 9, 13);
		}
	} else if (panel < 2) {
		if (row >= 20 && row <= 24 && (x < 22 || x >= 106))
			return panel == 0 ? rgb15(28, 4, 3) : rgb15(30, 28, 18);
		if (row >= 26 && row <= 28)
			return rgb15(8, 9, 10);
		if (row >= 20 && row <= 24 && x >= 45 && x < 83)
			return panel == 0 ? rgb15(24, 24, 19) : rgb15(4, 5, 6);
	} else {
		if (row == 20 && x >= 33 && x < 94)
			return rgb15(11, 19, 26);
		if (row == 19 && ((x >= 45 && x < 52) || (x >= 75 && x < 82)))
			return rgb15(22, 23, 23);
		if (row >= 29)
			return rgb15(4, 7, 10);
	}
	return paint;
}

void init_body_texture(void) {
	RECT area = { 768, 0, WIDTH, HEIGHT };
	for (int y = 0; y < HEIGHT; y++) {
		for (int x = 0; x < WIDTH; x += 2) {
			pixels[(y * WIDTH + x) / 2] =
				(uint32_t) body_pixel(x, y) |
				((uint32_t) body_pixel(x + 1, y) << 16);
		}
	}
	LoadImage(&area, pixels);
	DrawSync(0);
}
