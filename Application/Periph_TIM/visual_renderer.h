/*
 * visual_renderer.h
 *
 *  Created on: 2026. 5. 18.
 *      Author: nugur
 */

#ifndef PERIPH_TIM_VISUAL_RENDERER_H_
#define PERIPH_TIM_VISUAL_RENDERER_H_

#include <stdint.h>
//#include "arm_math.h"

#define MATRIX_WIDTH 16U
#define MATRIX_HEIGHT 16U

void VisualRenderer_Init(void);
void VisualRenderer_Clear(void);

void VisualRenderer_DrawSpectrum(const float *trail, const float *peakHold);

void VisualRenderer_ApplySpatialBlur(void);
void VisualRenderer_ApplyTemporalBlur(void);

const uint8_t *VisualRenderer_GetFrame(void);

#endif /* PERIPH_TIM_VISUAL_RENDERER_H_ */
