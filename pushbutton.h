/** \file pushbutton.h
*
* @brief Button Debouncer
*
* @par
* COPYRIGHT NOTICE: (C) 2017 Barr Group
* All rights reserved.
*/

#ifndef _PUSHBUTTON_H
#define _PUSHBUTTON_H

#include "os.h"

// TODO: "extern" the two switch semaphores here
extern OS_SEM sem_sw1, sem_sw2;

void  debounce_task_init(void);
void  debounce_task(void * p_arg);

#endif /* _PUSHBUTTON_H */
