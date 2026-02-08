#include "FreeRTOS.h"
#include "task.h"
#include "burner.h"
#include "uart.h"



static volatile uint32_t g_LoopsPerMs = 250000;


void Burn_Calibrate(void) {
    TickType_t xStartTick, xCurrentTick;
    volatile uint32_t ulTotalLoops = 0;
    const uint32_t ulBlockSize = 1000; 
    const TickType_t xCalibDuration = 100; 

    /* 1. SYNC */
    xStartTick = xTaskGetTickCount();
    while (xTaskGetTickCount() == xStartTick) { __asm("nop"); }

    /* 2. MEASURE */
    xStartTick = xTaskGetTickCount();
    while (1) {
        for (volatile uint32_t i = 0; i < ulBlockSize; i++) { __asm("nop"); }
        ulTotalLoops += ulBlockSize;
        xCurrentTick = xTaskGetTickCount();
        if ((xCurrentTick - xStartTick) >= xCalibDuration) break;
    }

    /* 3. CALCULATE WITH SAFETY FACTOR */
    if (ulTotalLoops > 0 && xCalibDuration > 0) {
        g_LoopsPerMs = ulTotalLoops / xCalibDuration;
        
 

        char buf[16]; char *ptr = &buf[15]; *ptr = '\0';
        uint32_t temp = g_LoopsPerMs;
        do { *--ptr = (temp % 10) + '0'; temp /= 10; } while(temp);
        UART_printf(ptr);
        UART_printf(" loops/ms\n");
        

    }
}


void Burn(uint32_t ulMillis) {
    volatile uint32_t i, j;

    for (i = 0; i < ulMillis; i++) {
        for (j = 0; j < g_LoopsPerMs; j++) {
            __asm("nop");
        }
    }
}