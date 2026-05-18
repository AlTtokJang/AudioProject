/*
 * fft.c
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#include "stm32f7xx_hal.h"

#define ARM_MATH_CM7
#include "arm_math.h"

#include "audio_ring.h"
#include "audio_pipeline.h"
#include "fft.h"
#include "global_define.h"

#include <string.h>
#include <math.h>


static arm_rfft_fast_instance_f32 s_fft;

static int16_t s_monoFrame[FFT_SIZE];

static float32_t s_window[FFT_SIZE];
static float32_t s_fftInput[FFT_SIZE];
static float32_t s_fftOutput[FFT_SIZE];
static float32_t s_mag[FFT_SIZE / 2U];

static uint32_t s_bandStartBin[FFT_BAND_COUNT];
static uint32_t s_bandEndBin[FFT_BAND_COUNT];

static float32_t s_bandRaw[FFT_BAND_COUNT];

// -------------------------
static uint32_t FFT_FreqToBin(float32_t f)
{
    float32_t bin = (f * FFT_SIZE) / 48000.0f;

    if (bin < 1) bin = 1;
    if (bin > FFT_SIZE/2 - 1) bin = FFT_SIZE/2 - 1;

    return (uint32_t)bin;
}

// -------------------------
static void FFT_Window(void)
{
    for (int i = 0; i < FFT_SIZE; i++)
    {
        float32_t x = (2.0f * PI * i) / (FFT_SIZE - 1);
        s_window[i] =
            0.35875f
          - 0.48829f * arm_cos_f32(x)
          + 0.14128f * arm_cos_f32(2*x)
          - 0.01168f * arm_cos_f32(3*x);
    }
}

// -------------------------
static void FFT_BandInit(void)
{
    float min = FFT_MIN_FREQ_HZ;
    float max = FFT_MAX_FREQ_HZ;

    for (int i = 0; i < FFT_BAND_COUNT; i++)
    {
        float t0 = (float)i / FFT_BAND_COUNT;
        float t1 = (float)(i+1) / FFT_BAND_COUNT;

        float start = min * powf(max/min, powf(t0, 0.78f));
        float end   = min * powf(max/min, powf(t1, 0.78f));

        s_bandStartBin[i] = FFT_FreqToBin(start);
        s_bandEndBin[i]   = FFT_FreqToBin(end);
    }
}

// -------------------------
void FFT_Init(void)
{
    arm_rfft_fast_init_f32(&s_fft, FFT_SIZE);
    FFT_Window();
    FFT_BandInit();
}

// -------------------------
uint8_t FFT_Run(void)
{
    if (AudioPipeline_Mono_Available() < FFT_SIZE)
        return 0;

    AudioPipeline_PopMono(s_monoFrame, FFT_SIZE);

    float mean = 0;
    for (int i = 0; i < FFT_SIZE; i++)
        mean += s_monoFrame[i];
    mean /= FFT_SIZE;

    for (int i = 0; i < FFT_SIZE; i++)
        s_fftInput[i] =
            ((float)s_monoFrame[i] - mean)/32768.0f
            * s_window[i];

    arm_rfft_fast_f32(&s_fft, s_fftInput, s_fftOutput, 0);

    for (int k = 1; k < FFT_SIZE/2; k++)
    {
        float re = s_fftOutput[2*k];
        float im = s_fftOutput[2*k+1];
        s_mag[k] = sqrtf(re*re + im*im);
    }

    for (int b = 0; b < FFT_BAND_COUNT; b++)
    {
        float sum = 0;
        int count = 0;

        for (int k = s_bandStartBin[b]; k <= s_bandEndBin[b]; k++)
        {
            sum += s_mag[k];
            count++;
        }

        s_bandRaw[b] = (count > 0) ? sum / count : 0;
    }

    return 1;
}

// -------------------------
const float *FFT_GetBandRaw(void)
{
    return s_bandRaw;
}
