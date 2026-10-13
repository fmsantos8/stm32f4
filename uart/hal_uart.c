#include "hal_uart_private.h"
#include "port_internal.h"

hal_status_t hal_uart_config(hal_uart_t *uart, const hal_uart_config_t *config)
{
    if (uart == NULL || config == NULL || config->baud_rate == 0U ||
        !hal_stm32f4_timeout_valid(uart->timeout_ms) ||
        (config->parity != HAL_UART_PARITY_NONE && config->parity != HAL_UART_PARITY_EVEN &&
         config->parity != HAL_UART_PARITY_ODD) ||
        (config->stop_bits != HAL_UART_STOP_BITS_1 && config->stop_bits != HAL_UART_STOP_BITS_2)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    return hal_uart_stm32f4_configure(uart, config);
}

hal_status_t hal_uart_write(hal_uart_t *uart, const uint8_t *data, uint32_t size)
{
    if (uart == NULL || data == NULL || size == 0U) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!uart->configured) {
        return HAL_STATUS_ERROR;
    }
    return hal_uart_stm32f4_write(uart, data, size);
}
