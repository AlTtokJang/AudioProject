/*
 * audio_ring.c
 *
 *  Created on: 2026. 5. 8.
 *      Author: ADJ
 */

#include "audio_ring.h"
#include "main.h"

void AudioRing_Init(AudioRing_t *ring, int16_t *memory, uint16_t size)
{
	if (ring == 0 || memory == 0 || size < 2)
	{
		return;
	}

	ring->buffer = memory;
	ring->size = size;

	ring->writeIndex = 0;
	ring->readIndex = 0;

	ring->overflow = 0;
	ring->underflow = 0;
}

void AudioRing_Clear(AudioRing_t *ring)
{
	if (ring == 0)
	{
		return;
	}

	ring->writeIndex = 0;
	ring->readIndex = 0;

	ring->overflow = 0;
	ring->underflow = 0;
}

uint16_t AudioRing_Available(const AudioRing_t *ring)
{
	if (ring == 0 || ring->buffer == 0 || ring->size == 0)
	{
		return 0;
	}

	if (ring->writeIndex >= ring->readIndex)
	{
		return ring->writeIndex - ring->readIndex;
	}

	return ring->size - ring->readIndex + ring->writeIndex;
}

uint16_t AudioRing_Free(const AudioRing_t *ring)
{
	if (ring == 0 || ring->buffer == 0 || ring->size < 2)
	{
		return 0;
	}

	return (ring->size - 1U) - AudioRing_Available(ring);
}

uint16_t AudioRing_Push(AudioRing_t *ring, const int16_t *src, uint16_t count)
{
	uint16_t pushed = 0;

	if (ring == 0 || src == 0 || ring->buffer == 0 || ring->size < 2)
	{
		return 0;
	}

	while (pushed < count)
	{
		uint16_t nextWrite = ring->writeIndex + 1U;

		if (nextWrite >= ring->size)
		{
			nextWrite = 0;
		}

		if (nextWrite == ring->readIndex)
		{
			ring->overflow = 1;

			ring->readIndex++;
			if (ring->readIndex >= ring->size)
			{
				ring->readIndex = 0;
			}
		}

		ring->buffer[ring->writeIndex] = src[pushed];
		ring->writeIndex = nextWrite;

		pushed++;
	}

	return pushed;
}

uint16_t AudioRing_Pop(AudioRing_t *ring, int16_t *dst, uint16_t count)
{
	uint16_t popped = 0;

	if (ring == 0 || dst == 0 || ring->buffer == 0 || ring->size < 2)
	{
		return 0;
	}

	while (popped < count)
	{
		if (ring->readIndex == ring->writeIndex)
		{
			ring->underflow = 1;
			break;
		}

		dst[popped] = ring->buffer[ring->readIndex];

		ring->readIndex++;
		if (ring->readIndex >= ring->size)
		{
			ring->readIndex = 0;
		}

		popped++;
	}

	return popped;
}

uint8_t AudioRing_HasOverflow(const AudioRing_t *ring)
{
	if (ring == 0)
	{
		return 0;
	}

	return ring->overflow;
}

uint8_t AudioRing_HasUnderflow(const AudioRing_t *ring)
{
	if (ring == 0)
	{
		return 0;
	}

	return ring->underflow;
}

void AudioRing_ClearFlags(AudioRing_t *ring)
{
	if (ring == 0)
	{
		return;
	}

	ring->overflow = 0;
	ring->underflow = 0;
}

















