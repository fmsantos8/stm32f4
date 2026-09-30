#include "hal_timer_stm32f4.h"
#include "port_status.h"

#include <stddef.h>

#define TIMER_RESOLUTION_HZ 1000000U

hal_status_t hal_timer_start(hal_timer_t *timer) {
    if (timer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (timer->started) {
        return HAL_STATUS_OK;
    }
    if (!IS_TIM_32B_COUNTER_INSTANCE(timer->instance) ||
        timer->clock_hz < TIMER_RESOLUTION_HZ ||
        timer->clock_hz % TIMER_RESOLUTION_HZ != 0U) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    timer->driver.Instance = timer->instance;
    timer->driver.Init = (TIM_Base_InitTypeDef){
        .Prescaler = timer->clock_hz / TIMER_RESOLUTION_HZ - 1U,
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = UINT32_MAX,
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
    };
    HAL_StatusTypeDef status = HAL_TIM_Base_Init(&timer->driver);
    if (status != HAL_OK) {
        HAL_TIM_Base_DeInit(&timer->driver);
        return hal_stm32_status(status);
    }

    TIM_ClockConfigTypeDef clock_config = {.ClockSource = TIM_CLOCKSOURCE_INTERNAL};
    status = HAL_TIM_ConfigClockSource(&timer->driver, &clock_config);
    if (status != HAL_OK) {
        HAL_TIM_Base_DeInit(&timer->driver);
        return hal_stm32_status(status);
    }

    __HAL_TIM_SET_COUNTER(&timer->driver, 0U);
    __HAL_TIM_CLEAR_FLAG(&timer->driver, TIM_FLAG_UPDATE);
    timer->overflows = 0U;
    status = HAL_TIM_Base_Start_IT(&timer->driver);
    if (status != HAL_OK) {
        HAL_TIM_Base_DeInit(&timer->driver);
        return hal_stm32_status(status);
    }
    timer->started = true;

    return HAL_STATUS_OK;
}

hal_status_t hal_timer_get_millis(hal_timer_t *timer, uint32_t *millis) {
    if (timer == NULL || millis == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!timer->started) {
        return HAL_STATUS_ERROR;
    }

    /* Snapshot the software high word and hardware low word atomically. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t high = timer->overflows;
    uint32_t low = __HAL_TIM_GET_COUNTER(&timer->driver);
    if (__HAL_TIM_GET_FLAG(&timer->driver, TIM_FLAG_UPDATE)) {
        /* Include a wrap even when its IRQ has not yet been serviced. */
        ++high;
        low = __HAL_TIM_GET_COUNTER(&timer->driver);
    }
    __set_PRIMASK(primask);

    uint64_t ticks = ((uint64_t)high << 32) | low;
    *millis = (uint32_t)(ticks / (TIMER_RESOLUTION_HZ / 1000U));
    return HAL_STATUS_OK;
}

void hal_timer_stm32_irq(hal_timer_t *timer) {
    if (timer == NULL || !timer->started) {
        return;
    }
    if (__HAL_TIM_GET_FLAG(&timer->driver, TIM_FLAG_UPDATE) &&
        __HAL_TIM_GET_IT_SOURCE(&timer->driver, TIM_IT_UPDATE)) {
        __HAL_TIM_CLEAR_IT(&timer->driver, TIM_IT_UPDATE);
        ++timer->overflows;
    }
}