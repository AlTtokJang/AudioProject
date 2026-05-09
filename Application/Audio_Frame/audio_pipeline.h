/*
 * audio_pipeline.h
 *
 *  Created on: 2026. 5. 8.
 *      Author: ADJ
 */

#ifndef AUDIO_FRAME_AUDIO_PIPELINE_H_
#define AUDIO_FRAME_AUDIO_PIPELINE_H_

#include <stdint.h>

void AudioPipeline_Init(void);
void AudioPipeline_Push(const int16_t *src, uint16_t count);
uint16_t AudioPipeline_PopOutput(int16_t *dst, uint16_t count);
void AudioPipeline_Process(void);
void AudioPipeline_RingClear(void);
void AudioPipeline_Loger(void);

#endif /* AUDIO_FRAME_AUDIO_PIPELINE_H_ */
