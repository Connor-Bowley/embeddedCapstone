/** \file alarm.c
*
* @brief Alarm Manager
*
* @par
* COPYRIGHT NOTICE: (c) 2017 Barr Group, LLC.
* All rights reserved.
*/

#include <stdint.h>
#include <stdio.h>
            
#include "project.h"
#include "os.h"
#include "alarm.h"
#include "adc.h"
#include "GUIDEMO_API.h"


/*!
*
* @brief Alarm Task
*/
void alarm_task(void * p_arg)
{

  // TODO: This task should pend on events from adc_task(),
  //       and then update the LCD background color accordingly.
  OS_FLAGS flags;
  OS_ERR err;
  uint32_t color =  BG_COLOR_GREEN;
  while(1){
    flags = OSFlagPend(&g_alarm_flags,ADC_ALARM_ALL,0,
               OS_OPT_PEND_BLOCKING | OS_OPT_PEND_FLAG_SET_ANY | OS_OPT_PEND_FLAG_CONSUME,
               NULL, &err);
    my_assert(err == OS_ERR_NONE);
    
    if(flags & ADC_ALARM_HIGH)
      color = BG_COLOR_RED;
    else if(flags & ADC_ALARM_MEDIUM)
      color = BG_COLOR_YELLOW;
    else if(flags & ADC_ALARM_LOW)
      color = BG_COLOR_BLUE;
    else if(flags & ADC_ALARM_NONE)
      color = BG_COLOR_GREEN;
    
    GUIDEMO_SetColorBG(color);
    
  }
}

