#ifndef STM32F4_PORT_INTERNAL_H
#define STM32F4_PORT_INTERNAL_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "status/hal_status.h"

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
