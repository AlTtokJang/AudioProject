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
#include "ws2812b.h"

uint16_t pwmBuffer[PWM_BUF_SIZE];
extern TIM_HandleTypeDef htim1;
static volatile uint8_t ws2812bBusy;

void App_Main(void)
{
	LCD_PlotUi();
	AudioPipeline_Init();
	EQ_Init();
	FFT_Init();

	DAC_OutputStart();

	HAL_TIM_Base_Start(&htim1);

	while (1)
	{
		AudioPipeline_Process();
		AudioPipeline_Loger();

		if (FFT_Run())
		{
			const uint8_t *levels = FFT_GetLedLevels();

			if (!ws2812bBusy)
			{
				Plot_WS2812B(levels);

				ws2812bBusy = 1;
				HAL_TIM_PWM_Start_DMA(
						&htim1, TIM_CHANNEL_1,
						(uint32_t *)pwmBuffer,
						PWM_BUF_SIZE);
			}
		}
	}
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM1)
	{
		HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
		ws2812bBusy = 0;
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
