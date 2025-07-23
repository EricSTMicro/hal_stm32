/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    zigbee_sys_if.c
  * @author  MCD Application Team
  * @brief   Source file for using Zigbee Layer with a RTOS
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include <zephyr/kernel.h>
#include "main.h"
#include "app_common.h"
#include "app_conf.h"
#include "app_zigbee.h"
#include "zigbee.stm32wba.sys.h"
#include "app_bsp.h"

/* Private defines -----------------------------------------------------------*/
//#define LOG_ZIG_DBG LOG_INFO_APP
#define LOG_ZIG_DBG(...) do{}while(0);

/* USER CODE BEGIN PD */
#define NUM_THREADS 1U
#define STACK_SIZE (4*1024U)
/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
K_THREAD_STACK_ARRAY_DEFINE(tstacksZig, NUM_THREADS, STACK_SIZE);
static struct k_thread tZig[NUM_THREADS];

K_SEM_DEFINE(sem_zigbee, 0U, 1U);
K_SEM_DEFINE(sem_ZigEvnt, 0U, 1U);
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Global variables ----------------------------------------------------------*/
/* USER CODE BEGIN GV */

/* USER CODE END GV */

/* Functions Definition ------------------------------------------------------*/

/**
  * @brief  Zigbee Layer Process Task
  * @param  None
  * @retval None
  */
void Zeph_ZigbeeSys_Process(void* , void* , void*)
{
  while(1)
  {
    k_sem_take(&sem_zigbee, K_FOREVER);
    LOG_ZIG_DBG("ZigbeeSys_Process Entry\n");

    APP_ZIGBEE_Task();

    LOG_ZIG_DBG("ZigbeeSys_Process Out\n");
  }
}

/**
  * @brief  Zigbee Layer Initialisation
  * @param  None
  * @retval None
  */
void ZigbeeSys_Init(void)
{
  k_tid_t tid;

  /* Register tasks */
  tid = k_thread_create(&tZig[0], tstacksZig[0], STACK_SIZE, Zeph_ZigbeeSys_Process, INT_TO_POINTER(1), NULL, NULL, K_PRIO_PREEMPT(16), 0, K_NO_WAIT);
  LOG_ZIG_DBG("ZigbeeSys_Init Thread %x\n", tid);
}

/**
  * @brief  Zigbee Layer Task Resume
  * @param  None
  * @retval None
  */
void ZigbeeSys_Resume(void)
{
  LOG_ZIG_DBG("ZigbeeSys_Resume\n");
  //Not used
}

/**
  * @brief  Zigbee Layer set Task.
  * @param  None
  * @retval None
  */
void ZigbeeSys_SemaphoreSet(void)
{
  LOG_ZIG_DBG("ZigbeeSys_SemaphoreSet\n");
  k_sem_give(&sem_zigbee);
}

/**
  * @brief  Zigbee Layer Task wait. Not used with Sequencer.
  * @param  None
  * @retval None
  */
void ZigbeeSys_SemaphoreWait( void )
{
  LOG_ZIG_DBG("ZigbeeSys_SemaphoreWait\n");
  k_sem_take(&sem_zigbee, K_FOREVER);
}

/**
  * @brief  Zigbee Layer set Event.
  * @param  None
  * @retval None
  */
void ZigbeeSys_EventSet( void )
{
  LOG_ZIG_DBG("ZigbeeSys_EventSet\n");
  k_sem_give(&sem_ZigEvnt);
}

/**
  * @brief  Zigbee Layer wait Event.
  * @param  None
  * @retval None
  */
void ZigbeeSys_EventWait( void )
{
  LOG_ZIG_DBG("ZigbeeSys_EventWait\n");
  k_sem_take(&sem_ZigEvnt, K_FOREVER);
}

