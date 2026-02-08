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
 * @file ptl_trace.h
 * @brief PTL Trace System API.
 *
 * Provides tick-level precision trace logging and statistics for the
 * Periodic Task Layer. Supports deadline monitoring, overrun detection,
 * and CPU utilization analysis.
 *
 * @author Member 3 (Group 38) - Gianmarco Fabbri
 */

#ifndef PTL_TRACE_H
#define PTL_TRACE_H

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "ptl_events.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------*/
/* PUBLIC DEFINITIONS                                         */
/*-----------------------------------------------------------*/

/** @brief Size of circular trace buffer. */
#define PTL_TRACE_BUFFER_SIZE (1024)

/**
 * @brief Single trace record in circular buffer.
 */
typedef struct {
  const char *pcTaskName; /* Task name (NULL for system events) */
  PTL_EventType_t eEvent; /* Event type identifier */
  TickType_t xTimestamp;  /* Timestamp in ticks */
} PTL_TraceRecord_t;

/**
 * @brief Aggregate statistics extracted from trace buffer.
 */
typedef struct {
  uint32_t ulTotalReleases;    /* Total task releases */
  uint32_t ulTotalCompletions; /* Total successful completions */
  uint32_t ulDeadlineMisses;   /* Total deadline misses */
  uint32_t ulOverrunCount;     /* Total overrun events */
  uint32_t ulIdleTimeMs;       /* Total idle time in milliseconds */
  uint32_t ulTotalTimeMs;      /* Total system runtime in ms */
  float fCPUUtilization;       /* CPU utilization percentage (0-1.0) */
} PTL_TraceStats_t;

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Initialize the trace system.
 *
 * Resets all internal buffers and counters. Must be called before
 * the scheduler starts.
 */
void PTL_TraceInit(void);

/**
 * @brief Log a trace event to the circular buffer.
 *
 * Thread-safe, can be called from ISRs and kernel hooks.
 *
 * @param[in] pcTaskName Name of the task or NULL.
 * @param[in] eType      Event type from PTL_EventType_t.
 * @param[in] xTime      Current tick count.
 */
void PTL_LogEvent(const char *pcTaskName, PTL_EventType_t eType,
                  TickType_t xTime);

/**
 * @brief Print full trace log to UART.
 *
 * Outputs all recorded events in chronological order.
 */
void PTL_PrintTrace(void);

/**
 * @brief Get aggregate statistics from trace buffer.
 *
 * @param[out] pxStats Pointer to structure to fill with statistics.
 */
void PTL_GetTraceStatistics(PTL_TraceStats_t *pxStats);

/**
 * @brief Print formatted statistics summary to UART.
 *
 * Includes pass/fail check for overhead and CPU utilization data.
 */
void PTL_PrintStatistics(void);

/**
 * @brief Track idle task entry (called from FreeRTOS trace hook).
 *
 * @param[in] xTime Current tick count.
 */
void PTL_TrackIdleEntry(TickType_t xTime);

/**
 * @brief Track idle task exit (called from FreeRTOS trace hook).
 *
 * @param[in] xTime Current tick count.
 */
void PTL_TrackIdleExit(TickType_t xTime);

/**
 * @brief FreeRTOS idle hook for cycle counting.
 */
void vApplicationIdleHook(void);

#ifdef __cplusplus
}
#endif

#endif /* PTL_TRACE_H */