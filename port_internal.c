#include "port_internal.h"

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
