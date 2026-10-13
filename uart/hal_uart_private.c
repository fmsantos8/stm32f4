#include "hal_uart_private.h"
#include "gpio/hal_gpio_private.h"
#include "port_internal.h"
#include <limits.h>

static bool is_supported_instance(USART_TypeDef *instance)
{
    return
#if defined(USART1)
        instance == USART1 ||
#endif
#if defined(USART2)
        instance == USART2 ||
#endif
#if defined(USART3)
        instance == USART3 ||
#endif
#if defined(UART4)
        instance == UART4 ||
#endif
#if defined(UART5)
        instance == UART5 ||
#endif
#if defined(USART6)
        instance == USART6 ||
#endif
#if defined(UART7)
        instance == UART7 ||
#endif
#if defined(UART8)
        instance == UART8 ||
#endif
        false;
}

static uint32_t peripheral_clock(USART_TypeDef *instance)
{
#if defined(USART1)
    if (instance == USART1) {
        return HAL_RCC_GetPCLK2Freq();
    }
#endif
#if defined(USART6)
    if (instance == USART6) {
        return HAL_RCC_GetPCLK2Freq();
    }
#endif
    return HAL_RCC_GetPCLK1Freq();
}

static void enable_clock_and_reset(USART_TypeDef *instance)
{
#if defined(USART1)
    if (instance == USART1) {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_USART1_FORCE_RESET();
        __HAL_RCC_USART1_RELEASE_RESET();
        return;
    }
#endif
#if defined(USART2)
    if (instance == USART2) {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_USART2_FORCE_RESET();
        __HAL_RCC_USART2_RELEASE_RESET();
        return;
    }
#endif
#if defined(USART3)
    if (instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_USART3_FORCE_RESET();
        __HAL_RCC_USART3_RELEASE_RESET();
        return;
    }
#endif
#if defined(UART4)
    if (instance == UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_UART4_FORCE_RESET();
        __HAL_RCC_UART4_RELEASE_RESET();
        return;
    }
#endif
#if defined(UART5)
    if (instance == UART5) {
        __HAL_RCC_UART5_CLK_ENABLE();
        __HAL_RCC_UART5_FORCE_RESET();
        __HAL_RCC_UART5_RELEASE_RESET();
        return;
    }
#endif
#if defined(USART6)
    if (instance == USART6) {
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_USART6_FORCE_RESET();
        __HAL_RCC_USART6_RELEASE_RESET();
        return;
    }
#endif
#if defined(UART7)
    if (instance == UART7) {
        __HAL_RCC_UART7_CLK_ENABLE();
        __HAL_RCC_UART7_FORCE_RESET();
        __HAL_RCC_UART7_RELEASE_RESET();
        return;
    }
#endif
#if defined(UART8)
    if (instance == UART8) {
        __HAL_RCC_UART8_CLK_ENABLE();
        __HAL_RCC_UART8_FORCE_RESET();
        __HAL_RCC_UART8_RELEASE_RESET();
        return;
    }
#endif
}

hal_status_t hal_uart_stm32f4_configure(hal_uart_t *uart, const hal_uart_config_t *config)
{
    if (!is_supported_instance(uart->instance) || !hal_stm32f4_pin_supported(&uart->tx) ||
        !IS_UART_BAUDRATE(config->baud_rate)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint32_t pclk = peripheral_clock(uart->instance);
    uint64_t divider = ((uint64_t)pclk + config->baud_rate / 2U) / config->baud_rate;
    if (divider < 16U || divider > UINT16_MAX ||
        UART_BRR_SAMPLING16(pclk, config->baud_rate) < 16U ||
        UART_BRR_SAMPLING16(pclk, config->baud_rate) > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    uart->configured = false;
    enable_clock_and_reset(uart->instance);
    hal_stm32f4_pin_init(&uart->tx, GPIO_MODE_AF_PP);
    uart->driver = (UART_HandleTypeDef){
        .Instance = uart->instance,
        .Init = {
            .BaudRate = config->baud_rate,
            /* STM32's word length includes the parity bit: preserve eight data bits. */
            .WordLength = config->parity == HAL_UART_PARITY_NONE
                              ? UART_WORDLENGTH_8B : UART_WORDLENGTH_9B,
            .StopBits = config->stop_bits == HAL_UART_STOP_BITS_1
                            ? UART_STOPBITS_1 : UART_STOPBITS_2,
            .Parity = config->parity == HAL_UART_PARITY_EVEN ? UART_PARITY_EVEN
                          : config->parity == HAL_UART_PARITY_ODD ? UART_PARITY_ODD
                          : UART_PARITY_NONE,
            .Mode = UART_MODE_TX, .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
    };
    HAL_StatusTypeDef status = HAL_UART_Init(&uart->driver);
    uart->configured = status == HAL_OK;
    return hal_stm32f4_status(status);
}

hal_status_t hal_uart_stm32f4_write(hal_uart_t *uart, const uint8_t *data, uint32_t size)
{
    uint32_t start = HAL_GetTick();
    uint32_t offset = 0U;
    while (offset < size) {
        uint32_t elapsed = HAL_GetTick() - start;
        if (elapsed >= uart->timeout_ms) {
            return HAL_STATUS_TIMEOUT;
        }
        uint32_t chunk = size - offset;
        if (chunk > UINT16_MAX) {
            chunk = UINT16_MAX;
        }
        HAL_StatusTypeDef status = HAL_UART_Transmit(
            &uart->driver, data + offset, (uint16_t)chunk, uart->timeout_ms - elapsed);
        if (status != HAL_OK) {
            return hal_stm32f4_status(status);
        }
        offset += chunk;
    }
    return HAL_STATUS_OK;
}
