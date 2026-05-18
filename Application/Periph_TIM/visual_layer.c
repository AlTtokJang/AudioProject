/*
 * visual_layer.c
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#include "visual_layer.h"
#include "visual_renderer.h"
#include "audio_pipeline.h"
#include "adc.h"

#include <string.h>
#include <math.h>

#define FFT_BAND_COUNT		16U

#define DEFAULT_GAIN		0.01F
#define PEAK_BUFFER_SIZE	512U

#define USB_THRESHOLD		0.001F
#define ADC_THRESHOLD		3.0F

#define DB_TO_LINEAR(db)	(powf(10.0f, (db) / 20.0f))
#define TARGET_PEAK_DB		-8.0F

#define DB_MAX				0.0F

extern volatile AudioSource_t audioSource;

static float s_gain = DEFAULT_GAIN;
static float s_targetGain;

static float s_peakBuffer[PEAK_BUFFER_SIZE];
static uint32_t s_peakIndex;
static float s_peakSum;
static uint8_t s_peakReady;

static float s_trail[FFT_BAND_COUNT];
static float s_peakHold[FFT_BAND_COUNT];

static uint32_t s_silenceCounter;
static uint8_t s_idleMode;

static VisualAGCMode_t s_agcMode = VISUAL_AGC_HOLD;

// -------------------------
void Visual_Init(void)
{
	Visual_Reset();
	VisualRenderer_Init();
}

// -------------------------
void Visual_Reset(void)
{
	memset(s_peakBuffer, 0, sizeof(s_peakBuffer));
	memset(s_trail, 0, sizeof(s_trail));
	memset(s_peakHold, 0, sizeof(s_peakHold));

	s_gain = DEFAULT_GAIN;
	s_targetGain = DEFAULT_GAIN;

	s_peakIndex = 0;
	s_peakSum = 0;
	s_peakReady = 0;

	s_idleMode = 0;
	s_silenceCounter = 0;

	VisualRenderer_Clear();
}

// -------------------------
void Visual_SetAGCMode(VisualAGCMode_t mode)
{
	s_agcMode = mode;
}

// -------------------------
void Visual_Process(const float *bandRaw)
{
	float peak = 0;
	float totalEnergy = 0;

	float threshold =
	(audioSource == AUDIO_SRC_USB) ? USB_THRESHOLD : ADC_THRESHOLD;

	for (int i = 0; i < FFT_BAND_COUNT; i++)
	if (bandRaw[i] > peak) peak = bandRaw[i];

	// ---------------- AGC ----------------
	if (peak > threshold)
	{
		s_peakSum -= s_peakBuffer[s_peakIndex];
		s_peakBuffer[s_peakIndex] = peak;
		s_peakSum += peak;

		if (++s_peakIndex >= PEAK_BUFFER_SIZE)
		{
			s_peakIndex = 0;
			s_peakReady = 1;
		}

		if (s_peakReady)
		{
			float avg = s_peakSum / PEAK_BUFFER_SIZE;
			s_targetGain = DB_TO_LINEAR(TARGET_PEAK_DB) / avg;
		}
	}

	if (s_agcMode == VISUAL_AGC_LIVE)
		s_gain += 0.08f * (s_targetGain - s_gain);
	else if (s_agcMode == VISUAL_AGC_OFF)
		s_gain = 1.0f;

    // ---------------- BAND ----------------
	for (int i = 0; i < FFT_BAND_COUNT; i++)
	{
		uint8_t vreg_raw = 0;
		ADC_GetValue_VOL(&vreg_raw);

		float dbMin = 0.0;
		dbMin = -60 +0.4f * (float)vreg_raw;

		float v = (s_agcMode == VISUAL_AGC_OFF) ? bandRaw[i] : bandRaw[i] * s_gain;

		float db = 20.0f * log10f(v < 1e-12f ? 1e-12f : v);

		float norm = (db - dbMin) / (DB_MAX - dbMin);

		float led = norm * 16.0f;

		totalEnergy += led;

		float alpha = (led > s_trail[i]) ? 0.35f : 0.05f;

		s_trail[i] += alpha * (led - s_trail[i]);

		if (s_trail[i] > s_peakHold[i])
			s_peakHold[i] = s_trail[i];
		else
			s_peakHold[i] *= 0.98f;
	}

    // ---------------- IDLE ----------------
	if (totalEnergy < 8.0f)
		s_silenceCounter++;
	else
	{
		s_silenceCounter = 0;
		s_idleMode = 0;
	}

	if (s_silenceCounter > 500)
		s_idleMode = 1;

	// ---------------- RENDER ----------------
	VisualRenderer_DrawSpectrum(s_trail, s_peakHold);
	//VisualRenderer_ApplySpatialBlur();
	//VisualRenderer_ApplyTemporalBlur();
}

// -------------------------
const float *Visual_GetTrail(void)
{
	return s_trail;
}
const float *Visual_GetPeak(void)
{
	return s_peakHold;
}


uint8_t Visual_IsIdleMode(void)
{
	return s_idleMode;
}
