/*
 * EQ 처리 모듈입니다
 * 입력 데이터 형태는 PCM(L, R, L, R, ...) 형태를 따릅니다.
 *
 */

#ifndef EQ_H
#define EQ_H

#include <stdint.h>

// EQ 초기화
void EQ_Init(void);

// EQ 수식 알고리즘
void EQ_ProcessStereo(const int16_t *in, int16_t *out, uint32_t frameCount);

#endif
