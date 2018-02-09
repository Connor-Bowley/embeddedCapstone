/** \file adc.c
*
* @brief ADC Task
*
* @par
* COPYRIGHT NOTICE: (c) 2017 Barr Group, LLC.
* All rights reserved.
*/

#include <stdlib.h>  // NULL
#include <stdio.h>   // sprintf()
#include "os.h"
#include "project.h"
#include "GUIDEMO_API.h"  // write to LCD
#include "adc.h"
#include "bsp_clock.h"

OS_Q adcQ;
OS_FLAG_GRP g_alarm_flags;

// **************************************************************************
// NOTE: We define the adc_t struct inside the .c file, not the .h file.
//       This is a form of encapsulation; we do not want to give
//       other code access to the register layout of the ADC registers.
// **************************************************************************

// Step 1a: Define a Register-Matching Struct for ADC3 peripheral
// See processor Reference Manual:
//     Table 98 in Section 15.13.18, "ADC Register Map"
typedef struct
{
  uint32_t SR;     // offset: 0x00
  uint32_t CR1;    // offset: 0x04
  uint32_t CR2;    // offset: 0x08
  uint32_t SMPR1;  // offset: 0x0C
  uint32_t SMPR2;  // offset: 0x10
  uint32_t JOFR1;  // offset: 0x14
  uint32_t JOFR2;  // offset: 0x18
  uint32_t JOFR3;  // offset: 0x1C
  uint32_t JOFR4;  // offset: 0x20
  uint32_t HTR;    // offset: 0x24
  uint32_t LTR;    // offset: 0x28
  uint32_t SQR1;   // offset: 0x2C
  uint32_t SQR2;   // offset: 0x30
  uint32_t SQR3;   // offset: 0x34
  uint32_t JSQR;   // offset: 0x38
  uint32_t JDR1;   // offset: 0x3C
  uint32_t JDR2;   // offset: 0x40
  uint32_t JDR3;   // offset: 0x44
  uint32_t JDR4;   // offset: 0x48
  uint32_t DR;     // offset: 0x4C
} ADC_T;

// Step 1b: Define a Register-Matching Struct for ADC Common registers.
// See processor Reference Manual:
//     Table 99 ("Common ADC Registers) in Section 15.13.18
//     Offset - ADC base + 0x300
typedef struct
{
  uint32_t CSR;    // Offset 0x00
  uint32_t CCR;    // Offset 0x04
  uint32_t CDR;    // Offset 0x08
} ADC_COMMON_T;

// Step 2a: Declare a pointer to the ADC3 Register Struct
// See processor Reference Manual:
//     Section 2.2.2, "Memory Map and Register Boundary Addresses"
//     Section 15.13.18, Table 97, "ADC Global Register Map"
#define ADC_BASE     (0x40012000UL)
#define ADC3_OFFSET  (0x200UL)
#define ADC3_ADDR    ((ADC_BASE) + (ADC3_OFFSET))
ADC_T volatile * const p_ADC3 = (ADC_T *) (ADC3_ADDR);
#define ADC3     (*p_ADC3)

// Step 2b: Declare a pointer to the ADC Common Registers Struct
// See processor Reference Manual:
//     Section 15.13.18, Table 97, "ADC Global Register Map"
#define ADC_COMMON_OFFSET    (0x300UL)
#define ADC_COMMON_ADDR ((ADC_BASE) + (ADC_COMMON_OFFSET))
ADC_COMMON_T volatile * const p_ADC_COMMON = (ADC_COMMON_T *) (ADC_COMMON_ADDR);
#define ADC_COMMON     (*p_ADC_COMMON)

// Macro for quick-and-dirty manipulation of registers
#define REG32(reg_addr)  ( * ( ( volatile uint32_t * ) (reg_addr) ) )
#define REG16(reg_addr)  ( * ( ( volatile uint16_t * ) (reg_addr) ) )
#define REG8(reg_addr)   ( * ( ( volatile  uint8_t * ) (reg_addr) ) )

// TODO: Define Message Queue for ADC ISR->Task Communication

// Local function prototypes
static void pot_trigger_conversion(void);
static void pot_init(void);
static void setADC(uint32_t val);

static OS_MUTEX g_adcMut;
static uint32_t g_adcRead;

/*!
*
* @brief: ADC Driver Task
*/
void adc_task(void * p_arg)
{
   OS_ERR     err;
   char str[15];
   OS_MSG_SIZE msg_size;
   uint32_t val;
  
   
   char alarmNone[] = "Alarm: None";
   char alarmLow[] = "Alarm: Low";
   char alarmMedium[] = "Alarm: Medium";
   char alarmHigh[] = "Alarm: High";
   char *curAlarm = alarmNone;

   OSFlagCreate(&g_alarm_flags,"Alarm Flag Group", 0, &err);
   my_assert(err == OS_ERR_NONE);

   (void)p_arg;    // NOTE: Silence compiler warning about unused param.

   // TODO: Create message queue used by ADC ISR to communicate ADC readings to task.
   OSQCreate(&adcQ, "ADC Queue", 100, &err);
   my_assert(err == OS_ERR_NONE);

   // Configure ADC hardware to read Potentiometer ADC channel
   // and then interrupt.
  pot_init();
  
    
  for (;;)    
  {
    // TODO: Wait 125 ms.
    OSTimeDlyHMSM(0,0,0,125,OS_OPT_TIME_DLY, &err);
    my_assert(err == OS_ERR_NONE);

    // Trigger ADC conversion.
    pot_trigger_conversion();

    // TODO: Wait for message from ADC ISR.
    val = (uint32_t)OSQPend(&adcQ, 0, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err);
    my_assert(err == OS_ERR_NONE);
    my_assert(msg_size > 0);
    setADC(val);
    
//    if(val < 10) {
//      OSFlagPost(&g_alarm_flags,ADC_ALARM_NONE,OS_OPT_POST_FLAG_SET,&err);
//      curAlarm = alarmNone;
//    }
//    else if(val < 400) {
//      OSFlagPost(&g_alarm_flags,ADC_ALARM_LOW,OS_OPT_POST_FLAG_SET,&err);
//      curAlarm = alarmLow;
//    }
//    else if(val < 600) {
//      OSFlagPost(&g_alarm_flags,ADC_ALARM_MEDIUM,OS_OPT_POST_FLAG_SET,&err);
//      curAlarm = alarmMedium;
//    }
//    else {
//      OSFlagPost(&g_alarm_flags,ADC_ALARM_HIGH,OS_OPT_POST_FLAG_SET,&err);
//      curAlarm = alarmHigh;
//    }
//    my_assert(err == OS_ERR_NONE);
//    
//    // TODO: Format and display the value.
//    sprintf(str,"ADC: %d",val);
//    GUIDEMO_API_writeLine(2, str);
//    GUIDEMO_API_writeLine(7, curAlarm);
  }
}

uint32_t getADC(void){
  uint32_t val;
  OS_ERR err;
  
  OSMutexPend(&g_adcMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
  my_assert(err == OS_ERR_NONE);
  
  val = g_adcRead;
  
  OSMutexPost(&g_adcMut,OS_OPT_POST_NONE,&err);
  my_assert(err == OS_ERR_NONE);
  
  return val;
}

void adcInit(void){
  OS_ERR err;
   OSMutexCreate(&g_adcMut, "ADC Mutex", &err);
   my_assert(err == OS_ERR_NONE);
}

void setADC(uint32_t val){
  OS_ERR err;
  OSMutexPend(&g_adcMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
  my_assert(err == OS_ERR_NONE);
  
  g_adcRead = val;
  
  OSMutexPost(&g_adcMut,OS_OPT_POST_NONE,&err);
  my_assert(err == OS_ERR_NONE);
}

// Base Address, GPIO Port F registers
#define  GPIO_PORT_F_BASE_ADDR    (0x40021400UL)

// GPIO port register offsets
#define GPIO_MODER_OFFSET  (0x00UL)
#define GPIO_PUPDR_OFFSET  (0x0CUL)

// GPIO port register addresses
#define GPIO_MODER(Port)   ((Port) + GPIO_MODER_OFFSET)
#define GPIO_PUPDR(Port)   ((Port) + GPIO_PUPDR_OFFSET)

// **********************************************************
/*!
* @brief Configure GPIO PF.10 as ADC input channel for potentiometer .
*/
static void AdcPin_init(void)
{
  volatile uint32_t temp;
  const uint32_t PortBaseAddr = GPIO_PORT_F_BASE_ADDR;
  const uint32_t Pin = 10UL;

  const uint32_t ThePin = Pin;

  // Configure GPIO as Analog Pin
  temp  = REG32(GPIO_MODER(PortBaseAddr));
  temp &= ~(0x03UL << (ThePin * 2UL));
  temp |= (0x03UL << (ThePin * 2UL));  // output
  REG32(GPIO_MODER(PortBaseAddr)) = temp;

  // Pull Up / Pull Down 
  temp  = REG32(GPIO_PUPDR(PortBaseAddr));
  temp &= ~(0x03UL << (ThePin * 2UL));
  temp |= (0x00UL << (ThePin * 2UL));  // No Pull Up or Pull Down
  REG32(GPIO_PUPDR(PortBaseAddr)) = temp;

}

// *******************************************************************
/*!
* @brief Configure hardware for potentiometer ADC channel
*/
static void ADC_Hardware_Config(void)
{
  volatile uint32_t temp;

  // -1- Enable clocks to ADC3 peripheral and GPIO Port for its pin
  // Be sure to perform a dummy read to delay after enabling clock to peripheral.

  // ADC3 clock enable
  REG32(0x40023844UL) |= (1UL << 10UL);
  temp = REG32(0x40023844UL) & (1UL << 10UL);  // dummy read

  // GPIO Port F clock enable
  REG32(0x40023830UL) |= (1UL << 5UL);
  temp = REG32(0x40023830UL) & (1UL << 5UL);  // dummy read

  // -2- Configure GPIO pin PF.10 for ADC3 peripheral function
  AdcPin_init();

  // -3- Configure the NVIC for ADC end-of-conversion interrupt
  // See Table 43, p. 286 of reference manual for ADC vector slot
  const uint32_t ADC_Int_Slot = 18;
  // Set priority of ADC interrupt
  REG8(0xE000E400UL + ADC_Int_Slot) = 0;
  // Enable ADC interrupt at NVIC
  REG32(0xE000E100UL + ((ADC_Int_Slot >> 5UL) << 2)) = (1UL << (ADC_Int_Slot & 0x1FUL));
}

// *******************************************************************
static void pot_init_pt1(void)
{
  // Configure ADC pin hardware and interrupts
  ADC_Hardware_Config();

  // ADC Clock prescaler - PCLK2 divided by 4
  ADC_COMMON.CCR &= ~(0x30000UL);
  ADC_COMMON.CCR |=  (0x10000UL);

  // ADC Scan Mode - disabled
  ADC3.CR1 &= ~(0x100UL);

  // ADC Resolution - 12 bits
  ADC3.CR1 &= ~(0x03000000UL);

  // ADC Data Alignment  - right-aligned
  ADC3.CR2 &= ~(1UL << 11UL);

  // External trigger conversion start - disabled
  ADC3.CR2 &= ~(0x0F000000UL);

  // External trigger polarity setting - N/A
  ADC3.CR2 &= ~(0x30000000UL);

  // Disable ADC Continuous Conversion Mode 
  ADC3.CR2 &= ~(1UL << 1UL);
  ADC3.CR2 |=  (0UL << 1UL);

  // Disable ADC regular discontinuous mode
  ADC3.CR1 &= ~(1UL << 11UL);

  // Set number of conversions (1)
  ADC3.SQR1 &= ~(0x0FUL << 20UL);
  ADC3.SQR1 |=  ((1UL - 1UL) << 20UL);

  // DMA Continuous requests - disabled
  ADC3.CR2 &= ~(1UL << 9UL);

  // ADC End of Conversion (EOC) selection
  // EOC bit is set at the end of each sequence of regular conversions.
  ADC3.CR2 &= ~(1UL << 10UL);

}

// *******************************************************************
static void pot_init_pt2(void)
{
  // Set sample time to 3 cycles
  ADC3.SMPR2 &= ~(7UL << 24UL);
  ADC3.SMPR2 |=  (0UL << 24UL);

  // Set ADC_SQR3 bits for Rank 1
  ADC3.SQR3 &= ~(0x1FUL << 0UL);
  ADC3.SQR3 |=  (0x08UL << 0UL);

}

// *******************************************************************
static void pot_init_pt3(void)
{
  volatile uint32_t counter = 0;
  const    uint32_t sysclock_speed = BSP_ClkFreqGet(BSP_CLK_ID_SYSCLK);
  my_assert(sysclock_speed == (200UL * 1000UL * 1000UL));

  // Enable ADC peripheral
  ADC3.CR2 |=  (1UL << 0UL);

  // Since ADC was disabled, we must wait a short period
  // for the ADC peripheral to stabilize.
  // tSTAB = 1usec per device datasheet, let's give it 3uS.
  counter = (3UL * (sysclock_speed / (1000UL * 1000UL)));
  while (counter--)
  {
    // just decrement and kill some time
  }

  // Sanity check: ensure ADC is enabled
  my_assert((ADC3.CR2 & (1UL << 0UL)));

  // Clear OVR and EOC status register flags
  ADC3.SR &= ~((1UL << 5UL) | (1UL << 1UL));

  // Enable interrupts:  over-run and end of conversion (EOC)
  ADC3.CR1 |= ((1UL << 26UL) | (1UL << 5UL));
}

// *******************************************************************
/*!
* @brief Initialize potentiometer's ADC channel
*/
static void pot_init(void)
{
  // -1- Configure the ADC peripheral
  pot_init_pt1();

  // -2- Configure specific ADC channel
  pot_init_pt2();

  // -3- Start the continuous ADC conversion
  pot_init_pt3();
}

// *******************************************************************
/*!
* @brief Trigger (start) conversion on potentiometer's ADC channel
*/
static void pot_trigger_conversion(void)
{
  // Should be nothing hanging around from last time
  my_assert( 0 == (ADC3.SR & ((1UL << 5UL) | (1UL << 4UL) | (1UL << 1UL))) );

  // Start the conversion!
  ADC3.CR2 |=  (1UL << 30UL);
}

/********************************************************************
*
*       ADC_IRQHandler()
*
* Purpose:
*   ADC peripheral interrupt handler for potentiometer
*
* Note(s):
*   (1) 'startup_stm32f746xx.s' defines ADC_IRQHandler() as a WEAK
*       reference. So we re-define the handler here to override it.
*
*   (2) The ISR must follow the template shown below:
*
*           void MyISRHandler (void)
*           {
*               CPU_SR_ALLOC();
*
*               CPU_CRITICAL_ENTER();
*               OSIntEnter();
*               CPU_CRITICAL_EXIT();
*
*               ----- HANDLER THE ISR HERE ------
*
*               OSIntExit();
*           }
*
*/

#define LPF_SIZE   (4UL)
#define LPF_SHIFT  (2UL)
void ADC_IRQHandler(void)
{
  uint32_t tmp1, tmp2;
  uint32_t sample;
  OS_ERR     err;

  static uint32_t adc_lpf[LPF_SIZE] = { 0 };
  static uint32_t lpf_index = 0;
  static uint32_t lpf_sum   = 0;

  // Need for critical section
  CPU_SR_ALLOC();

  // Tell OS that we are entering an ISR
  CPU_CRITICAL_ENTER();
  OSIntEnter();
  CPU_CRITICAL_EXIT();

  // Read out the converted value
  sample = ADC3.DR;

//   Low pass filter
  adc_lpf[lpf_index] = sample;
  lpf_sum += sample;
  sample = adc_lpf[(lpf_index - 1) & (LPF_SIZE - 1)];
  lpf_sum = ((lpf_sum >= sample) ? (lpf_sum - sample) : 0);
  lpf_index = ((lpf_index + 1) & (LPF_SIZE - 1));
  sample = lpf_sum >> LPF_SHIFT;
  
  // TODO: Send the value ("sample") to the ADC task through a message queue
  OSQPost(&adcQ, (void*)sample, sizeof(void *), OS_OPT_POST_FIFO, &err);
  my_assert(err == OS_ERR_NONE);

  // Clear regular group conversion flag
  ADC3.SR &= ~((1UL << 4UL) | (1UL << 1UL));

  // Check for overrun
  //   1) Did an overrun happen?
  //   2) Is the overrun interrupt enabled?
  tmp1 = ((ADC3.SR  >>  5UL) & 1UL);
  tmp2 = ((ADC3.CR1 >> 26UL) & 1UL);
  if (tmp1 && tmp2)
  {
    // Clear the ADC overrun flag
    ADC3.SR = ~(1UL << 5UL);
  }

  // Tell OS that we are leaving the ISR
  OSIntExit();

}

