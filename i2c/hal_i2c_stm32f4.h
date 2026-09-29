#ifndef HAL_I2C_STM32F4_H
#define HAL_I2C_STM32F4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "i2c/hal_i2c.h"

/**
 * @brief Concrete I2C controller implementation for STM32F4.
 *
 * The BSP supplies the instance and initializes the remaining fields to zero.
 * HAL_I2C_MspInit configures its peripheral clock and SDA/SCL alternate-function
 * open-drain pins. The board must provide suitable pull-up resistors.
 *
 * The port owns driver state and supports blocking, 7-bit controller transfers
 * of up to UINT16_MAX bytes with a 1000 ms transfer timeout. Access must be
 * serialized by the caller, with the STM32 HAL tick running during transfers.
 */
struct hal_i2c {
    I2C_TypeDef *const instance; /** Peripheral selected by the BSP */
    I2C_HandleTypeDef driver;   /** STM32Cube HAL handle managed by this port */
    uint32_t clock_speed;      /** Configured clock speed in Hz */
    bool configured;           /** True after successful configuration */
};

#ifdef __cplusplus
}
#endif

#endif
