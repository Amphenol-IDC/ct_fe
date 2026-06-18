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
#include "connectivity_tester.h"
#include "software_ver.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define WD_TASK_STACK_SIZE     512U
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

  /* Create the watchdog task */
  static TX_THREAD wdTaskHandle;
  static ULONG wdTaskStack[WD_TASK_STACK_SIZE / sizeof(ULONG)];

  const fw_info_t *b1 = BANK1_FW_INFO_ADDR;
  char ver_str[40];

  if (b1->magic == FW_MAGIC) {
      snprintf(ver_str, sizeof(ver_str), "%d.%d.%d", b1->major, b1->minor,b1->patch);

      printf("Software image version: %-8s %-24s\r\n", ver_str, b1->build_time);
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

/* USER CODE END 1 */
