#ifndef HAL_UART_STM32F4_PRIVATE_H
#define HAL_UART_STM32F4_PRIVATE_H

#include "hal_uart_stm32f4.h"

/* Internal operations: public entry points validate arguments and state. */
hal_status_t hal_uart_stm32f4_configure(hal_uart_t *uart, const hal_uart_config_t *config);
hal_status_t hal_uart_stm32f4_write(hal_uart_t *uart, const uint8_t *data, uint32_t size);

#endif
