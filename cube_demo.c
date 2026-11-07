#include <stdint.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <psxpad.h>
#include <inline_c.h>

typedef struct {
	uint16_t vertices[3];
	uint8_t color[3];
	uint8_t textured;
	uint8_t uv[3][2];
} ModelFace;

#include "car_model.h"
#include "wheel_texture.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 1024
#define CAMERA_TILT 256
#define CAMERA_DEPTH 700
#define GROUND_HEIGHT 65
#define GRID_LINES 16
#define DRIVE_LIMIT_X 180
#define DRIVE_LIMIT_Z 120
#define FIXED_SCALE 4096
#define BODY_SUBDIVISIONS 8
#define BODY_GRID_VERTICES ((BODY_SUBDIVISIONS + 1) * (BODY_SUBDIVISIONS + 2) / 2)
#define BODY_TRIANGLES (CAR_BODY_FACE_COUNT * BODY_SUBDIVISIONS * BODY_SUBDIVISIONS)
#define MAX_MODEL_TRIANGLES (BODY_TRIANGLES + CAR_FACE_COUNT - CAR_BODY_FACE_COUNT)

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	union {
		POLY_F3 flat;
		POLY_FT3 textured;
	} faces[MAX_MODEL_TRIANGLES];
	LINE_F2 grid[GRID_LINES];
} FrameBuffer;

static FrameBuffer buffers[2];
static uint8_t pad_buffers[2][34];
static SVECTOR body_vertices[CAR_BODY_FACE_COUNT][BODY_GRID_VERTICES];
static uint16_t body_triangles[BODY_SUBDIVISIONS * BODY_SUBDIVISIONS][3];

static MATRIX light_matrix = {
	-2048, -2896, -2048,
	0, 0, 0,
	0, 0, 0
};

static MATRIX color_matrix = {
	ONE * 3 / 4, 0, 0,
	ONE * 3 / 4, 0, 0,
	ONE * 3 / 4, 0, 0
};

static int grid_vertex_index(int row, int column) {
	return row * (BODY_SUBDIVISIONS + 1) - row * (row - 1) / 2 + column;
}

/* Smaller body triangles improve painter's-algorithm sorting against the wheels. */
static void init_body_mesh(void) {
	int triangle_count = 0;

	for (int row = 0; row < BODY_SUBDIVISIONS; row++) {
		for (int column = 0; column < BODY_SUBDIVISIONS - row; column++) {
			int a = grid_vertex_index(row, column);
			int b = grid_vertex_index(row + 1, column);
			int c = grid_vertex_index(row, column + 1);
			body_triangles[triangle_count][0] = a;
			body_triangles[triangle_count][1] = b;
			body_triangles[triangle_count++][2] = c;
			if (row + column < BODY_SUBDIVISIONS - 1) {
				body_triangles[triangle_count][0] = b;
				body_triangles[triangle_count][1] = grid_vertex_index(row + 1, column + 1);
				body_triangles[triangle_count++][2] = c;
			}
		}
	}

	for (int i = 0; i < CAR_BODY_FACE_COUNT; i++) {
		const ModelFace *face = &car_faces[i];
		const SVECTOR *a = &car_vertices[face->vertices[0]];
		const SVECTOR *b = &car_vertices[face->vertices[1]];
		const SVECTOR *c = &car_vertices[face->vertices[2]];

		for (int row = 0; row <= BODY_SUBDIVISIONS; row++) {
			for (int column = 0; column <= BODY_SUBDIVISIONS - row; column++) {
				SVECTOR *v = &body_vertices[i][grid_vertex_index(row, column)];
				int weight = BODY_SUBDIVISIONS - row - column;
				v->vx = (a->vx * weight + b->vx * row + c->vx * column) / BODY_SUBDIVISIONS;
				v->vy = (a->vy * weight + b->vy * row + c->vy * column) / BODY_SUBDIVISIONS;
				v->vz = (a->vz * weight + b->vz * row + c->vz * column) / BODY_SUBDIVISIONS;
				v->pad = 0;
			}
		}
	}
}

static void draw_grid_line(FrameBuffer *buffer, int index, SVECTOR *start, SVECTOR *end) {
	LINE_F2 *line = &buffer->grid[index];
	int depth;

	gte_ldv3(start, end, end);
	gte_rtpt();
	gte_avsz3();
	gte_stotz(&depth);
	if (depth <= 0 || depth >= OT_LENGTH)
		return;

	setLineF2(line);
	setRGB0(line, 72, 94, 103);
	gte_stsxy0(&line->x0);
	gte_stsxy1(&line->x1);
	addPrim(&buffer->ordering_table[depth], line);
}

static void draw_grid(FrameBuffer *buffer) {
	SVECTOR camera_rotation = { CAMERA_TILT, 0, 0, 0 };
	VECTOR camera_position = { 0, 0, CAMERA_DEPTH };
	MATRIX camera;
	int line = 0;

	RotMatrix(&camera_rotation, &camera);
	TransMatrix(&camera, &camera_position);
	gte_SetRotMatrix(&camera);
	gte_SetTransMatrix(&camera);

	for (int x = -400; x <= 400; x += 100) {
		SVECTOR start = { x, GROUND_HEIGHT, -240, 0 };
		SVECTOR end = { x, GROUND_HEIGHT, 240, 0 };
		draw_grid_line(buffer, line++, &start, &end);
	}
	for (int z = -240; z <= 240; z += 80) {
		SVECTOR start = { -400, GROUND_HEIGHT, z, 0 };
		SVECTOR end = { 400, GROUND_HEIGHT, z, 0 };
		draw_grid_line(buffer, line++, &start, &end);
	}
}

int main(void) {
	SVECTOR rotation = { CAMERA_TILT, 0, 0, 0 };
	int32_t car_x = 0;
	int32_t car_z = 0;
	int active_buffer = 0;

	ResetGraph(0);
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;
		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 18, 24, 38);
		buffers[i].draw.isbg = 1;
	}

	InitGeom();
	init_body_mesh();
	gte_SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	gte_SetGeomScreen(220);
	gte_SetBackColor(64, 64, 64);
	gte_SetColorMatrix(&color_matrix);
	init_wheel_texture();
	InitPAD(pad_buffers[0], sizeof(pad_buffers[0]), pad_buffers[1], sizeof(pad_buffers[1]));
	StartPAD();
	ChangeClearPAD(0);
	SetDispMask(1);

	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		volatile PADTYPE *pad = (volatile PADTYPE *) pad_buffers[0];
		int pad_ready = !pad->stat &&
			(pad->type == PAD_ID_DIGITAL ||
			 pad->type == PAD_ID_ANALOG ||
			 pad->type == PAD_ID_ANALOG_STICK);
		uint16_t buttons = pad_ready ? pad->btn : 0xffff;
		int drive = (!(buttons & PAD_UP)) - (!(buttons & PAD_DOWN));
		int steer = (!(buttons & PAD_RIGHT)) - (!(buttons & PAD_LEFT));
		int x;
		int z;
		VECTOR position;
		MATRIX transform;
		MATRIX lighting;
		int packet_count = 0;

		if (drive) {
			rotation.vy = (rotation.vy + steer * drive * 20) & 4095;
			car_x += drive * 4 * icos(rotation.vy);
			car_z -= drive * 4 * isin(rotation.vy);
			if (car_x < -DRIVE_LIMIT_X * FIXED_SCALE)
				car_x = -DRIVE_LIMIT_X * FIXED_SCALE;
			if (car_x > DRIVE_LIMIT_X * FIXED_SCALE)
				car_x = DRIVE_LIMIT_X * FIXED_SCALE;
			if (car_z < -DRIVE_LIMIT_Z * FIXED_SCALE)
				car_z = -DRIVE_LIMIT_Z * FIXED_SCALE;
			if (car_z > DRIVE_LIMIT_Z * FIXED_SCALE)
				car_z = DRIVE_LIMIT_Z * FIXED_SCALE;
		}
		x = car_x / FIXED_SCALE;
		z = car_z / FIXED_SCALE;
		position.vx = x;
		position.vy = -(z * isin(CAMERA_TILT)) / FIXED_SCALE;
		position.vz = CAMERA_DEPTH + (z * icos(CAMERA_TILT)) / FIXED_SCALE;

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		draw_grid(draw_buffer);
		RotMatrix(&rotation, &transform);
		TransMatrix(&transform, &position);
		MulMatrix0(&light_matrix, &transform, &lighting);
		gte_SetRotMatrix(&transform);
		gte_SetTransMatrix(&transform);
		gte_SetLightMatrix(&lighting);

		for (int i = 0; i < CAR_FACE_COUNT; i++) {
			const ModelFace *face = &car_faces[i];
			int triangle_count = i < CAR_BODY_FACE_COUNT
				? BODY_SUBDIVISIONS * BODY_SUBDIVISIONS : 1;
			for (int triangle = 0; triangle < triangle_count; triangle++) {
				const SVECTOR *a;
				const SVECTOR *b;
				const SVECTOR *c;
				int facing;
				int depth;

				if (i < CAR_BODY_FACE_COUNT) {
					a = &body_vertices[i][body_triangles[triangle][0]];
					b = &body_vertices[i][body_triangles[triangle][1]];
					c = &body_vertices[i][body_triangles[triangle][2]];
				} else {
					a = &car_vertices[face->vertices[0]];
					b = &car_vertices[face->vertices[1]];
					c = &car_vertices[face->vertices[2]];
				}
				gte_ldv3(a, b, c);
				gte_rtpt();
				gte_nclip();
				gte_stopz(&facing);
				if (facing >= 0)
					continue;

				gte_avsz3();
				gte_stotz(&depth);
				if (depth <= 0 || depth >= OT_LENGTH)
					continue;

				if (face->textured) {
					POLY_FT3 *polygon = &draw_buffer->faces[packet_count].textured;
					setPolyFT3(polygon);
					setRGB0(polygon, face->color[0], face->color[1], face->color[2]);
					setUV3(polygon, face->uv[0][0], face->uv[0][1],
						face->uv[1][0], face->uv[1][1], face->uv[2][0], face->uv[2][1]);
					polygon->tpage = getTPage(2, 0, 640, 0);
					gte_stsxy0(&polygon->x0);
					gte_stsxy1(&polygon->x1);
					gte_stsxy2(&polygon->x2);
					gte_ldrgb(&polygon->r0);
					gte_ldv0(&car_normals[i]);
					gte_nccs();
					gte_strgb(&polygon->r0);
					addPrim(&draw_buffer->ordering_table[depth], polygon);
				} else {
					POLY_F3 *polygon = &draw_buffer->faces[packet_count].flat;
					setPolyF3(polygon);
					setRGB0(polygon, face->color[0], face->color[1], face->color[2]);
					gte_stsxy0(&polygon->x0);
					gte_stsxy1(&polygon->x1);
					gte_stsxy2(&polygon->x2);
					gte_ldrgb(&polygon->r0);
					gte_ldv0(&car_normals[i]);
					gte_nccs();
					gte_strgb(&polygon->r0);
					addPrim(&draw_buffer->ordering_table[depth], polygon);
				}
				packet_count++;
			}
		}

		DrawSync(0);
		VSync(0);
		PutDispEnv(&display_buffer->display);
		DrawOTagEnv(&draw_buffer->ordering_table[OT_LENGTH - 1], &draw_buffer->draw);
		active_buffer ^= 1;
	}
}
