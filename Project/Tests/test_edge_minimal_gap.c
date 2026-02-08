#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"

void Job_Tight(void *p) { 
    Burn(8); /* 8ms work, 2ms slack for 10ms period */
}

void Job_Check(void *p) {
    vTaskDelay(pdMS_TO_TICKS(500));
    PTL_TraceStats_t s;
    PTL_GetTraceStatistics(&s);

    UART_printf("\n=== TEST: MINIMAL TIME GAPS ===\n");
    PTL_PrintStatistics();

    /* Success if no overruns occur despite the tight timing */
    if (s.ulOverrunCount == 0 && s.ulTotalCompletions > 10) {
        UART_printf("[PASS] Timing consistency maintained with minimal gaps.\n");
    } else {
        UART_printf("[FAIL] Jitter caused overruns in tight schedule.\n");
    }
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Tight", 10, 10, 2, 256, Job_Tight, NULL, PTL_POLICY_CATCH_UP },
    { "REF",  600, 600, 3, 256, Job_Check, NULL, PTL_POLICY_USE_GLOBAL }
};

int main(void) {
    UART_init();
    PTL_TraceInit();
    PTL_GlobalConfig_t c = { PTL_POLICY_CATCH_UP, pdTRUE, 2 };
    if (PTL_Init(&c, t, 2) == pdPASS) PTL_Start();
    for(;;); return 0;
}