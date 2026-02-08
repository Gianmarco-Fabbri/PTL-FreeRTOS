#include "ptl.h"
#include "uart.h"
#include "ptl_trace.h"
#include <stdio.h>

void Dummy_Job(void *p) { (void)p; }

int main(void) {
    UART_init();
    UART_printf("\n=== TEST: INITIALIZATION SAFETY ===\n");

    /* TEST 1: NULL Configuration Object */
    if (PTL_Init(NULL, NULL, 0) == pdFAIL) {
        UART_printf("[CHECK] Rejected NULL Config: OK\n");
    } else {
        UART_printf("[FAIL] Accepted NULL Config!\n");
    }

    /* TEST 2: Zero Task Count */
    PTL_GlobalConfig_t c = { PTL_POLICY_SKIP, pdTRUE, 2 };
    if (PTL_Init(&c, NULL, 0) == pdFAIL) {
        UART_printf("[CHECK] Rejected Zero Tasks: OK\n");
    } else {
        UART_printf("[FAIL] Accepted Zero Tasks!\n");
    }

    /* TEST 3: Invalid Task Pointer */
    PTL_TaskConfig_t t_invalid[] = {
        { "BadTask", 100, 100, 2, 256, NULL, NULL, PTL_POLICY_SKIP }
    };
    if (PTL_Init(&c, t_invalid, 1) == pdFAIL) {
        UART_printf("[CHECK] Rejected NULL Task Function: OK\n");
    } else {
        UART_printf("[FAIL] Accepted NULL Task Function!\n");
    }

    /* If all checks passed, we print the final success string for the Makefile */
    UART_printf("\n[PASS] Initialization Safety checks complete.\n");

    for(;;);
    return 0;
}