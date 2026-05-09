/*
 * bt.c
 *
 *  Created on: 2026. 5. 10.
 *      Author: ADJ
 */

#include "bt.h"
#include "main.h"

#include "global_define.h"
#include "audio_pipeline.h"

#define I2S_RX_BUFFER_SIZE	(MASTER_BLOCK_SIZE * 2)
#define I2S_RX_DROP_BLOCKS	2U

extern I2S_HandleTypeDef hi2s1;

static int16_t i2sRxBuffer[I2S_RX_BUFFER_SIZE];
static uint8_t i2sDropCount;

static void BT_I2S_ClearRxBuffer(void);

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI1)
	{
		if (i2sDropCount > 0)
		{
			i2sDropCount--;
			return;
		}

		AudioPipeline_Push(&i2sRxBuffer[0], MASTER_BLOCK_SIZE);
	}
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI1)
	{
		if (i2sDropCount > 0)
		{
			i2sDropCount--;
			return;
		}

		AudioPipeline_Push(&i2sRxBuffer[MASTER_BLOCK_SIZE], MASTER_BLOCK_SIZE);
	}
}

void I2S_Start_BT(void)
{
	HAL_I2S_DMAStop(&hi2s1);

	if (hi2s1.hdmarx != NULL)
	{
		HAL_DMA_Abort(hi2s1.hdmarx);
	}

	BT_I2S_ClearRxBuffer();

	i2sDropCount = I2S_RX_DROP_BLOCKS;

	HAL_I2S_Receive_DMA(&hi2s1, (uint16_t*)i2sRxBuffer, I2S_RX_BUFFER_SIZE);
}

void I2S_Stop_BT(void)
{
	i2sDropCount = 0;

	HAL_I2S_DMAStop(&hi2s1);

	if (hi2s1.hdmarx != NULL)
	{
		HAL_DMA_Abort(hi2s1.hdmarx);
	}

	BT_I2S_ClearRxBuffer();
}

static void BT_I2S_ClearRxBuffer(void)
{
	for (uint16_t i = 0; i < I2S_RX_BUFFER_SIZE; i++)
	{
		i2sRxBuffer[i] = 0;
	}
}






