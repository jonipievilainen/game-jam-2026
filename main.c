#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
#define COLLECTIBLE_COUNT 4
#define COLLECTIBLE_RADIUS 8
#define EFFECT_COUNT 5
#define EFFECT_QUEUE_CAPACITY 8

enum {
	EFFECT_SPEED,
	EFFECT_SLOW,
	EFFECT_REVERSED,
	EFFECT_SCREEN_SHAKE,
	EFFECT_COLOR_GLITCH
};

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	uint8_t packets[PACKET_BUFFER_SIZE];
} FrameBuffer;

typedef struct {
	int x;
	int y;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
} Collectible;

static FrameBuffer buffers[2];
static uint8_t pad_buffers[2][34];

static const uint8_t collectible_colors[COLLECTIBLE_COUNT][3] = {
	{ 245, 76, 82 },
	{ 255, 205, 62 },
	{ 145, 96, 245 },
	{ 65, 225, 145 }
};

static const char *const effect_names[EFFECT_COUNT] = {
	"SPEED BOOST!",
	"SLOW MOTION!",
	"REVERSED CONTROLS!",
	"SCREEN SHAKE!",
	"COLOR GLITCH!"
};

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

static void apply_color_filter(
	int *red,
	int *green,
	int *blue,
	int color_filter
) {
	if (color_filter) {
		int old_red = *red;
		*red = *blue;
		*blue = *green;
		*green = old_red;
	}
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
	uint32_t *ordering_table,
	int offset_x,
	int offset_y,
	int color_filter
) {
	POLY_F4 *platform = (POLY_F4 *) packet;
	int red = 66;
	int green = 83;
	int blue = 91;

	setPolyF4(platform);
	apply_color_filter(&red, &green, &blue, color_filter);
	setRGB0(platform, red, green, blue);
	platform->x0 = PLATFORM_LEFT + offset_x;
	platform->y0 = PLATFORM_TOP + offset_y;
	platform->x1 = PLATFORM_RIGHT + offset_x;
	platform->y1 = PLATFORM_TOP + offset_y;
	platform->x2 = PLATFORM_LEFT + offset_x;
	platform->y2 = PLATFORM_BOTTOM + offset_y;
	platform->x3 = PLATFORM_RIGHT + offset_x;
	platform->y3 = PLATFORM_BOTTOM + offset_y;
	addPrim(&ordering_table[2], platform);

	return packet + sizeof(POLY_F4);
}

static uint8_t *draw_circle(
	uint8_t *packet,
	uint32_t *ordering_table,
	int center_x,
	int center_y,
	int radius,
	int red,
	int green,
	int blue,
	int offset_x,
	int offset_y,
	int color_filter
) {
	apply_color_filter(&red, &green, &blue, color_filter);
	for (int i = 0; i < 16; i++) {
		int next = (i + 1) & 15;
		POLY_F3 *triangle = (POLY_F3 *) packet;

		setPolyF3(triangle);
		setRGB0(triangle, red, green, blue);
		triangle->x0 = (int16_t) (center_x + offset_x);
		triangle->y0 = (int16_t) (center_y + offset_y);
		triangle->x1 = (int16_t) (
			center_x + offset_x +
			circle_offsets[i][0] * radius / CIRCLE_RADIUS
		);
		triangle->y1 = (int16_t) (
			center_y + offset_y +
			circle_offsets[i][1] * radius / CIRCLE_RADIUS
		);
		triangle->x2 = (int16_t) (
			center_x + offset_x +
			circle_offsets[next][0] * radius / CIRCLE_RADIUS
		);
		triangle->y2 = (int16_t) (
			center_y + offset_y +
			circle_offsets[next][1] * radius / CIRCLE_RADIUS
		);
		addPrim(&ordering_table[1], triangle);
		packet += sizeof(POLY_F3);
	}

	return packet;
}

static void place_collectible(
	Collectible *collectible,
	const Collectible collectibles[COLLECTIBLE_COUNT],
	int index,
	int player_x,
	int player_y
) {
	int safe = 0;

	while (!safe) {
		safe = 1;
		collectible->x = PLATFORM_LEFT + COLLECTIBLE_RADIUS +
			rand() % (PLATFORM_RIGHT - PLATFORM_LEFT -
				2 * COLLECTIBLE_RADIUS + 1);
		collectible->y = PLATFORM_TOP + COLLECTIBLE_RADIUS +
			rand() % (PLATFORM_BOTTOM - PLATFORM_TOP -
				2 * COLLECTIBLE_RADIUS + 1);

		int player_dx = collectible->x - player_x;
		int player_dy = collectible->y - player_y;
		if (player_dx * player_dx + player_dy * player_dy < 40 * 40)
			safe = 0;

		for (int i = 0; safe && i < COLLECTIBLE_COUNT; i++) {
			if (i == index)
				continue;
			int dx = collectible->x - collectibles[i].x;
			int dy = collectible->y - collectibles[i].y;
			if (dx * dx + dy * dy < 24 * 24)
				safe = 0;
		}
	}

	collectible->red = collectible_colors[index][0];
	collectible->green = collectible_colors[index][1];
	collectible->blue = collectible_colors[index][2];
}

static void start_next_effect(
	const int effect_queue[EFFECT_QUEUE_CAPACITY],
	int *queue_head,
	int *queue_count,
	int *effect_type,
	int *effect_frames,
	int effect_duration
) {
	if (*effect_frames || !*queue_count)
		return;

	*effect_type = effect_queue[*queue_head];
	*queue_head = (*queue_head + 1) % EFFECT_QUEUE_CAPACITY;
	(*queue_count)--;
	*effect_frames = effect_duration;
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
	int effect_type = -1;
	int effect_frames = 0;
	int effect_queue[EFFECT_QUEUE_CAPACITY];
	int queue_head = 0;
	int queue_count = 0;
	int effect_duration =
		GetVideoMode() == MODE_PAL ? 100 : 120;
	char effect_status[48];
	Collectible collectibles[COLLECTIBLE_COUNT] = { 0 };

	srand(VSync(-1));
	for (int i = 0; i < COLLECTIBLE_COUNT; i++)
		place_collectible(
			&collectibles[i], collectibles, i, player_x, player_y
		);

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

		start_next_effect(
			effect_queue,
			&queue_head,
			&queue_count,
			&effect_type,
			&effect_frames,
			effect_duration
		);

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

		if (effect_frames > 0) {
			if (effect_type == EFFECT_SPEED) {
				move_x *= 2;
				move_y *= 2;
			} else if (effect_type == EFFECT_SLOW) {
				if (move_x)
					move_x = move_x > 0 ? 1 : -1;
				if (move_y)
					move_y = move_y > 0 ? 1 : -1;
			} else if (effect_type == EFFECT_REVERSED) {
				move_x = -move_x;
				move_y = -move_y;
			}
		}

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

		for (int i = 0; i < COLLECTIBLE_COUNT; i++) {
			int dx = player_x - collectibles[i].x;
			int dy = player_y - collectibles[i].y;
			int collision_distance = CIRCLE_RADIUS + COLLECTIBLE_RADIUS;
			if (dx * dx + dy * dy <=
				collision_distance * collision_distance &&
				queue_count < EFFECT_QUEUE_CAPACITY) {
				int queue_tail =
					(queue_head + queue_count) % EFFECT_QUEUE_CAPACITY;
				effect_queue[queue_tail] = rand() % EFFECT_COUNT;
				queue_count++;
				place_collectible(
					&collectibles[i], collectibles, i, player_x, player_y
				);
			}
		}

		start_next_effect(
			effect_queue,
			&queue_head,
			&queue_count,
			&effect_type,
			&effect_frames,
			effect_duration
		);

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		int color_filter = effect_frames > 0 &&
			effect_type == EFFECT_COLOR_GLITCH;
		int shake_x = effect_frames > 0 &&
			effect_type == EFFECT_SCREEN_SHAKE
			? ((VSync(-1) & 4) ? 3 : -3)
			: 0;
		int shake_y = effect_frames > 0 &&
			effect_type == EFFECT_SCREEN_SHAKE
			? ((VSync(-1) & 2) ? 2 : -2)
			: 0;

		setRGB0(
			&draw_buffer->draw,
			color_filter ? 38 : 18,
			color_filter ? 18 : 24,
			color_filter ? 24 : 38
		);
		packet = draw_platform(
			packet,
			draw_buffer->ordering_table,
			shake_x,
			shake_y,
			color_filter
		);
		for (int i = 0; i < COLLECTIBLE_COUNT; i++) {
			packet = draw_circle(
				packet,
				draw_buffer->ordering_table,
				collectibles[i].x,
				collectibles[i].y,
				COLLECTIBLE_RADIUS,
				collectibles[i].red,
				collectibles[i].green,
				collectibles[i].blue,
				shake_x,
				shake_y,
				color_filter
			);
		}
		packet = draw_circle(
			packet,
			draw_buffer->ordering_table,
			player_x,
			player_y,
			CIRCLE_RADIUS,
			70,
			190,
			245,
			shake_x,
			shake_y,
			color_filter
		);
		if (effect_frames > 0 && queue_count > 0) {
			snprintf(
				effect_status,
				sizeof(effect_status),
				"%s  QUEUED: %d",
				effect_names[effect_type],
				queue_count
			);
		} else if (effect_frames > 0) {
			snprintf(
				effect_status,
				sizeof(effect_status),
				"%s",
				effect_names[effect_type]
			);
		} else if (queue_count > 0) {
			snprintf(
				effect_status,
				sizeof(effect_status),
				"EFFECTS QUEUED: %d",
				queue_count
			);
		} else if (!pad_ready) {
			snprintf(effect_status, sizeof(effect_status), "CONNECT CONTROLLER");
		} else if (
			analog_pad &&
			calibration_samples < STICK_CALIBRATION_SAMPLES
		) {
			snprintf(
				effect_status,
				sizeof(effect_status),
				"CENTER STICK TO CALIBRATE"
			);
		} else {
			snprintf(
				effect_status,
				sizeof(effect_status),
				"COLLECT COLORED CIRCLES!"
			);
		}

		FntSort(
			&draw_buffer->ordering_table[0],
			packet,
			8,
			20,
			effect_status
		);

		DrawSync(0);
		VSync(0);
		if (effect_frames > 0)
			effect_frames--;
		PutDispEnv(&display_buffer->display);
		DrawOTagEnv(
			&draw_buffer->ordering_table[OT_LENGTH - 1],
			&draw_buffer->draw
		);
		active_buffer ^= 1;
	}

	return 0;
}
