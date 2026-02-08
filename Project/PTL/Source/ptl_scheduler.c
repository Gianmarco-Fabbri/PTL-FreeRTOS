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
 * @file ptl_scheduler.c
 * @brief PTL Supervisor and Dispatcher Implementation.
 *
 * This module implements the high-priority supervisor task that manages
 * periodic task releases, deadline checking, and overrun policy enforcement.
 *
 * @author Member 1 (Group 38) - Bartolini Riccardo
 */

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "burner.h"
#include "ptl.h"
#include "ptl_events.h"
#include "ptl_trace.h"
#include "uart.h"
#include <stdio.h>
#include <string.h>

/*-----------------------------------------------------------*/
/* PRIVATE DEFINITIONS                                        */
/*-----------------------------------------------------------*/

/**
 * @brief Supervisor priority - highest in system.
 *
 * The supervisor runs at the absolute highest priority to ensure
 * strict timing enforcement over all other tasks.
 */
#define PTL_SUPERVISOR_PRIORITY (configMAX_PRIORITIES - 1)

/** @brief Stack size for the supervisor task. */
#define PTL_SUPERVISOR_STACK (configMINIMAL_STACK_SIZE * 2)

/** @brief Supervisor wake-up period in milliseconds (1 tick). */
#define PTL_SUPERVISOR_PERIOD_MS (1)

/*-----------------------------------------------------------*/
/* EXTERNAL DECLARATIONS                                      */
/*-----------------------------------------------------------*/

/** @brief Generic wrapper function (defined in ptl_wrapper.c). */
extern void PTL_GenericWrapper(void *pvParameters);

/*-----------------------------------------------------------*/
/* PRIVATE FUNCTION PROTOTYPES                                */
/*-----------------------------------------------------------*/

static void prvPTL_SupervisorTask(void *pvParameters);
static void prvApplyPolicy_Kill(PTL_TaskObj_t *pxTask);

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Start the PTL scheduler.
 *
 * Creates the high-priority supervisor task and starts the FreeRTOS
 * scheduler. This function does not return under normal operation.
 *
 * @return pdPASS if supervisor task created successfully, pdFAIL otherwise.
 */
BaseType_t PTL_Scheduler_Start(void) {
  BaseType_t xReturn;

  UART_printf("[SCHEDULER] Creating Supervisor Task...\n");

  xReturn = xTaskCreate(prvPTL_SupervisorTask, "PTL_Sup", PTL_SUPERVISOR_STACK,
                        NULL, PTL_SUPERVISOR_PRIORITY, NULL);

  if (xReturn == pdPASS) {
    /* Start the FreeRTOS scheduler (blocking call). */
    vTaskStartScheduler();
  }

  return xReturn;
}

/*-----------------------------------------------------------*/
/* PRIVATE FUNCTIONS                                          */
/*-----------------------------------------------------------*/

/**
 * @brief Main supervisor task loop.
 *
 * Runs every tick to check for task releases, deadline misses,
 * and applies overrun policies (SKIP, KILL, CATCH_UP) as needed.
 *
 * @param[in] pvParameters  Unused.
 */
static void prvPTL_SupervisorTask(void *pvParameters) {
  (void)pvParameters;

  PTL_TaskObj_t *pxTaskList;
  UBaseType_t uxTaskCount;
  TickType_t xNow;
  char pcDebugBuf[64];

  pxTaskList = PTL_GetTaskList(&uxTaskCount);

  /* Calibrate CPU timing (~100ms pause). */
  UART_printf("[SCHEDULER] Calibrating System Timer...\n");
  Burn_Calibrate();
  UART_printf("[SCHEDULER] System Calibrated.\n");

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xSupervisorPeriod = pdMS_TO_TICKS(PTL_SUPERVISOR_PERIOD_MS);

  /* Align all tasks to start at current time. */
  TickType_t xSystemStartTime = xLastWakeTime;
  for (UBaseType_t i = 0; i < uxTaskCount; i++) {
    pxTaskList[i].xNextReleaseTime = xSystemStartTime;
  }

  snprintf(pcDebugBuf, sizeof(pcDebugBuf),
           "[SCHEDULER] Supervisor Running at Priority %d\n",
           (int)PTL_SUPERVISOR_PRIORITY);
  UART_printf(pcDebugBuf);

  /* Main supervisor loop. */
  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, xSupervisorPeriod);
    xNow = xTaskGetTickCount();

    /* Check all registered periodic tasks. */
    for (UBaseType_t i = 0; i < uxTaskCount; i++) {
      PTL_TaskObj_t *pxTask = &pxTaskList[i];
      TickType_t xDeadlineAbs;

      xDeadlineAbs = pxTask->xCurrentReleaseTime + pxTask->xConfig.xDeadline;

      /* Check deadline miss (task active past deadline). */
      if ((xNow >= xDeadlineAbs) && (pxTask->xIsActive == pdTRUE) &&
          (pxTask->xDeadlineMissed == pdFALSE)) {
        pxTask->ulDeadlineMisses++;
        pxTask->xDeadlineMissed = pdTRUE;
        PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_DEADLINE_MISS, xNow);
      }

      /* Check if it's time to release this task. */
      if (xNow >= pxTask->xNextReleaseTime) {
        BaseType_t xTaskIsStillRunning;

        /* Read state atomically to avoid race condition. */
        taskENTER_CRITICAL();
        xTaskIsStillRunning = pxTask->xIsActive;
        taskEXIT_CRITICAL();

        /* Reset deadline flag for next job. */
        pxTask->xDeadlineMissed = pdFALSE;

        if (xTaskIsStillRunning == pdTRUE) {
          /* Apply overrun policy. */
          switch (PTL_GetEffectivePolicy(pxTask)) {
          case PTL_POLICY_SKIP:
            pxTask->ulOverrunSkips++;
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_SKIP, xNow);
            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
            break;

          case PTL_POLICY_CATCH_UP:
            pxTask->ulOverrunCatchUps++;
            pxTask->xDeadlineMissed = pdTRUE;
            pxTask->ulDeadlineMisses++;

            taskENTER_CRITICAL();
            pxTask->xIsActive = pdFALSE;
            taskEXIT_CRITICAL();

            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_CATCHUP,
                         xNow);
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);

            pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
            xTaskNotifyGive(pxTask->xTaskHandle);
            break;

          case PTL_POLICY_KILL:
            pxTask->ulOverrunKills++;
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_KILL, xNow);
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);

            prvApplyPolicy_Kill(pxTask);

            pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
            xTaskNotifyGive(pxTask->xTaskHandle);
            break;

          default:
            /* Unknown policy - do nothing. */
            break;
          }
        } else {
          /* Normal release. */
          PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);
          pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
          pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
          xTaskNotifyGive(pxTask->xTaskHandle);
        }
      }
    }
  }
}
/*-----------------------------------------------------------*/

/**
 * @brief Apply KILL policy to an overrunning task.
 *
 * Deletes the current task instance and recreates it fresh.
 *
 * @param[in] pxTask  Pointer to the task object to kill and resurrect.
 */
static void prvApplyPolicy_Kill(PTL_TaskObj_t *pxTask) {
  TaskHandle_t xOldHandle = pxTask->xTaskHandle;
  BaseType_t xResult;

  /* Delete the overrun task. */
  vTaskDelete(xOldHandle);

  /* Reset task state. */
  pxTask->xIsActive = pdFALSE;
  pxTask->xDeadlineMissed = pdFALSE;

  /* Recreate the task. */
  xResult = xTaskCreate(PTL_GenericWrapper, pxTask->xConfig.pcName,
                        pxTask->xConfig.usStackDepth, (void *)pxTask,
                        pxTask->xConfig.uxPriority, &pxTask->xTaskHandle);

  if (xResult != pdPASS) {
    UART_printf("[SCHEDULER] FATAL: Failed to resurrect task: ");
    UART_printf(pxTask->xConfig.pcName);
    UART_printf("\n");

    /* Halt system on fatal error. */
    for (;;) {
      /* Infinite loop. */
    }
  }
}
/*-----------------------------------------------------------*/
