#ifndef STM32F4_PORT_STATUS_H
#define STM32F4_PORT_STATUS_H

#include "stm32f4xx_hal.h"
#include "status/hal_status.h"

static inline hal_status_t hal_stm32_status(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:
            return HAL_STATUS_OK;
        case HAL_TIMEOUT:
            return HAL_STATUS_TIMEOUT;
        default:
            /* The portable API has no BUSY code. The caller serializes access. */
            return HAL_STATUS_ERROR;
    }
}

#endif
