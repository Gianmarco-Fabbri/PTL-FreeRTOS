/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file ptl_wrapper.c
 * @brief PTL Task Wrapper and Initialization.
 *
 * This module provides the generic task wrapper that executes user job
 * functions, and the PTL initialization and configuration API.
 *
 * @author Member 2 (Group 38) - Rocco Caliandro
 */

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "ptl.h"
#include "ptl_events.h"
#include "ptl_trace.h"
#include "uart.h"
#include <string.h>

/*-----------------------------------------------------------*/
/* PRIVATE DATA                                               */
/*-----------------------------------------------------------*/

/** @brief Pool of task objects. */
static PTL_TaskObj_t xTaskPool[PTL_MAX_TASKS];

/** @brief Number of registered tasks. */
static UBaseType_t uxRegisteredTaskCount = 0;

/** @brief Initialization flag. */
static BaseType_t xIsInitialized = pdFALSE;

/** @brief Global overrun policy. */
static PTL_OverrunPolicy_t eGlobalPolicy = PTL_POLICY_SKIP;

/** @brief Tracing enabled flag. */
static volatile BaseType_t xTracingEnabled = pdFALSE;

/** @brief Maximum tasks allowed. */
static UBaseType_t uxMaxTasksAllowed = 0;

/*-----------------------------------------------------------*/
/* EXTERNAL DECLARATIONS                                      */
/*-----------------------------------------------------------*/

/** @brief Scheduler start function (defined in ptl_scheduler.c). */
extern BaseType_t PTL_Scheduler_Start(void);

/*-----------------------------------------------------------*/
/* PRIVATE FUNCTION PROTOTYPES                                */
/*-----------------------------------------------------------*/

static void prvUint32ToString(uint32_t ulNum, char *pcBuf);

/*-----------------------------------------------------------*/
/* PRIVATE FUNCTIONS                                          */
/*-----------------------------------------------------------*/

/**
 * @brief Convert uint32 to string without printf.
 *
 * @param[in]  ulNum  Number to convert.
 * @param[out] pcBuf  Output buffer (must be at least 12 bytes).
 */
static void prvUint32ToString(uint32_t ulNum, char *pcBuf) {
  char cTemp[12];
  int i = 0;
  int j = 0;

  if (ulNum == 0) {
    pcBuf[0] = '0';
    pcBuf[1] = '\0';
    return;
  }

  while (ulNum > 0) {
    cTemp[i++] = '0' + (ulNum % 10);
    ulNum /= 10;
  }

  while (i > 0) {
    pcBuf[j++] = cTemp[--i];
  }
  pcBuf[j] = '\0';
}
/*-----------------------------------------------------------*/

/**
 * @brief Generic task wrapper function.
 *
 * This function is the entry point for all PTL-managed tasks. It waits
 * for release notifications from the supervisor, executes the user's
 * job function, and performs deadline checking.
 *
 * @param[in] pvParameters  Pointer to PTL_TaskObj_t for this task.
 */
void PTL_GenericWrapper(void *pvParameters) {
  PTL_TaskObj_t *pxTask = (PTL_TaskObj_t *)pvParameters;
  TickType_t xEffectiveDeadline;

  configASSERT(pxTask != NULL);
  configASSERT(pxTask->xConfig.pvEntryFunction != NULL);

  /* Effective deadline: if D=0, use T. */
  xEffectiveDeadline = (pxTask->xConfig.xDeadline > 0)
                           ? pxTask->xConfig.xDeadline
                           : pxTask->xConfig.xPeriod;

  for (;;) {
    /* Wait for notification from supervisor. */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    /* Mark active and record start time. */
    pxTask->xIsActive = pdTRUE;
    TickType_t xJobStartTime = xTaskGetTickCount();

    if (xTracingEnabled) {
      PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_START, xJobStartTime);
    }

    /* Execute user job body. */
    UART_printf("[PTL] Executing: ");
    UART_printf(pxTask->xConfig.pcName);
    UART_printf("\n");
    pxTask->xConfig.pvEntryFunction(pxTask->xConfig.pvParameters);

    /* Record finish time. */
    TickType_t xJobEndTime = xTaskGetTickCount();

    if (xTracingEnabled) {
      PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_COMPLETE, xJobEndTime);
    }

    /* Check deadline BEFORE marking inactive. */
    TickType_t xAbsoluteDeadline;
    xAbsoluteDeadline = pxTask->xCurrentReleaseTime + xEffectiveDeadline;

    if (xJobEndTime > xAbsoluteDeadline) {
      pxTask->xDeadlineMissed = pdTRUE;
      pxTask->ulDeadlineMisses++;

      if (xTracingEnabled) {
        PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_DEADLINE_MISS,
                     xJobEndTime);
      }
    }

    /* Mark complete. */
    pxTask->xIsActive = pdFALSE;
    pxTask->ulJobsCompleted++;
  }
}
/*-----------------------------------------------------------*/

/**
 * @brief Initialize the PTL system.
 *
 * Validates configuration, stores global settings, and creates
 * FreeRTOS tasks with the generic wrapper.
 *
 * @param[in] pxGlobalConfig  Global configuration (policy, tracing).
 * @param[in] pxTaskConfigs   Array of task configurations.
 * @param[in] uxTaskCount     Number of tasks to create.
 *
 * @return pdPASS on success, pdFAIL on error.
 */
BaseType_t PTL_Init(const PTL_GlobalConfig_t *pxGlobalConfig,
                    const PTL_TaskConfig_t *pxTaskConfigs,
                    UBaseType_t uxTaskCount) {
  char cNumBuf[12];

  /* Input validation. */
  if ((pxGlobalConfig == NULL) || (pxTaskConfigs == NULL)) {
    UART_printf("[PTL] Error: NULL config\n");
    return pdFAIL;
  }

  if ((uxTaskCount == 0) || (uxTaskCount > PTL_MAX_TASKS)) {
    UART_printf("[PTL] Error: Invalid task count\n");
    return pdFAIL;
  }

  if (uxTaskCount > pxGlobalConfig->uxMaxTasks) {
    UART_printf("[PTL] Error: Exceeds max_tasks\n");
    return pdFAIL;
  }

  if (xIsInitialized == pdTRUE) {
    UART_printf("[PTL] Error: Already initialized\n");
    return pdFAIL;
  }

  /* Store global config. */
  eGlobalPolicy = pxGlobalConfig->eOverrunPolicy;
  xTracingEnabled = pxGlobalConfig->xTracingEnabled;
  uxMaxTasksAllowed = pxGlobalConfig->uxMaxTasks;

  /* Initialize trace system if enabled. */
  if (xTracingEnabled) {
    PTL_TraceInit();
  }

  UART_printf("[PTL] Initializing ");
  prvUint32ToString(uxTaskCount, cNumBuf);
  UART_printf(cNumBuf);
  UART_printf(" tasks...\n");

  /* Create wrapper tasks. */
  for (UBaseType_t i = 0; i < uxTaskCount; i++) {
    PTL_TaskObj_t *pxTask = &xTaskPool[i];
    const PTL_TaskConfig_t *pxConfig = &pxTaskConfigs[i];
    BaseType_t xResult;

    if (pxConfig->pvEntryFunction == NULL) {
      UART_printf("[PTL] Error: NULL entry function\n");
      return pdFAIL;
    }

    /* Copy configuration. */
    memcpy(&pxTask->xConfig, pxConfig, sizeof(PTL_TaskConfig_t));

    /* If deadline=0, use period. */
    if (pxTask->xConfig.xDeadline == 0) {
      pxTask->xConfig.xDeadline = pxTask->xConfig.xPeriod;
    }

    /* Initialize runtime state. */
    pxTask->xTaskHandle = NULL;
    pxTask->xNextReleaseTime = 0;
    pxTask->xCurrentReleaseTime = 0;
    pxTask->xIsActive = pdFALSE;
    pxTask->xDeadlineMissed = pdFALSE;
    pxTask->ulJobsCompleted = 0;
    pxTask->ulDeadlineMisses = 0;
    pxTask->ulOverrunKills = 0;
    pxTask->ulOverrunCatchUps = 0;
    pxTask->ulOverrunSkips = 0;

    /* Create FreeRTOS task. */
    xResult = xTaskCreate(PTL_GenericWrapper, pxConfig->pcName,
                          pxConfig->usStackDepth, (void *)pxTask,
                          pxConfig->uxPriority, &pxTask->xTaskHandle);

    if (xResult != pdPASS) {
      UART_printf("[PTL] Error: Task creation failed\n");
      return pdFAIL;
    }

    UART_printf("[PTL] Created: ");
    UART_printf(pxConfig->pcName);
    UART_printf("\n");
  }

  uxRegisteredTaskCount = uxTaskCount;
  xIsInitialized = pdTRUE;

  UART_printf("[PTL] Init complete\n");
  return pdPASS;
}
/*-----------------------------------------------------------*/

/**
 * @brief Start the PTL scheduler.
 *
 * Must be called after PTL_Init(). Starts the supervisor task
 * and the FreeRTOS scheduler.
 *
 * @return pdPASS on success, pdFAIL if not initialized.
 */
BaseType_t PTL_Start(void) {
  if (xIsInitialized != pdTRUE) {
    UART_printf("[PTL] Error: Not initialized\n");
    return pdFAIL;
  }

  UART_printf("[PTL] Starting dispatcher...\n");
  return PTL_Scheduler_Start();
}
/*-----------------------------------------------------------*/

/**
 * @brief Check if tracing is enabled.
 *
 * @return pdTRUE if tracing enabled, pdFALSE otherwise.
 */
BaseType_t PTL_IsTracingEnabled(void) { return xTracingEnabled; }
/*-----------------------------------------------------------*/

/**
 * @brief Get the global overrun policy.
 *
 * @return The global overrun policy.
 */
PTL_OverrunPolicy_t PTL_GetPolicy(void) { return eGlobalPolicy; }
/*-----------------------------------------------------------*/

/**
 * @brief Get the effective policy for a task.
 *
 * Returns the per-task policy if set, otherwise the global policy.
 *
 * @param[in] pxTask  Pointer to task object.
 *
 * @return The effective overrun policy for this task.
 */
PTL_OverrunPolicy_t PTL_GetEffectivePolicy(const PTL_TaskObj_t *pxTask) {
  if (pxTask == NULL) {
    return eGlobalPolicy;
  }

  /* If per-task policy is set (not USE_GLOBAL), use it. */
  if (pxTask->xConfig.eOverrunPolicy != PTL_POLICY_USE_GLOBAL) {
    return pxTask->xConfig.eOverrunPolicy;
  }

  /* Otherwise fall back to global policy. */
  return eGlobalPolicy;
}
/*-----------------------------------------------------------*/

/**
 * @brief Get statistics for a specific task.
 *
 * @param[in]  uxTaskIndex   Index of task in pool.
 * @param[out] pulJobs       Pointer to store completed job count.
 * @param[out] pulMisses     Pointer to store deadline miss count.
 * @param[out] pulOverruns   Pointer to store total overrun count.
 */
void PTL_GetTaskStats(UBaseType_t uxTaskIndex, uint32_t *pulJobs,
                      uint32_t *pulMisses, uint32_t *pulOverruns) {
  if (uxTaskIndex >= uxRegisteredTaskCount) {
    return;
  }

  PTL_TaskObj_t *pxTask = &xTaskPool[uxTaskIndex];

  taskENTER_CRITICAL();
  {
    if (pulJobs != NULL) {
      *pulJobs = pxTask->ulJobsCompleted;
    }
    if (pulMisses != NULL) {
      *pulMisses = pxTask->ulDeadlineMisses;
    }
    if (pulOverruns != NULL) {
      *pulOverruns = pxTask->ulOverrunSkips + pxTask->ulOverrunKills +
                     pxTask->ulOverrunCatchUps;
    }
  }
  taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/**
 * @brief Get the task list and count.
 *
 * @param[out] puxCount  Pointer to store task count (can be NULL).
 *
 * @return Pointer to the task pool array.
 */
PTL_TaskObj_t *PTL_GetTaskList(UBaseType_t *puxCount) {
  if (puxCount != NULL) {
    *puxCount = uxRegisteredTaskCount;
  }
  return xTaskPool;
}
/*-----------------------------------------------------------*/