#include "hal_gpio_private.h"

bool hal_stm32f4_gpio_supported(GPIO_TypeDef *port, uint16_t pin)
{
    if (pin == 0U || (pin & (pin - 1U)) != 0U) {
        return false;
    }
    return
#if defined(GPIOA)
        port == GPIOA ||
#endif
#if defined(GPIOB)
        port == GPIOB ||
#endif
#if defined(GPIOC)
        port == GPIOC ||
#endif
#if defined(GPIOD)
        port == GPIOD ||
#endif
#if defined(GPIOE)
        port == GPIOE ||
#endif
#if defined(GPIOF)
        port == GPIOF ||
#endif
#if defined(GPIOG)
        port == GPIOG ||
#endif
#if defined(GPIOH)
        port == GPIOH ||
#endif
#if defined(GPIOI)
        port == GPIOI ||
#endif
#if defined(GPIOJ)
        port == GPIOJ ||
#endif
#if defined(GPIOK)
        port == GPIOK ||
#endif
        false;
}

bool hal_stm32f4_pin_supported(const hal_stm32f4_pin_t *pin)
{
    return hal_stm32f4_gpio_supported(pin->port, pin->pin) &&
           IS_GPIO_AF(pin->alternate) && IS_GPIO_PULL(pin->pull) &&
           IS_GPIO_SPEED(pin->speed);
}

void hal_stm32f4_gpio_enable_clock(GPIO_TypeDef *port)
{
#if defined(GPIOA)
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOB)
    if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOC)
    if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOD)
    if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOE)
    if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOF)
    if (port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOG)
    if (port == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOH)
    if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOI)
    if (port == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOJ)
    if (port == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        return;
    }
#endif
#if defined(GPIOK)
    if (port == GPIOK) {
        __HAL_RCC_GPIOK_CLK_ENABLE();
        return;
    }
#endif
}

void hal_stm32f4_pin_init(const hal_stm32f4_pin_t *pin, uint32_t mode)
{
    GPIO_InitTypeDef config = {
        .Pin = pin->pin, .Mode = mode, .Pull = pin->pull,
        .Speed = pin->speed, .Alternate = pin->alternate,
    };
    hal_stm32f4_gpio_enable_clock(pin->port);
    HAL_GPIO_Init(pin->port, &config);
}

hal_status_t hal_gpio_stm32f4_configure(hal_gpio_t *gpio, const hal_gpio_config_t *config)
{
    if (!hal_stm32f4_gpio_supported(gpio->port, gpio->pin) ||
        !IS_GPIO_SPEED(gpio->speed)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    GPIO_InitTypeDef driver_config = {
        .Pin = gpio->pin,
        .Mode = config->direction == HAL_GPIO_DIRECTION_OUTPUT
                    ? GPIO_MODE_OUTPUT_PP : GPIO_MODE_INPUT,
        .Pull = config->pull == HAL_GPIO_PULL_UP ? GPIO_PULLUP
                    : config->pull == HAL_GPIO_PULL_DOWN ? GPIO_PULLDOWN : GPIO_NOPULL,
        .Speed = gpio->speed,
    };
    hal_stm32f4_gpio_enable_clock(gpio->port);
    if (config->direction == HAL_GPIO_DIRECTION_OUTPUT) {
        /* Preload the latch before enabling output, including reconfiguration. */
        HAL_GPIO_WritePin(gpio->port, gpio->pin, GPIO_PIN_RESET);
    }
    HAL_GPIO_Init(gpio->port, &driver_config);
    gpio->direction = config->direction;
    gpio->configured = true;
    return HAL_STATUS_OK;
}

bool hal_gpio_stm32f4_read(hal_gpio_t *gpio)
{
    return HAL_GPIO_ReadPin(gpio->port, gpio->pin) == GPIO_PIN_SET;
}

void hal_gpio_stm32f4_write(hal_gpio_t *gpio, bool level)
{
    HAL_GPIO_WritePin(gpio->port, gpio->pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
