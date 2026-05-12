/*
 * dac.c
 *
 *  Created on: 2026. 5. 9.
 *      Author: ADJ
 */

#include "dac.h"
#include "main.h"
#include "global_define.h"
#include "audio_pipeline.h"

#define I2S_TX_BUFFER_SIZE	(MASTER_BLOCK_SIZE * 2)

extern I2S_HandleTypeDef hi2s2;
static int16_t i2sTxBuffer[I2S_TX_BUFFER_SIZE];
static volatile uint8_t i2sOutputRunning = 0;

static int16_t lastOutL = 0;
static int16_t lastOutR = 0;

static void DAC_ClearTxBuffer(void);
static void DAC_FillUnderflowFade(int16_t *dst, uint16_t start, uint16_t count);
static void DAC_UpdateLastSample(const int16_t *buf, uint16_t count);

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2)
	{
		if (i2sOutputRunning == 0)
		{
			return;
		}

		uint16_t got = AudioPipeline_PopOutput(&i2sTxBuffer[0], MASTER_BLOCK_SIZE);

		if (got < MASTER_BLOCK_SIZE)
		{
			DAC_FillUnderflowFade(&i2sTxBuffer[0], got, MASTER_BLOCK_SIZE);
		}

		DAC_UpdateLastSample(&i2sTxBuffer[0], MASTER_BLOCK_SIZE);
	}
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2)
	{
		if (i2sOutputRunning == 0)
		{
			return;
		}

		uint16_t got = AudioPipeline_PopOutput(&i2sTxBuffer[MASTER_BLOCK_SIZE], MASTER_BLOCK_SIZE);

		if (got < MASTER_BLOCK_SIZE)
		{
			DAC_FillUnderflowFade(&i2sTxBuffer[MASTER_BLOCK_SIZE], got, MASTER_BLOCK_SIZE);
		}

		DAC_UpdateLastSample(&i2sTxBuffer[MASTER_BLOCK_SIZE], MASTER_BLOCK_SIZE);
	}
}

void DAC_OutputStart(void)
{
	i2sOutputRunning = 0;

	HAL_I2S_DMAStop(&hi2s2);

	if (hi2s2.hdmatx != NULL)
	{
		HAL_DMA_Abort(hi2s2.hdmatx);
	}

	DAC_ClearTxBuffer();

	if (HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t *)i2sTxBuffer, I2S_TX_BUFFER_SIZE) == HAL_OK)
	{
		i2sOutputRunning = 1;
	}
	else
	{
		Error_Loger(AUDIO_ERR_I2S_TX_START_FAIL);
		i2sOutputRunning = 0;
	}
}

void DAC_OutputStop(void)
{
	i2sOutputRunning = 0;

	if (HAL_I2S_DMAStop(&hi2s2) != HAL_OK)
	{
		Error_Loger(AUDIO_ERR_I2S_TX_STOP_FAIL);
	}

	if (hi2s2.hdmatx != NULL)
	{
		HAL_DMA_Abort(hi2s2.hdmatx);
	}

	DAC_ClearTxBuffer();
}

static void DAC_ClearTxBuffer(void)
{
	for (uint16_t i = 0; i < I2S_TX_BUFFER_SIZE; i++)
	{
		i2sTxBuffer[i] = 0;
	}

	lastOutL = 0;
	lastOutR = 0;
}

static void DAC_UpdateLastSample(const int16_t *buf, uint16_t count)
{
	if (buf == NULL || count < 2)
	{
		return;
	}

	lastOutL = buf[count - 2];
	lastOutR = buf[count - 1];
}

static void DAC_FillUnderflowFade(int16_t *dst, uint16_t start, uint16_t count)
{
	if (dst == NULL || start >= count)
	{
		return;
	}

	uint16_t remain = count - start;
	uint16_t frameCount = remain / 2U;

	if (frameCount == 0)
	{
		for (uint16_t i = start; i < count; i++)
		{
			dst[i] = 0;
		}
		return;
	}

	for (uint16_t frame = 0; frame < frameCount; frame++)
	{
		int32_t scale = (int32_t)(frameCount - frame);

		int16_t fadeL = (int16_t)(((int32_t)lastOutL * scale) / (int32_t)frameCount);
		int16_t fadeR = (int16_t)(((int32_t)lastOutR * scale) / (int32_t)frameCount);

		uint16_t index = start + (frame * 2U);

		dst[index] = fadeL;
		dst[index + 1U] = fadeR;
	}

	if ((remain % 2U) != 0U)
	{
		dst[count - 1U] = 0;
	}

	lastOutL = 0;
	lastOutR = 0;
}
