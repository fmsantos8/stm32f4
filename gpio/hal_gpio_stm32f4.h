#ifndef HAL_GPIO_STM32F4_H
#define HAL_GPIO_STM32F4_H

#include "stm32f4xx_hal.h"
#include "gpio/hal_gpio.h"

/** Initialize with a BSP-selected port, one GPIO_PIN_x mask, and SDK speed.
 * All remaining fields must initially be zero and are owned by the port.
 * Keep the handle alive and its physical resource unchanged while in use.
 */
struct hal_gpio {
    GPIO_TypeDef *const port;
    const uint16_t pin;
    const uint32_t speed;
    hal_gpio_direction_t direction;
    bool configured;
};

#endif
