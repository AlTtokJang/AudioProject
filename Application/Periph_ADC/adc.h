/*
 * adc.h
 *
 *  Created on: 2026. 5. 7.
 *      Author: ADJ
 */

#ifndef PERIPH_ADC_ADC_H_
#define PERIPH_ADC_ADC_H_

#include <stdint.h>

#define VREG_BUFFER_SIZE 4

void ADC_Start_Aux(void);
void ADC_Stop_Aux(void);

void ADC_Start_VReg(void);
void ADC_Stop_VReg(void);

void ADC_GetValue_EQ(uint8_t *out);
void ADC_GetValue_VOL(uint8_t *out);

/*
 * 디버거 -------------------------------------------------------------------------------------
 */
//#define ADC_DEBUG

#ifdef ADC_DEBUG

void ADC_DebugStartAuxCapture(void);
void ADC_DebugDumpAuxCaptureCsv(void);
uint8_t ADC_DebugIsAuxCaptureDone(void);

#endif

#endif /* PERIPH_ADC_ADC_H_ */
