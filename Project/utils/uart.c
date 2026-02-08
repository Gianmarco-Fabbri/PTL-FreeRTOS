/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file uart.c
 * @brief Simple UART Driver Implementation.
 *
 * Implements serial output for the MPS2-AN385 platform on QEMU.
 *
 * @author Member 2 (Group 38)
 */

#include "uart.h"
#include <stdint.h>

/*-----------------------------------------------------------*/
/* PRIVATE DEFINITIONS                                        */
/*-----------------------------------------------------------*/

/** @brief UART0 Base Address on MPS2-AN385. */
#define UART0_BASE (0x40004000)

/** @brief UART0 Data Register. */
#define UART0_DR (*(volatile uint32_t *)(UART0_BASE + 0x00))

/** @brief UART0 State Register. */
#define UART0_STATE (*(volatile uint32_t *)(UART0_BASE + 0x04))

/** @brief UART0 Control Register. */
#define UART0_CTRL (*(volatile uint32_t *)(UART0_BASE + 0x08))

/*-----------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                           */
/*-----------------------------------------------------------*/

/**
 * @brief Initialize the UART.
 */
void UART_init(void) { UART0_CTRL = 1; /* Transmit Enable */ }
/*-----------------------------------------------------------*/

/**
 * @brief Print a null-terminated string to UART.
 *
 * @param[in] s Pointer to string.
 */
void UART_printf(const char *s) {
  while (*s != '\0') {
    UART0_DR = (uint32_t)(*s);
    s++;
  }
}
/*-----------------------------------------------------------*/
