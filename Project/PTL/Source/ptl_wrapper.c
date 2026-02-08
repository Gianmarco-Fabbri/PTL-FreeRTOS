/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file ptl_wrapper.c
 * @brief PTL Task Wrapper and Initialization
 * @author Member 2 (Group 38) - Rocco Caliandro
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "ptl_events.h"
#include "uart.h"
#include <string.h>

static PTL_TaskObj_t xTaskPool[PTL_MAX_TASKS];
static UBaseType_t uxRegisteredTaskCount = 0;
static BaseType_t xIsInitialized = pdFALSE;

/* Global config (stored at init) */
static PTL_OverrunPolicy_t eGlobalPolicy = PTL_POLICY_SKIP;
static volatile BaseType_t xTracingEnabled = pdFALSE;
static UBaseType_t uxMaxTasksAllowed = 0;

/* Required for PTL_Start() */
extern BaseType_t PTL_Scheduler_Start(void);

/* Convert uint32 to string without printf */
static void prvUint32ToString(uint32_t num, char* buf)
{
    char temp[12];
    int i = 0;
    
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    int j = 0;
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

/**
 * Generic task wrapper.
 * Waits for scheduler notification, runs user job.
 */
void PTL_GenericWrapper(void* pvParameters)
{
    PTL_TaskObj_t* pxTask = (PTL_TaskObj_t*)pvParameters;
    
    configASSERT(pxTask != NULL);
    configASSERT(pxTask->xConfig.pvEntryFunction != NULL);
    
    
    for (;;)
    {
        /* Wait for notification from scheduler */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        /* Mark active, log start */
        pxTask->xIsActive = pdTRUE;
        TickType_t xJobStartTime = xTaskGetTickCount();
        
        if (xTracingEnabled)
        {
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_START, xJobStartTime);
        }
        
        /* Execute user job body */
        UART_printf("[PTL] Executing: ");
        UART_printf(pxTask->xConfig.pcName);
        UART_printf("\n");
        pxTask->xConfig.pvEntryFunction(pxTask->xConfig.pvParameters);
        
        /* Record finish, log end */
        TickType_t xJobEndTime = xTaskGetTickCount();
        
        if (xTracingEnabled)
        {
            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_COMPLETE, xJobEndTime);
        }

    
        
        /* Mark complete */
        pxTask->xIsActive = pdFALSE;
        pxTask->ulJobsCompleted++;
    }
}

/**
 * Initialize the PTL system.
 * Validates config, creates FreeRTOS tasks with GenericWrapper.
 */
BaseType_t PTL_Init(const PTL_GlobalConfig_t* pxGlobalConfig,
                    const PTL_TaskConfig_t* pxTaskConfigs, 
                    UBaseType_t uxTaskCount)
{
    char numBuf[12];
    
    /* Input validation */
    if (pxGlobalConfig == NULL || pxTaskConfigs == NULL)
    {
        UART_printf("[PTL] Error: NULL config\n");
        return pdFAIL;
    }
    
    if (uxTaskCount == 0 || uxTaskCount > PTL_MAX_TASKS)
    {
        UART_printf("[PTL] Error: Invalid task count\n");
        return pdFAIL;
    }
    
    if (uxTaskCount > pxGlobalConfig->uxMaxTasks)
    {
        UART_printf("[PTL] Error: Exceeds max_tasks\n");
        return pdFAIL;
    }
    
    if (xIsInitialized == pdTRUE)
    {
        UART_printf("[PTL] Error: Already initialized\n");
        return pdFAIL;
    }
    
    /* Store global config */
    eGlobalPolicy = pxGlobalConfig->eOverrunPolicy;
    xTracingEnabled = pxGlobalConfig->xTracingEnabled;
    uxMaxTasksAllowed = pxGlobalConfig->uxMaxTasks;
    
    /* Initialize trace system if enabled */
    if (xTracingEnabled)
    {
        PTL_TraceInit();
    }
    
    UART_printf("[PTL] Initializing ");
    prvUint32ToString(uxTaskCount, numBuf);
    UART_printf(numBuf);
    UART_printf(" tasks...\n");
    
    /* Create wrapper tasks */
    for (UBaseType_t i = 0; i < uxTaskCount; i++)
    {
        PTL_TaskObj_t* pxTask = &xTaskPool[i];
        const PTL_TaskConfig_t* pxConfig = &pxTaskConfigs[i];
        
        if (pxConfig->pvEntryFunction == NULL)
        {
            UART_printf("[PTL] Error: NULL entry function\n");
            return pdFAIL;
        }
        
        /* Copy configuration */
        memcpy(&pxTask->xConfig, pxConfig, sizeof(PTL_TaskConfig_t));
        
        /* If deadline=0, use period */
        if (pxTask->xConfig.xDeadline == 0)
        {
            pxTask->xConfig.xDeadline = pxTask->xConfig.xPeriod;
        }
        
        /* Initialize runtime state */
        pxTask->xTaskHandle         = NULL;
        pxTask->xNextReleaseTime    = 0;  
        pxTask->xCurrentReleaseTime = 0;
        pxTask->xIsActive           = pdFALSE;
        pxTask->xDeadlineMissed     = pdFALSE;
        pxTask->ulJobsCompleted     = 0;
        pxTask->ulDeadlineMisses    = 0;
        pxTask->ulOverrunKills      = 0;
        pxTask->ulOverrunCatchUps   = 0;
        pxTask->ulOverrunSkips      = 0;
        
        /* Create FreeRTOS task */
        BaseType_t xResult = xTaskCreate(
            PTL_GenericWrapper,
            pxConfig->pcName,
            pxConfig->usStackDepth,
            (void*)pxTask,
            pxConfig->uxPriority,
            &pxTask->xTaskHandle
        );
        
        if (xResult != pdPASS)
        {
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

/**
 * Start the PTL scheduler.
 * Called after PTL_Init(), starts the supervisor and FreeRTOS scheduler.
 */
BaseType_t PTL_Start(void)
{
    if (xIsInitialized != pdTRUE)
    {
        UART_printf("[PTL] Error: Not initialized\n");
        return pdFAIL;
    }
    
    UART_printf("[PTL] Starting dispatcher...\n");
    return PTL_Scheduler_Start();
}

BaseType_t PTL_IsTracingEnabled(void)
{
    return xTracingEnabled;
}

PTL_OverrunPolicy_t PTL_GetPolicy(void)
{
    return eGlobalPolicy;
}

PTL_OverrunPolicy_t PTL_GetEffectivePolicy(const PTL_TaskObj_t* pxTask)
{
    if (pxTask == NULL)
    {
        return eGlobalPolicy;
    }
    
    /* If per-task policy is set (not USE_GLOBAL), use it */
    if (pxTask->xConfig.eOverrunPolicy != PTL_POLICY_USE_GLOBAL)
    {
        return pxTask->xConfig.eOverrunPolicy;
    }
    
    /* Otherwise fall back to global policy */
    return eGlobalPolicy;
}

void PTL_GetTaskStats(UBaseType_t uxTaskIndex, uint32_t* pulJobs, 
                      uint32_t* pulMisses, uint32_t* pulOverruns)
{
    if (uxTaskIndex >= uxRegisteredTaskCount)
        return;
    
    PTL_TaskObj_t* pxTask = &xTaskPool[uxTaskIndex];
    
    taskENTER_CRITICAL();
    if (pulJobs)     *pulJobs     = pxTask->ulJobsCompleted;
    if (pulMisses)   *pulMisses   = pxTask->ulDeadlineMisses;
    if (pulOverruns) *pulOverruns = pxTask->ulOverrunSkips + 
                                    pxTask->ulOverrunKills + 
                                    pxTask->ulOverrunCatchUps;
    taskEXIT_CRITICAL();
}

PTL_TaskObj_t* PTL_GetTaskList(UBaseType_t* puxCount)
{
    if (puxCount != NULL)
        *puxCount = uxRegisteredTaskCount;
    return xTaskPool;
}