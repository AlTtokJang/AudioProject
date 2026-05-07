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

#endif /* AUDIO_FRAME_AUDIO_PIPELINE_H_ */
