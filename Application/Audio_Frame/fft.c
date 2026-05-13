#include "stm32f7xx_hal.h"

#define ARM_MATH_CM7
#include "arm_math.h"

#include "audio_ring.h"
#include "audio_pipeline.h"
#include "fft.h"
#include "global_define.h"

#include <string.h>
#include <math.h>

extern AudioSource_t audioSource;

// ================================================================
// 내부 상태
// ================================================================
static arm_rfft_fast_instance_f32 s_fft;

static int16_t  s_monoFrame[FFT_SIZE];
//static uint32_t s_monoCount;

static float32_t s_window[FFT_SIZE];
static float32_t s_fftInput[FFT_SIZE];
static float32_t s_fftOutput[FFT_SIZE];
static float32_t s_fftMagnitude[FFT_SIZE / 2U];

static float32_t s_bandStartHz[FFT_BAND_COUNT];
static float32_t s_bandEndHz[FFT_BAND_COUNT];
static uint32_t  s_bandStartBin[FFT_BAND_COUNT];
static uint32_t  s_bandEndBin[FFT_BAND_COUNT];

static float32_t s_bandLevel[FFT_BAND_COUNT];
static float32_t s_bandSmooth[FFT_BAND_COUNT];
static uint8_t   s_ledLevel[FFT_BAND_COUNT];

static float32_t s_gain = DEFAULT_GAIN;
static float32_t s_peakBuffer[PEAK_BUFFER_SIZE];
static uint32_t  s_peakBufIndex;
static float32_t s_peakBufSum;
static uint8_t   s_peakBufReady;

float32_t debugPeak;

// ================================================================
// 내부 함수 선언
// ================================================================
static void FFT_BuildWindow(void);
static uint32_t FFT_FreqToBin(float32_t freqHz);
static void FFT_PrecomputeBandBins(void);

static void FFT_ConvertFrameToFloat(void);
static void FFT_ComputeMagnitude(void);
static void FFT_ComputeBands(void);
static void FFT_ApplySmoothing(void);
static void FFT_UpdateLedLevels(void);
static void FFT_ShiftFrame(void);

// ================================================================
// 초기화
// ================================================================
void FFT_Init(void)
{
    arm_rfft_fast_init_f32(&s_fft, FFT_SIZE);
    FFT_Reset();
    FFT_BuildWindow();
    FFT_PrecomputeBandBins();
}

void FFT_Reset(void)
{
    memset(s_monoFrame, 0, sizeof(s_monoFrame));
    memset(s_fftInput, 0, sizeof(s_fftInput));
    memset(s_fftOutput, 0, sizeof(s_fftOutput));
    memset(s_fftMagnitude, 0, sizeof(s_fftMagnitude));

    memset(s_bandLevel, 0, sizeof(s_bandLevel));
    memset(s_bandSmooth, 0, sizeof(s_bandSmooth));
    memset(s_ledLevel, 0, sizeof(s_ledLevel));

    memset(s_peakBuffer, 0, sizeof(s_peakBuffer));

    //s_monoCount   = 0;
    s_gain         = DEFAULT_GAIN;
    s_peakBufIndex = 0;
    s_peakBufSum   = 0.0f;
    s_peakBufReady = 0;
}

/*
void FFT_PushMonoBlock(const int16_t *samples, uint32_t count)
{
    if (samples == NULL)
    {
        return;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (s_monoCount >= FFT_SIZE)
        {
            break;
        }

        s_monoFrame[s_monoCount] = samples[i];
        s_monoCount++;
    }
}
*/

uint8_t FFT_Run(void)
{
	if (AudioPipeline_Mono_Available() >= FFT_SIZE)
	{
		AudioPipeline_PopMono(s_monoFrame, FFT_SIZE);
	}
	else
	{
		return 0;
	}

    FFT_ConvertFrameToFloat();

    arm_rfft_fast_f32(&s_fft, s_fftInput, s_fftOutput, 0);

    FFT_ComputeMagnitude();
    FFT_ComputeBands();
    FFT_ApplySmoothing();
    FFT_UpdateLedLevels();
    FFT_ShiftFrame();

    return 1;
}

const uint8_t *FFT_GetLedLevels(void)
{
    return s_ledLevel;
}

// ================================================================
// Window
// ================================================================
static void FFT_BuildWindow(void)
{
    const float32_t a0 = 0.35875f;
    const float32_t a1 = 0.48829f;
    const float32_t a2 = 0.14128f;
    const float32_t a3 = 0.01168f;

    for (uint32_t n = 0; n < FFT_SIZE; n++)
    {
        float32_t x = (2.0f * PI * (float32_t)n) / (float32_t)(FFT_SIZE - 1U);

        s_window[n] =
              a0
            - a1 * arm_cos_f32(x)
            + a2 * arm_cos_f32(2.0f * x)
            - a3 * arm_cos_f32(3.0f * x);
    }
}

// ================================================================
// 주파수 -> Bin
// ================================================================
static uint32_t FFT_FreqToBin(float32_t freqHz)
{
    float32_t bin = (freqHz * (float32_t)FFT_SIZE) / 48000.0f;

    if (bin < 1.0f)
    {
        bin = 1.0f;
    }

    if (bin > (float32_t)(FFT_SIZE / 2U - 1U))
    {
        bin = (float32_t)(FFT_SIZE / 2U - 1U);
    }

    return (uint32_t)bin;
}

// ================================================================
// Band bin 미리 계산
// ================================================================
/*
static void FFT_PrecomputeBandBins(void)
{
    float32_t ratio = powf(FFT_MAX_FREQ_HZ / FFT_MIN_FREQ_HZ,
                           1.0f / (float32_t)FFT_BAND_COUNT);

    float32_t startHz = FFT_MIN_FREQ_HZ;

    for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
    {
        float32_t endHz = startHz * ratio;

        s_bandStartHz[b] = startHz;
        s_bandEndHz[b]   = endHz;

        s_bandStartBin[b] = FFT_FreqToBin(startHz);
        s_bandEndBin[b]   = FFT_FreqToBin(endHz);

        if (s_bandEndBin[b] < s_bandStartBin[b])
        {
            s_bandEndBin[b] = s_bandStartBin[b];
        }

        startHz = endHz;
    }
}
*/

// ================================================================
// Band bin 미리 계산 (Custom Log Spacing)
// ================================================================
// 목적:
// FFT 주파수 영역을 LED band 개수에 맞게 분할.
//
// 기존:
// 순수 logarithmic spacing
//
// 변경:
// custom spacingCurve 적용
// → 고역 band를 더 촘촘하게 분할
//
// 효과:
// - 고역 움직임 증가
// - 고역 FFT 평균화 감소
// - 시각적 밸런스 개선
// ================================================================
static void FFT_PrecomputeBandBins(void)
{
	// analyzer 최소 / 최대 주파수
	float32_t minHz = FFT_MIN_FREQ_HZ;
	float32_t maxHz = FFT_MAX_FREQ_HZ;

	// ============================================================
	// spacingCurve
	//
	// 1.0  -> 기존 순수 logarithmic spacing
	// 0.9  -> 약간 고역 강조
	// 0.8  -> 자연스러운 고역 증가
	// 0.7  -> 고역 많이 세분화
	//
	// 값이 작아질수록:
	// → 고역 band 개수 증가
	// → 고역 band 폭 감소
	// ============================================================
	const float32_t spacingCurve = 0.78f;

	// 모든 LED band 계산
	for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
	{
		// ========================================================
		// band 위치를 0 ~ 1 범위로 정규화
		//
		// 예:
		// band 0   -> 0.0
		// band 중간 -> 0.5
		// 마지막 band -> 거의 1.0
		//
		// t0 = 현재 band 시작 위치
		// t1 = 현재 band 끝 위치
		// ========================================================
		float32_t t0 =
				(float32_t)b /
				(float32_t)FFT_BAND_COUNT;

		float32_t t1 =
				(float32_t)(b + 1U) /
				(float32_t)FFT_BAND_COUNT;

		// ========================================================
		// custom spacing curve 적용
		//
		// pow(t, 0.78):
		// → 고역 방향 band 밀도 증가
		//
		// 결과:
		// 기존보다 고역이 더 세밀하게 분할됨
		// ========================================================
		t0 = powf(t0, spacingCurve);
		t1 = powf(t1, spacingCurve);

		// ========================================================
		// logarithmic interpolation
		//
		// 정규화된 t값(0~1)을 실제 Hz 주파수로 변환
		//
		// 결과:
		// 각 band의 시작/끝 주파수 생성
		// ========================================================
		float32_t startHz =
				minHz *
				powf(maxHz / minHz, t0);

		float32_t endHz =
				minHz *
				powf(maxHz / minHz, t1);

		// 계산된 주파수 범위 저장
		s_bandStartHz[b] = startHz;
		s_bandEndHz[b]   = endHz;

		// ========================================================
		// Hz → FFT bin 번호 변환
		//
		// FFT 결과는 bin index 기반이므로
		// 실제 FFT 배열 접근을 위해 변환 필요
		// ========================================================
		s_bandStartBin[b] = FFT_FreqToBin(startHz);
		s_bandEndBin[b]   = FFT_FreqToBin(endHz);

		// ========================================================
		// 최소 1개 이상의 FFT bin 보장
		//
		// 아주 좁은 대역에서는:
		// startBin == endBin
		// 또는
		// endBin < startBin
		//
		// 상황이 생길 수 있으므로 보정
		// ========================================================
		if (s_bandEndBin[b] < s_bandStartBin[b])
		{
			s_bandEndBin[b] = s_bandStartBin[b];
		}
	}
}

// ================================================================
// DC 제거 + 정규화 + Window
// ================================================================
static void FFT_ConvertFrameToFloat(void)
{
    float32_t mean = 0.0f;

    for (uint32_t i = 0; i < FFT_SIZE; i++)
    {
        mean += (float32_t)s_monoFrame[i];
    }

    mean /= (float32_t)FFT_SIZE;

    for (uint32_t i = 0; i < FFT_SIZE; i++)
    {
        float32_t x = ((float32_t)s_monoFrame[i] - mean) / 32768.0f;
        s_fftInput[i] = x * s_window[i];
    }
}

// ================================================================
// Magnitude 계산
// ================================================================
static void FFT_ComputeMagnitude(void)
{
    s_fftMagnitude[0] = 0.0f;

    for (uint32_t k = 1; k < FFT_SIZE / 2U; k++)
    {
        float32_t re = s_fftOutput[2U * k];
        float32_t im = s_fftOutput[2U * k + 1U];

        s_fftMagnitude[k] = sqrtf(re * re + im * im);
    }
}

// ================================================================
// Band 계산
// ================================================================
static void FFT_ComputeBands(void)
{
    for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
    {
        uint32_t start = s_bandStartBin[b];
        uint32_t end   = s_bandEndBin[b];

        float32_t sum = 0.0f;
        float32_t peak = 0.0f;
        uint32_t count = 0;

        for (uint32_t k = start; k <= end; k++)
        {
            float32_t v = s_fftMagnitude[k];

            sum += v * v;

            if (v > peak)
            {
                peak = v;
            }

            count++;
        }

        if (count == 0)
        {
            s_bandLevel[b] = 0.0f;
            continue;
        }

        float32_t rms = sqrtf(sum / (float32_t)count);
        float32_t center = (s_bandStartHz[b] + s_bandEndHz[b]) * 0.5f;

        float32_t rmsW = 0.6f;
        float32_t peakW = 0.4f;

        if (center < 200.0f)
        {
            peakW += 0.2f;
            rmsW  -= 0.2f;
        }
        else if (center > 5000.0f)
        {
            peakW -= 0.2f;
            rmsW  += 0.2f;
        }

        {
            float32_t sumW = rmsW + peakW;
            rmsW  /= sumW;
            peakW /= sumW;
        }

        s_bandLevel[b] = rmsW * rms + peakW * peak;
    }
}

// ================================================================
// Smoothing
// ================================================================
static void FFT_ApplySmoothing(void)
{
    const float32_t attack  = 0.7f;
    const float32_t release = 0.3f;

    for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
    {
        if (s_bandLevel[b] > s_bandSmooth[b])
        {
            s_bandSmooth[b] += attack * (s_bandLevel[b] - s_bandSmooth[b]);
        }
        else
        {
            s_bandSmooth[b] += release * (s_bandLevel[b] - s_bandSmooth[b]);
        }
    }
}

// ================================================================
// AGC + LED 레벨
// ================================================================
static void FFT_UpdateLedLevels(void)
{
    const float32_t dbMin = DB_MIN;
    const float32_t dbMax = DB_MAX;

    float32_t peak = 0.0f;
    float32_t peakAvg = 0.0f;
    float32_t threshold = 0.0f;

    if (audioSource == AUDIO_SRC_USB)
    {
        threshold = USB_THRESHOLD;
    }
    else
    {
        threshold = ADC_THRESHOLD;
    }

    for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
    {
        if (s_bandLevel[b] > peak)
        {
            peak = s_bandLevel[b];
        }
    }

    debugPeak = peak;

    if (peak > threshold)
    {
        s_peakBufSum -= s_peakBuffer[s_peakBufIndex];
        s_peakBuffer[s_peakBufIndex] = peak;
        s_peakBufSum += s_peakBuffer[s_peakBufIndex];

        s_peakBufIndex++;
        if (s_peakBufIndex >= PEAK_BUFFER_SIZE)
        {
            s_peakBufIndex = 0;
            s_peakBufReady = 1U;
        }

        if (s_peakBufReady)
        {
            peakAvg = s_peakBufSum / (float32_t)PEAK_BUFFER_SIZE;

            if (peakAvg > 0.0f)
            {
                float32_t targetGain = DB_TO_LINEAR(TARGET_PEAK_DB) / peakAvg;
                float32_t delta = targetGain - s_gain;
                s_gain += 0.1f * delta;
            }
        }
    }

    for (uint32_t b = 0; b < FFT_BAND_COUNT; b++)
    {
        float32_t v = s_bandLevel[b] * s_gain;
        float32_t db;
        float32_t norm;

        if (v < 1e-12f)
        {
            v = 1e-12f;
        }

        db = 20.0f * log10f(v);

        if (db < dbMin)
        {
            db = dbMin;
        }

        if (db > dbMax)
        {
            db = dbMax;
        }

        norm = (db - dbMin) / (dbMax - dbMin);
        s_ledLevel[b] = (uint8_t)(norm * 16.0f);
    }
}

// ================================================================
// Hop 처리
// ================================================================
static void FFT_ShiftFrame(void)
{
    memmove(&s_monoFrame[0],
            &s_monoFrame[FFT_HOP_SIZE],
            (FFT_SIZE - FFT_HOP_SIZE) * sizeof(int16_t));

    //s_monoCount -= FFT_HOP_SIZE;
}
