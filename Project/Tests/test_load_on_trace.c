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

#include "burner.h"
#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"
#include <stdio.h>

/*-----------------------------------------------------------*/

/**
 * @brief High frequency job for stress testing buffer.
 */
static void vJob_HighFreq(void *pvParameters) {
  (void)pvParameters;
  /* Small work to stress-test the trace system with many events. */
  Burn(1);
}

/**
 * @brief Check task that verifies trace statistics.
 */
static void vJob_Check(void *pvParameters) {
  (void)pvParameters;
  char pcBuffer[80];
  PTL_TraceStats_t xStats;
  BaseType_t xReleasesOk, xTraceOk, xCompletionsOk;

  /* Let the high-frequency tasks run for a bit to generate events. */
  vTaskDelay(pdMS_TO_TICKS(500));

  UART_printf("\n=== COMPLEX TEST: TRACE STRESS TEST ===\n");

  /* 1. Show the event log from the circular buffer. */
  PTL_PrintTrace();

  /* 2. Show the performance results. */
  PTL_PrintStatistics();

  PTL_GetTraceStatistics(&xStats);

  /* Validate Success Criteria:
   * - The system should have recorded events (ulTotalReleases > 10).
   * - Trace system should not have crashed.
   * - We should have completions (tasks finishing).
   */

  xReleasesOk = (xStats.ulTotalReleases >= 10);
  xTraceOk = (xStats.ulTotalTimeMs > 400);
  xCompletionsOk = (xStats.ulTotalCompletions > 0);

  if (xReleasesOk && xTraceOk && xCompletionsOk) {
    snprintf(pcBuffer, sizeof(pcBuffer),
             "[PASS] Trace stress test: %u releases, %u completions.\n",
             (unsigned int)xStats.ulTotalReleases,
             (unsigned int)xStats.ulTotalCompletions);
    UART_printf(pcBuffer);
  } else {
    if (!xReleasesOk) {
      snprintf(pcBuffer, sizeof(pcBuffer),
               "[FAIL] Not enough releases: %u (expected >= 10)\n",
               (unsigned int)xStats.ulTotalReleases);
      UART_printf(pcBuffer);
    }
    if (!xCompletionsOk) {
      UART_printf("[FAIL] No task completions recorded.\n");
    }
    if (!xTraceOk) {
      snprintf(pcBuffer, sizeof(pcBuffer),
               "[FAIL] Trace timing issue: total time %u ms\n",
               (unsigned int)xStats.ulTotalTimeMs);
      UART_printf(pcBuffer);
    }
  }

  /* Stop all tasks to ensure the Makefile sees the [PASS] string. */
  vTaskEndScheduler();
  for (;;) {
    /* Trap */
  }
}

static PTL_TaskConfig_t xTaskConfig[] = {
    {"Fast1", 10, 10, 2, 512, vJob_HighFreq, NULL, PTL_POLICY_SKIP},
    {"Fast2", 10, 10, 2, 512, vJob_HighFreq, NULL, PTL_POLICY_SKIP},
    {"Check", 800, 800, 5, 512, vJob_Check, NULL, PTL_POLICY_USE_GLOBAL}};

static PTL_GlobalConfig_t xGlobalConfig = {PTL_POLICY_SKIP, pdTRUE, 5};

int main(void) {
  UART_init();
  PTL_TraceInit();

  if (PTL_Init(&xGlobalConfig, xTaskConfig, 3) == pdPASS) {
    PTL_Start();
  }

  for (;;) {
    /* Trap */
  }
  return 0;
}