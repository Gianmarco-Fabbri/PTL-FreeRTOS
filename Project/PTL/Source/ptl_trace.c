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
 * @file ptl_trace.c
 * @brief PTL Trace and Monitoring System Implementation.
 *
 * This module provides tick-level precision logging for task scheduling
 * events, deadline misses, overruns, and CPU idle time tracking.
 *
 * @author Member 3 (Group 38) - Gianmarco Fabbri
 */

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "ptl_trace.h"
#include "uart.h"
#include <stdio.h>
#include <string.h>

/*-----------------------------------------------------------*/
/* PRIVATE DATA                                               */
/*-----------------------------------------------------------*/

/** @brief Circular buffer for trace records (pre-allocated in RAM). */
static PTL_TraceRecord_t xTraceBuffer[PTL_TRACE_BUFFER_SIZE];

/** @brief Write index for circular buffer. */
static volatile uint32_t ulWriteIndex = 0;

/** @brief Flag indicating if buffer has wrapped around. */
static BaseType_t xBufferWrapped = pdFALSE;

/** @brief Accumulated idle time in ticks. */
static volatile TickType_t xIdleTimeTotal = 0;

/** @brief Timestamp of last idle task entry. */
static volatile TickType_t xLastIdleEntry = 0;

/** @brief Readable event names (aligned with PTL_EventType_t). */
static const char *const pcEventNames[] = {
    "RELEASE",         /* PTL_EVENT_RELEASE        */
    "START",           /* PTL_EVENT_START          */
    "COMPLETE",        /* PTL_EVENT_COMPLETE       */
    "DEADLINE_MISS",   /* PTL_EVENT_DEADLINE_MISS  */
    "OVERRUN_SKIP",    /* PTL_EVENT_OVERRUN_SKIP   */
    "OVERRUN_KILL",    /* PTL_EVENT_OVERRUN_KILL   */
    "OVERRUN_CATCHUP", /* PTL_EVENT_OVERRUN_CATCHUP*/
    "SWITCH_IN",       /* PTL_EVENT_SWITCH_IN      */
    "SWITCH_OUT",      /* PTL_EVENT_SWITCH_OUT     */
    "IDLE_START",      /* PTL_EVENT_IDLE_START     */
    "IDLE_END"         /* PTL_EVENT_IDLE_END       */
};

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Initialize the trace system.
 *
 * Resets the circular buffer, write index, and idle time counters.
 * Must be called before any other trace functions.
 */
void PTL_TraceInit(void) {
  ulWriteIndex = 0;
  xBufferWrapped = pdFALSE;
  xIdleTimeTotal = 0;
  xLastIdleEntry = 0;
}
/*-----------------------------------------------------------*/

/**
 * @brief Log a trace event to the circular buffer.
 *
 * This function is ISR-safe and uses critical sections to protect
 * shared data. Events are stored with timestamp for later analysis.
 *
 * @param[in] pcTaskName  Name of the task generating the event.
 * @param[in] eType       Type of event (see PTL_EventType_t).
 * @param[in] xTime       Timestamp in ticks when event occurred.
 */
void PTL_LogEvent(const char *pcTaskName, PTL_EventType_t eType,
                  TickType_t xTime) {
  UBaseType_t uxSavedInterruptStatus;

  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  {
    xTraceBuffer[ulWriteIndex].pcTaskName = pcTaskName;
    xTraceBuffer[ulWriteIndex].eEvent = eType;
    xTraceBuffer[ulWriteIndex].xTimestamp = xTime;

    ulWriteIndex++;

    if (ulWriteIndex >= PTL_TRACE_BUFFER_SIZE) {
      ulWriteIndex = 0;
      xBufferWrapped = pdTRUE;
    }
  }
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}
/*-----------------------------------------------------------*/

/**
 * @brief Print the trace buffer contents to UART.
 *
 * Outputs all logged events in chronological order with timestamps.
 * Events from PTL_Sup task (SWITCH_IN/OUT) are filtered to reduce noise.
 */
void PTL_PrintTrace(void) {
  uint32_t i, ulCount, ulStartIndex;
  char pcBuffer[64];

  taskENTER_CRITICAL();
  {
    if (xBufferWrapped == pdTRUE) {
      ulCount = PTL_TRACE_BUFFER_SIZE;
      ulStartIndex = ulWriteIndex;
    } else {
      ulCount = ulWriteIndex;
      ulStartIndex = 0;
    }
  }
  taskEXIT_CRITICAL();

  UART_printf("\n===== PTL TRACE =====\n");

  for (i = 0; i < ulCount; i++) {
    uint32_t ulIdx = (ulStartIndex + i) % PTL_TRACE_BUFFER_SIZE;
    PTL_TraceRecord_t *pxRec = &xTraceBuffer[ulIdx];

    const char *pcSafeName =
        (pxRec->pcTaskName != NULL) ? pxRec->pcTaskName : "SYS";

    const char *pcEventStr = (pxRec->eEvent < PTL_EVENT_COUNT)
                                 ? pcEventNames[pxRec->eEvent]
                                 : "UNKNOWN";

    /* Filter out PTL_Sup SWITCH_IN/SWITCH_OUT to reduce noise. */
    if (pxRec->pcTaskName != NULL) {
      if ((pxRec->pcTaskName[0] == 'P') && (pxRec->pcTaskName[1] == 'T') &&
          (pxRec->pcTaskName[2] == 'L') &&
          ((pxRec->eEvent == PTL_EVENT_SWITCH_IN) ||
           (pxRec->eEvent == PTL_EVENT_SWITCH_OUT))) {
        continue; /* Skip noisy supervisor events */
      }
    }

    snprintf(pcBuffer, sizeof(pcBuffer), "[%5u ms] %-10s %s\n",
             (unsigned int)pxRec->xTimestamp, pcSafeName, pcEventStr);
    UART_printf(pcBuffer);
  }

  UART_printf("======================================\n");
}
/*-----------------------------------------------------------*/

/**
 * @brief Record idle task entry for CPU utilization tracking.
 *
 * Called from traceTASK_SWITCHED_IN when idle task starts.
 *
 * @param[in] xTime  Current tick count.
 */
void PTL_TrackIdleEntry(TickType_t xTime) {
  UBaseType_t uxSavedInterruptStatus;

  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  {
    xLastIdleEntry = xTime;
  }
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

  PTL_LogEvent("IDLE", PTL_EVENT_IDLE_START, xTime);
}
/*-----------------------------------------------------------*/

/**
 * @brief Record idle task exit and accumulate idle time.
 *
 * Called from traceTASK_SWITCHED_OUT when idle task ends.
 *
 * @param[in] xTime  Current tick count.
 */
void PTL_TrackIdleExit(TickType_t xTime) {
  UBaseType_t uxSavedInterruptStatus;

  uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
  {
    if (xTime >= xLastIdleEntry) {
      xIdleTimeTotal += (xTime - xLastIdleEntry);
    }
  }
  taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

  PTL_LogEvent("IDLE", PTL_EVENT_IDLE_END, xTime);
}
/*-----------------------------------------------------------*/

/**
 * @brief Calculate and populate trace statistics.
 *
 * Iterates through the trace buffer to count releases, completions,
 * deadline misses, and overruns. Also calculates CPU utilization.
 *
 * @param[out] pxStats  Pointer to statistics structure to populate.
 */
void PTL_GetTraceStatistics(PTL_TraceStats_t *pxStats) {
  uint32_t i, ulCount, ulStartIndex;

  if (pxStats == NULL) {
    return;
  }

  memset(pxStats, 0, sizeof(PTL_TraceStats_t));

  taskENTER_CRITICAL();
  {
    if (xBufferWrapped == pdTRUE) {
      ulCount = PTL_TRACE_BUFFER_SIZE;
      ulStartIndex = ulWriteIndex;
    } else {
      ulCount = ulWriteIndex;
      ulStartIndex = 0;
    }

    if (ulCount > 0) {
      uint32_t ulLastIdx;
      ulLastIdx = (ulStartIndex + ulCount - 1) % PTL_TRACE_BUFFER_SIZE;
      pxStats->ulTotalTimeMs = xTraceBuffer[ulLastIdx].xTimestamp;
    }

    pxStats->ulIdleTimeMs = xIdleTimeTotal;
  }
  taskEXIT_CRITICAL();

  /* Count events from buffer */
  for (i = 0; i < ulCount; i++) {
    uint32_t ulIdx = (ulStartIndex + i) % PTL_TRACE_BUFFER_SIZE;
    PTL_TraceRecord_t *pxRec = &xTraceBuffer[ulIdx];

    switch (pxRec->eEvent) {
    case PTL_EVENT_RELEASE:
      pxStats->ulTotalReleases++;
      break;

    case PTL_EVENT_COMPLETE:
      pxStats->ulTotalCompletions++;
      break;

    case PTL_EVENT_DEADLINE_MISS:
      pxStats->ulDeadlineMisses++;
      break;

    case PTL_EVENT_OVERRUN_SKIP:
    case PTL_EVENT_OVERRUN_KILL:
    case PTL_EVENT_OVERRUN_CATCHUP:
      pxStats->ulOverrunCount++;
      break;

    default:
      /* Other events not counted */
      break;
    }
  }

  /* Calculate CPU utilization */
  if (pxStats->ulTotalTimeMs > 0) {
    uint32_t ulActiveTime = pxStats->ulTotalTimeMs - pxStats->ulIdleTimeMs;
    pxStats->fCPUUtilization =
        (float)ulActiveTime / (float)pxStats->ulTotalTimeMs;
  } else {
    pxStats->fCPUUtilization = 0.0f;
  }
}
/*-----------------------------------------------------------*/

/**
 * @brief Print formatted statistics to UART.
 *
 * Displays releases, completions, deadline misses, overruns,
 * CPU utilization, and system overhead assessment.
 */
void PTL_PrintStatistics(void) {
  PTL_TraceStats_t xStats;
  char pcBuffer[80];

  PTL_GetTraceStatistics(&xStats);

  UART_printf("\n====== PTL STATISTICS ======\n");

  snprintf(pcBuffer, sizeof(pcBuffer), "Total Releases:     %u\n",
           (unsigned int)xStats.ulTotalReleases);
  UART_printf(pcBuffer);

  snprintf(pcBuffer, sizeof(pcBuffer), "Total Completions:  %u\n",
           (unsigned int)xStats.ulTotalCompletions);
  UART_printf(pcBuffer);

  snprintf(pcBuffer, sizeof(pcBuffer), "Deadline Misses:    %u\n",
           (unsigned int)xStats.ulDeadlineMisses);
  UART_printf(pcBuffer);

  snprintf(pcBuffer, sizeof(pcBuffer), "Overruns:           %u\n",
           (unsigned int)xStats.ulOverrunCount);
  UART_printf(pcBuffer);

  snprintf(pcBuffer, sizeof(pcBuffer), "Total Time:         %u ms\n",
           (unsigned int)xStats.ulTotalTimeMs);
  UART_printf(pcBuffer);

  snprintf(pcBuffer, sizeof(pcBuffer), "Idle Time:          %u ms\n",
           (unsigned int)xStats.ulIdleTimeMs);
  UART_printf(pcBuffer);

  /* Format CPU utilization: fCPUUtilization is 0.0-1.0, we need XX.YY% */
  uint32_t ulCpuUtilPct = (uint32_t)(xStats.fCPUUtilization * 10000.0f);

  snprintf(pcBuffer, sizeof(pcBuffer), "CPU Utilization:    %u.%02u%%\n",
           (unsigned int)(ulCpuUtilPct / 100),
           (unsigned int)(ulCpuUtilPct % 100));
  UART_printf(pcBuffer);

  /* Overhead calculation - only valid for high-load scenarios */
  uint32_t ulActiveTime = xStats.ulTotalTimeMs - xStats.ulIdleTimeMs;

  if ((ulActiveTime > 0) && (ulCpuUtilPct >= 5000)) {
    uint32_t ulOverheadPct = 10000 - ulCpuUtilPct;

    if (ulOverheadPct <= 1000) {
      snprintf(pcBuffer, sizeof(pcBuffer),
               "System Overhead:    %u.%02u%% [OK]\n",
               (unsigned int)(ulOverheadPct / 100),
               (unsigned int)(ulOverheadPct % 100));
    } else {
      snprintf(pcBuffer, sizeof(pcBuffer),
               "System Overhead:    %u.%02u%% [FAIL - Required <=10%%]\n",
               (unsigned int)(ulOverheadPct / 100),
               (unsigned int)(ulOverheadPct % 100));
    }
  } else {
    snprintf(pcBuffer, sizeof(pcBuffer),
             "System Overhead:    N/A (low CPU load)\n");
  }
  UART_printf(pcBuffer);

  UART_printf("============================\n\n");
}
/*-----------------------------------------------------------*/

/**
 * @brief Application idle hook (FreeRTOS callback).
 *
 * Called repeatedly when no other tasks are running.
 * Idle time tracking is handled in PTL_TrackIdleEntry/Exit.
 */
void vApplicationIdleHook(void) {
  /* No action needed - tracking done in trace hooks */
}
/*-----------------------------------------------------------*/

/**
 * @brief Stack overflow hook (FreeRTOS callback).
 *
 * Called when a stack overflow is detected.
 *
 * @param[in] xTask      Handle of the offending task.
 * @param[in] pcTaskName Name of the offending task.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  (void)xTask;

  UART_printf("[FATAL] Stack Overflow: ");
  UART_printf(pcTaskName);
  UART_printf("\n");

  for (;;) {
    /* Halt system on stack overflow */
  }
}
/*-----------------------------------------------------------*/
