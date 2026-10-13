#ifndef HAL_GPIO_STM32F4_PRIVATE_H
#define HAL_GPIO_STM32F4_PRIVATE_H

#include "hal_gpio_stm32f4.h"
#include "port_pin.h"

/* Shared physical-pin helpers used by GPIO, I2C and UART implementations. */
bool hal_stm32f4_gpio_supported(GPIO_TypeDef *port, uint16_t pin);
bool hal_stm32f4_pin_supported(const hal_stm32f4_pin_t *pin);
void hal_stm32f4_gpio_enable_clock(GPIO_TypeDef *port);
void hal_stm32f4_pin_init(const hal_stm32f4_pin_t *pin, uint32_t mode);

/* Internal operations: public entry points validate arguments and state. */
hal_status_t hal_gpio_stm32f4_configure(hal_gpio_t *gpio, const hal_gpio_config_t *config);
bool hal_gpio_stm32f4_read(hal_gpio_t *gpio);
void hal_gpio_stm32f4_write(hal_gpio_t *gpio, bool level);

#endif
