#ifndef INC_WS2812B_H_
#define INC_WS2812B_H_

/*
#include "stm32f7xx_hal.h"

#define PWM_BUF_SIZE		6208

#define LED_WIDTH			16
#define LED_HEIGHT			16
#define LED_COUNT			(LED_WIDTH * LED_HEIGHT)

#define WS2812_BITS_PER_LED	24
#define WS2812_RESET_SLOTS	64

#define WS2812_T0H			90     // 0비트 duty
#define WS2812_T1H			180    // 1비트 duty

#define LED_ON_R			0x20
#define LED_ON_G			0x20
#define LED_ON_B			0x20

void Plot_WS2812B(const uint8_t *levels);
*/


#include <stdint.h>

// ================================================================
// MATRIX CONFIG
// ================================================================
#define LED_WIDTH           16U
#define LED_HEIGHT          16U
#define LED_COUNT           (LED_WIDTH * LED_HEIGHT)

// ================================================================
// PWM / TIM CONFIG
// ================================================================
#define PWM_BUF_SIZE        6208U

#define WS2812_BITS_PER_LED 24U

#define WS2812_T0H          90U
#define WS2812_T1H          180U

// ================================================================
// API
// ================================================================
void WS2812B_Show(const uint8_t *frame);

#endif /* INC_WS2812B_H_ */
