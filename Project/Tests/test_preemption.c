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
 * @file test_preemption.c
 * @brief Test: Priority-Based Preemption Validation with robust timing.
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"

/* Helper for robust busy waiting */
static void vBusyWaitTicks(TickType_t xTicksToWait) {
  TickType_t xStart = xTaskGetTickCount();
  TickType_t xEnd = xStart + xTicksToWait;
  while (xTaskGetTickCount() < xEnd) {
    __asm volatile("nop");
  }
}

/* Flags to track execution order */
static volatile uint32_t ulLowPrioStartCount = 0;
static volatile uint32_t ulHighPrioStartCount = 0;
static volatile uint32_t ulLowPrioEndCount = 0;
static volatile uint32_t ulHighPrioEndCount = 0;
static volatile BaseType_t xPreemptionDetected = pdFALSE;

/**
 * @brief Low priority task job.
 */
static void vJob_LowPrio(void *pvParameters) {
  (void)pvParameters;

  ulLowPrioStartCount++;
  UART_printf("[LOW_PRIO] Started execution\n");

  /* Burn 50ms of CPU time */
  vBusyWaitTicks(pdMS_TO_TICKS(50));

  ulLowPrioEndCount++;
  UART_printf("[LOW_PRIO] Completed execution\n");
}

/**
 * @brief High priority task job.
 */
static void vJob_HighPrio(void *pvParameters) {
  (void)pvParameters;

  ulHighPrioStartCount++;
  UART_printf("[HIGH_PRIO] Started\n");

  /* Check if low priority was active when we started */
  if (ulLowPrioStartCount > ulLowPrioEndCount) {
    xPreemptionDetected = pdTRUE;
    UART_printf("[HIGH_PRIO] PREEMPTION! LOW_PRIO was active\n");
  }

  /* Burn 20ms of CPU time */
  vBusyWaitTicks(pdMS_TO_TICKS(20));

  ulHighPrioEndCount++;
  UART_printf("[HIGH_PRIO] Completed\n");
}

/**
 * @brief Referee task - validates test results.
 */
static void vJob_Referee(void *pvParameters) {
  (void)pvParameters;
  PTL_TraceStats_t xStats;
  BaseType_t xTestPassed = pdTRUE;

  /* Wait for at least one full cycle of both tasks */
  vTaskDelay(pdMS_TO_TICKS(300));

  UART_printf("\n=== TEST: PREEMPTION VALIDATION ===\n");

  PTL_PrintTrace();
  PTL_PrintStatistics();
  PTL_GetTraceStatistics(&xStats);

  /* Checks */
  if (ulLowPrioEndCount == 0) {
    UART_printf("[FAIL] Low priority task never completed\n");
    xTestPassed = pdFALSE;
  }
  if (ulHighPrioEndCount == 0) {
    UART_printf("[FAIL] High priority task never completed\n");
    xTestPassed = pdFALSE;
  }

  if (xPreemptionDetected == pdFALSE) {
    UART_printf("[FAIL] Preemption not detected\n");
    xTestPassed = pdFALSE;
  } else {
    UART_printf("[CHECK] Preemption detected correctly\n");
  }

  if (xStats.ulTotalReleases < 3) {
    UART_printf("[FAIL] Insufficient releases recorded in trace\n");
    xTestPassed = pdFALSE;
  } else {
    UART_printf("[CHECK] Tasks released correctly\n");
  }

  /* Final result */
  if (xTestPassed == pdTRUE) {
    UART_printf("[PASS] Preemption test successful.\n");
  } else {
    UART_printf("[FAIL] Preemption test failed.\n");
  }

  vTaskEndScheduler();
  for (;;) {
    /* Trap */
  }
}

static PTL_TaskConfig_t xTasks[] = {
    {"LowPrio", pdMS_TO_TICKS(100), pdMS_TO_TICKS(100), 1, 512, vJob_LowPrio,
     NULL, PTL_POLICY_SKIP},
    {"HighPrio", pdMS_TO_TICKS(150), pdMS_TO_TICKS(150), 3, 512, vJob_HighPrio,
     NULL, PTL_POLICY_SKIP},
    {"Referee", pdMS_TO_TICKS(500), pdMS_TO_TICKS(500), 4, 512, vJob_Referee,
     NULL, PTL_POLICY_USE_GLOBAL}};

static PTL_GlobalConfig_t xGlobalConfig = {PTL_POLICY_SKIP, pdTRUE, 3};

int main(void) {
  UART_init();
  UART_printf("\n========================================\n");
  UART_printf("   TEST: PRIORITY-BASED PREEMPTION      \n");
  UART_printf("========================================\n\n");

  PTL_TraceInit();

  if (PTL_Init(&xGlobalConfig, xTasks, 3) == pdPASS) {
    PTL_Start();
  } else {
    UART_printf("[FAIL] PTL initialization failed\n");
  }

  for (;;) {
    /* Trap */
  }
  return 0;
}
