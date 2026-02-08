#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"
#include <stdio.h>

void Job_HighFreq(void *pvParameters) {
    (void)pvParameters; 
    /* No UART_printf here! We keep it quiet so the buffer can fill 
     * and the trace system can record events silently. */
    Burn(1); 
}

void Job_Check(void *pvParameters) {
    (void)pvParameters; 
    
    /* Let the high-frequency tasks fill the trace buffer. */
    vTaskDelay(pdMS_TO_TICKS(600));
    
    UART_printf("\n=== COMPLEX TEST: TRACE BUFFER WRAP ===\n");
    
    /* 1. Show the event log from the circular buffer. */
    PTL_PrintTrace(); 
    
    /* 2. Show the performance results (Overhead/Utilization). */
    PTL_PrintStatistics(); 
    
    PTL_TraceStats_t s;
    PTL_GetTraceStatistics(&s);

    /* Validate Success Criteria:
     * - The buffer must have wrapped around (ulTotalReleases > 100).
     * - System Overhead must be <= 10.00%.
     */
    uint32_t ulCpuUtilPct = (uint32_t)(s.fCPUUtilization * 100.0f);
    uint32_t ulOverheadPct = 10000 - ulCpuUtilPct;

    if (s.ulTotalReleases > PTL_TRACE_BUFFER_SIZE && ulOverheadPct <= 1000) {
        UART_printf("[PASS] Buffer wrapped correctly and overhead is within 10%%.\n");
    } else {
        UART_printf("[FAIL] Performance or trace wrap criteria not met.\n");
    }
    
    /* Stop all tasks to ensure the Makefile sees the [PASS] string clearly. */
    vTaskEndScheduler(); 
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Fast1", 5, 5, 2, 256, Job_HighFreq, NULL, PTL_POLICY_SKIP },
    { "Fast2", 5, 5, 2, 256, Job_HighFreq, NULL, PTL_POLICY_SKIP },
    { "Check", 800, 800, 5, 256, Job_Check, NULL, PTL_POLICY_USE_GLOBAL }
};

int main(void) {
    UART_init();
    PTL_TraceInit();
    
    PTL_GlobalConfig_t c = { PTL_POLICY_SKIP, pdTRUE, 5 };
    
    if (PTL_Init(&c, t, 3) == pdPASS) {
        PTL_Start();
    }
    
    for(;;);
    return 0;
}