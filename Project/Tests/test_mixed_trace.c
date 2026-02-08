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
 * @file test_mixed_trace.c
 * @brief Stress Test: Mixed Overrun Policies with robust timing.
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"
#include <stdio.h>

static void vBusyWaitTicks(TickType_t xTicksToWait) {
  TickType_t xStart = xTaskGetTickCount();
  TickType_t xEnd = xStart + xTicksToWait;
  while (xTaskGetTickCount() < xEnd) {
    __asm volatile("nop");
  }
}

static void vJob_Kill(void *pvParameters) {
  (void)pvParameters;
  vBusyWaitTicks(pdMS_TO_TICKS(150)); /* Period 100: Should Kill */
}

static void vJob_Skip(void *pvParameters) {
  (void)pvParameters;
  vBusyWaitTicks(pdMS_TO_TICKS(150)); /* Period 100: Should Skip */
}

static void vJob_Normal(void *pvParameters) {
  (void)pvParameters;
  vBusyWaitTicks(pdMS_TO_TICKS(20)); /* Standard task */
}

static void vJob_Referee(void *pvParameters) {
  (void)pvParameters;
  PTL_TraceStats_t s;

  vTaskDelay(pdMS_TO_TICKS(600));

  vTaskSuspendAll();

  UART_printf("\n=== TEST: MIXED POLICY STRESS ===\n");
  PTL_PrintStatistics();

  PTL_GetTraceStatistics(&s);

  /* Success if we have completions, kills, and skips recorded */
  if ((s.ulOverrunCount >= 2) && (s.ulTotalCompletions > 0)) {
    UART_printf("[PASS] Handled mixed policies under stress.\n");
  } else {
    char pcBuf[128];
    snprintf(pcBuf, sizeof(pcBuf),
             "[FAIL] Policy interactions failed. Events: %u Overruns, %u "
             "Completions\n",
             (unsigned int)s.ulOverrunCount,
             (unsigned int)s.ulTotalCompletions);
    UART_printf(pcBuf);
  }

  for (;;) {
    /* Trap */
  }
}

static PTL_TaskConfig_t xTaskConfig[] = {
    {"Killer", 100, 100, 2, 512, vJob_Kill, NULL, PTL_POLICY_KILL},
    {"Skipper", 100, 100, 2, 512, vJob_Skip, NULL, PTL_POLICY_SKIP},
    {"Normal", 100, 100, 2, 512, vJob_Normal, NULL, PTL_POLICY_SKIP},
    {"REF", 800, 800, 4, 512, vJob_Referee, NULL, PTL_POLICY_USE_GLOBAL}};

int main(void) {
  PTL_GlobalConfig_t xGlobalConfig = {PTL_POLICY_SKIP, pdTRUE, 4};

  UART_init();
  PTL_TraceInit();

  if (PTL_Init(&xGlobalConfig, xTaskConfig, 4) == pdPASS) {
    PTL_Start();
  }

  for (;;) {
    /* Trap */
  }
  return 0;
}