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
 * @file ptl_trace.c
 * @brief PTL Trace System Implementation
 * @author Member 3 (Group 38) - Gianmarco Fabbri
 */

#include "ptl_trace.h"
#include "uart.h"
#include <string.h>
#include <stdio.h>

/** @brief Circular buffer for trace records (pre-allocated in RAM). */
static PTL_TraceRecord_t xTraceBuffer[ PTL_TRACE_BUFFER_SIZE ];

/** @brief Write index for circular buffer. */
static volatile uint32_t ulWriteIndex = 0;

/** @brief Flag indicating if buffer has wrapped around. */
static BaseType_t xBufferWrapped = pdFALSE;

/** @brief Accumulated idle time in ticks. */
static volatile TickType_t xIdleTimeTotal = 0;

/** @brief Timestamp of last idle task entry. */
static volatile TickType_t xLastIdleEntry = 0;

/** @brief Readable event names (aligned with PTL_EventType_t). */
static const char *pcEventNames[] = {
    "RELEASE",         /* 0 */
    "START",           /* 1 */
    "COMPLETE",        /* 2 */
    "DEADLINE_MISS",   /* 3 */
    "OVERRUN_SKIP",    /* 4 */
    "OVERRUN_KILL",    /* 5 */
    "OVERRUN_CATCHUP", /* 6 */
    "SWITCH_IN",       /* 7 */
    "SWITCH_OUT",      /* 8 */
    "IDLE_START",      /* 9 */
    "IDLE_END"         /* 10 */
};

void PTL_TraceInit( void )
{
    ulWriteIndex = 0;
    xBufferWrapped = pdFALSE;
    xIdleTimeTotal = 0;
    xLastIdleEntry = 0;
}

void PTL_LogEvent( const char *pcTaskName, PTL_EventType_t eType, TickType_t xTime )
{
    /* Called from trace hook - this is a ISR-safe version */
    UBaseType_t uxSavedInterruptStatus;
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    {
        xTraceBuffer[ ulWriteIndex ].pcTaskName = pcTaskName;
        xTraceBuffer[ ulWriteIndex ].eEvent = eType;
        xTraceBuffer[ ulWriteIndex ].xTimestamp = xTime;

        ulWriteIndex++;
        if( ulWriteIndex >= PTL_TRACE_BUFFER_SIZE )
        {
            ulWriteIndex = 0;
            xBufferWrapped = pdTRUE;
        }
    }
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
}

void PTL_PrintTrace( void )
{
    uint32_t i, ulCount, ulStartIndex;
    char pcBuffer[ 256 ];

    taskENTER_CRITICAL();
    if( xBufferWrapped == pdTRUE )
    {
        ulCount = PTL_TRACE_BUFFER_SIZE;
        ulStartIndex = ulWriteIndex;
    }
    else
    {
        ulCount = ulWriteIndex;
        ulStartIndex = 0;
    }
    taskEXIT_CRITICAL();

    UART_printf( "\n===== PTL TRACE =====\n" );

    for( i = 0; i < ulCount; i++ )
    {
        uint32_t ulIdx = ( ulStartIndex + i ) % PTL_TRACE_BUFFER_SIZE;
        PTL_TraceRecord_t *pxRec = &xTraceBuffer[ ulIdx ];

        const char *safeName = ( pxRec->pcTaskName ) ? pxRec->pcTaskName : "SYS";
        const char *eventStr = ( pxRec->eEvent < PTL_EVENT_COUNT ) ?
                               pcEventNames[ pxRec->eEvent ] : "UNKNOWN";

        snprintf( pcBuffer, sizeof( pcBuffer ), "[%4u] %s %s\n",
                  ( unsigned int ) pxRec->xTimestamp, safeName, eventStr );
        UART_printf( pcBuffer );
    }

    UART_printf( "======================================\n" );
}

void PTL_TrackIdleEntry( TickType_t xTime )
{
    /* Called from trace hook - this is a ISR-safe version */
    UBaseType_t uxSavedInterruptStatus;
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    xLastIdleEntry = xTime;
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
    
    PTL_LogEvent( "IDLE", PTL_EVENT_IDLE_START, xTime );
}

void PTL_TrackIdleExit( TickType_t xTime )
{
    /* Called from trace hook - this is a ISR-safe version */
    UBaseType_t uxSavedInterruptStatus;
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    
    if( xTime >= xLastIdleEntry )
    {
        xIdleTimeTotal += ( xTime - xLastIdleEntry );
    }
    
    taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
    
    PTL_LogEvent( "IDLE", PTL_EVENT_IDLE_END, xTime );
}


void PTL_GetTraceStatistics( PTL_TraceStats_t *pxStats )
{
    uint32_t i, ulCount, ulStartIndex;

    if( pxStats == NULL )
    {
        return;
    }

    memset( pxStats, 0, sizeof( PTL_TraceStats_t ) );

    taskENTER_CRITICAL();
    if( xBufferWrapped == pdTRUE )
    {
        ulCount = PTL_TRACE_BUFFER_SIZE;
        ulStartIndex = ulWriteIndex;
    }
    else
    {
        ulCount = ulWriteIndex;
        ulStartIndex = 0;
    }

    if( ulCount > 0 )
    {
        uint32_t ulLastIdx = ( ulStartIndex + ulCount - 1 ) % PTL_TRACE_BUFFER_SIZE;
        pxStats->ulTotalTimeMs = xTraceBuffer[ ulLastIdx ].xTimestamp;
    }

    pxStats->ulIdleTimeMs = xIdleTimeTotal;
    taskEXIT_CRITICAL();

    for( i = 0; i < ulCount; i++ )
    {
        uint32_t ulIdx = ( ulStartIndex + i ) % PTL_TRACE_BUFFER_SIZE;
        PTL_TraceRecord_t *pxRec = &xTraceBuffer[ ulIdx ];

        switch( pxRec->eEvent )
        {
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
                break;
        }
    }

    if( pxStats->ulTotalTimeMs > 0 )
    {
        uint32_t ulActiveTicks = pxStats->ulTotalTimeMs - pxStats->ulIdleTimeMs;
        pxStats->fCPUUtilization = ( ( float ) ulActiveTicks / ( float ) pxStats->ulTotalTimeMs ) * 100.0f;
    }
}

void PTL_PrintStatistics( void )
{
    PTL_TraceStats_t xStats;
    char pcBuffer[ 128 ];

    PTL_GetTraceStatistics( &xStats );

    UART_printf( "\n====== PTL STATISTICS ======\n" );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Total Releases:     %u\n",
              ( unsigned int ) xStats.ulTotalReleases );
    UART_printf( pcBuffer );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Total Completions:  %u\n",
              ( unsigned int ) xStats.ulTotalCompletions );
    UART_printf( pcBuffer );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Deadline Misses:    %u\n",
              ( unsigned int ) xStats.ulDeadlineMisses );
    UART_printf( pcBuffer );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Overruns:           %u\n",
              ( unsigned int ) xStats.ulOverrunCount );
    UART_printf( pcBuffer );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Total Time:         %u ms\n",
              ( unsigned int ) xStats.ulTotalTimeMs );
    UART_printf( pcBuffer );

    snprintf( pcBuffer, sizeof( pcBuffer ), "Idle Time:          %u ms\n",
              ( unsigned int ) xStats.ulIdleTimeMs );
    UART_printf( pcBuffer );

    /* Convert float to integer percentage */
    uint32_t ulCpuUtilPct = ( uint32_t )( xStats.fCPUUtilization * 100.0f );
    snprintf( pcBuffer, sizeof( pcBuffer ), "CPU Utilization:    %u.%02u%%\n",
              ( unsigned int )( ulCpuUtilPct / 100 ), 
              ( unsigned int )( ulCpuUtilPct % 100 ) );
    UART_printf( pcBuffer );

    /* Calculate overhead and format entire message with snprintf */
    uint32_t ulOverheadPct = 10000 - ulCpuUtilPct;
    
    if( ulOverheadPct <= 1000 ) /* 10.00% */
    {
        snprintf( pcBuffer, sizeof( pcBuffer ), "System Overhead:    %u.%02u%% [OK]\n",
                  ( unsigned int )( ulOverheadPct / 100 ), 
                  ( unsigned int )( ulOverheadPct % 100 ) );
    }
    else
    {
        snprintf( pcBuffer, sizeof( pcBuffer ), "System Overhead:    %u.%02u%% [FAIL - Required <=10%%]\n",
                  ( unsigned int )( ulOverheadPct / 100 ), 
                  ( unsigned int )( ulOverheadPct % 100 ) );
    }
    UART_printf( pcBuffer );

    UART_printf( "============================\n\n" );
}

void vApplicationIdleHook( void )
{
    /* Called repeatedly during idle - no action needed here.
     * Idle time tracking is handled in PTL_TrackIdleEntry/Exit(). */
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    ( void ) xTask;
    
    UART_printf( "[FATAL] Stack Overflow: " );
    UART_printf( pcTaskName );
    UART_printf( "\n" );
    
    for( ;; ){}
}
