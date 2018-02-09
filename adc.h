/** \file adc.h
*
* @brief ADC Task interface
*
* @par
* COPYRIGHT NOTICE: (c) 2017 Barr Group, LLC.
* All rights reserved.
*/
#ifndef _ADC_H
#define _ADC_H

#include "os.h"

#define ADC_ALARM_NONE (0X1)
#define ADC_ALARM_LOW (0X2)
#define ADC_ALARM_MEDIUM (0X4)
#define ADC_ALARM_HIGH (0X8)
#define ADC_ALARM_ALL (ADC_ALARM_LOW | ADC_ALARM_MEDIUM | ADC_ALARM_HIGH | ADC_ALARM_NONE)

extern void     adc_task (void * p_arg);
extern void     adcInit(void);
extern void     ADC_IRQHandler(void);
extern OS_FLAG_GRP g_alarm_flags;

#endif /* _ADC_H */
