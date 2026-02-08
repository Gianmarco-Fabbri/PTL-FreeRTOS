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
 * @file burner.c
 * @brief CPU Burner Utility Implementation.
 *
 * Implements a calibrated busy-wait loop for simulating task execution time.
 *
 * @author Member 2 (Group 38)
 */

#include "burner.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart.h" /* For debug output during calibration */
#include <stdio.h>

/*-----------------------------------------------------------*/
/* PRIVATE DATA                                               */
/*-----------------------------------------------------------*/

/** @brief Calibrated loop iterations per millisecond. */
static volatile uint32_t ulLoopsPerMs = 1000;

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Calibrate the burner loop against the system timer.
 *
 * Runs a test loop for 100ms measured by FreeRTOS tick count to
 * determine the CPU speed relative to the loop overhead.
 */
void Burn_Calibrate(void) {
  TickType_t xStart, xEnd;
  volatile uint32_t ulCount = 0;
  const TickType_t xCalibrationTicks = 100; /* 100ms */

  xStart = xTaskGetTickCount();

  /* Wait for the start of a new tick alignment */
  while (xTaskGetTickCount() == xStart) {
    /* Spin */
  }

  xStart = xTaskGetTickCount();

  /* Run loop for 100 ticks */
  while ((xTaskGetTickCount() - xStart) < xCalibrationTicks) {
    ulCount++;
  }

  xEnd = xTaskGetTickCount();

  /* Calculate iterations per ms (assuming 1 tick = 1 ms) */
  /* Be careful to avoid divide by zero if something went wrong */
  if ((xEnd > xStart) && (ulCount > 0)) {
    /* Convert ticks to ms (assuming configTICK_RATE_HZ=1000) */
    uint32_t ulDurationMs = (xEnd - xStart) * portTICK_PERIOD_MS;
    ulLoopsPerMs = ulCount / ulDurationMs;
  }

  char cBuf[32];
  snprintf(cBuf, sizeof(cBuf), "%u loops/ms\n", (unsigned int)ulLoopsPerMs);
  UART_printf(cBuf);
}
/*-----------------------------------------------------------*/

/**
 * @brief Burn CPU cycles for approximately N milliseconds.
 *
 * @param[in] iMillis Duration to burn.
 */
void Burn(int iMillis) {
  volatile uint32_t ulTotalLoops = ulLoopsPerMs * iMillis;
  volatile uint32_t i;

  for (i = 0; i < ulTotalLoops; i++) {
    __asm volatile("nop");
  }
}
/*-----------------------------------------------------------*/