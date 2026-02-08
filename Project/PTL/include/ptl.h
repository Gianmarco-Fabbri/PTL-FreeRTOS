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
 * @file ptl.h
 * @brief PTL Core Definitions and API.
 *
 * Defines the public API for the Periodic Task Layer (PTL), including
 * configuration structures, initialization functions, and task management.
 *
 * @author Member 2 (Group 38) - Rocco Caliandro
 */

#ifndef PTL_H
#define PTL_H

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------*/
/* PUBLIC DEFINITIONS                                         */
/*-----------------------------------------------------------*/

/** @brief Maximum number of periodic tasks supported. */
#define PTL_MAX_TASKS (8)

/** @brief Maximum length of a task name. */
#define PTL_TASK_NAME_MAX_LEN (configMAX_TASK_NAME_LEN)

/**
 * @brief Overrun policy when a task exceeds its period.
 */
typedef enum {
  PTL_POLICY_USE_GLOBAL = -1, /* Use global policy (default for per-task) */
  PTL_POLICY_SKIP,            /* Skip new job, let late one finish */
  PTL_POLICY_KILL,            /* Terminate running job, start new one */
  PTL_POLICY_CATCH_UP         /* Release now, mark previous as missed */
} PTL_OverrunPolicy_t;

/**
 * @brief Global PTL configuration.
 */
typedef struct {
  PTL_OverrunPolicy_t eOverrunPolicy; /* Global policy for all tasks */
  BaseType_t xTracingEnabled;         /* pdTRUE to enable tracing */
  UBaseType_t uxMaxTasks;             /* Maximum number of tasks */
} PTL_GlobalConfig_t;

/**
 * @brief Per-task configuration provided by user.
 */
typedef struct {
  const char *pcName;     /* Human-readable task name */
  TickType_t xPeriod;     /* Period T in ticks */
  TickType_t xDeadline;   /* Relative deadline D (0 = use period) */
  UBaseType_t uxPriority; /* FreeRTOS priority */
  configSTACK_DEPTH_TYPE usStackDepth; /* Stack size in words */
  void (*pvEntryFunction)(void *);     /* User's job body */
  void *pvParameters;                  /* Argument passed to entry */
  PTL_OverrunPolicy_t eOverrunPolicy;  /* Per-task policy or global */
} PTL_TaskConfig_t;

/**
 * @brief Internal task object tracking runtime state.
 */
typedef struct {
  PTL_TaskConfig_t xConfig;            /* Original user config */
  TaskHandle_t xTaskHandle;            /* FreeRTOS task handle */
  TickType_t xNextReleaseTime;         /* Next R_k+1 tick */
  TickType_t xCurrentReleaseTime;      /* Current R_k tick */
  volatile BaseType_t xIsActive;       /* pdTRUE while job executing */
  volatile BaseType_t xDeadlineMissed; /* pdTRUE if last job missed */
  uint32_t ulJobsCompleted;            /* Total completions */
  uint32_t ulDeadlineMisses;           /* Count of deadline misses */
  uint32_t ulOverrunSkips;             /* SKIP policy count */
  uint32_t ulOverrunKills;             /* KILL policy count */
  uint32_t ulOverrunCatchUps;          /* CATCH_UP policy count */
} PTL_TaskObj_t;

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Initialize the PTL system.
 *
 * @param[in] pxGlobalConfig Pointer to global configuration.
 * @param[in] pxTaskConfigs  Array of per-task configurations.
 * @param[in] uxTaskCount    Number of tasks in the array.
 *
 * @return pdPASS on success, pdFAIL on error.
 */
BaseType_t PTL_Init(const PTL_GlobalConfig_t *pxGlobalConfig,
                    const PTL_TaskConfig_t *pxTaskConfigs,
                    UBaseType_t uxTaskCount);

/**
 * @brief Start the PTL scheduler.
 *
 * Creates supervisor task and starts FreeRTOS scheduler.
 * This function does not return to the caller.
 *
 * @return pdFAIL if initialization failed (otherwise does not return).
 */
BaseType_t PTL_Start(void);

/**
 * @brief Get statistics for a specific task.
 *
 * @param[in]  uxTaskIndex Index of task (0 to count-1).
 * @param[out] pulJobs     Output: total jobs completed.
 * @param[out] pulMisses   Output: total deadline misses.
 * @param[out] pulOverruns Output: total overruns.
 */
void PTL_GetTaskStats(UBaseType_t uxTaskIndex, uint32_t *pulJobs,
                      uint32_t *pulMisses, uint32_t *pulOverruns);

/**
 * @brief Get pointer to internal task list.
 *
 * @param[out] puxCount Output: number of registered tasks.
 *
 * @return Pointer to task pool array.
 */
PTL_TaskObj_t *PTL_GetTaskList(UBaseType_t *puxCount);

/**
 * @brief Check if tracing is enabled.
 *
 * @return pdTRUE if tracing is enabled, pdFALSE otherwise.
 */
BaseType_t PTL_IsTracingEnabled(void);

/**
 * @brief Get the global overrun policy.
 *
 * @return The globally configured overrun policy.
 */
PTL_OverrunPolicy_t PTL_GetPolicy(void);

/**
 * @brief Get effective policy for a task.
 *
 * @param[in] pxTask Pointer to the task object.
 *
 * @return Per-task policy if set, otherwise global policy.
 */
PTL_OverrunPolicy_t PTL_GetEffectivePolicy(const PTL_TaskObj_t *pxTask);

/**
 * @brief Generic task wrapper function.
 *
 * @param[in] pvParameters Pointer to the PTL_TaskObj_t.
 */
void PTL_GenericWrapper(void *pvParameters);

/**
 * @brief Start the supervisor task.
 *
 * @return pdPASS on success, pdFAIL on failure.
 */
BaseType_t PTL_Scheduler_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* PTL_H */