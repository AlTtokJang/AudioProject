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
#include "lcd.h"
#include "adc.h"

void App_Main(void)
{
	LCD_PlotUi();
	AudioPipeline_Init();

	ADC_Start_VReg();
	ADC_Start_Aux();
}

void Error_Indicator(uint8_t code)
{
	switch (code)
	{
		case AUDIO_ERR_RING_OVERFLOW:
			HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
			break;
		case AUDIO_ERR_RING_UNDERFLOW:
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
			break;
	}
}
