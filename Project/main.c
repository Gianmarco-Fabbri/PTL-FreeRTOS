/*
 * ======================================================================================
 * PTL (Periodic Task Language) - DEMONSTRATION MAIN
 * Group 38 - Real-Time Systems Project
 * ======================================================================================
 *
 * DESCRIPTION:
 * This file demonstrates the capabilities of the PTL Scheduler.
 * It defines three tasks with different behaviors to showcase:
 * 1. Normal Periodic Execution
 * 2. Deadline Enforcement (KILL Policy)
 * 3. Overrun Handling (SKIP Policy)
 *
 * HOW TO USE:
 * 1. Compile: "make cleanobj all"
 * 2. Run: "make qemu_start" (or "make qemu_debug")
 * 3. Observe: UART Output showing task start/end and supervisor interventions.
 * ======================================================================================
 */

#include "ptl.h"       /* PTL Core API */
#include "uart.h"      /* UART Driver for output */
#include "burner.h"    /* CPU Burner for simulating workload */
#include <string.h>    /* For strcmp if needed */

/* * --------------------------------------------------------------------------------------
 * SECTION 1: JOB FUNCTIONS (The "Work" code)
 * --------------------------------------------------------------------------------------
 * NOTE FOR PROFESSOR: 
 * These functions simulate real tasks. Instead of 'vTaskDelay', we use 'Burn(ms)'
 * to consume actual CPU cycles, forcing the scheduler to manage preemption and
 * overruns realistically.
 */

/* * Job 1: "Sensor_Read"
 * Behavior: Runs quickly and finishes well before its deadline.
 * Goal: Show standard periodic behavior.
 */
void Job_Sensor(void *pvParameters) {
    (void)pvParameters;
    
    UART_printf("[SENSOR] Reading data... (10ms work)\n");
    Burn(10); 
    UART_printf("[SENSOR] Done.\n");
}

/* * Job 2: "Image_Proc"
 * Behavior: INTENTIONALLY FAILS. Tries to run for 80ms with a 50ms deadline.
 * Policy:   PTL_POLICY_KILL
 * Goal:     Show the Supervisor terminating a rogue task.
 */
void Job_ImageProc(void *pvParameters) {
    (void)pvParameters;
    
    UART_printf("[IMG_PROC] Processing heavy frame... (Will Exceed Deadline)\n");
    
    /* * Simulating a heavy calculation that hangs or takes too long.
     * The PTL Supervisor should interrupt this at 50ms (Deadline).
     */
    Burn(80); 
    
    /* If the system works, this line should NEVER print */
    UART_printf("[FAIL] ImageProc finished! (Should have been KILLED)\n");
}

/* * Job 3: "Data_Log"
 * Behavior: Runs late but is allowed to finish.
 * Policy:   PTL_POLICY_SKIP
 * Goal:     Show the SKIP policy (finish late, skip next job to recover).
 */
void Job_Logger(void *pvParameters) {
    (void)pvParameters;
    
    UART_printf("[LOG] Writing to flash... (Running late)\n");
    
    /* Run for 60ms (Deadline is 50ms) */
    Burn(60); 
    
    UART_printf("[LOG] Done (Late but Safe).\n");
}

/* * --------------------------------------------------------------------------------------
 * SECTION 2: SYSTEM CONFIGURATION
 * --------------------------------------------------------------------------------------
 */

/* * Task Configuration Array 
 * Fields: { Name, Period, Deadline, Priority, Stack, Function, Arg, POLICY }
 */
static PTL_TaskConfig_t t_config[] = {
    /* * Task 1: Sensor (Normal)
     * Period: 100ms, Deadline: 100ms, Priority: 2 (Medium)
     * Policy: USE_GLOBAL (Inherit from Global Config)
     */
    { "Sensor",   pdMS_TO_TICKS(100), pdMS_TO_TICKS(100), 2, 256, Job_Sensor,    NULL, PTL_POLICY_USE_GLOBAL },

    /* * Task 2: ImageProc (Rogue Task)
     * Period: 200ms, Deadline: 50ms, Priority: 1 (Low)
     * Policy: KILL (Terminate immediately on deadline miss)
     */
    { "ImgProc",  pdMS_TO_TICKS(200), pdMS_TO_TICKS(50),  1, 256, Job_ImageProc, NULL, PTL_POLICY_KILL },

    /* * Task 3: Logger (Late Task)
     * Period: 200ms, Deadline: 50ms, Priority: 3 (High)
     * Policy: SKIP (Let it finish, then skip the next release)
     */
    { "Logger",   pdMS_TO_TICKS(200), pdMS_TO_TICKS(50),  3, 256, Job_Logger,    NULL, PTL_POLICY_SKIP }
};

/* * Global Configuration 
 * - Default Policy: If a task says USE_GLOBAL, use this (e.g., CATCH_UP).
 * - Tracing: Enable internal logging for debug.
 * - Max Tasks: Must match or exceed the array size (Max 8).
 */
static PTL_GlobalConfig_t global_config = {
    .eOverrunPolicy  = PTL_POLICY_CATCH_UP,
    .xTracingEnabled = pdTRUE,
    .uxMaxTasks      = 3
};

/* * --------------------------------------------------------------------------------------
 * SECTION 3: MAIN ENTRY POINT
 * --------------------------------------------------------------------------------------
 */
int main(void) {
    /* 1. Hardware Init */
    UART_init();
    UART_printf("\n\n");
    UART_printf("========================================\n");
    UART_printf("   PTL REAL-TIME SCHEDULER DEMO v1.0    \n");
    UART_printf("========================================\n");

    /* 2. PTL Initialization (Validates config and creates tasks) */
    if (PTL_Init(&global_config, t_config, 3) != pdPASS) {
        UART_printf("[ERROR] PTL Initialization Failed!\n");
        while(1); /* Halt on error */
    }

    UART_printf("[INFO] System Initialized. Starting Scheduler...\n");

    /* * 3. Start Scheduler
     * This function creates the Supervisor task and starts the FreeRTOS kernel.
     * It should never return.
     */
    PTL_Start();

    /* 4. Safety Trap (Should never be reached) */
    while(1) {
        __asm("nop");
    }

    return 0;
}