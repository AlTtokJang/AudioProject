/*
 * app_main.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "app_main.h"
#include "main.h"

#include "global_define.h"
#include "audio_pipeline.h"
#include "eq.h"
#include "lcd.h"
#include "adc.h"
#include "dac.h"
#include "fft.h"
#include "visual_layer.h"
#include "visual_renderer.h"
#include "ws2812b.h"

uint16_t pwmBuffer[PWM_BUF_SIZE];
extern TIM_HandleTypeDef htim1;
static volatile uint8_t ws2812bBusy;

void App_Main(void)
{
	LCD_AppModeInit();

	AudioPipeline_Init();
	EQ_Init();
	FFT_Init();
	VisualRenderer_Init();

	ADC_Start_VReg();
	DAC_OutputStart();
	HAL_TIM_Base_Start(&htim1);


	#ifdef ADC_DEBUG
	ADC_Start_Aux();
	ADC_DebugStartAuxCapture();

	while (!ADC_DebugIsAuxCaptureDone())
	{
	}

	ADC_Stop_Aux();
	ADC_DebugDumpAuxCaptureCsv();

	while (1)
	{
	}
	#endif

	while (1)
	{
		LCD_DrawMainScreen();
		AudioPipeline_Process();
		AudioPipeline_Loger();

		if (FFT_Run())
		{
			const float *fft = FFT_GetBandRaw();
			Visual_Process(fft);
			WS2812B_Show(VisualRenderer_GetFrame());
		}
	}
}

/*
 * 디버거 -------------------------------------------------------------------------------------
 */
static volatile uint8_t errorFlags[AUDIO_ERR_COUNT];
static volatile AudioErrorCode_t lastErrorCode;

void Error_Loger(AudioErrorCode_t code)
{
	if (code >= AUDIO_ERR_COUNT)
	{
		return;
	}

	errorFlags[code] = 1;
	lastErrorCode = code;
}
