#include "gpio/hal_gpio.h"

#include "hal_stm32f4_private.h"
#include <stm32f4xx_hal_gpio.h>

struct hal_gpio {
    GPIO_TypeDef *port;
    uint16_t mask;
    hal_gpio_direction_t direction;
    bool initialized;
};

_Static_assert(sizeof(hal_gpio_t) <= UINT32_MAX, "GPIO object exceeds arena size range");
_Static_assert(_Alignof(hal_gpio_t) <= _Alignof(max_align_t), "GPIO object requires extended alignment");

static inline bool is_gpio_config_valid(const hal_gpio_config_t *config) {
    if (config == NULL) {
        return false;
    }

    const bool direction_valid = config->direction == HAL_GPIO_INPUT || config->direction == HAL_GPIO_OUTPUT;
    const bool pull_valid = config->pull == HAL_GPIO_PULL_NONE ||
                            config->pull == HAL_GPIO_PULL_UP ||
                            config->pull == HAL_GPIO_PULL_DOWN;

    return direction_valid && pull_valid;
}

static uint32_t map_gpio_mode(hal_gpio_direction_t direction) {
    switch (direction) {
        case HAL_GPIO_OUTPUT:
            return GPIO_MODE_OUTPUT_PP;
        case HAL_GPIO_INPUT:
        default:
            return GPIO_MODE_INPUT;
    }
}

static uint32_t map_gpio_pull(hal_gpio_pull_t pull) {
    switch (pull) {
        case HAL_GPIO_PULL_UP:
            return GPIO_PULLUP;
        case HAL_GPIO_PULL_DOWN:
            return GPIO_PULLDOWN;
        case HAL_GPIO_PULL_NONE:
        default:
            return GPIO_NOPULL;
    }
}

hal_status_t hal_gpio_config(hal_gpio_t **gpio, const hal_gpio_config_t *config) {
    if (gpio == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    *gpio = NULL;
    if (!is_gpio_config_valid(config)) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    hal_status_t status = hal_stm32f4_pins_claim(config->pin, NULL);
    if (status != HAL_STATUS_OK) {
        return status;
    }
    void *storage = NULL;
    status = hal_stm32f4_memory_status(hal_memory_alloc((uint32_t)sizeof(hal_gpio_t), &storage));
    if (status != HAL_STATUS_OK) {
        hal_stm32f4_pin_release(config->pin);
        return status;
    }
    hal_gpio_t *object = storage;
    *object = (hal_gpio_t){
        .port = hal_stm32f4_pin_port(config->pin),
        .mask = (uint16_t)(1U << config->pin.number),
        .direction = config->direction,
    };
    hal_stm32f4_pin_clock_enable(config->pin);
    if (config->direction == HAL_GPIO_OUTPUT) {
        GPIO_PinState initial_state = config->initial_level ? GPIO_PIN_SET : GPIO_PIN_RESET;
        HAL_GPIO_WritePin(object->port, object->mask, initial_state);
    }
    GPIO_InitTypeDef sdk_config = {
        .Pin = object->mask,
        .Mode = map_gpio_mode(config->direction),
        .Pull = map_gpio_pull(config->pull),
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(object->port, &sdk_config);
    object->initialized = true;
    *gpio = object;
    return HAL_STATUS_OK;
}

hal_status_t hal_gpio_read(hal_gpio_t *gpio, bool *out_level) {
    if (gpio == NULL || out_level == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!gpio->initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (gpio->direction != HAL_GPIO_INPUT) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    *out_level = HAL_GPIO_ReadPin(gpio->port, gpio->mask) == GPIO_PIN_SET;
    return HAL_STATUS_OK;
}

hal_status_t hal_gpio_write(hal_gpio_t *gpio, bool level) {
    if (gpio == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!gpio->initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (gpio->direction != HAL_GPIO_OUTPUT) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    HAL_GPIO_WritePin(gpio->port, gpio->mask, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return HAL_STATUS_OK;
}
