/*
 * audio_pipeline.c
 *
 *  Created on: 2026. 5. 8.
 *      Author: ADJ
 */

#include "audio_pipeline.h"
#include "main.h"

#include "global_define.h"
#include "audio_ring.h"

#define AUDIO_RING_SIZE (MASTER_BLOCK_SIZE * 16U)
#define FFT_RING_SIZE	(MASTER_BLOCK_SIZE * 32U)

static AudioRing_t inputRing;
static AudioRing_t outputRing;
static AudioRing_t fftRing;

static int16_t inputRingMemory[AUDIO_RING_SIZE];
static int16_t outputRingMemory[AUDIO_RING_SIZE];
static int16_t fftRingMemory[FFT_RING_SIZE];

void AudioPipeline_Init(void)
{
	AudioRing_Init(&inputRing, inputRingMemory, AUDIO_RING_SIZE);
	AudioRing_Init(&outputRing, outputRingMemory, AUDIO_RING_SIZE);
	AudioRing_Init(&fftRing, fftRingMemory, FFT_RING_SIZE);
}

void AudioPipeline_Push(const int16_t *src, uint16_t count)
{
	if (src == NULL || count == 0)
	{
		return;
	}

	AudioRing_Push(&inputRing, src, count);

	if (AudioRing_HasOverflow(&inputRing))
	{
		Error_Indicator(AUDIO_ERR_RING_OVERFLOW);
		AudioRing_ClearFlags(&inputRing);
	}
}












