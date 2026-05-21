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

#include <stdio.h>

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

#ifdef ADC_DEBUG
static void ADC_DebugCaptureAuxBlock(uint8_t target);
#endif

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		if (auxIsRunning)
		{
			DigitalFilter_Aux(0);

			#ifdef ADC_DEBUG
			ADC_DebugCaptureAuxBlock(0);
			#endif
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

		#ifdef ADC_DEBUG
		ADC_DebugCaptureAuxBlock(1);
		#endif
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
		auxSample[i] = ((int16_t)auxBuffer[offset + i] - 2048) << 1;
	}

	AudioPipeline_Push(auxSample, ADC_SAMPLE_SIZE);
}

static void DigitalFilter_VReg(void)
{
	for (uint8_t i = 0; i < VREG_BUFFER_SIZE; i++)
	{
		uint16_t raw = vregBuffer[i];
		uint16_t targetLpf;

		if (raw <= 50)
		{
			// 0~50 -> 0~6700
			targetLpf = (raw * 6700U + 25U) / 50U;
		}
		else
		{
			// 50~255 -> 6700~10000
			targetLpf = 6700U + (((raw - 50U) * 3300U + 102U) / 205U);
		}

		if (targetLpf > vregLpf[i])
		{
			uint16_t diff = targetLpf - vregLpf[i];
			vregLpf[i] += (diff + 3U) >> 2;
		}
		else
		{
			uint16_t diff = vregLpf[i] - targetLpf;
			vregLpf[i] -= (diff + 3U) >> 2;
		}

		vregValue[i] = (vregLpf[i] + 50U) / 100U;

		if (vregValue[i] > 100U)
		{
			vregValue[i] = 100U;
		}
	}
}

/*
 * 디버거 -------------------------------------------------------------------------------------
 */

#ifdef ADC_DEBUG

#define AUX_CAPTURE_PAIRS		8192U
#define AUX_CAPTURE_VALUES		(AUX_CAPTURE_PAIRS * 2U)

static uint16_t auxCapture[AUX_CAPTURE_VALUES];
static volatile uint32_t auxCaptureIndex = 0;
static volatile uint8_t auxCaptureRunning = 0;
static volatile uint8_t auxCaptureDone = 0;

void ADC_DebugStartAuxCapture(void)
{
	auxCaptureIndex = 0;
	auxCaptureDone = 0;
	auxCaptureRunning = 1;
}

uint8_t ADC_DebugIsAuxCaptureDone(void)
{
	return auxCaptureDone;
}

static void ADC_DebugCaptureAuxBlock(uint8_t target)
{
	uint16_t offset;

	if (!auxCaptureRunning)
	{
		return;
	}

	if (auxCaptureDone)
	{
		return;
	}

	if (target == 0)
	{
		offset = 0;
	}
	else
	{
		offset = ADC_SAMPLE_SIZE;
	}

	for (uint16_t i = 0; i < ADC_SAMPLE_SIZE; i++)
	{
		if (auxCaptureIndex >= AUX_CAPTURE_VALUES)
		{
			auxCaptureRunning = 0;
			auxCaptureDone = 1;
			return;
		}

		auxCapture[auxCaptureIndex] = auxBuffer[offset + i];
		auxCaptureIndex++;
	}

	if (auxCaptureIndex >= AUX_CAPTURE_VALUES)
	{
		auxCaptureRunning = 0;
		auxCaptureDone = 1;
	}
}

void ADC_DebugDumpAuxCaptureCsv(void)
{
	char line[48];

	UART3_Print("n,left,right\r\n");

	for (uint32_t n = 0; n < AUX_CAPTURE_PAIRS; n++)
	{
		uint16_t left = auxCapture[n * 2U];
		uint16_t right = auxCapture[n * 2U + 1U];

		snprintf(line, sizeof(line), "%lu,%u,%u\r\n", n, left, right);
		UART3_Print(line);
	}
}

#endif
