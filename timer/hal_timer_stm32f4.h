#ifndef HAL_TIMER_STM32F4_H
#define HAL_TIMER_STM32F4_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "timer/hal_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dedicated TIM2 or TIM5, zero-initialized except for instance and clock_hz.
 * clock_hz is the actual timer kernel clock supplied by the BSP (including
 * APB prescaler / TIMPRE effects), and must remain constant after start.
 * HAL_TIM_Base_MspInit must enable its clock and NVIC interrupt. The BSP's
 * TIMx_IRQHandler must call hal_timer_stm32_irq() directly for this handle.
 * Do not also dispatch this interrupt through HAL_TIM_IRQHandler().
 */
struct hal_timer {
    TIM_TypeDef *const instance;
    const uint32_t clock_hz;
    TIM_HandleTypeDef driver;   /** Owned exclusively by the port */
    volatile uint32_t overflows;
    bool started;
};

/**
 * Service the dedicated timer's update interrupt. Must run at least once per
 * hardware wrap (~71 minutes); normal millisecond reads need no polling rate.
 */
void hal_timer_stm32_irq(hal_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif
