/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include "stm32h5xx_nucleo.h"
#include "connectivity_tester.h"
#include "software_ver.h"
#include "gitversion.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WD_TASK_STACK_SIZE     2048U
#define WD_TASK_PRIORITY       4U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
extern void StartWDTask(ULONG argument);
static VOID App_StackErrorHandler(TX_THREAD *thread_ptr);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */

  /* Trap any thread stack overflow instead of letting it silently corrupt
     adjacent globals (this was corrupting the hspi4 handle). */
  tx_thread_stack_error_notify(App_StackErrorHandler);

  /* Create the watchdog task */
  static TX_THREAD wdTaskHandle;
  static ULONG wdTaskStack[WD_TASK_STACK_SIZE / sizeof(ULONG)];

  const fw_info_t *b1 = BANK1_FW_INFO_ADDR;
  char ver_str[40];

  if (b1->magic == FW_MAGIC) {
      snprintf(ver_str, sizeof(ver_str), "%d.%d.%d", b1->major, b1->minor,b1->patch);

      printf("Software image version: %-8s (%s) %-24s\r\n",
              ver_str, GIT_SHORT_SHA, b1->build_time);
  } else {
      printf("Invalid software version\r\n");
  }

  printf("Starting StartWDTask\r\n");
  if (tx_thread_create(&wdTaskHandle, "wdTask", StartWDTask, 0,
                       wdTaskStack, sizeof(wdTaskStack),
                       WD_TASK_PRIORITY, WD_TASK_PRIORITY,
                       TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS) {
      printf("Failed to create wdTask\r\n");
  }

  ConnectivityTesterInit(); // Start the connectivity tester task


  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/* Made global + volatile so it is visible in a debugger watch/Live Expression
 * and can be checked from anywhere. Non-zero => a stack overflow was detected. */
volatile TX_THREAD *g_stack_error_thread = TX_NULL;
volatile const char *g_stack_error_name  = "none";

/**
  * @brief  Called by ThreadX when a thread overruns its stack.
  * @note   Requires TX_ENABLE_STACK_CHECKING (see tx_user.h).
  *
  * How you KNOW you landed here (instead of a silent HardFault):
  *   1. The RED LED blinks forever (visible with no debugger attached).
  *   2. g_stack_error_name holds the offending thread name
  *      (add it to Live Expressions / Watch, or read it after halting).
  *   3. If a debugger is attached, execution stops on the __BKPT below.
  */
static VOID App_StackErrorHandler(TX_THREAD *thread_ptr)
{
  g_stack_error_thread = thread_ptr;
  g_stack_error_name   = (thread_ptr != TX_NULL) ? thread_ptr->tx_thread_name : "unknown";

  /* Announce the culprit once over UART. Keep it to a single string print:
   * we are already in a fatal state, so avoid heavy formatting. */
  printf("\r\n[FATAL] Stack overflow detected in thread: %s\r\n",
         (g_stack_error_name != NULL) ? (const char *)g_stack_error_name : "unknown");

  /* Stop only if a debugger is connected (avoids a HardFault in the field). */
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
  {
    __BKPT(0);
  }

  // TBD replace the LED off and on with the APIs gpio_led1_on, gpio_led1_off for our HW

  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_YELLOW);

  /* Blink RED forever so the fault is unmistakable without any tooling. */
  for (;;)
  {
    BSP_LED_Toggle(LED_RED);
    /* The LED will toggle every ~100 ms, so: ~100 ms RED on, ~100 ms RED off → ~200 ms full blink cycle. */
    for (volatile uint32_t d = 0; d < 2000000U; d++) { __NOP(); }
  }
}

/* USER CODE END 1 */