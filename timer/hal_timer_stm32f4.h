#ifndef HAL_TIMER_STM32F4_H
#define HAL_TIMER_STM32F4_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "timer/hal_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Dedicated TIM2..TIM5 with a 1 ms update interrupt. The BSP supplies the
 * instance and NVIC priority/subpriority (according to its priority grouping).
 * Zero-initialize the remaining fields; keep the handle alive while running.
 * Forward the corresponding TIMx_IRQHandler to hal_timer_stm32f4_irq().
 */
struct hal_timer {
    TIM_TypeDef *const instance;
    const uint32_t irq_priority;
    const uint32_t irq_subpriority;
    TIM_HandleTypeDef driver;
    volatile uint32_t elapsed_millis;
    bool started;
};

/** Platform integration hook, called only from the selected timer's ISR.
 * Services this timer's update flag; does not dispatch other timer events.
 */
void hal_timer_stm32f4_irq(hal_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif
