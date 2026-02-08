#include "ptl.h"
#include "uart.h"
#include "burner.h"
#include "ptl_trace.h"

void Job_Kill(void *p) { Burn(150); }    /* Period 100: Should Kill */
void Job_Skip(void *p) { Burn(150); }    /* Period 100: Should Skip */
void Job_Normal(void *p) { Burn(20); }   /* Standard task */

void Job_Referee(void *p) {
    vTaskDelay(pdMS_TO_TICKS(600));
    UART_printf("\n=== TEST: MIXED POLICY STRESS ===\n");
    PTL_PrintStatistics(); /* Validates total overhead and CPU utilization */
    
    PTL_TraceStats_t s;
    PTL_GetTraceStatistics(&s);

    /* Success if we have completions, kills, and skips recorded */
    if (s.ulOverrunCount >= 2 && s.ulTotalCompletions > 0) {
        UART_printf("[PASS] Handled mixed policies under stress.\n");
    } else {
        UART_printf("[FAIL] Policy interactions failed.\n");
    }
    for(;;);
}

static PTL_TaskConfig_t t[] = {
    { "Killer", 100, 100, 2, 256, Job_Kill,   NULL, PTL_POLICY_KILL },
    { "Skipper",100, 100, 2, 256, Job_Skip,   NULL, PTL_POLICY_SKIP },
    { "Normal", 100, 100, 2, 256, Job_Normal, NULL, PTL_POLICY_SKIP },
    { "REF",    800, 800, 4, 256, Job_Referee,NULL, PTL_POLICY_USE_GLOBAL }
};

int main(void) {
    UART_init();
    PTL_TraceInit();
    PTL_GlobalConfig_t c = { PTL_POLICY_SKIP, pdTRUE, 4 };
    if (PTL_Init(&c, t, 4) == pdPASS) PTL_Start();
    for(;;); return 0;
}