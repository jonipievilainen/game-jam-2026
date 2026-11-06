#include <stdint.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include <inline_c.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 256
#define PACKET_BUFFER_SIZE 8192
#define FLOOR_TILE_SIZE 80
#define CAMERA_DISTANCE 360
#define DEADZONE 16
#define MOVE_SPEED 4

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	uint8_t packets[PACKET_BUFFER_SIZE];
} FrameBuffer;

typedef struct {
	int x;
	int y;
	int z;
} WorldPoint;

static FrameBuffer buffers[2];
static uint8_t pad_buffers[2][34];

static const int box_faces[6][4] = {
	{ 0, 1, 2, 3 },
	{ 4, 5, 6, 7 },
	{ 5, 4, 0, 1 },
	{ 6, 7, 3, 2 },
	{ 0, 2, 5, 7 },
	{ 3, 1, 6, 4 }
};

static void setup_buffers(void) {
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;

		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 24, 34, 48);
		buffers[i].draw.isbg = 1;
		buffers[i].draw.dtd = 1;
	}
}

static int deadzone_axis(int axis) {
	if (axis > -DEADZONE && axis < DEADZONE)
		return 0;
	if (axis > 0)
		return (axis - DEADZONE) * 127 / (127 - DEADZONE);
	return (axis + DEADZONE) * 127 / (127 - DEADZONE);
}

static int divide_rounded(int value, int divisor) {
	if (value >= 0)
		return (value + divisor / 2) / divisor;
	return (value - divisor / 2) / divisor;
}

static int clamp_position(int position) {
	if (position < -30000)
		return -30000;
	if (position > 30000)
		return 30000;
	return position;
}

static void cross_product(const SVECTOR *a, const SVECTOR *b, VECTOR *result) {
	result->vx = ((int) a->vy * b->vz - (int) a->vz * b->vy) >> 12;
	result->vy = ((int) a->vz * b->vx - (int) a->vx * b->vz) >> 12;
	result->vz = ((int) a->vx * b->vy - (int) a->vy * b->vx) >> 12;
}

static void look_at(const VECTOR *eye, const VECTOR *target, MATRIX *matrix) {
	SVECTOR world_up = { 0, -ONE, 0, 0 };
	SVECTOR forward, right, up;
	VECTOR direction, temporary, translation, offset;

	direction.vx = target->vx - eye->vx;
	direction.vy = target->vy - eye->vy;
	direction.vz = target->vz - eye->vz;
	VectorNormalS(&direction, &forward);
	cross_product(&forward, &world_up, &temporary);
	VectorNormalS(&temporary, &right);
	cross_product(&forward, &right, &temporary);
	VectorNormalS(&temporary, &up);

	matrix->m[0][0] = right.vx;
	matrix->m[1][0] = up.vx;
	matrix->m[2][0] = forward.vx;
	matrix->m[0][1] = right.vy;
	matrix->m[1][1] = up.vy;
	matrix->m[2][1] = forward.vy;
	matrix->m[0][2] = right.vz;
	matrix->m[1][2] = up.vz;
	matrix->m[2][2] = forward.vz;

	offset.vx = -eye->vx;
	offset.vy = -eye->vy;
	offset.vz = -eye->vz;
	ApplyMatrixLV(matrix, &offset, &translation);
	TransMatrix(matrix, &translation);
}

static uint8_t *draw_quad(
	uint8_t *packet,
	uint32_t *ordering_table,
	const SVECTOR vertices[4],
	int red,
	int green,
	int blue,
	int cull_backface
) {
	int depth, area;
	POLY_F4 *polygon = (POLY_F4 *) packet;

	gte_ldv3(&vertices[0], &vertices[1], &vertices[2]);
	gte_rtpt();

	if (cull_backface) {
		gte_nclip();
		gte_stopz(&area);
		if (area <= 0)
			return packet;
	}

	gte_ldv0(&vertices[3]);
	gte_rtps();
	gte_avsz4();
	gte_stotz(&depth);
	depth >>= 2;
	if (depth <= 0 || depth >= OT_LENGTH)
		return packet;

	setPolyF4(polygon);
	setRGB0(polygon, red, green, blue);
	gte_stsxy0(&polygon->x0);
	gte_stsxy1(&polygon->x1);
	gte_stsxy2(&polygon->x2);
	gte_stsxy(&polygon->x3);
	addPrim(&ordering_table[depth], polygon);

	return packet + sizeof(POLY_F4);
}

static void make_box(
	WorldPoint points[8],
	int player_x,
	int player_z,
	int player_yaw,
	int center_x,
	int center_y,
	int center_z,
	int half_width,
	int half_height,
	int half_depth
) {
	static const int8_t signs[8][3] = {
		{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 },
		{ 1, -1, 1 }, { -1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
	};
	int sine = isin(player_yaw);
	int cosine = icos(player_yaw);

	for (int i = 0; i < 8; i++) {
		int local_x = center_x + signs[i][0] * half_width;
		int local_y = center_y + signs[i][1] * half_height;
		int local_z = center_z + signs[i][2] * half_depth;

		points[i].x = player_x + ((local_x * cosine + local_z * sine) >> 12);
		points[i].y = local_y;
		points[i].z = player_z + ((-local_x * sine + local_z * cosine) >> 12);
	}
}

static uint8_t *draw_box(
	uint8_t *packet,
	uint32_t *ordering_table,
	int player_x,
	int player_z,
	int player_yaw,
	int center_x,
	int center_y,
	int center_z,
	int half_width,
	int half_height,
	int half_depth,
	int red,
	int green,
	int blue
) {
	WorldPoint points[8];

	make_box(
		points, player_x, player_z, player_yaw,
		center_x, center_y, center_z,
		half_width, half_height, half_depth
	);

	for (int face = 0; face < 6; face++) {
		SVECTOR vertices[4];

		for (int corner = 0; corner < 4; corner++) {
			WorldPoint point = points[box_faces[face][corner]];
			vertices[corner] = (SVECTOR) {
				.vx = (int16_t) point.x,
				.vy = (int16_t) point.y,
				.vz = (int16_t) point.z,
				.pad = 0
			};
		}

		packet = draw_quad(
			packet, ordering_table, vertices, red, green, blue, 1
		);
	}

	return packet;
}

static uint8_t *draw_floor(
	uint8_t *packet,
	uint32_t *ordering_table,
	int player_x,
	int player_z
) {
	int start_x = player_x - 3 * FLOOR_TILE_SIZE;
	int start_z = player_z - 3 * FLOOR_TILE_SIZE;

	for (int row = 0; row < 7; row++) {
		for (int column = 0; column < 7; column++) {
			int x0 = start_x + column * FLOOR_TILE_SIZE;
			int x1 = x0 + FLOOR_TILE_SIZE;
			int z0 = start_z + row * FLOOR_TILE_SIZE;
			int z1 = z0 + FLOOR_TILE_SIZE;
			int shade = ((row + column) & 1) ? 92 : 72;
			SVECTOR vertices[4] = {
				{ x0, 0, z1, 0 },
				{ x1, 0, z1, 0 },
				{ x0, 0, z0, 0 },
				{ x1, 0, z0, 0 }
			};

			packet = draw_quad(
				packet, ordering_table, vertices,
				shade, shade + 16, shade + 20, 1
			);
		}
	}

	return packet;
}

int main(void) {
	ResetGraph(0);
	InitGeom();
	gte_SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	gte_SetGeomScreen(SCREEN_WIDTH / 2);
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

	int player_x = 0;
	int player_z = 0;
	int player_x_fraction = 0;
	int player_z_fraction = 0;
	int camera_yaw = 0;
	int camera_pitch = 80;
	int active_buffer = 0;

	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		uint8_t *packet = draw_buffer->packets;
		volatile PADTYPE *pad = (volatile PADTYPE *) pad_buffers[0];
		int analog_pad =
			!pad->stat &&
			(pad->type == PAD_ID_ANALOG || pad->type == PAD_ID_ANALOG_STICK);
		int move_x = analog_pad ? deadzone_axis((int) pad->ls_x - 128) : 0;
		int move_y = analog_pad ? deadzone_axis(128 - (int) pad->ls_y) : 0;
		int look_x = analog_pad ? deadzone_axis((int) pad->rs_x - 128) : 0;
		int look_y = analog_pad ? deadzone_axis(128 - (int) pad->rs_y) : 0;
		int sine = isin(camera_yaw);
		int cosine = icos(camera_yaw);

		camera_yaw = (camera_yaw + look_x / 64) & 1023;
		camera_pitch += look_y / 64;
		if (camera_pitch < 40)
			camera_pitch = 40;
		if (camera_pitch > 175)
			camera_pitch = 175;

		sine = isin(camera_yaw);
		cosine = icos(camera_yaw);
		player_x_fraction += divide_rounded(
			(-sine * move_y + cosine * move_x) * MOVE_SPEED * 256,
			ONE * 127
		);
		player_z_fraction += divide_rounded(
			(-cosine * move_y - sine * move_x) * MOVE_SPEED * 256,
			ONE * 127
		);
		player_x = clamp_position(
			player_x + player_x_fraction / 256
		);
		player_z = clamp_position(
			player_z + player_z_fraction / 256
		);
		player_x_fraction %= 256;
		player_z_fraction %= 256;

		int camera_horizontal_distance =
			(icos(camera_pitch) * CAMERA_DISTANCE) >> 12;
		int camera_height = (isin(camera_pitch) * CAMERA_DISTANCE) >> 12;
		VECTOR eye = {
			player_x + ((sine * camera_horizontal_distance) >> 12),
			-camera_height,
			player_z + ((cosine * camera_horizontal_distance) >> 12)
		};
		VECTOR target = { player_x, -36, player_z };
		MATRIX view_matrix;
		look_at(&eye, &target, &view_matrix);
		gte_SetRotMatrix(&view_matrix);
		gte_SetTransMatrix(&view_matrix);

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		packet = draw_floor(
			packet, draw_buffer->ordering_table, player_x, player_z
		);
		packet = draw_box(
			packet, draw_buffer->ordering_table,
			player_x, player_z, 0,
			0, -20, 0, 20, 20, 20,
			50, 165, 230
		);

		FntSort(
			&draw_buffer->ordering_table[0],
			packet,
			8,
			8,
			analog_pad
				? "L STICK: MOVE\nR STICK: CAMERA"
				: "NO ANALOG PAD\nSET CONTROLLER TO ANALOG"
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
