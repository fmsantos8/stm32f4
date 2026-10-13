#ifndef HAL_UART_STM32F4_H
#define HAL_UART_STM32F4_H

#include <stdbool.h>
#include "port_pin.h"
#include "uart/hal_uart.h"

/** Blocking TX-only UART, eight data bits, optional parity, no flow control.
 * Supply the instance, TX pin and finite whole-write timeout in milliseconds.
 * Zero-initialize the driver/state; they belong exclusively to this port.
 */
struct hal_uart {
    USART_TypeDef *const instance;
    const hal_stm32f4_pin_t tx;
    const uint32_t timeout_ms;
    UART_HandleTypeDef driver;
    bool configured;
};

#endif
