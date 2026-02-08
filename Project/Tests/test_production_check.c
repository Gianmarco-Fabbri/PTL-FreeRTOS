#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"
#include <stdio.h>

/* We only need a few jobs to calculate stable statistics */
static volatile int iJobsCompleted = 0;
#define TOTAL_TEST_JOBS 6

void Job_Stress_Worker(void *p) {
    (void)p;
    
    /* 40ms work in 100ms period = 40% load per task */
    Burn(40);
    
    iJobsCompleted++;

    /* The last job triggers the Trace Report */
    if (iJobsCompleted == TOTAL_TEST_JOBS) {
        UART_printf("\n=== PRODUCTION TEST: TRACE-BASED VALIDATION ===\n");
        
        /* Prints every event logged in the circular buffer */
        PTL_PrintTrace(); 
        
        /* Automatically calculates CPU Util and Overhead */
        PTL_PrintStatistics(); 

        PTL_TraceStats_t xStats;
        PTL_GetTraceStatistics(&xStats);

        /* Success check: Overhead must be <= 10.00% */
        uint32_t ulCpuUtilPct = (uint32_t)(xStats.fCPUUtilization * 100.0f);
        uint32_t ulOverheadPct = 10000 - ulCpuUtilPct;

        if (ulOverheadPct <= 1000) { /* 1000 = 10.00% */
            UART_printf("[PASS] Production Stress Test: Overhead within limits.\n");
        } else {
            UART_printf("[FAIL] Production Stress Test: Overhead exceeded 10%.\n");
        }
        
        /* Suspend all to stop the output spam for the Makefile */
        vTaskEndScheduler(); 
    }
}

static PTL_TaskConfig_t t[] = {
    { "Worker_A", 100, 100, 2, 256, Job_Stress_Worker, NULL, PTL_POLICY_SKIP },
    { "Worker_B", 100, 100, 2, 256, Job_Stress_Worker, NULL, PTL_POLICY_SKIP }
};

static PTL_GlobalConfig_t c = { PTL_POLICY_SKIP, pdTRUE, 2 };

int main(void) {
    UART_init();
    
    /* Resets write index and idle timers */
    PTL_TraceInit(); 

    if (PTL_Init(&c, t, 2) == pdPASS) {
        PTL_Start();
    }
    
    for(;;);
    return 0;
}