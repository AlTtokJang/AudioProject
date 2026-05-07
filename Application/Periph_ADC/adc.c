/*
 * adc.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "adc.h"
#include "main.h"

#include "global_define.h"
#include "audio_pipeline.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc3;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc3;

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim6;

static volatile uint16_t auxBuffer[MASTER_BLOCK_SIZE * 2];
static int16_t auxSample[MASTER_BLOCK_SIZE];

static volatile uint16_t vregBuffer[VREG_BUFFER_SIZE];
static uint8_t  vregValue[VREG_BUFFER_SIZE];
static uint16_t vregLpf[VREG_BUFFER_SIZE];

static void DigitalFilter_Aux(uint8_t target);
static void DigitalFilter_VReg(void);

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		DigitalFilter_Aux(0);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		DigitalFilter_Aux(1);
	}
	else if (hadc->Instance == ADC3)
	{
		DigitalFilter_VReg();
	}
}

void ADC_Start_Aux(void)
{
	HAL_TIM_Base_Stop(&htim2);

	HAL_ADC_Stop_DMA(&hadc1);
	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

	for (int i = 0; i < MASTER_BLOCK_SIZE * 2; i++)
	{
		auxBuffer[i] = 0;
	}

	for (int i = 0; i < MASTER_BLOCK_SIZE; i++)
	{
		auxSample[i] = 0;
	}

	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)auxBuffer, MASTER_BLOCK_SIZE * 2);

	HAL_TIM_Base_Start(&htim2);
}

void ADC_Stop_Aux(void)
{
	HAL_TIM_Base_Stop(&htim2);

	HAL_ADC_Stop_DMA(&hadc1);
	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
}

void ADC_Start_VReg(void)
{
	HAL_TIM_Base_Stop(&htim6);

	HAL_ADC_Stop_DMA(&hadc3);
	HAL_DMA_Abort(&hdma_adc3);

	__HAL_TIM_SET_COUNTER(&htim6, 0);
	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

	for (int i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		vregBuffer[i] = 0;
		vregValue[i] = 0;
		vregLpf[i] = 0;
	}

	HAL_ADC_Start_DMA(&hadc3, (uint32_t *)vregBuffer, VREG_BUFFER_SIZE);

	HAL_TIM_Base_Start(&htim6);
}

void ADC_Stop_VReg(void)
{
	HAL_TIM_Base_Stop(&htim6);

	HAL_ADC_Stop_DMA(&hadc3);
	HAL_DMA_Abort(&hdma_adc3);

	__HAL_TIM_SET_COUNTER(&htim6, 0);
	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
}

void ADC_GetValue_EQ(uint8_t *out)
{
	if (out == NULL)
	{
		return;
	}

	for (uint8_t i = 0; i < 3; i++)
	{
		out[i] = vregValue[i + 1];
	}
}

void ADC_GetValue_VOL(uint8_t *out)
{
	if (out == NULL)
	{
		return;
	}

	*out = vregValue[0];
}

static void DigitalFilter_Aux(uint8_t target)
{
	// 향후 필터 개선 필요

	uint16_t offset;

	if (target == 0)
		offset = 0;
	else
		offset = MASTER_BLOCK_SIZE;

	for (uint16_t i = 0; i < MASTER_BLOCK_SIZE; i++)
	{
		auxSample[i] = (int16_t)auxBuffer[offset + i] - 2048;
	}

	AudioPipeline_Push(auxSample, MASTER_BLOCK_SIZE);
}

static void DigitalFilter_VReg(void)
{
	for (uint8_t i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		uint16_t raw = vregBuffer[i];
		uint16_t target;

		if (raw <= 50)
		{
			target = (raw * 67) / 50;				// 0~50 -> 0~67
		}
		else
		{
			target = 67 + ((raw - 50) * 33) / 205;	// 50~255 -> 67~100
		}

		uint16_t targetLpf = target * 100;			// 0~10000

		if (targetLpf > vregLpf[i])
			vregLpf[i] += (targetLpf - vregLpf[i]) >> 2;
		else
			vregLpf[i] -= (vregLpf[i] - targetLpf) >> 2;

		vregValue[i] = vregLpf[i] / 100;

		if (raw == 255 && vregValue[i] == 99)
			vregValue[i] = 100;
	}
}
