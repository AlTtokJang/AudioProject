#include "eq.h"
#include "adc.h"
#include <math.h>


/*
===============================================================================================================
 * BiQuad 필터 구조체 (IIR 2차 필터)
*/
typedef struct
{
	float b0, b1, b2; // 입력 계수
	float a1, a2; // 출력 계수
	float z1, z2; // 상태 변수 (delay)
} Biquad;


/*
===============================================================================================================
 * 좌/우 채널 필터
*/
static Biquad lowL, lowR;
static Biquad midL, midR;
static Biquad highL, highR;


/*
===============================================================================================================
 * 샘플링 주파수
*/
#define FS 48000.0f


/*
===============================================================================================================
 * BiQuad 실행 함수
 * Direct Form II 구조
*/
static float Biquad_Process(Biquad *f, float x)
{
	float y = f->b0 * x + f->z1;
	f->z1 = f->b1 * x - f->a1 * y + f->z2;
	f->z2 = f->b2 * x - f->a2 * y;
	return y;
}


/*
===============================================================================================================
 * Low-pass (저음 필터)
 * ~250Hz 이하 통과
*/
static void Biquad_SetLowpass(Biquad *f, float fc)
{
	float w0 = 2.0f * 3.1415926f * fc / FS;
	float cosw = cosf(w0);
	float sinw = sinf(w0);
	float Q = 0.707f;
	float alpha = sinw / (2.0f * Q);
	float b0 = (1 - cosw) / 2;
	float b1 = (1 - cosw);
	float b2 = (1 - cosw) / 2;
	float a0 = 1 + alpha;
	float a1 = -2 * cosw;
	float a2 = 1 - alpha;
	f->b0 = b0 / a0;
	f->b1 = b1 / a0;
	f->b2 = b2 / a0;
	f->a1 = a1 / a0;
	f->a2 = a2 / a0;
}
/*
===============================================================================================================
 * Band-pass (중음 필터)
 * Q=0.7 → 넓은 중음 대역 (보컬 자연스러움)
*/
static void Biquad_SetBandpass(Biquad *f, float fc)
{
	float w0 = 2.0f * 3.1415926f * fc / FS;
	float cosw = cosf(w0);
	float sinw = sinf(w0);
	float Q = 0.7f; // 넓은 대역
	float alpha = sinw / (2.0f * Q);
	float b0 = alpha;
	float b1 = 0;
	float b2 = -alpha;
	float a0 = 1 + alpha;
	float a1 = -2 * cosw;
	float a2 = 1 - alpha;
	f->b0 = b0 / a0;
	f->b1 = b1 / a0;
	f->b2 = b2 / a0;
	f->a1 = a1 / a0;
	f->a2 = a2 / a0;
}
/*
===============================================================================================================
 * High-pass (고음 필터)
 * ~4kHz 이상 통과
*/
static void Biquad_SetHighpass(Biquad *f, float fc)
{
	float w0 = 2.0f * 3.1415926f * fc / FS;
	float cosw = cosf(w0);
	float sinw = sinf(w0);
	float Q = 0.707f;
	float alpha = sinw / (2.0f * Q);
	float b0 = (1 + cosw) / 2;
	float b1 = -(1 + cosw);
	float b2 = (1 + cosw) / 2;
	float a0 = 1 + alpha;
	float a1 = -2 * cosw;
	float a2 = 1 - alpha;
	f->b0 = b0 / a0;
	f->b1 = b1 / a0;
	f->b2 = b2 / a0;
	f->a1 = a1 / a0;
	f->a2 = a2 / a0;
}
/*
===============================================================================================================
* EQ 초기화
*/
void EQ_Init(void)
{
	Biquad_SetLowpass(&lowL, 250.0f);
	Biquad_SetLowpass(&lowR, 250.0f);
	Biquad_SetBandpass(&midL, 1000.0f);
	Biquad_SetBandpass(&midR, 1000.0f);
	Biquad_SetHighpass(&highL, 4000.0f);
	Biquad_SetHighpass(&highR, 4000.0f);
}
/*
===============================================================================================================
 * EQ 처리
 *
 * [핵심 흐름]
 * 노브 → (로그 커브) → dB → gain → 필터 → 합성 → 출력
*/
void EQ_ProcessStereo(const int16_t *in, int16_t *out, uint32_t frameCount)
{
	/* --------------------------------------------------------------------------------
	 * 1. 노브 입력 → 정규화 (-1 ~ +1)
	* -------------------------------------------------------------------------------- */
	uint8_t vreg[3];
	ADC_GetValue_EQ(vreg);

	float normLow = ((float)vreg[0] - 50.0f) / 50.0f;
	float normMid = ((float)vreg[1] - 50.0f) / 50.0f;
	float normHigh = ((float)vreg[2] - 50.0f) / 50.0f;
	/* --------------------------------------------------------------------------------
	 * 2. 로그 커브 적용 (cubic)
	 * → 작은 조작은 부드럽게, 큰 조작은 강하게
	* -------------------------------------------------------------------------------- */
	float curveLow = normLow * normLow * normLow;
	float curveMid = normMid * normMid * normMid;
	float curveHigh = normHigh * normHigh * normHigh;
	/* --------------------------------------------------------------------------------
	 * 3. dB 변환
	* -------------------------------------------------------------------------------- */
	float dBLow = curveLow * 18.0f; // 저음 강조
	float dBMid = curveMid * 12.0f;
	float dBHigh = curveHigh * 12.0f;
	/* --------------------------------------------------------------------------------
	 * 4. dB → 선형 gain 변환
	* -------------------------------------------------------------------------------- */
	float gLow = powf(10.0f, dBLow / 20.0f);
	float gMid = powf(10.0f, dBMid / 20.0f);
	float gHigh = powf(10.0f, dBHigh / 20.0f);
	/* --------------------------------------------------------------------------------
	 * 5. 샘플 처리
	* -------------------------------------------------------------------------------- */
	for (uint32_t i = 0; i < frameCount; i++)
	{
		float inL = (float)in[2*i];
		float inR = (float)in[2*i + 1];
		/* -------- 대역 분리 -------- */
		float low_l = Biquad_Process(&lowL, inL);
		float mid_l = Biquad_Process(&midL, inL);
		float high_l = Biquad_Process(&highL, inL);
		float low_r = Biquad_Process(&lowR, inR);
		float mid_r = Biquad_Process(&midR, inR);
		float high_r = Biquad_Process(&highR, inR);
		/* -------- Gain 적용 + 합성 -------- */
		float outL = low_l * gLow * 1.5f // 베이스 부스트
		+ mid_l * gMid
		+ high_l * gHigh;
		float outR = low_r * gLow * 1.5f
		+ mid_r * gMid
		+ high_r * gHigh;
		/* -------- 클리핑 방지 -------- */
		if (outL > 32767.0f) outL = 32767.0f;
		if (outL < -32768.0f) outL = -32768.0f;
		if (outR > 32767.0f) outR = 32767.0f;
		if (outR < -32768.0f) outR = -32768.0f;
		out[2*i] = (int16_t)outL;
		out[2*i + 1] = (int16_t)outR;
	}
}
