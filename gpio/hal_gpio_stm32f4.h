#ifndef HAL_GPIO_STM32F4_H
#define HAL_GPIO_STM32F4_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"
#include "gpio/hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete GPIO implementation for STM32F4 series microcontrollers.
 * 
 */
struct hal_gpio {
    GPIO_TypeDef *const port;
    const uint16_t pin;             /** One GPIO_PIN_x mask, never a pin number. */
    hal_gpio_direction_t direction;
    bool configured;
};

#ifdef __cplusplus
}
#endif

#endif
