#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"

void Job_Fast(void *p) {
    (void)p;
    UART_printf("[CATCHUP] Start (120ms burn, 100ms period)\n");
    Burn(120);
}

void Job_Check(void *p) {
    (void)p;
    vTaskDelay(pdMS_TO_TICKS(400));
    
    PTL_TraceStats_t xStats;
    PTL_GetTraceStatistics(&xStats);

    UART_printf("\n=== TEST: CATCHUP POLICY VALIDATION ===\n");
    PTL_PrintTrace();
    PTL_PrintStatistics();

    if (xStats.ulOverrunCount > 0) {
        UART_printf("[PASS] Task successfully caught up after delay.\n");
    } else {
        UART_printf("[FAIL] Catchup event missing from trace.\n");
    }
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Fast",  100, 100, 2, 256, Job_Fast,  NULL, PTL_POLICY_CATCH_UP },
    { "Check", 500, 500, 3, 256, Job_Check, NULL, PTL_POLICY_USE_GLOBAL }
};

static PTL_GlobalConfig_t c = { PTL_POLICY_CATCH_UP, pdTRUE, 2 };

int main(void) {
    UART_init();
    PTL_TraceInit();
    if (PTL_Init(&c, t, 2) == pdPASS) {
        PTL_Start();
    }
    for(;;);
    return 0;
}