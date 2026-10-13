#include "hal_timer_stm32f4.h"

void hal_timer_stm32f4_irq(hal_timer_t *timer)
{
    if (timer != NULL && timer->started &&
        __HAL_TIM_GET_FLAG(&timer->driver, TIM_FLAG_UPDATE) != RESET &&
        __HAL_TIM_GET_IT_SOURCE(&timer->driver, TIM_IT_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&timer->driver, (uint32_t)TIM_IT_UPDATE);
        ++timer->elapsed_millis;
    }
}
