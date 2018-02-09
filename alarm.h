/** \file alarm.h
*
* @brief Alarm Manager
*
* @par
* COPYRIGHT NOTICE: (c) 2017 Barr Group
* All rights reserved.
*/

#ifndef _ALARM_H
#define _ALARM_H

#define BIT(n)  (1UL << (n))

#define ALARM_NONE    BIT(0UL)
#define ALARM_LOW     BIT(1UL)
#define ALARM_MEDIUM  BIT(2UL)
#define ALARM_HIGH    BIT(3UL)

extern OS_FLAG_GRP g_alarm_flags;

extern void alarm_task(void * p_arg);

#endif /* _ALARM_H */

