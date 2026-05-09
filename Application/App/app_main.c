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

void App_Main(void)
{
	LCD_PlotUi();
	AudioPipeline_Init();
	EQ_Init();

	DAC_OutputStart();

	while (1)
	{
		AudioPipeline_Process();
		AudioPipeline_Loger();
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
