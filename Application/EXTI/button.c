/*
 * button.c
 *
 *  Created on: 2026. 5. 9.
 *      Author: ADJ
 */

#include "main.h"
#include "global_define.h"

#include "audio_pipeline.h"
#include "adc.h"
#include "bt.h"

volatile AudioSource_t audioSource = AUDIO_SRC_USB;
volatile uint8_t fftUseEq = 0;
volatile uint8_t i2sUseEq = 0;

extern TIM_HandleTypeDef htim7;

static uint16_t buttonDebounceTarget;
static uint8_t buttonDebounceCount;

static void ButtonDebounce_TimerStart(void);
static void ButtonDebounce_TimerStop(void);
static void Enable_EXTI(void);
static void Disable_EXTI(void);
static void Confirm_EXTI(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
		case BTN1_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN1_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN2_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN2_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN3_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN3_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		case BTN4_Pin:
			Disable_EXTI();
			buttonDebounceTarget = BTN4_Pin;
			buttonDebounceCount = 0;
			ButtonDebounce_TimerStart();
			break;
		default:
			ButtonDebounce_TimerStop();
			break;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM7)
	{
		GPIO_PinState raw;

		switch (buttonDebounceTarget)
		{
			case BTN1_Pin:
				raw = HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin);
				break;
			case BTN2_Pin:
				raw = HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin);
				break;
			case BTN3_Pin:
				raw = HAL_GPIO_ReadPin(BTN3_GPIO_Port, BTN3_Pin);
				break;
			case BTN4_Pin:
				raw = HAL_GPIO_ReadPin(BTN4_GPIO_Port, BTN4_Pin);
				break;
			default:
				ButtonDebounce_TimerStop();
				buttonDebounceTarget = 0;
				buttonDebounceCount = 0;
				Enable_EXTI();
				return;
		}

		if (raw == GPIO_PIN_SET)
		{
			buttonDebounceCount++;

			if (buttonDebounceCount >= 50)
			{
				ButtonDebounce_TimerStop();
				Confirm_EXTI();
				buttonDebounceTarget = 0;
				buttonDebounceCount = 0;
				Enable_EXTI();
			}
		}
		else
		{
			ButtonDebounce_TimerStop();
			buttonDebounceTarget = 0;
			buttonDebounceCount = 0;
			Enable_EXTI();
		}
	}
}

static void ButtonDebounce_TimerStart(void)
{
	HAL_TIM_Base_Stop_IT(&htim7);

	__HAL_TIM_SET_COUNTER(&htim7, 0);
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);

	HAL_TIM_Base_Start_IT(&htim7);
}

static void ButtonDebounce_TimerStop(void)
{
	HAL_TIM_Base_Stop_IT(&htim7);

	__HAL_TIM_SET_COUNTER(&htim7, 0);
	__HAL_TIM_CLEAR_FLAG(&htim7, TIM_FLAG_UPDATE);
}

static void Enable_EXTI(void)
{
	__HAL_GPIO_EXTI_CLEAR_IT(BTN1_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN2_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN3_Pin);
	__HAL_GPIO_EXTI_CLEAR_IT(BTN4_Pin);

	HAL_NVIC_EnableIRQ(EXTI2_IRQn);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void Disable_EXTI(void)
{
	HAL_NVIC_DisableIRQ(EXTI2_IRQn);
	HAL_NVIC_DisableIRQ(EXTI4_IRQn);
	HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
	HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
}

static void Confirm_EXTI(void)
{
	switch (buttonDebounceTarget)
	{
		case BTN1_Pin:
			switch (audioSource)
			{
				case AUDIO_SRC_USB:
					audioSource = AUDIO_SRC_AUX;
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					ADC_Start_Aux();
					break;
				case AUDIO_SRC_AUX:
					audioSource = AUDIO_SRC_BT;
					ADC_Stop_Aux();
					AudioPipeline_RingClear();
					I2S_Start_BT();
					break;
				case AUDIO_SRC_BT:
					ADC_Stop_Aux();
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					audioSource = AUDIO_SRC_USB;
					break;
				default:
					ADC_Stop_Aux();
					I2S_Stop_BT();
					AudioPipeline_RingClear();
					audioSource = AUDIO_SRC_USB;
					break;
			}
			break;
		case BTN2_Pin:
			break;
		case BTN3_Pin:
			break;
		case BTN4_Pin:
			break;
		default:
			break;
	}
}
