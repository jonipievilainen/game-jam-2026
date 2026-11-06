#include <stdint.h>
#include <stdio.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxpad.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 2
#define PACKET_BUFFER_SIZE 4096
#define BUTTON_COUNT 16

typedef struct {
	uint16_t mask;
	const char *name;
} ControllerButton;

static const ControllerButton buttons[BUTTON_COUNT] = {
	{ PAD_SELECT, "SELECT" },
	{ PAD_START, "START" },
	{ PAD_L3, "L3" },
	{ PAD_R3, "R3" },
	{ PAD_UP, "UP" },
	{ PAD_LEFT, "LEFT" },
	{ PAD_DOWN, "DOWN" },
	{ PAD_RIGHT, "RIGHT" },
	{ PAD_L2, "L2" },
	{ PAD_L1, "L1" },
	{ PAD_R1, "R1" },
	{ PAD_R2, "R2" },
	{ PAD_SQUARE, "SQUARE" },
	{ PAD_TRIANGLE, "TRIANGLE" },
	{ PAD_CROSS, "X" },
	{ PAD_CIRCLE, "CIRCLE" }
};

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	uint8_t packets[PACKET_BUFFER_SIZE];
} FrameBuffer;

static FrameBuffer buffers[2];
static uint8_t pad_buffers[2][34];

static void setup_buffers(void) {
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;

		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 12, 12, 36);
		buffers[i].draw.isbg = 1;
	}
}

static int controller_connected(const volatile PADTYPE *pad) {
	if (pad->stat)
		return 0;

	return
		(pad->type == PAD_ID_DIGITAL) ||
		(pad->type == PAD_ID_ANALOG_STICK) ||
		(pad->type == PAD_ID_ANALOG);
}

int main(void) {
	ResetGraph(0);
	FntLoad(960, 0);
	setup_buffers();

	InitPAD(pad_buffers[0], sizeof(pad_buffers[0]), pad_buffers[1], sizeof(pad_buffers[1]));
	StartPAD();
	ChangeClearPAD(0);

	SetDispMask(1);

	int active_buffer = 0;
	uint16_t previous_buttons = 0;
	const char *last_pressed = "NONE";

	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		uint8_t *next_packet = draw_buffer->packets;
		const volatile PADTYPE *pad = (const volatile PADTYPE *) pad_buffers[0];
		int connected = controller_connected(pad);
		uint16_t current_buttons = connected ? (uint16_t) ~pad->btn : 0;
		uint16_t newly_pressed = current_buttons & (uint16_t) ~previous_buttons;

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);

		if (newly_pressed) {
			for (int i = 0; i < BUTTON_COUNT; i++) {
				if (newly_pressed & buttons[i].mask) {
					last_pressed = buttons[i].name;
					break;
				}
			}
		}

		for (int i = 0; i < BUTTON_COUNT; i++) {
			int x = 12 + (i % 4) * 72;
			int y = 65 + (i / 4) * 24;
			TILE *tile = (TILE *) next_packet;

			setTile(tile);
			setXY0(tile, x, y);
			setWH(tile, 70, 22);
			if (current_buttons & buttons[i].mask)
				setRGB0(tile, 30, 190, 130);
			else
				setRGB0(tile, 35, 44, 75);
			addPrim(&draw_buffer->ordering_table[1], tile);
			next_packet += sizeof(TILE);
		}

		char text[256];
		int text_length = snprintf(
			text,
			sizeof(text),
			"CONTROLLER TEST\n%s\n\n\n\n\n",
			connected ? "PRESS ANY BUTTON" : "NO CONTROLLER"
		);

		for (int row = 0; row < 4; row++) {
			for (int column = 0; column < 4; column++) {
				int index = row * 4 + column;
				char label[10];

				snprintf(label, sizeof(label), "%s", buttons[index].name);

				text_length += snprintf(
					text + text_length,
					sizeof(text) - text_length,
					"%-9s",
					label
				);
			}

			if (row < 3) {
				text[text_length++] = '\n';
				text[text_length++] = '\n';
				text[text_length++] = '\n';
			}
		}

		text_length += snprintf(
			text + text_length,
			sizeof(text) - text_length,
			"\n\n\n\n\n\nLAST PRESSED: %s",
			connected ? last_pressed : "NONE"
		);

		FntSort(
			&draw_buffer->ordering_table[0],
			next_packet,
			12,
			18,
			text
		);

		DrawSync(0);
		VSync(0);
		PutDispEnv(&display_buffer->display);
		DrawOTagEnv(
			&draw_buffer->ordering_table[OT_LENGTH - 1],
			&draw_buffer->draw
		);

		active_buffer ^= 1;
		previous_buttons = current_buttons;
	}

	return 0;
}
