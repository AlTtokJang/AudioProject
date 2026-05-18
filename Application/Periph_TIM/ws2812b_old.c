#include "ws2812b.h"
#include <string.h>

extern uint16_t pwmBuffer[PWM_BUF_SIZE];

static void WS2812_WriteByte(uint16_t *buf, uint8_t byte)
{
	for (int i = 0; i < 8; i++)
	{
		if (byte & (1 << (7 - i)))
			buf[i] = WS2812_T1H;
		else
			buf[i] = WS2812_T0H;
	}
}

void Plot_WS2812B(const uint8_t *levels)
{
	// 여기에 밝기 조절 코드 작성


	memset(pwmBuffer, 0, sizeof(uint16_t) * PWM_BUF_SIZE);

	for (uint32_t x = 0; x < LED_WIDTH; x++)
	{
		// 높이 클램핑
		uint8_t height = levels[LED_WIDTH - 1 - x];
		if (height > LED_HEIGHT)
			height = LED_HEIGHT;

		for (uint32_t y = 0; y < LED_HEIGHT; y++)
		{
			uint32_t ledIndex;
			uint8_t on = (y < height) ? 1 : 0;

			uint8_t g = 0, r = 0, b = 0;

			if (on)
			{
				float t = (float)y / (LED_HEIGHT - 1);  // 0 ~ 1

				if (t < 0.3f) // 초록 → 파랑
				{
					float k = t / 0.3f;  // 0~1
					g = (uint8_t)((1.0f - k) * LED_ON_G);
					b = (uint8_t)(k * LED_ON_B);
				}
				else // 파랑 → 빨강
				{
					float k = (t - 0.3f) / 0.7f; // 0~1
					b = (uint8_t)((1.0f - k) * LED_ON_B);
					r = (uint8_t)(k * LED_ON_R);
				}
			}

			// 지그재그 매핑
			if ((x & 1U) == 0U)
			{
				// 짝수 밴드: 아래 -> 위
				ledIndex = x * LED_HEIGHT + y;
			}
			else
			{
				// 홀수 밴드: 위 -> 아래
				ledIndex = x * LED_HEIGHT + (LED_HEIGHT - 1U - y);
			}

			uint32_t offset = ledIndex * WS2812_BITS_PER_LED;

			WS2812_WriteByte(&pwmBuffer[offset + 0],  g);
			WS2812_WriteByte(&pwmBuffer[offset + 8],  r);
			WS2812_WriteByte(&pwmBuffer[offset + 16], b);
		}
	}
}
