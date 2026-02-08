/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
 * @file    ptl_scheduler.c
 * @brief   PTL Supervisor & Dispatcher Implementation
 * @author  Member 1 (Bartolini Riccardo) (Group 38)
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "ptl_events.h"
#include "uart.h"
#include "burner.h"
#include <string.h>
#include <stdio.h>

/* * The Supervisor runs at the absolute highest priority to ensure 
 * strict timing enforcement over all other tasks.
 */
#define PTL_SUPERVISOR_PRIORITY   ( configMAX_PRIORITIES - 1 )

/* Stack size for the supervisor */
#define PTL_SUPERVISOR_STACK      ( configMINIMAL_STACK_SIZE * 2 )

/* Required for KILL policy regeneration */
extern void PTL_GenericWrapper(void* pvParameters); 


static void prvPTL_SupervisorTask(void* pvParameters);
static void prvApplyPolicy_Kill(PTL_TaskObj_t* pxTask);


/**
 * Starts the High-Priority Supervisor Task.
 * Called by PTL_Start() in ptl_wrapper.c
 */
BaseType_t PTL_Scheduler_Start(void)
{
    BaseType_t xReturn;

    UART_printf("[SCHEDULER] Creating Supervisor Task...\n");

    xReturn = xTaskCreate(
        prvPTL_SupervisorTask,      
        "PTL_Sup",                  
        PTL_SUPERVISOR_STACK,       
        NULL,                       
        PTL_SUPERVISOR_PRIORITY,    
        NULL                        
    );

    if (xReturn == pdPASS)
    {
        /* Start the FreeRTOS Scheduler (blocking call) */
        vTaskStartScheduler();
    }

    return xReturn;
}

static void prvPTL_SupervisorTask(void* pvParameters)
{
    (void) pvParameters;
    PTL_TaskObj_t* pxTaskList;
    UBaseType_t uxTaskCount;
    TickType_t xNow;
    char pcDebugBuf[64];

    pxTaskList = PTL_GetTaskList(&uxTaskCount);
    /* Initialize timing with current tick */
    /* This pauses the Supervisor for ~100ms to calculate CPU speed. */
    UART_printf("[SCHEDULER] Calibrating System Timer...\n");
    Burn_Calibrate();
    UART_printf("[SCHEDULER] System Calibrated.\n");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xSupervisorPeriod = pdMS_TO_TICKS( 1 );

    /* Alignment at system start time */
    TickType_t xSystemStartTime = xLastWakeTime;
    for (UBaseType_t i = 0; i < uxTaskCount; i++)
        {
        pxTaskList[i].xNextReleaseTime = xSystemStartTime;
        }

    snprintf( pcDebugBuf, sizeof(pcDebugBuf), 
              "[SCHEDULER] Supervisor Running at Priority %d\n", 
              (int)PTL_SUPERVISOR_PRIORITY );

    UART_printf(pcDebugBuf);

    for(;;)
    {  
       vTaskDelayUntil(&xLastWakeTime, xSupervisorPeriod);
       xNow = xTaskGetTickCount();

       
         /* Check all registered periodic tasks */
        for (UBaseType_t i = 0; i < uxTaskCount; i++)
        {
            PTL_TaskObj_t* pxTask = &pxTaskList[i];

            TickType_t xDeadlineAbs = pxTask->xCurrentReleaseTime + pxTask->xConfig.xDeadline;

            /* * Check if time is up, task is still running, and we haven't logged it yet */
            if ( (xNow >= xDeadlineAbs) && 
                 (pxTask->xIsActive == pdTRUE) && 
                 (pxTask->xDeadlineMissed == pdFALSE) )
            {
                /* 1. Update the counter (So the test passes) */
                pxTask->ulDeadlineMisses++;

                pxTask->xDeadlineMissed = pdTRUE; /* Mark as missed to avoid multiple counts for same deadline */

                /* 2. Log the event (For the trace) */
                PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_DEADLINE_MISS, xNow);
            }

            /* Check if it's time to release this task */
            if (xNow >= pxTask->xNextReleaseTime)
            {
                /* Check atomicity: read the state in critical section in order to avoid race condition*/
                BaseType_t xTaskIsStillRunning;
                taskENTER_CRITICAL();
                xTaskIsStillRunning = pxTask->xIsActive;
                taskEXIT_CRITICAL();

                /* Reset the deadline flag for the NEXT job */
                pxTask->xDeadlineMissed = pdFALSE;

                if (xTaskIsStillRunning == pdTRUE)
                {
                    switch (PTL_GetEffectivePolicy(pxTask))
                    {
                        case PTL_POLICY_SKIP:
                            pxTask->ulOverrunSkips++;

                            /* Skip */
                            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_SKIP, xNow);
                            
                            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
                            break;

                        case PTL_POLICY_CATCH_UP:
                            pxTask->ulOverrunCatchUps++; 

                            /* Catch Up*/
                            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_CATCHUP, xNow);
                            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);
                            pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
                            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
                            xTaskNotifyGive(pxTask->xTaskHandle);
                            break;

                        case PTL_POLICY_KILL:
                            pxTask->ulOverrunKills++;
                            /* Kill */
                            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_OVERRUN_KILL, xNow);
                            PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);
                            prvApplyPolicy_Kill(pxTask);

                            pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
                            pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
                            
                            xTaskNotifyGive(pxTask->xTaskHandle);
                            break;
                        default:
                            /* Should not happen, but just in case, release normally */
                            break;
                    }
                } else
                    {
                        /* Release the task normally */
                        PTL_LogEvent(pxTask->xConfig.pcName, PTL_EVENT_RELEASE, xNow);
                        pxTask->xCurrentReleaseTime = pxTask->xNextReleaseTime;
                        pxTask->xNextReleaseTime += pxTask->xConfig.xPeriod;
                        xTaskNotifyGive(pxTask->xTaskHandle);
                    }
            }
        }
    }
}

static void prvApplyPolicy_Kill(PTL_TaskObj_t* pxTask)
{
    /* Delete the overrun task */
    TaskHandle_t xOldHandle = pxTask->xTaskHandle;
    vTaskDelete(xOldHandle);
    
    /* Reset task state */
    pxTask->xIsActive = pdFALSE;
    pxTask->xDeadlineMissed = pdFALSE;
    
    /* Re-create the task */
    BaseType_t xResult = xTaskCreate(
        PTL_GenericWrapper,
        pxTask->xConfig.pcName,
        pxTask->xConfig.usStackDepth,
        (void*)pxTask,
        pxTask->xConfig.uxPriority,
        &pxTask->xTaskHandle
    );
    
    if (xResult != pdPASS)
        {
            UART_printf("[SCHEDULER] FATAL: Failed to resurrect task: ");
            UART_printf(pxTask->xConfig.pcName);
            UART_printf("\n");
        
            /* Hang system */
            for(;;);
        }
}
