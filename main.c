#include <stdint.h>
#include <psxgpu.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 2
#define PACKET_BUFFER_SIZE 1024

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	uint8_t packets[PACKET_BUFFER_SIZE];
} FrameBuffer;

static FrameBuffer buffers[2];

static void setup_buffers(void) {
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;

		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 12, 12, 36);
		buffers[i].draw.isbg = 1;
	}
}

int main(void) {
	ResetGraph(0);
	FntLoad(960, 0);
	setup_buffers();
	SetDispMask(1);

	int active_buffer = 0;
	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		uint8_t *next_packet = draw_buffer->packets;

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);

		TILE *tile = (TILE *) next_packet;
		setTile(tile);
		setXY0(tile, 96, 142);
		setWH(tile, 128, 12);
		setRGB0(tile, 40, 220, 180);
		addPrim(&draw_buffer->ordering_table[1], tile);
		next_packet += sizeof(TILE);

		next_packet = (uint8_t *) FntSort(
			&draw_buffer->ordering_table[0],
			next_packet,
			104,
			104,
			"HELLO, WORLD!"
		);
		FntSort(
			&draw_buffer->ordering_table[0],
			next_packet,
			88,
			120,
			"MY FIRST PLAYSTATION GAME"
		);

		DrawSync(0);
		VSync(0);
		PutDispEnv(&display_buffer->display);
		DrawOTagEnv(
			&draw_buffer->ordering_table[OT_LENGTH - 1],
			&draw_buffer->draw
		);

		active_buffer ^= 1;
	}

	return 0;
}
