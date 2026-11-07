#include <stdint.h>
#include <psxgpu.h>
#include <psxgte.h>
#include <inline_c.h>

typedef struct {
	uint16_t vertices[3];
	uint8_t color[3];
} ModelFace;

#include "car_model.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define OT_LENGTH 1024

typedef struct {
	DISPENV display;
	DRAWENV draw;
	uint32_t ordering_table[OT_LENGTH];
	POLY_F3 faces[CAR_FACE_COUNT];
} FrameBuffer;

static FrameBuffer buffers[2];

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

int main(void) {
	SVECTOR rotation = { 192, 0, 0, 0 };
	VECTOR position = { 0, 0, 500 };
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
	gte_SetGeomOffset(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	gte_SetGeomScreen(220);
	gte_SetBackColor(64, 64, 64);
	gte_SetColorMatrix(&color_matrix);
	SetDispMask(1);

	for (;;) {
		FrameBuffer *draw_buffer = &buffers[active_buffer];
		FrameBuffer *display_buffer = &buffers[active_buffer ^ 1];
		MATRIX transform;
		MATRIX lighting;
		int packet_count = 0;

		ClearOTagR(draw_buffer->ordering_table, OT_LENGTH);
		RotMatrix(&rotation, &transform);
		TransMatrix(&transform, &position);
		MulMatrix0(&light_matrix, &transform, &lighting);
		gte_SetRotMatrix(&transform);
		gte_SetTransMatrix(&transform);
		gte_SetLightMatrix(&lighting);

		for (int i = 0; i < CAR_FACE_COUNT; i++) {
			const ModelFace *face = &car_faces[i];
			POLY_F3 *polygon;
			int facing;
			int depth;

			gte_ldv3(
				&car_vertices[face->vertices[0]],
				&car_vertices[face->vertices[1]],
				&car_vertices[face->vertices[2]]
			);
			gte_rtpt();
			gte_nclip();
			gte_stopz(&facing);
			if (facing >= 0)
				continue;

			gte_avsz3();
			gte_stotz(&depth);
			if (depth <= 0 || depth >= OT_LENGTH)
				continue;

			polygon = &draw_buffer->faces[packet_count];
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
			packet_count++;
		}

		DrawSync(0);
		VSync(0);
		PutDispEnv(&display_buffer->display);
		DrawOTagEnv(&draw_buffer->ordering_table[OT_LENGTH - 1], &draw_buffer->draw);
		active_buffer ^= 1;
		rotation.vy = (rotation.vy + 8) & 4095;
	}
}
