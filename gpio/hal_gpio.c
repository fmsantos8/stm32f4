#include "hal_gpio_private.h"

hal_status_t hal_gpio_config(hal_gpio_t *gpio, const hal_gpio_config_t *config)
{
    if (gpio == NULL || config == NULL ||
        (config->direction != HAL_GPIO_DIRECTION_INPUT &&
         config->direction != HAL_GPIO_DIRECTION_OUTPUT) ||
        (config->pull != HAL_GPIO_PULL_NONE && config->pull != HAL_GPIO_PULL_UP &&
         config->pull != HAL_GPIO_PULL_DOWN)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    return hal_gpio_stm32f4_configure(gpio, config);
}

hal_status_t hal_gpio_read(hal_gpio_t *gpio, bool *level)
{
    if (gpio == NULL || level == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!gpio->configured) {
        return HAL_STATUS_ERROR;
    }
    *level = hal_gpio_stm32f4_read(gpio);
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
    hal_gpio_stm32f4_write(gpio, level);
    return HAL_STATUS_OK;
}
