#ifndef HAL_I2C_STM32F4_H
#define HAL_I2C_STM32F4_H

#include <stdbool.h>
#include "port_pin.h"
#include "i2c/hal_i2c.h"

/** Master, 7-bit addressing, blocking transfers. Supply the instance, two
 * physical pins and a finite timeout in milliseconds (1..HAL_MAX_DELAY-1).
 * The BSP supplies suitable external pull-ups and electrical pin settings.
 * Zero-initialize the driver/state; they belong exclusively to this port.
 */
struct hal_i2c {
    I2C_TypeDef *const instance;
    const hal_stm32f4_pin_t scl;
    const hal_stm32f4_pin_t sda;
    const uint32_t timeout_ms;
    I2C_HandleTypeDef driver;
    bool configured;
};

#endif
