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
 * @brief UART concrete implementation for STM32F4.
 *
 */
struct hal_uart {
    USART_TypeDef *const instance; /** Peripheral instance */
    UART_HandleTypeDef driver;     /** STM32Cube HAL handle managed by this port */
    uint32_t baud_rate;            /** Configured baud rate */
    bool configured;               /** True after successful configuration */
};

#ifdef __cplusplus
}
#endif

#endif
