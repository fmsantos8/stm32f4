#ifndef HAL_UART_STM32F4_H
#define HAL_UART_STM32F4_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "uart/hal_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART peripheral selected by the BSP and managed by this port.
 *
 * Initialize with .instance = USARTx/UARTx, leaving mutable fields zero. The BSP
 * supplies HAL_UART_MspInit() to enable clocks and configure TX/RX alternate
 * functions. Configure the system/APB clocks first and keep them stable during
 * use. HAL_Init() and a running HAL tick are required for polling timeouts.
 *
 * Frames always carry eight data bits. Baud rates use 16x oversampling where
 * possible, otherwise 8x; rates outside the peripheral BRR range are rejected.
 * Writes block until transmission completes. Large buffers are sent in chunks
 * of at most UINT16_MAX bytes, each with a finite transmission-time allowance
 * plus 1000 ms. Calls must be serialized and made outside interrupt handlers.
 */
struct hal_uart {
    USART_TypeDef *const instance;
    UART_HandleTypeDef driver;
    uint32_t baud_rate;
    bool configured;
};

#ifdef __cplusplus
}
#endif

#endif
