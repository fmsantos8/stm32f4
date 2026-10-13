#ifndef STM32F4_PORT_INTERNAL_H
#define STM32F4_PORT_INTERNAL_H

#include <stdbool.h>
#include "port_pin.h"
#include "status/hal_status.h"

bool hal_stm32f4_gpio_supported(GPIO_TypeDef *port, uint16_t pin);
bool hal_stm32f4_pin_supported(const hal_stm32f4_pin_t *pin);
void hal_stm32f4_gpio_enable_clock(GPIO_TypeDef *port);
void hal_stm32f4_pin_init(const hal_stm32f4_pin_t *pin, uint32_t mode);

static inline hal_status_t hal_stm32f4_status(HAL_StatusTypeDef status)
{
    switch (status) {
    case HAL_OK:
        return HAL_STATUS_OK;
    case HAL_TIMEOUT:
        return HAL_STATUS_TIMEOUT;
    default:
        return HAL_STATUS_ERROR;
    }
}

static inline bool hal_stm32f4_timeout_valid(uint32_t timeout_ms)
{
    return timeout_ms > 0U && timeout_ms < HAL_MAX_DELAY;
}

#endif
