#ifndef STM32F4_PORT_PIN_H
#define STM32F4_PORT_PIN_H

#include "stm32f4xx_hal.h"

/** Physical alternate-function pin supplied by the BSP. Use one GPIO_PIN_x
 * mask, a GPIO_AFx_xxx value, and SDK GPIO_PULL/SPEED constants. The BSP must
 * verify the peripheral mapping and pin availability for its MCU package.
 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t alternate;
    uint32_t pull;
    uint32_t speed;
} hal_stm32f4_pin_t;

#endif
