#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"

void Job_Victim(void *p) {
    (void)p;
    UART_printf("[VICTIM] Starting long job...\n");
    /* Period is 100ms. Burn 200ms to force a KILL. */
    Burn(200); 
    UART_printf("[FAIL] Victim was not killed!\n");
}

void Job_Check(void *p) {
    (void)p;
    vTaskDelay(pdMS_TO_TICKS(350));
    
    PTL_TraceStats_t xStats;
    PTL_GetTraceStatistics(&xStats);

    UART_printf("\n=== TEST: KILL POLICY VALIDATION ===\n");
    PTL_PrintTrace();

    /* The trace system must have recorded an OVERRUN_KILL event */
    if (xStats.ulOverrunCount > 0) {
        UART_printf("[PASS] Supervisor detected overrun and killed the task.\n");
    } else {
        UART_printf("[FAIL] Overrun not detected in trace stats.\n");
    }
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Victim", 100, 100, 2, 256, Job_Victim, NULL, PTL_POLICY_KILL },
    { "Check",  500, 500, 3, 256, Job_Check,  NULL, PTL_POLICY_USE_GLOBAL }
};

static PTL_GlobalConfig_t c = { PTL_POLICY_KILL, pdTRUE, 2 };

int main(void) {
    UART_init();

    PTL_TraceInit();
    
    if (PTL_Init(&c, t, 2) == pdPASS) {
        PTL_Start();
    }

    for(;;);
    return 0;
}