#include "hal_gpio_stm32f4.h"

#include <stddef.h>

static bool is_valid_config(const hal_gpio_config_t *config)
{
    return config != NULL &&
           (config->direction == HAL_GPIO_DIRECTION_INPUT ||
            config->direction == HAL_GPIO_DIRECTION_OUTPUT) &&
           (config->pull == HAL_GPIO_PULL_NONE ||
            config->pull == HAL_GPIO_PULL_UP ||
            config->pull == HAL_GPIO_PULL_DOWN);
}

static bool is_supported_gpio(const hal_gpio_t *gpio)
{
    return IS_GPIO_ALL_INSTANCE(gpio->port) && gpio->pin != 0U &&
           (gpio->pin & (gpio->pin - 1U)) == 0U;
}

hal_status_t hal_gpio_config(hal_gpio_t *gpio, const hal_gpio_config_t *config)
{
    if (gpio == NULL || !is_valid_config(config)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!is_supported_gpio(gpio)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    GPIO_InitTypeDef driver_config = {
        .Pin = gpio->pin,
        .Mode = config->direction == HAL_GPIO_DIRECTION_OUTPUT
                    ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT,
        .Pull = config->pull == HAL_GPIO_PULL_UP ? GPIO_PULLUP
                : config->pull == HAL_GPIO_PULL_DOWN ? GPIO_PULLDOWN
                : GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
        .Alternate = 0U,
    };

    if (config->direction == HAL_GPIO_DIRECTION_OUTPUT) {
        /* Also reset an already configured output before reconfiguring it. */
        HAL_GPIO_WritePin(gpio->port, gpio->pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_Init(gpio->port, &driver_config);
    gpio->direction = config->direction;
    gpio->configured = true;
    return HAL_STATUS_OK;
}

hal_status_t hal_gpio_read(hal_gpio_t *gpio, bool *level)
{
    if (gpio == NULL || level == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!gpio->configured) {
        return HAL_STATUS_ERROR;
    }

    *level = HAL_GPIO_ReadPin(gpio->port, gpio->pin) == GPIO_PIN_SET;
    return HAL_STATUS_OK;
}

hal_status_t hal_gpio_write(hal_gpio_t *gpio, bool level)
{
    if (gpio == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!gpio->configured || gpio->direction != HAL_GPIO_DIRECTION_OUTPUT) {
        return HAL_STATUS_ERROR;
    }

    HAL_GPIO_WritePin(gpio->port, gpio->pin,
                      level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return HAL_STATUS_OK;
}
