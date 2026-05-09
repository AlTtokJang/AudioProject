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

#endif /* PERIPH_ADC_ADC_H_ */
