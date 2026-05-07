/*
 * audio_ring.h
 *
 *  Created on: 2026. 5. 8.
 *      Author: ADJ
 *
 * 링 버퍼 기능
 * 	저장
 * 	꺼내기
 * 	available/free 계산
 * 	overflow/underflow 기록
 *
 * 링 버퍼 정책
 * 	push full	오래된 데이터 drop, 새 데이터 저장, overflow flag set
 * 	Pop 부족		가능한 만큼 pop, 실제 pop 수 반환, underflow flag set
 */

#ifndef AUDIO_FRAME_AUDIO_RING_H_
#define AUDIO_FRAME_AUDIO_RING_H_

#include <stdint.h>

typedef struct
{
	int16_t *buffer;
	uint16_t size;

	volatile uint16_t writeIndex;
	volatile uint16_t readIndex;

	volatile uint8_t overflow;
	volatile uint8_t underflow;
} AudioRing_t;

void AudioRing_Init(AudioRing_t *ring, int16_t *memory, uint16_t size);
void AudioRing_Clear(AudioRing_t *ring);

uint16_t AudioRing_Available(const AudioRing_t *ring);
uint16_t AudioRing_Free(const AudioRing_t *ring);

uint16_t AudioRing_Push(AudioRing_t *ring, const int16_t *src, uint16_t count);
uint16_t AudioRing_Pop(AudioRing_t *ring, int16_t *dst, uint16_t count);

uint8_t AudioRing_HasOverflow(const AudioRing_t *ring);
uint8_t AudioRing_HasUnderflow(const AudioRing_t *ring);
void AudioRing_ClearFlags(AudioRing_t *ring);

#endif /* AUDIO_FRAME_AUDIO_RING_H_ */
