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
 * @file main.c
 * @brief PTL (Periodic Task Layer) Demonstration Application.
 *
 * This application demonstrates the capabilities of the PTL Scheduler by
 * defining three periodic tasks with different behaviors and overrun policies.
 *
 * @author Member 3 (Group 38)
 */

/*-----------------------------------------------------------*/
/* INCLUDES                                                   */
/*-----------------------------------------------------------*/

#include "burner.h"
#include "ptl.h"
#include "uart.h"
#include <string.h>

/*-----------------------------------------------------------*/
/* PRIVATE DEFINITIONS                                        */
/*-----------------------------------------------------------*/

/** @brief Task stack size. */
#define MAIN_TASK_STACK_SIZE (256)

/*-----------------------------------------------------------*/
/* PRIVATE FUNCTION PROTOTYPES                                */
/*-----------------------------------------------------------*/

void vJob_Sensor(void *pvParameters);
void vJob_ImageProc(void *pvParameters);
void vJob_Logger(void *pvParameters);

/*-----------------------------------------------------------*/
/* PRIVATE DATA                                               */
/*-----------------------------------------------------------*/

/**
 * @brief Task Configuration Array.
 *
 * Defines three tasks:
 * 1. Sensor: Normal periodic execution.
 * 2. ImgProc: Intentionally fails deadline (KILL policy).
 * 3. Logger: Runs late but finishes (SKIP policy).
 */
static const PTL_TaskConfig_t xTaskConfig[] = {
    /* Name, Period, Deadline, Priority, Stack, Func, Arg, Policy */
    {"Sensor", pdMS_TO_TICKS(100), pdMS_TO_TICKS(100), 2, MAIN_TASK_STACK_SIZE,
     vJob_Sensor, NULL, PTL_POLICY_USE_GLOBAL},
    {"ImgProc", pdMS_TO_TICKS(50), pdMS_TO_TICKS(50), 1, MAIN_TASK_STACK_SIZE,
     vJob_ImageProc, NULL, PTL_POLICY_KILL},
    {"Logger", pdMS_TO_TICKS(50), pdMS_TO_TICKS(50), 3, MAIN_TASK_STACK_SIZE,
     vJob_Logger, NULL, PTL_POLICY_SKIP}};

/**
 * @brief Global PTL Configuration.
 */
static const PTL_GlobalConfig_t xGlobalConfig = {
    PTL_POLICY_CATCH_UP, /* Default Policy */
    pdTRUE,              /* Tracing Enabled */
    3                    /* Max Tasks */
};

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Main application entry point.
 *
 * Initializes hardware, PTL, and starts the scheduler.
 *
 * @return 0 (Should never return).
 */
int main(void) {
  /* Initialize Hardware. */
  UART_init();
  UART_printf("\n\n");
  UART_printf("========================================\n");
  UART_printf("   PTL REAL-TIME SCHEDULER DEMO v1.0    \n");
  UART_printf("========================================\n");

  /* Initialize PTL. */
  if (PTL_Init(&xGlobalConfig, xTaskConfig, 3) != pdPASS) {
    UART_printf("[ERROR] PTL Initialization Failed!\n");
    for (;;) {
      /* Halt on error. */
    }
  }

  UART_printf("[INFO] System Initialized. Starting Scheduler...\n");

  /* Start Scheduler (Never returns). */
  PTL_Start();

  /* Should not be reached. */
  for (;;) {
    /* Trap. */
  }

  return 0;
}
/*-----------------------------------------------------------*/

/**
 * @brief Job 1: Sensor Read
 *
 * Simulates a sensor reading task. Runs quickly (10ms) and
 * completes well within its deadline.
 *
 * @param[in] pvParameters Unused.
 */
void vJob_Sensor(void *pvParameters) {
  (void)pvParameters;

  UART_printf("[SENSOR] Reading data... (10ms work)\n");
  Burn(10);
  UART_printf("[SENSOR] Done.\n");
}
/*-----------------------------------------------------------*/

/**
 * @brief Job 2: Image Processing
 *
 * Simulates a heavy task running for 80ms with a 50ms deadline.
 * Demonstrates the KILL policy (supervisor terminates it).
 *
 * @param[in] pvParameters Unused.
 */
void vJob_ImageProc(void *pvParameters) {
  (void)pvParameters;

  UART_printf("[IMG_PROC] Processing frame... (Will Exceed Deadline)\n");

  /* Simulates work exceeding deadline (50ms). */
  Burn(80);

  /* This line should never execute if KILL policy works. */
  UART_printf("[FAIL] ImageProc finished! (Should be KILLED)\n");
}
/*-----------------------------------------------------------*/

/**
 * @brief Job 3: Data Logger
 *
 * Simulates a logging task running for 60ms with a 50ms deadline.
 * Demonstrates the SKIP policy (allowed to finish, next job skipped).
 *
 * @param[in] pvParameters Unused.
 */
void vJob_Logger(void *pvParameters) {
  (void)pvParameters;

  UART_printf("[LOG] Writing to flash... (Running late)\n");

  /* Run for 60ms (Deadline is 50ms). */
  Burn(60);

  UART_printf("[LOG] Done (Late but Safe).\n");
}
/*-----------------------------------------------------------*/