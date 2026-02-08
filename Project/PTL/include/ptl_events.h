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
 * @file ptl_events.h
 * @brief PTL Trace Event Type Definitions
 * @author Member 3 (Group 38) - Gianmarco Fabbri
 */

#ifndef PTL_EVENTS_H
#define PTL_EVENTS_H

/**
 * @brief Trace event types for PTL system monitoring.
 *
 * These events are logged by the trace system to provide tick-level
 * precision visibility into task scheduling and timing behavior.
 */
typedef enum
{
    PTL_EVENT_RELEASE = 0,        /* Task released by dispatcher */
    PTL_EVENT_START,              /* Task job execution started */
    PTL_EVENT_COMPLETE,           /* Task job completed successfully */
    PTL_EVENT_DEADLINE_MISS,      /* Task missed its deadline */
    PTL_EVENT_OVERRUN_SKIP,       /* Overrun detected, SKIP policy applied */
    PTL_EVENT_OVERRUN_KILL,       /* Overrun detected, KILL policy applied */
    PTL_EVENT_OVERRUN_CATCHUP,    /* Overrun detected, CATCH_UP policy applied */
    PTL_EVENT_SWITCH_IN,          /* Task switched in by FreeRTOS scheduler */
    PTL_EVENT_SWITCH_OUT,         /* Task switched out by FreeRTOS scheduler */
    PTL_EVENT_IDLE_START,         /* Idle task execution started */
    PTL_EVENT_IDLE_END,           /* Idle task execution ended */
    PTL_EVENT_COUNT               /* Total number of event types */
} PTL_EventType_t;

#endif /* PTL_EVENTS_H */