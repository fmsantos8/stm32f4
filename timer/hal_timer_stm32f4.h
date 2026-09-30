#ifndef HAL_TIMER_STM32F4_H
#define HAL_TIMER_STM32F4_H

#include "stm32f4xx_hal.h"
#include "timer/hal_timer.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete timer implementation for STM32F4.
 *
 */
struct hal_timer {
    TIM_TypeDef *const instance; /** Peripheral instance */
    const uint32_t clock_hz;     /** Configured clock frequency in Hz */
    TIM_HandleTypeDef driver;    /** Owned exclusively by the port */
    volatile uint32_t overflows; /** Number of times the timer has wrapped since start */
    bool started;                /** True after successful configuration and start */
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
