#ifndef FFT_H
#define FFT_H

#include <stdint.h>
#include <math.h>

#define FFT_SIZE			2048U
#define FFT_HOP_SIZE		(FFT_SIZE / 2U)
#define FFT_BAND_COUNT		16U

#define FFT_MIN_FREQ_HZ		60.0F
#define FFT_MAX_FREQ_HZ		24000.0F

#define DEFAULT_GAIN		0.01F
#define USB_THRESHOLD		0.001F
#define ADC_THRESHOLD		3.0F
#define PEAK_BUFFER_SIZE	512U

#define DB_MIN				-40.0F
#define DB_MAX				0.0F
#define DB_TO_LINEAR(db)	(powf(10.0f, (db) / 20.0f))
#define TARGET_PEAK_DB		-8.0F

void FFT_Init(void);
void FFT_Reset(void);

void FFT_PushMonoBlock(const int16_t *samples, uint32_t count);
uint8_t FFT_Run(void);

const uint8_t *FFT_GetLedLevels(void);

#endif
