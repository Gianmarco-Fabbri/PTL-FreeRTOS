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
 * @file test_policy_kill_trace.c
 * @brief Validation of PTL_POLICY_KILL with robust timing.
 *
 * Ensures that a task exceeding its period/deadline is terminated
 * by the supervisor. Uses system tick loop for precise timing.
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"

/* Helper for robust busy waiting */
static void vBusyWaitTicks(TickType_t xTicksToWait) {
  TickType_t xStart = xTaskGetTickCount();
  TickType_t xEnd = xStart + xTicksToWait;

  /* Loop until deadline reached, allowing preemption */
  while (xTaskGetTickCount() < xEnd) {
    __asm volatile("nop");
  }
}

static void vJob_Victim(void *pvParameters) {
  (void)pvParameters;
  UART_printf("[VICTIM] Starting long job...\n");

  /* Period is 100ms. Run for 200ms to force KILL.
   * Note: Supervisor kills at 100ms. */
  vBusyWaitTicks(pdMS_TO_TICKS(200));

  UART_printf("[FAIL] Victim was not killed!\n");
}

static void vJob_Check(void *pvParameters) {
  (void)pvParameters;
  PTL_TraceStats_t xStats;

  vTaskDelay(pdMS_TO_TICKS(350));

  PTL_GetTraceStatistics(&xStats);

  UART_printf("\n=== TEST: KILL POLICY VALIDATION ===\n");
  PTL_PrintTrace();

  /* The trace system must have recorded an OVERRUN_KILL event */
  if (xStats.ulOverrunCount > 0) {
    UART_printf("[PASS] Supervisor detected overrun and killed the task.\n");
  } else {
    UART_printf("[FAIL] Overrun not detected in trace stats.\n");
  }

  for (;;) {
    /* Trap */
  }
}

static PTL_TaskConfig_t xTaskConfig[] = {
    {"Victim", 100, 100, 2, 512, vJob_Victim, NULL, PTL_POLICY_KILL},
    {"Check", 500, 500, 3, 512, vJob_Check, NULL, PTL_POLICY_USE_GLOBAL}};

static PTL_GlobalConfig_t xGlobalConfig = {PTL_POLICY_KILL, pdTRUE, 2};

int main(void) {
  UART_init();

  PTL_TraceInit();

  if (PTL_Init(&xGlobalConfig, xTaskConfig, 2) == pdPASS) {
    PTL_Start();
  }

  for (;;) {
    /* Trap */
  }

  return 0;
}