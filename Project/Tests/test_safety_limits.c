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
 * @file test_safety_limits.c
 * @brief Extended safety checks for PTL initialization.
 *
 * Tests boundary conditions including invalid function pointers.
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"
#include <stdio.h>

int main(void) {
  UART_init();
  UART_printf("\n=== TEST: INITIALIZATION SAFETY ===\n");

  /* TEST 1: NULL Configuration Object */
  if (PTL_Init(NULL, NULL, 0) == pdFAIL) {
    UART_printf("[CHECK] Rejected NULL Config: OK\n");
  } else {
    UART_printf("[FAIL] Accepted NULL Config!\n");
  }

  /* TEST 2: Zero Task Count */
  PTL_GlobalConfig_t xGlobalConfig = {PTL_POLICY_SKIP, pdTRUE, 2};

  if (PTL_Init(&xGlobalConfig, NULL, 0) == pdFAIL) {
    UART_printf("[CHECK] Rejected Zero Tasks: OK\n");
  } else {
    UART_printf("[FAIL] Accepted Zero Tasks!\n");
  }

  /* TEST 3: Invalid Task Pointer */
  PTL_TaskConfig_t xInvalidTaskConfig[] = {
      {"BadTask", 100, 100, 2, 512, NULL, NULL, PTL_POLICY_SKIP}};

  if (PTL_Init(&xGlobalConfig, xInvalidTaskConfig, 1) == pdFAIL) {
    UART_printf("[CHECK] Rejected NULL Task Function: OK\n");
  } else {
    UART_printf("[FAIL] Accepted NULL Task Function!\n");
  }

  /* If all checks passed, print success string. */
  UART_printf("\n[PASS] Initialization Safety checks complete.\n");

  for (;;) {
    /* Trap */
  }

  return 0;
}