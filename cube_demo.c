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
#include "body_texture.h"
#include "road.h"
#include "wheel_texture.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 240
#define OT_LENGTH 4096
#define CAMERA_TILT 220
#define CAMERA_DEPTH 650
#define GROUND_HEIGHT 65
#define ROAD_SEGMENTS 81
#define ROAD_POLYGONS (ROAD_SEGMENTS * 6)
#define ROAD_DEPTH_BIAS 1024
#define POST_POLYGONS (((ROAD_SEGMENTS + 3) / 4) * 4)
#define MOUNTAIN_COUNT 17
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
	POLY_F4 road[ROAD_POLYGONS];
	POLY_F4 posts[POST_POLYGONS];
	POLY_G4 sky;
	POLY_F3 mountains[MOUNTAIN_COUNT];
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

static void body_uv(const SVECTOR *vertex, const SVECTOR *normal, uint8_t *u, uint8_t *v) {
	int horizontal;
	int panel;
	if (normal->vx) {
		horizontal = (vertex->vz + 90) * 127 / 180;
		panel = normal->vx < 0 ? 0 : 1;
	} else {
		horizontal = (vertex->vx + 160) * 127 / 320;
		panel = 2;
	}
	*u = (uint8_t) horizontal;
	*v = (uint8_t) (panel * 32 + (vertex->vy + 70) * 31 / 120);
}

static void draw_sky(FrameBuffer *buffer, int heading) {
	static const int peaks[] = { 58, 48, 63, 53, 60, 45, 59, 51 };
	POLY_G4 *sky = &buffer->sky;
	setPolyG4(sky);
	setXY4(sky, 0, 0, SCREEN_WIDTH, 0, 0, 76, SCREEN_WIDTH, 76);
	setRGB0(sky, 43, 95, 155);
	setRGB1(sky, 43, 95, 155);
	setRGB2(sky, 161, 197, 211);
	setRGB3(sky, 161, 197, 211);
	addPrim(&buffer->ordering_table[OT_LENGTH - 1], sky);
	for (int i = 0; i < MOUNTAIN_COUNT; i++) {
		POLY_F3 *mountain = &buffer->mountains[i];
		int offset = heading * 1280 / 4096;
		int x = i * 160 - 160 - offset;
		if (x > SCREEN_WIDTH || x + 160 < 0)
			continue;
		setPolyF3(mountain);
		setXY3(mountain, x, 76, x + 80, peaks[i % 8], x + 160, 76);
		setRGB0(mountain, 94, 135, 131);
		addPrim(&buffer->ordering_table[OT_LENGTH - 2], mountain);
	}
}

static void draw_post_quad(FrameBuffer *buffer, int *count, int x, int z, int top, int bottom, int red, int green, int blue) {
	SVECTOR a = { x - 5, top, z, 0 };
	SVECTOR b = { x + 5, top, z, 0 };
	SVECTOR c = { x - 5, bottom, z, 0 };
	SVECTOR d = { x + 5, bottom, z, 0 };
	int za, zb, zc, zd, depth;
	POLY_F4 *polygon = &buffer->posts[*count];

	gte_ldv3(&a, &b, &c);
	gte_rtpt();
	gte_stsxy0(&polygon->x0);
	gte_stsxy1(&polygon->x1);
	gte_stsxy2(&polygon->x2);
	gte_ldv0(&d);
	gte_rtps();
	gte_stsxy(&polygon->x3);
	gte_stsz4(&za, &zb, &zc, &zd);
	if (za < 180 || zb < 180 || zc < 180 || zd < 180)
		return;
	gte_avsz4();
	gte_stotz(&depth);
	if (depth <= 2 || depth >= OT_LENGTH - 1)
		return;
	polygon->y0 /= 2;
	polygon->y1 /= 2;
	polygon->y2 /= 2;
	polygon->y3 /= 2;
	setPolyF4(polygon);
	setRGB0(polygon, red, green, blue);
	addPrim(&buffer->ordering_table[depth - 1], polygon);
	(*count)++;
}

static void draw_road_quad(
	FrameBuffer *buffer, int *count, int near_x, int far_x, int near_z, int far_z,
	int left, int right, int red, int green, int blue, int overlay
) {
	SVECTOR a = { near_x + left, GROUND_HEIGHT, near_z, 0 };
	SVECTOR b = { near_x + right, GROUND_HEIGHT, near_z, 0 };
	SVECTOR c = { far_x + left, GROUND_HEIGHT, far_z, 0 };
	SVECTOR d = { far_x + right, GROUND_HEIGHT, far_z, 0 };
	int za, zb, zc, zd, depth;
	POLY_F4 *polygon = &buffer->road[*count];

	gte_ldv3(&a, &b, &c);
	gte_rtpt();
	gte_stsxy0(&polygon->x0);
	gte_stsxy1(&polygon->x1);
	gte_stsxy2(&polygon->x2);
	gte_ldv0(&d);
	gte_rtps();
	gte_stsxy(&polygon->x3);
	gte_stsz4(&za, &zb, &zc, &zd);
	polygon->y0 /= 2;
	polygon->y1 /= 2;
	polygon->y2 /= 2;
	polygon->y3 /= 2;
	if (za < 180 || zb < 180 || zc < 180 || zd < 180)
		return;
	gte_avsz4();
	gte_stotz(&depth);
	if (depth <= 2 || depth + ROAD_DEPTH_BIAS >= OT_LENGTH - 2)
		return;
	setPolyF4(polygon);
	setRGB0(polygon, red, green, blue);
	/* Ground must be drawn behind the car, even when a road quad crosses its depth. */
	addPrim(&buffer->ordering_table[depth + ROAD_DEPTH_BIAS - overlay], polygon);
	(*count)++;
}

static void draw_road(FrameBuffer *buffer, MATRIX *camera, int64_t car_x, int64_t car_z, uint32_t seed) {
	int64_t first = road_segment_at(car_z) - ROAD_SEGMENTS / 2;
	int count = 0;
	int post_count = 0;

	gte_SetRotMatrix(camera);
	gte_SetTransMatrix(camera);
	for (int i = 0; i < ROAD_SEGMENTS; i++) {
		int64_t segment = first + i;
		int64_t x0 = road_center(segment, seed) - car_x;
		int64_t x1 = road_center(segment + 1, seed) - car_x;
		int64_t z0 = segment * ROAD_SEGMENT_LENGTH - car_z;
		int64_t z1 = z0 + ROAD_SEGMENT_LENGTH;
		if (x0 < -3000 || x0 > 3000 || x1 < -3000 || x1 > 3000)
			continue;
		draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
			-ROAD_HALF_WIDTH - 48, -ROAD_HALF_WIDTH, 116, 105, 78, 0);
		draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
			ROAD_HALF_WIDTH, ROAD_HALF_WIDTH + 48, 116, 105, 78, 0);
		draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
			-ROAD_HALF_WIDTH, ROAD_HALF_WIDTH, 62, 66, 70, 0);
		draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
			-ROAD_HALF_WIDTH + 8, -ROAD_HALF_WIDTH + 16, 215, 215, 200, 1);
		draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
			ROAD_HALF_WIDTH - 16, ROAD_HALF_WIDTH - 8, 215, 215, 200, 1);
		if ((segment & 1) == 0)
			draw_road_quad(buffer, &count, (int) x0, (int) x1, (int) z0, (int) z1,
				-4, 4, 220, 202, 130, 1);
		if (segment % 4 == 0) {
			for (int side = -1; side <= 1; side += 2) {
				int post_x = (int) x0 + side * (ROAD_HALF_WIDTH + 58);
				draw_post_quad(buffer, &post_count, post_x, (int) z0, 10, GROUND_HEIGHT, 225, 222, 203);
				draw_post_quad(buffer, &post_count, post_x, (int) z0, -5, 10, 184, 47, 35);
			}
		}
	}
}

int main(void) {
	SVECTOR car_rotation = { CAMERA_TILT, -1024, 0, 0 };
	int heading = 0;
	int64_t car_x = 0;
	int64_t car_z = 0;
	uint32_t road_seed;
	int active_buffer = 0;

	ResetGraph(0);
	for (int i = 0; i < 2; i++) {
		int y = i * SCREEN_HEIGHT;
		SetDefDrawEnv(&buffers[i].draw, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		SetDefDispEnv(&buffers[i].display, 0, y, SCREEN_WIDTH, SCREEN_HEIGHT);
		setRGB0(&buffers[i].draw, 63, 107, 57);
		buffers[i].draw.isbg = 1;
		buffers[i].draw.dtd = 1;
	}

	InitGeom();
	init_body_mesh();
	gte_SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT);
	gte_SetGeomScreen(360);
	gte_SetBackColor(64, 64, 64);
	gte_SetColorMatrix(&color_matrix);
	init_wheel_texture();
	init_body_texture();
	InitPAD(pad_buffers[0], sizeof(pad_buffers[0]), pad_buffers[1], sizeof(pad_buffers[1]));
	StartPAD();
	ChangeClearPAD(0);
	SetDispMask(1);
	road_seed = (uint32_t) VSync(-1);

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
		SVECTOR camera_rotation = { CAMERA_TILT, 0, 0, 0 };
		VECTOR position = { 0, 75, CAMERA_DEPTH };
		MATRIX camera;
		MATRIX transform;
		MATRIX lighting;
		int packet_count = 0;

		if (drive) {
			heading = (heading + steer * drive * 20) & 4095;
			car_x += drive * 4 * isin(heading);
			car_z += drive * 4 * icos(heading);
		}

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		draw_sky(draw_buffer, heading);
		camera_rotation.vy = -heading;
		RotMatrix(&camera_rotation, &camera);
		TransMatrix(&camera, &position);
		draw_road(draw_buffer, &camera, car_x / FIXED_SCALE, car_z / FIXED_SCALE, road_seed);
		RotMatrix(&car_rotation, &transform);
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

				if (face->textured || (i < CAR_BODY_FACE_COUNT &&
					(car_normals[i].vx || car_normals[i].vz))) {
					POLY_FT3 *polygon = &draw_buffer->faces[packet_count].textured;
					uint8_t uv[3][2];
					setPolyFT3(polygon);
					if (face->textured) {
						setRGB0(polygon, face->color[0], face->color[1], face->color[2]);
						setUV3(polygon, face->uv[0][0], face->uv[0][1],
							face->uv[1][0], face->uv[1][1], face->uv[2][0], face->uv[2][1]);
						polygon->tpage = getTPage(2, 0, 640, 0);
					} else {
						body_uv(a, &car_normals[i], &uv[0][0], &uv[0][1]);
						body_uv(b, &car_normals[i], &uv[1][0], &uv[1][1]);
						body_uv(c, &car_normals[i], &uv[2][0], &uv[2][1]);
						setRGB0(polygon, 128, 128, 128);
						setUV3(polygon, uv[0][0], uv[0][1], uv[1][0], uv[1][1], uv[2][0], uv[2][1]);
						polygon->tpage = getTPage(2, 0, 768, 0);
					}
					gte_stsxy0(&polygon->x0);
					gte_stsxy1(&polygon->x1);
					gte_stsxy2(&polygon->x2);
					polygon->y0 /= 2;
					polygon->y1 /= 2;
					polygon->y2 /= 2;
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
					polygon->y0 /= 2;
					polygon->y1 /= 2;
					polygon->y2 /= 2;
					setRGB0(polygon, 48, 112, 184);
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
