/*
 * lcd.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "main.h"
#include "lcd.h"
#include "ssd1306.h"
#include "global_define.h"
#include "adc.h"
#include <string.h>
#include <stdio.h>

extern volatile AudioSource_t audioSource;
extern volatile uint8_t i2sUseEq;
extern volatile uint8_t agcRun;

extern I2C_HandleTypeDef hi2c2;
extern DMA_HandleTypeDef hdma_i2c2_tx;

#define LCD_BUFFER_SIZE		(SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define LCD_EQ_HOLD_MS		2000

typedef enum
{
	LCD_SCREEN_MAIN = 0,
	LCD_SCREEN_EQ
} LCD_Screen_t;

static uint8_t lcd_buffer[1 + LCD_BUFFER_SIZE];
static volatile uint8_t lcd_dma_busy = 0;

static LCD_Screen_t lcd_screen = LCD_SCREEN_MAIN;
static uint32_t lcd_eq_hold_tick = 0;


static uint8_t eq_value[3];
static uint8_t eq_value_prev[3];

static HAL_StatusTypeDef WriteCommand(uint8_t command)
{
    return HAL_I2C_Mem_Write(
    		&hi2c2,
    		SSD1306_I2C_ADDR,
			0x00,
			I2C_MEMADD_SIZE_8BIT,
			&command,
			1,
			10);
}

static void LCD_ClearBuffer(void)
{
	lcd_buffer[0] = 0x40;
	memset(&lcd_buffer[1], 0x00, LCD_BUFFER_SIZE);
}

static void LCD_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
	{
		return;
	}

	uint16_t index = 1 + x + (y / 8) * SSD1306_WIDTH;
	uint8_t bit = 1 << (y % 8);

	if (on)
	{
		lcd_buffer[index] |= bit;
	}
	else
	{
		lcd_buffer[index] &= ~bit;
	}
}

static void LCD_DrawHLine(uint8_t x, uint8_t y, uint8_t width, uint8_t on)
{
	for (uint8_t i = 0; i < width; i++)
	{
		LCD_DrawPixel(x + i, y, on);
	}
}

static void LCD_DrawVLine(uint8_t x, uint8_t y, uint8_t height, uint8_t on)
{
	for (uint8_t i = 0; i < height; i++)
	{
		LCD_DrawPixel(x, y + i, on);
	}
}

static void LCD_WriteChar(uint8_t x, uint8_t y, char ch, FontDef font, uint8_t on)
{
	if (ch < 32 || ch > 126)
	{
		ch = '?';
	}

	for (uint8_t row = 0; row < font.FontHeight; row++)
	{
		uint16_t line = font.data[(ch - 32) * font.FontHeight + row];

		for (uint8_t col = 0; col < font.FontWidth; col++)
		{
			if ((line << col) & 0x8000)
			{
				LCD_DrawPixel(x + col, y + row, on);
			}
		}
	}
}

static void LCD_DrawGauge(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int value, int min, int max)
{
	if (value < min)
	{
		value = min;
	}

	if (value > max)
	{
		value = max;
	}

	uint8_t fill = (uint8_t)((value - min) * (w - 4) / (max - min));

	for (uint8_t px = 0; px < w; px++)
	{
		LCD_DrawPixel(x + px, y, 1);
		LCD_DrawPixel(x + px, y + h - 1, 1);
	}

	for (uint8_t py = 0; py < h; py++)
	{
		LCD_DrawPixel(x, y + py, 1);
		LCD_DrawPixel(x + w - 1, y + py, 1);
	}

	for (uint8_t py = 2; py < h - 2; py++)
	{
		for (uint8_t px = 0; px < fill; px++)
		{
			LCD_DrawPixel(x + 2 + px, y + py, 1);
		}
	}
}

static void LCD_WriteString(uint8_t x, uint8_t y, const char *str, FontDef font, uint8_t on)
{
	while (*str)
	{
		LCD_WriteChar(x, y, *str, font, on);
		x += font.FontWidth;

		if (x + font.FontWidth >= SSD1306_WIDTH)
		{
			break;
		}

		str++;
	}
}

static HAL_StatusTypeDef LCD_RefreshDMA(void)
{
	HAL_StatusTypeDef ret;

	if (lcd_dma_busy)
	{
		return HAL_BUSY;
	}

	lcd_dma_busy = 1;

	ret = HAL_I2C_Master_Transmit_DMA(
		&hi2c2,
		SSD1306_I2C_ADDR,
		lcd_buffer,
		sizeof(lcd_buffer));

	if (ret != HAL_OK)
	{
		lcd_dma_busy = 0;
		return ret;
	}

	return HAL_OK;
}

void LCD_AppModeInit(void)
{
	WriteCommand(0x20);	// 메모리 어드레싱 설정 진입
	WriteCommand(0x00);	// 수평 어드레싱 모드

	LCD_ClearBuffer();

	WriteCommand(0x21);	// 열 주소 설정 진입
	WriteCommand(0x00);	// 열 시작 0
	WriteCommand(0x7F);	// 열 끝  127

	WriteCommand(0x22);	// 페이지 주소 설정 진입
	WriteCommand(0x00);	// 페이지 시작 0
	WriteCommand(0x07);	// 페이지 끝  7

	LCD_RefreshDMA();
}

static uint8_t EqValueChanged(const uint8_t *now, const uint8_t *prev, uint8_t size)
{
	for (uint8_t i = 0; i < size; i++)
	{
		int diff = (int)now[i] - (int)prev[i];

		if (diff < 0)
		{
			diff = -diff;
		}

		if (diff >= 2)
		{
			return 1;
		}
	}

	return 0;
}

static void LCD_DrawEQScreen(void)
{
	LCD_ClearBuffer();

	// 노랑/파랑 영역 경계선
	for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
	{
		LCD_DrawPixel(x, 15, 1);
	}

	LCD_WriteString(29, 3, "EQ SETTING", Font_7x10, 1);

	LCD_DrawGauge(28, 28, 100, 10, (int)eq_value[0], 0, 100);	LCD_WriteString(0, 30, "LOW", Font_7x10, 1);
	LCD_DrawGauge(28, 40, 100, 10, (int)eq_value[1], 0, 100);	LCD_WriteString(0, 42, "MID", Font_7x10, 1);
	LCD_DrawGauge(28, 52, 100, 10, (int)eq_value[2], 0, 100);	LCD_WriteString(0, 54, "HIG", Font_7x10, 1);

	LCD_RefreshDMA();
}

void LCD_DrawMainScreen(void)
{
	if (lcd_dma_busy)
	{
		return;
	}

	static uint8_t firstView = 1;

	ADC_GetValue_EQ(eq_value);

	if (firstView || EqValueChanged(eq_value, eq_value_prev, sizeof(eq_value)))
	{
		firstView = 0;

		memcpy(eq_value_prev, eq_value, sizeof(eq_value));

		lcd_screen = LCD_SCREEN_EQ;
		lcd_eq_hold_tick = HAL_GetTick();

		LCD_DrawEQScreen();
		return;
	}

	if (lcd_screen == LCD_SCREEN_EQ)
	{
		if ((HAL_GetTick() - lcd_eq_hold_tick) < LCD_EQ_HOLD_MS)
		{
			LCD_DrawEQScreen();
			return;
		}

		lcd_screen = LCD_SCREEN_MAIN;
	}

	char *input_mode;
	switch (audioSource)
	{
		case AUDIO_SRC_USB:
			input_mode = "USB";
			break;
		case AUDIO_SRC_AUX:
			input_mode = "AUX";
			break;
		case AUDIO_SRC_BT:
			input_mode = " BT";
			break;
	}

	uint8_t db_min;
	ADC_GetValue_VOL(&db_min);

	LCD_ClearBuffer();

	// y = 0~15 노랑 영역
	LCD_WriteString(36, 3, input_mode, Font_7x10, 1);
	LCD_WriteString(64, 3, "MODE", Font_7x10, 1);

	// 노랑/파랑 영역 경계선
	for (uint8_t x = 0; x < SSD1306_WIDTH; x++)
	{
		LCD_DrawPixel(x, 15, 1);
	}

	// 상태 표시
	LCD_WriteString(2, 18, "EQ:", Font_7x10, 1);
	LCD_WriteString(23, 18, i2sUseEq ? "ON" : "OFF", Font_7x10, 1);

	LCD_WriteString(69, 18, "AGC:", Font_7x10, 1);
	LCD_WriteString(97, 18, agcRun ? "RUN" : "STOP", Font_7x10, 1);

	// dB_min 게이지
	LCD_WriteString(19, 38, "LED RANGE(dB)", Font_7x10, 1);
	LCD_WriteString(0, 54, "-20", Font_7x10, 1);
	LCD_DrawGauge(28, 52, 72, 10, (int)db_min, 0, 100);
	LCD_WriteString(106, 54, "-60", Font_7x10, 1);

	LCD_RefreshDMA();
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if (hi2c->Instance == I2C2)
	{
		lcd_dma_busy = 0;
	}
}
