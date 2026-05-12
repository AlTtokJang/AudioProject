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

#define ADC_SAMPLE_SIZE MASTER_BLOCK_SIZE
#define ADC_BUFFER_SIZE ADC_SAMPLE_SIZE * 2

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc3;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc3;

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim6;

static volatile uint16_t auxBuffer[ADC_BUFFER_SIZE];
static int16_t auxSample[ADC_SAMPLE_SIZE];

static volatile uint16_t vregBuffer[VREG_BUFFER_SIZE];
static uint8_t  vregValue[VREG_BUFFER_SIZE];
static uint16_t vregLpf[VREG_BUFFER_SIZE];

static void DigitalFilter_Aux(uint8_t target);
static void DigitalFilter_VReg(void);

static volatile uint8_t auxIsRunning;
static volatile uint8_t vregIsRunning;

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		if (auxIsRunning)
		{
			DigitalFilter_Aux(0);
		}
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		if (auxIsRunning)
		{
			DigitalFilter_Aux(1);
		}
	}
	else if (hadc->Instance == ADC3)
	{
		if (vregIsRunning)
		{
			DigitalFilter_VReg();
		}
	}
}

void ADC_Start_Aux(void)
{
	auxIsRunning = 0;

	HAL_TIM_Base_Stop(&htim2);
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

	for (uint16_t i = 0; i < ADC_BUFFER_SIZE; i++)
	{
		auxBuffer[i] = 0;
	}

	for (uint16_t i = 0; i < ADC_SAMPLE_SIZE; i++)
	{
		auxSample[i] = 0;
	}

	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)auxBuffer, ADC_BUFFER_SIZE) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_START_FAIL);
		auxIsRunning = 0;
		return;
	}

	if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_TIM_START_FAIL);

		HAL_ADC_Stop_DMA(&hadc1);
		HAL_DMA_Abort(&hdma_adc1);

		auxIsRunning = 0;
		return;
	}

	auxIsRunning = 1;
}

void ADC_Stop_Aux(void)
{
	auxIsRunning = 0;

	if (HAL_TIM_Base_Stop(&htim2) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_TIM_STOP_FAIL);
	}

	if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_AUX_STOP_FAIL);
	}

	HAL_DMA_Abort(&hdma_adc1);

	__HAL_TIM_SET_COUNTER(&htim2, 0);
	__HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
}

void ADC_Start_VReg(void)
{
	vregIsRunning = 0;

	HAL_TIM_Base_Stop(&htim6);
	HAL_ADC_Stop_DMA(&hadc3);
	HAL_DMA_Abort(&hdma_adc3);

	__HAL_TIM_SET_COUNTER(&htim6, 0);
	__HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

	for (uint8_t i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		vregBuffer[i] = 0;
		vregValue[i] = 0;
		vregLpf[i] = 0;
	}

	if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)vregBuffer, VREG_BUFFER_SIZE) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_START_FAIL);
		vregIsRunning = 0;
		return;
	}

	if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_TIM_START_FAIL);

		HAL_ADC_Stop_DMA(&hadc3);
		HAL_DMA_Abort(&hdma_adc3);

		vregIsRunning = 0;
		return;
	}

	vregIsRunning = 1;
}

void ADC_Stop_VReg(void)
{
	vregIsRunning = 0;

	if (HAL_TIM_Base_Stop(&htim6) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_TIM_STOP_FAIL);
	}

	if (HAL_ADC_Stop_DMA(&hadc3) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_ADC_VREG_STOP_FAIL);
	}

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
		offset = ADC_SAMPLE_SIZE;

	for (uint16_t i = 0; i < ADC_SAMPLE_SIZE; i++)
	{
		auxSample[i] = (int16_t)auxBuffer[offset + i] - 2048;
	}

	AudioPipeline_Push(auxSample, ADC_SAMPLE_SIZE);
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
