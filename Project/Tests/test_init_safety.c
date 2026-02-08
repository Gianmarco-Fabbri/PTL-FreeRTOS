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
 * @file test_init_safety.c
 * @brief Test for PTL initialization safety.
 *
 * Verifies that PTL_Init() correctly rejects invalid configurations
 * such as NULL pointers or zero tasks.
 */

#include "ptl.h"
#include "ptl_trace.h"
#include "uart.h"

int main(void) {
  UART_init();
  UART_printf("\n=== TEST: INIT SAFETY CHECKS ===\n");

  /* Test 1: NULL config */
  if (PTL_Init(NULL, NULL, 0) == pdFAIL) {
    UART_printf("[PASS] NULL config rejected\n");
  } else {
    UART_printf("[FAIL] NULL config accepted\n");
    return 1;
  }

  /* Test 2: Zero tasks */
  PTL_GlobalConfig_t xConfig = {PTL_POLICY_SKIP, pdTRUE, 8};

  if (PTL_Init(&xConfig, NULL, 0) == pdFAIL) {
    UART_printf("[PASS] Zero tasks rejected\n");
  } else {
    UART_printf("[FAIL] Zero tasks accepted\n");
    return 1;
  }

  UART_printf("[PASS] All safety checks passed\n");
  return 0;
}