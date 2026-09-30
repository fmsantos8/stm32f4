#ifndef HAL_I2C_STM32F4_H
#define HAL_I2C_STM32F4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "i2c/hal_i2c.h"
#include "stm32f4xx_hal.h"

/**
 * @brief Concrete I2C controller implementation for STM32F4.
 *
 */
struct hal_i2c {
    I2C_TypeDef *const instance; /** Peripheral selected by the BSP */
    I2C_HandleTypeDef driver;    /** STM32Cube HAL handle managed by this port */
    uint32_t clock_speed;        /** Configured clock speed in Hz */
    bool configured;             /** True after successful configuration */
};

#ifdef __cplusplus
}
#endif

#endif
