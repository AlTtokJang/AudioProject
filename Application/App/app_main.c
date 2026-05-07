/*
 * app_main.c
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#include "app_main.h"
#include "main.h"

#include "lcd.h"
#include "adc.h"

void App_Main(void)
{
	LCD_PlotUi();
	ADC_Start_VReg();
}
