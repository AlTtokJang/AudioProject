/*
 * visual_renderer.c
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#include "visual_renderer.h"
#include <string.h>
#include <math.h>

static uint8_t s_frame[MATRIX_HEIGHT][MATRIX_WIDTH][3];
static uint8_t s_prevFrame[MATRIX_HEIGHT][MATRIX_WIDTH][3];
static uint8_t s_gammaTable[256];

// gamma
static void BuildGamma(void)
{
	for (int i = 0; i < 256; i++)
	{
		float x = (float)i / 255.0f;
		s_gammaTable[i] = powf(x, 2.2f) * 255;
	}
}

// init
void VisualRenderer_Init(void)
{
	memset(s_frame, 0, sizeof(s_frame));
	memset(s_prevFrame, 0, sizeof(s_prevFrame));
	BuildGamma();
}

void VisualRenderer_Clear(void)
{
	memset(s_frame, 0, sizeof(s_frame));
}

// spectrum
void VisualRenderer_DrawSpectrum(const float *trail, const float *peakHold)
{
	for (int x = 0; x < MATRIX_WIDTH; x++)
	for (int y = 0; y < MATRIX_HEIGHT; y++)
	{
		float r=0,g=0,b=0;

		float glow = expf(-powf(y - trail[x],2)*0.6f);

		float t = (float)y / (MATRIX_HEIGHT-1);

		if (t < 0.4f) { g=255; b=t*450; }
		else if (t < 0.7f) { r=(t-0.4f)*666; b=200; }
		else { r=255; b=255-(t-0.7f)*850; }

		r*=glow; g*=glow; b*=glow;

		float pg = expf(-powf(y-peakHold[x],2)*0.6f)*160;

		r+=pg; g+=pg; b+=pg;

		s_frame[y][x][0] = s_gammaTable[(uint8_t)r];
		s_frame[y][x][1] = s_gammaTable[(uint8_t)g];
		s_frame[y][x][2] = s_gammaTable[(uint8_t)b];
	}
}

// blur
void VisualRenderer_ApplyTemporalBlur(void)
{
	for (int y=0;y<MATRIX_HEIGHT;y++)
	for (int x=0;x<MATRIX_WIDTH;x++)
	for (int c=0;c<3;c++)
	{
		uint16_t m =
			s_frame[y][x][c]*180 +
			s_prevFrame[y][x][c]*76;

		m >>= 8;

		s_prevFrame[y][x][c] = s_frame[y][x][c];
		s_frame[y][x][c] = m;
	}
}

void VisualRenderer_ApplySpatialBlur(void)
{
	uint8_t tmp[MATRIX_HEIGHT][MATRIX_WIDTH][3];
	memcpy(tmp,s_frame,sizeof(tmp));

	for (int y=1;y<MATRIX_HEIGHT-1;y++)
	for (int x=1;x<MATRIX_WIDTH-1;x++)
	for (int c=0;c<3;c++)
	{
		s_frame[y][x][c] =
		(tmp[y][x][c]*4 +
		tmp[y-1][x][c] +
		tmp[y+1][x][c] +
		tmp[y][x-1][c] +
		tmp[y][x+1][c]) / 8;
	}
}

const uint8_t *VisualRenderer_GetFrame(void)
{
	return &s_frame[0][0][0];
}
