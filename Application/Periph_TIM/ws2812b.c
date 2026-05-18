/*
 * ws2812b.c
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */
#include "main.h"
#include "ws2812b.h"
#include <string.h>

// ================================================================
// STATE
// ================================================================
extern TIM_HandleTypeDef htim1;

static uint8_t s_ledData[LED_COUNT][3];
static uint16_t s_pwmBuf[PWM_BUF_SIZE];

static volatile uint8_t s_dmaRunning;

static uint16_t XYToIndex(uint8_t x, uint8_t y)
{
	if (y & 1U)
		return (y * LED_WIDTH) + (LED_WIDTH - 1U - x);
    else
    	return (y * LED_WIDTH) + x;
}

static void WriteByte(uint16_t *buf, uint8_t byte)
{
    for (int i = 0; i < 8; i++)
    {
        buf[i] = (byte & (1U << (7 - i)))
               ? WS2812_T1H
               : WS2812_T0H;
    }
}

static void WS2812B_RenderFrame(const uint8_t *frame)
{
	for (uint32_t y = 0; y < LED_HEIGHT; y++)
	{
		for (uint32_t x = 0; x < LED_WIDTH; x++)
		{
			uint32_t base = (y * LED_WIDTH + x) * 3U;

			uint16_t idx = XYToIndex(x, y);

			s_ledData[idx][0] = frame[base + 1]; // G
			s_ledData[idx][1] = frame[base + 0]; // R
			s_ledData[idx][2] = frame[base + 2]; // B
		}
	}
}

void WS2812B_Show(const uint8_t *frame)
{
	if (s_dmaRunning)
		return;

	WS2812B_RenderFrame(frame);

	uint32_t idx = 0;

	for (uint32_t led = 0; led < LED_COUNT; led++)
	{
		// GRB order
		WriteByte(&s_pwmBuf[idx + 0],  s_ledData[led][0]);
		WriteByte(&s_pwmBuf[idx + 8],  s_ledData[led][1]);
		WriteByte(&s_pwmBuf[idx + 16], s_ledData[led][2]);

		idx += 24;
	}

	while (idx < PWM_BUF_SIZE)
	{
		s_pwmBuf[idx++] = 0;
	}

	s_dmaRunning = 1;

    HAL_TIM_PWM_Start_DMA(&htim1, TIM_CHANNEL_1, (uint32_t *)s_pwmBuf, PWM_BUF_SIZE);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM1)
	{
		HAL_TIM_PWM_Stop_DMA(&htim1, TIM_CHANNEL_1);
		s_dmaRunning = 0;
	}
}
