#include "hal_uart_stm32f4.h"
#include "port_status.h"

#include <stddef.h>

#define UART_TIMEOUT_MARGIN_MS 1000U
#define UART_MAX_FRAME_BITS    12U

static bool is_valid_config(const hal_uart_config_t *config)
{
    return config != NULL && config->baud_rate != 0U &&
           (config->parity == HAL_UART_PARITY_NONE ||
            config->parity == HAL_UART_PARITY_EVEN ||
            config->parity == HAL_UART_PARITY_ODD) &&
           (config->stop_bits == HAL_UART_STOP_BITS_1 ||
            config->stop_bits == HAL_UART_STOP_BITS_2);
}

static uint32_t peripheral_clock(const USART_TypeDef *instance)
{
    if (instance == USART1
#if defined(USART6)
        || instance == USART6
#endif
#if defined(UART9)
        || instance == UART9
#endif
#if defined(UART10)
        || instance == UART10
#endif
    ) {
        return HAL_RCC_GetPCLK2Freq();
    }
    return HAL_RCC_GetPCLK1Freq();
}

static bool select_oversampling(const hal_uart_t *uart, uint32_t baud_rate,
                                uint32_t *oversampling)
{
    if (!IS_UART_BAUDRATE(baud_rate)) {
        return false;
    }

    uint32_t clock_hz = peripheral_clock(uart->instance);
    uint32_t brr;
    if (baud_rate <= clock_hz / 16U) {
        *oversampling = UART_OVERSAMPLING_16;
        brr = UART_BRR_SAMPLING16(clock_hz, baud_rate);
    } else if (baud_rate <= clock_hz / 8U) {
        *oversampling = UART_OVERSAMPLING_8;
        brr = UART_BRR_SAMPLING8(clock_hz, baud_rate);
    } else {
        return false;
    }

    /* HAL_UART_Init otherwise silently truncates an overflowing divider. */
    return brr >= 0x10U && brr <= UINT16_MAX;
}

hal_status_t hal_uart_config(hal_uart_t *uart, const hal_uart_config_t *config)
{
    if (uart == NULL || !is_valid_config(config)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!IS_UART_INSTANCE(uart->instance)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    uint32_t oversampling;
    if (!select_oversampling(uart, config->baud_rate, &oversampling)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    uart->driver.Instance = uart->instance;
    uart->driver.Init.BaudRate = config->baud_rate;
    /* STM32 includes the parity bit in WordLength; retain eight payload bits. */
    uart->driver.Init.WordLength = config->parity == HAL_UART_PARITY_NONE
                                      ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B;
    uart->driver.Init.StopBits = config->stop_bits == HAL_UART_STOP_BITS_1
                                    ? UART_STOPBITS_1 : UART_STOPBITS_2;
    uart->driver.Init.Parity = config->parity == HAL_UART_PARITY_EVEN
                                  ? UART_PARITY_EVEN
                                  : config->parity == HAL_UART_PARITY_ODD
                                        ? UART_PARITY_ODD : UART_PARITY_NONE;
    uart->driver.Init.Mode = UART_MODE_TX_RX;
    uart->driver.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->driver.Init.OverSampling = oversampling;

    uart->configured = false;
    HAL_StatusTypeDef status = HAL_UART_Init(&uart->driver);
    if (status != HAL_OK) {
        return hal_stm32_status(status);
    }

    uart->baud_rate = config->baud_rate;
    uart->configured = true;
    return HAL_STATUS_OK;
}

hal_status_t hal_uart_write(hal_uart_t *uart, const uint8_t *data, uint32_t size)
{
    if (uart == NULL || data == NULL || size == 0U) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!uart->configured || uart->baud_rate == 0U) {
        return HAL_STATUS_ERROR;
    }

    uint32_t offset = 0U;
    while (offset < size) {
        uint32_t remaining = size - offset;
        uint16_t chunk = remaining > UINT16_MAX ? UINT16_MAX : (uint16_t)remaining;
        uint32_t timeout_ms = UART_TIMEOUT_MARGIN_MS +
            (uint32_t)(((uint64_t)chunk * UART_MAX_FRAME_BITS * 1000U +
                        uart->baud_rate - 1U) / uart->baud_rate);

        HAL_StatusTypeDef status = HAL_UART_Transmit(&uart->driver, data + offset,
                                                     chunk, timeout_ms);
        if (status != HAL_OK) {
            return hal_stm32_status(status);
        }
        offset += chunk;
    }
    return HAL_STATUS_OK;
}
