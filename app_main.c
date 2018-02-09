/*
*********************************************************************************************************
*                                            EXAMPLE CODE
*
*               This file is provided as an example on how to use Micrium products.
*
*               Please feel free to use any application code labeled as 'EXAMPLE CODE' in
*               your application products.  Example code may be used as is, in whole or in
*               part, or may be used as a reference only. This file can be modified as
*               required to meet the end-product requirements.
*
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*
*               You can find our product's user manual, API reference, release notes and
*               more information at https://doc.micrium.com.
*               You can contact us at www.micrium.com.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                         STM32F746G-DISCO
*                                         Evaluation Board
*
* Filename      : app_main.c
* Version       : V1.00
* Programmer(s) : FF
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include  "stdarg.h"
#include  "stdio.h"
#include  "stm32f7xx_hal.h"
#include  "string.h"

#include  "cpu.h"
#include  "lib_math.h"
#include  "lib_mem.h"
#include  "os.h"
#include  "os_app_hooks.h"

#include  "app_cfg.h"
#include  "bsp.h"
#include  "bsp_led.h"
#include  "bsp_clock.h"
#include  "bsp_pb.h"
#include  "bsp_test.h"
#include  "GUI.h"
#include  "GUIDEMO_API.h"
#include  "pushbutton.h"
#include  "adc.h"
#include  "alarm.h"
#include  "scuba.h"

#define LED1_STKSZ (192)
#define LED2_STKSZ (LED1_STKSZ)
#define STARTUP_STKSZ (192)
#define DB_STKSZ (192)
#define BUTTON_STKSZ (192)
#define ADC_STKSZ (192)
#define ALARM_STKSZ (192)
#define MASTER_STKSZ (192)

#define MAX_AIR (200000)
#define MAX_SAFE_DEPTH (40000)
#define MAX_SAFE_ASCENT (15000)

typedef struct {
  BOARD_LED_ID led;
  uint32_t delay;
} led_struct;
static led_struct l1, l2;

typedef struct {
  OS_SEM *sem;
  void (*func)(void);
  char name[16];
} buttonArgs;
static buttonArgs b1, b2;

static void led_task(void *);
static void buttonPress(void *);
static void master_task(void *);
static void toggleIsMeters(void);

static int32_t getAir(void);
static void addAir(void);
static void useAir(int32_t amount_cl);
// *****************************************************************
// Define storage for each Task Control Block (TCB) and task stacks
// *****************************************************************
static  OS_TCB   AppTaskGUI_TCB, led1_task_TCB, 
   led2_task_TCB,startup_task_TCB,
   debounce_TCB,
   button1_TCB,
   button2_TCB,
   adc_TCB,
   alarm_TCB,
   master_TCB;
static  CPU_STK  AppTaskGUI_Stk[APP_CFG_TASK_GUI_STK_SIZE], 
   led1_task_stk[LED1_STKSZ], 
   led2_task_stk[LED2_STKSZ],
   startup_task_stk[STARTUP_STKSZ],
   debounce_stk[DB_STKSZ],
   button1_stk[BUTTON_STKSZ],
   button2_stk[BUTTON_STKSZ],
   adc_stk[ADC_STKSZ],
   alarm_stk[ALARM_STKSZ],
   master_stk[MASTER_STKSZ];

static OS_MUTEX ledMut;

static int32_t g_air; //in centiliters, from 0 - 200000 (0 - 2000L)
static int32_t g_depth; //in millimeters
int8_t g_isMeters = 1; //feet if 0, meters otherwise

OS_MUTEX g_isMetersMut;
OS_MUTEX g_airMut;


static void startup(void * p_arg)
{
    OS_ERR  err;
    // Initialize BSP
    BSP_Init();

   #if OS_CFG_STAT_TASK_EN > 0u
       // Compute CPU capacity with no other task running
       OSStatTaskCPUUsageInit(&err);
       my_assert(OS_ERR_NONE == err);
       OSStatReset(&err);
       my_assert(OS_ERR_NONE == err);
   #endif

   #ifdef CPU_CFG_INT_DIS_MEAS_EN
       CPU_IntDisMeasMaxCurReset();
   #endif
   
   //prep stuff for dive
   g_depth = 0;
   g_air = 5000; // 50 L
   GUIDEMO_API_writeLine(0, "Brand");
   adcInit();
       
   //create mutexes
   OSMutexCreate(&ledMut, "LED Mutex", &err);
   my_assert(OS_ERR_NONE == err);
   OSMutexCreate(&g_isMetersMut, "Units Mutex", &err);
   my_assert(OS_ERR_NONE == err);
   OSMutexCreate(&g_airMut, "Air Mutex", &err);
   my_assert(OS_ERR_NONE == err);
   
   l1.led = LED1;
   l1.delay = 166;
   l2.led = LED2;
   l2.delay = 166;

   // Create task to blink LED1
   OSTaskCreate(&led1_task_TCB, "Led1 task", (OS_TASK_PTR) led_task,
              (void *)&l1, 17, 
              &led1_task_stk[0],LED1_STKSZ/10u,LED1_STKSZ,
              0u,0u,0,
              OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   // Create task to blink LED2
   OSTaskCreate(&led2_task_TCB, "Led2 task", (OS_TASK_PTR) led_task,
              (void *)&l2, 17, 
              &led2_task_stk[0],LED2_STKSZ/10u,LED2_STKSZ,
              0u,0u,0,
              OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   b1.sem = &sem_sw1;
   b1.func = toggleIsMeters;
   strcpy(b1.name, "Short Click: ");
   b2.sem = &sem_sw2;
   b2.func = addAir;
   strcpy(b2.name, "Long Click:  ");
   
   OSTaskCreate(&button1_TCB, "Button 1 task",(OS_TASK_PTR)buttonPress,
                &b1, 15,
                &button1_stk[0],BUTTON_STKSZ/10,BUTTON_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   OSTaskCreate(&button2_TCB, "Button 2 task",(OS_TASK_PTR)buttonPress,
                &b2, 15,
                &button2_stk[0],BUTTON_STKSZ/10,BUTTON_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   debounce_task_init();
   OSTaskCreate(&debounce_TCB, "Debounce task", (OS_TASK_PTR)debounce_task,
                0, 14,
                &debounce_stk[0],DB_STKSZ/10,DB_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   //init and make adc task
   OSTaskCreate(&adc_TCB, "ADC Task", (OS_TASK_PTR)adc_task,
                0,14,
                &adc_stk[0],ADC_STKSZ/10,ADC_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   OSTaskCreate(&alarm_TCB, "Alarm Task", (OS_TASK_PTR)alarm_task,
                0,14,
                &alarm_stk[0],ALARM_STKSZ/10,ALARM_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
   
   OSTaskCreate(&master_TCB, "Master Task", (OS_TASK_PTR)master_task,
                0,13,
                &master_stk[0],MASTER_STKSZ/10,MASTER_STKSZ,
                0u,0u,0,
                OS_OPT_TASK_NONE,&err);
   my_assert(OS_ERR_NONE == err);
}

static uint8_t getIsMeters(){
     uint8_t val;
     OS_ERR err;
  
     OSMutexPend(&g_isMetersMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
     my_assert(err == OS_ERR_NONE);
     
     val = g_isMeters;
     
     OSMutexPost(&g_isMetersMut,OS_OPT_POST_NONE,&err);
     my_assert(err == OS_ERR_NONE);
     
     return val;
}

static void toggleIsMeters(void){
   OS_ERR err;
   OSMutexPend(&g_isMetersMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
   my_assert(err == OS_ERR_NONE);

   g_isMeters = !g_isMeters;

   OSMutexPost(&g_isMetersMut,OS_OPT_POST_NONE,&err);
   my_assert(err == OS_ERR_NONE);
}

static int32_t getAir(void){
  OS_ERR err;
  int32_t val;
   OSMutexPend(&g_airMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
   my_assert(err == OS_ERR_NONE);

   val = g_air;

   OSMutexPost(&g_airMut,OS_OPT_POST_NONE,&err);
   my_assert(err == OS_ERR_NONE);
   
   return val;
}

static void addAir(void){
  OS_ERR err;
  if(g_depth != 0)
    return;
  
   OSMutexPend(&g_airMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
   my_assert(err == OS_ERR_NONE);

   g_air = g_air + 2000 < MAX_AIR ? g_air + 2000 : MAX_AIR; //add 20 L

   OSMutexPost(&g_airMut,OS_OPT_POST_NONE,&err);
   my_assert(err == OS_ERR_NONE);
}

static void useAir(int32_t amount_cl){
  OS_ERR err;
   OSMutexPend(&g_airMut,0,OS_OPT_PEND_BLOCKING,NULL,&err);
   my_assert(err == OS_ERR_NONE);

   g_air = g_air - amount_cl > 0 ? g_air - amount_cl : 0;

   OSMutexPost(&g_airMut,OS_OPT_POST_NONE,&err);
   my_assert(err == OS_ERR_NONE);
}

static void det_alarms(curAir, rate, g_depth)
{
    uint32_t gas_req_to_surf = 0;
    uint32_t color =  BG_COLOR_GREEN;
    
    gas_req_to_surf = gas_to_surface_in_cl(g_depth);
      
    if(curAir < gas_req_to_surf)
      color = BG_COLOR_RED;
    else if(rate > 15)
      color = BG_COLOR_YELLOW;
    else if(g_depth>40)
      color = BG_COLOR_BLUE;
    
    GUIDEMO_SetColorBG(color);
}

static void led_task(void * p_arg)
{
    OS_ERR  err;
    led_struct *l = (led_struct *)p_arg;
    // Task main loop
    BSP_LED_On(LED1);
    for (;;)
    {
        // TODO: Toggle LED1
      OSMutexPend(&ledMut, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
      my_assert(err == OS_ERR_NONE);
      BSP_LED_Toggle(l->led);
      OSMutexPost(&ledMut,OS_OPT_POST_NONE,&err);
      my_assert(err == OS_ERR_NONE);

        // TODO: Sleep for the delay
      OSTimeDlyHMSM(0,0,0,l->delay,OS_OPT_TIME_DLY,&err);
      my_assert(err == OS_ERR_NONE);
    }
}

static void master_task(void * p_arg){
   OS_ERR err;
   char depth_str[15], rate_str[15], air_str[15], time_str[15];
   
   while(1){
     
      //calc rate ascent/descent and display
      int32_t rate = ADC2RATE(getADC());
      
      int32_t depthChange = depth_change_in_mm(rate);
        
      //calc cur depth
      g_depth = g_depth + depthChange > 0 ? g_depth + depthChange : 0;
        
     //calc air and use it
      int32_t airChange = gas_rate_in_cl(g_depth);
      useAir(airChange);
     
     //add to dive time
     
     //calculate alarms
      int32_t curAir = getAir();     
      det_alarms(curAir, rate, g_depth);
      
     //display depth, rate, air, alarms
      if(getIsMeters()){
        sprintf(depth_str,"DEPTH: %d m",g_depth / 1000);
        sprintf(rate_str,"RATE: %d m",rate);
      }
      else {
        sprintf(depth_str,"DEPTH: %d ft",MM2FT(g_depth));
        sprintf(rate_str,"RATE: %d ft",MM2FT(rate * 1000));
      }
      sprintf(air_str,"AIR: %d.%d L",curAir/100,curAir % 100);
      GUIDEMO_API_writeLine(2,depth_str);
      GUIDEMO_API_writeLine(3,rate_str);
      GUIDEMO_API_writeLine(4,air_str);
     
     //sleep for our half second CHANGE THIS???
     OSTimeDlyHMSM(0,0,0,500,OS_OPT_TIME_DLY,&err);
     my_assert(err == OS_ERR_NONE);
   }
}



static void buttonPress(void * p_arg){
  OS_ERR err;
  buttonArgs *arg = (buttonArgs *)p_arg;
  
  while(1){
      OSSemPend(arg->sem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
      my_assert(err == OS_ERR_NONE);
      arg->func();
  }
}



// *****************************************************************
int main(void)
{
    OS_ERR   err;

    HAL_Init();
    BSP_SystemClkCfg();   // Init. system clock frequency to 200MHz
    CPU_Init();           // Initialize the uC/CPU services
    Mem_Init();           // Initialize Memory Managment Module
    Math_Init();          // Initialize Mathematical Module
    CPU_IntDis();         // Disable all Interrupts.
        

    // TODO: Init uC/OS-III.
    //OSInit(DEF_NULL,(MEM_SEG *)DEF_NULL,&err);
    OSInit(&err);
    // Create the GUI task
    OSTaskCreate(&AppTaskGUI_TCB, "uC/GUI Task", (OS_TASK_PTR ) GUI_DemoTask,
                 0, APP_CFG_TASK_GUI_PRIO,
                 &AppTaskGUI_Stk[0], (APP_CFG_TASK_GUI_STK_SIZE / 10u),
                  APP_CFG_TASK_GUI_STK_SIZE, 0u, 0u, 0,
                  (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);
    my_assert(OS_ERR_NONE == err);
 
    OSTaskCreate(&startup_task_TCB, "startup task", (OS_TASK_PTR)startup,
                 0, 10,
                 &startup_task_stk[0],(STARTUP_STKSZ/10),STARTUP_STKSZ,
                 0u, 0u, 0,
                 OS_OPT_TASK_NONE, &err);

    // TODO: Start multitasking (i.e. give control to uC/OS-III)
    OSStart(&err);

    // Should never get here
    my_assert(0);
}

