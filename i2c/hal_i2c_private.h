#ifndef HAL_I2C_STM32F4_PRIVATE_H
#define HAL_I2C_STM32F4_PRIVATE_H

#include "hal_i2c_stm32f4.h"

/* Internal operations: public entry points validate arguments and state. */
hal_status_t hal_i2c_stm32f4_configure(hal_i2c_t *i2c, const hal_i2c_config_t *config);
hal_status_t hal_i2c_stm32f4_write(hal_i2c_t *i2c, uint8_t address,
                                 const uint8_t *data, uint16_t size);
hal_status_t hal_i2c_stm32f4_read(hal_i2c_t *i2c, uint8_t address, uint8_t *data, uint16_t size);

#endif
