/*
 * visual_layer.h
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#ifndef PERIPH_TIM_VISUAL_LAYER_H_
#define PERIPH_TIM_VISUAL_LAYER_H_

#include <stdint.h>
#include "global_define.h"

#define FFT_BAND_COUNT 16U

void Visual_Init(void);
void Visual_Reset(void);

void Visual_SetAGCMode(VisualAGCMode_t mode);

void Visual_Process(const float *bandRaw);

const float *Visual_GetTrail(void);
const float *Visual_GetPeak(void);

uint8_t Visual_IsIdleMode(void);

#endif /* PERIPH_TIM_VISUAL_LAYER_H_ */
