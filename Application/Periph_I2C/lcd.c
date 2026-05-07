/*
 * lcd.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "lcd.h"

#include "ssd1306.h"

extern I2C_HandleTypeDef hi2c2;
extern DMA_HandleTypeDef hdma_i2c2_tx;

void LCD_PlotUi(void)
{
	// Y = 15까지가 노랑
	// Font_7x10 Font_11x18 Font_16x26
	ssd1306_Init(&hi2c2);

	// 가로줄
	for (int i = 0; i < SSD1306_WIDTH; i++)
	{
		ssd1306_DrawPixel(i , 0, White);
		ssd1306_DrawPixel(i , 15, White);
	}

	ssd1306_SetCursor(5, 4);	ssd1306_WriteString("VOL:", Font_7x10, White);
	//ssd1306_SetCursor(33, 4);	ssd1306_WriteString("000", Font_7x10, White);
	ssd1306_SetCursor(78, 4);	ssd1306_WriteString("SRC:", Font_7x10, White);
	//ssd1306_SetCursor(106, 4);	ssd1306_WriteString("MOD", Font_7x10, White);

	ssd1306_UpdateScreen(&hi2c2);
}
