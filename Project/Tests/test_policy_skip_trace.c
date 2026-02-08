#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"

void Job_Skipper(void *p) {
    (void)p;
    UART_printf("[SKIPPER] Start (250ms burn, 100ms period)\n");
    Burn(250);
}

void Job_Check(void *p) {
    (void)p;
    vTaskDelay(pdMS_TO_TICKS(500));
    
    PTL_TraceStats_t xStats;
    PTL_GetTraceStatistics(&xStats);

    UART_printf("\n=== TEST: SKIP POLICY VALIDATION ===\n");
    PTL_PrintTrace();
    PTL_PrintStatistics();

    if (xStats.ulOverrunCount >= 2) {
        UART_printf("[PASS] Backlog avoided via SKIP policy.\n");
    } else {
        UART_printf("[FAIL] Skip logic failed or not logged.\n");
    }
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Skipper", 100, 100, 2, 256, Job_Skipper, NULL, PTL_POLICY_SKIP },
    { "Check",   600, 600, 3, 256, Job_Check,   NULL, PTL_POLICY_USE_GLOBAL }
};

static PTL_GlobalConfig_t c = { PTL_POLICY_SKIP, pdTRUE, 2 };

int main(void) {
    UART_init();
    PTL_TraceInit();
    if (PTL_Init(&c, t, 2) == pdPASS) {
        PTL_Start();
    }
    for(;;);
    return 0;
}