#include <stdint.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxpad.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 256
#define PACKET_BUFFER_SIZE 8192
#define PLATFORM_LEFT 24
#define PLATFORM_TOP 32
#define PLATFORM_RIGHT 296
#define PLATFORM_BOTTOM 208
#define CIRCLE_RADIUS 14
#define PLAYER_SPEED 2
#define STICK_DEADZONE 28
#define STICK_CALIBRATION_SAMPLES 16

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	uint8_t packets[PACKET_BUFFER_SIZE];
} FrameBuffer;

static FrameBuffer buffers[2];
static uint8_t pad_buffers[2][34];

static const int8_t circle_offsets[16][2] = {
	{ 0, -14 }, { 5, -13 }, { 10, -10 }, { 13, -5 },
	{ 14, 0 }, { 13, 5 }, { 10, 10 }, { 5, 13 },
	{ 0, 14 }, { -5, 13 }, { -10, 10 }, { -13, 5 },
	{ -14, 0 }, { -13, -5 }, { -10, -10 }, { -5, -13 }
};

static int stick_direction(int value, int center) {
	int offset = value - center;
	if (offset > -STICK_DEADZONE && offset < STICK_DEADZONE)
		return 0;
	if (offset > 0)
		return offset > 72 ? PLAYER_SPEED : 1;
	return offset < -72 ? -PLAYER_SPEED : -1;
}

static void setup_buffers(void) {
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;

		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 18, 24, 38);
		buffers[i].draw.isbg = 1;
	}
}

static uint8_t *draw_platform(
	uint8_t *packet,
	uint32_t *ordering_table
) {
	POLY_F4 *platform = (POLY_F4 *) packet;

	setPolyF4(platform);
	setRGB0(platform, 66, 83, 91);
	platform->x0 = PLATFORM_LEFT;
	platform->y0 = PLATFORM_TOP;
	platform->x1 = PLATFORM_RIGHT;
	platform->y1 = PLATFORM_TOP;
	platform->x2 = PLATFORM_LEFT;
	platform->y2 = PLATFORM_BOTTOM;
	platform->x3 = PLATFORM_RIGHT;
	platform->y3 = PLATFORM_BOTTOM;
	addPrim(&ordering_table[2], platform);

	return packet + sizeof(POLY_F4);
}

static uint8_t *draw_circle(
	uint8_t *packet,
	uint32_t *ordering_table,
	int center_x,
	int center_y
) {
	for (int i = 0; i < 16; i++) {
		int next = (i + 1) & 15;
		POLY_F3 *triangle = (POLY_F3 *) packet;

		setPolyF3(triangle);
		setRGB0(triangle, 70, 190, 245);
		triangle->x0 = (int16_t) center_x;
		triangle->y0 = (int16_t) center_y;
		triangle->x1 = (int16_t) (center_x + circle_offsets[i][0]);
		triangle->y1 = (int16_t) (center_y + circle_offsets[i][1]);
		triangle->x2 = (int16_t) (center_x + circle_offsets[next][0]);
		triangle->y2 = (int16_t) (center_y + circle_offsets[next][1]);
		addPrim(&ordering_table[1], triangle);
		packet += sizeof(POLY_F3);
	}

	return packet;
}

int main(void) {
	ResetGraph(0);
	FntLoad(960, 0);
	setup_buffers();

	InitPAD(
		pad_buffers[0],
		sizeof(pad_buffers[0]),
		pad_buffers[1],
		sizeof(pad_buffers[1])
	);
	StartPAD();
	ChangeClearPAD(0);
	SetDispMask(1);

	int player_x = SCREEN_WIDTH / 2;
	int player_y = SCREEN_HEIGHT / 2;
	int active_buffer = 0;
	int controller_type = -1;
	int calibration_samples = 0;
	int calibration_sum_x = 0;
	int calibration_sum_y = 0;
	int stick_center_x = 128;
	int stick_center_y = 128;

	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		uint8_t *packet = draw_buffer->packets;
		volatile PADTYPE *pad = (volatile PADTYPE *) pad_buffers[0];
		int pad_ready =
			!pad->stat &&
			(pad->type == PAD_ID_DIGITAL ||
			 pad->type == PAD_ID_ANALOG ||
			 pad->type == PAD_ID_ANALOG_STICK);
		uint16_t buttons = pad_ready ? pad->btn : 0xffff;
		int move_x = 0;
		int move_y = 0;
		int analog_pad =
			pad_ready &&
			(pad->type == PAD_ID_ANALOG ||
			 pad->type == PAD_ID_ANALOG_STICK);

		if (pad_ready && pad->type != controller_type) {
			controller_type = pad->type;
			calibration_samples = 0;
			calibration_sum_x = 0;
			calibration_sum_y = 0;
			stick_center_x = 128;
			stick_center_y = 128;
		}

		if (analog_pad &&
			calibration_samples < STICK_CALIBRATION_SAMPLES) {
			calibration_sum_x += pad->ls_x;
			calibration_sum_y += pad->ls_y;
			calibration_samples++;
			if (calibration_samples == STICK_CALIBRATION_SAMPLES) {
				stick_center_x =
					calibration_sum_x / STICK_CALIBRATION_SAMPLES;
				stick_center_y =
					calibration_sum_y / STICK_CALIBRATION_SAMPLES;
			}
		}

		if (!(buttons & PAD_LEFT))
			move_x = -PLAYER_SPEED;
		else if (!(buttons & PAD_RIGHT))
			move_x = PLAYER_SPEED;
		else if (analog_pad &&
			calibration_samples == STICK_CALIBRATION_SAMPLES)
			move_x = stick_direction(pad->ls_x, stick_center_x);

		if (!(buttons & PAD_UP))
			move_y = -PLAYER_SPEED;
		else if (!(buttons & PAD_DOWN))
			move_y = PLAYER_SPEED;
		else if (analog_pad &&
			calibration_samples == STICK_CALIBRATION_SAMPLES)
			move_y = stick_direction(pad->ls_y, stick_center_y);

		player_x += move_x;
		player_y += move_y;

		if (player_x < PLATFORM_LEFT + CIRCLE_RADIUS)
			player_x = PLATFORM_LEFT + CIRCLE_RADIUS;
		if (player_x > PLATFORM_RIGHT - CIRCLE_RADIUS)
			player_x = PLATFORM_RIGHT - CIRCLE_RADIUS;
		if (player_y < PLATFORM_TOP + CIRCLE_RADIUS)
			player_y = PLATFORM_TOP + CIRCLE_RADIUS;
		if (player_y > PLATFORM_BOTTOM - CIRCLE_RADIUS)
			player_y = PLATFORM_BOTTOM - CIRCLE_RADIUS;

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		packet = draw_platform(packet, draw_buffer->ordering_table);
		packet = draw_circle(
			packet,
			draw_buffer->ordering_table,
			player_x,
			player_y
		);
		FntSort(
			&draw_buffer->ordering_table[0],
			packet,
			8,
			20,
			!pad_ready
				? "CONNECT CONTROLLER"
				: analog_pad &&
					calibration_samples < STICK_CALIBRATION_SAMPLES
					? "CENTER STICK TO CALIBRATE"
					: "D-PAD / STICK: MOVE CIRCLE"
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
