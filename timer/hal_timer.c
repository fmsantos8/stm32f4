#include "hal_timer_stm32f4.h"
#include "port_internal.h"

static bool get_irq(TIM_TypeDef *instance, IRQn_Type *irq)
{
#if defined(TIM2)
    if (instance == TIM2) {
        *irq = TIM2_IRQn;
        return true;
    }
#endif
#if defined(TIM3)
    if (instance == TIM3) {
        *irq = TIM3_IRQn;
        return true;
    }
#endif
#if defined(TIM4)
    if (instance == TIM4) {
        *irq = TIM4_IRQn;
        return true;
    }
#endif
#if defined(TIM5)
    if (instance == TIM5) {
        *irq = TIM5_IRQn;
        return true;
    }
#endif
    return false;
}

static void enable_clock_and_reset(TIM_TypeDef *instance)
{
#if defined(TIM2)
    if (instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_TIM2_FORCE_RESET(); __HAL_RCC_TIM2_RELEASE_RESET(); return;
    }
#endif
#if defined(TIM3)
    if (instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
        __HAL_RCC_TIM3_FORCE_RESET(); __HAL_RCC_TIM3_RELEASE_RESET(); return;
    }
#endif
#if defined(TIM4)
    if (instance == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
        __HAL_RCC_TIM4_FORCE_RESET(); __HAL_RCC_TIM4_RELEASE_RESET(); return;
    }
#endif
#if defined(TIM5)
    if (instance == TIM5) {
        __HAL_RCC_TIM5_CLK_ENABLE();
        __HAL_RCC_TIM5_FORCE_RESET(); __HAL_RCC_TIM5_RELEASE_RESET(); return;
    }
#endif
}

static uint32_t timer_clock(void)
{
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    if (pclk == 0U || hclk < pclk || hclk % pclk != 0U) {
        return 0U;
    }
    uint32_t apb_divider = hclk / pclk;
#if defined(RCC_DCKCFGR_TIMPRE)
    if ((RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE) != 0U) {
        return apb_divider <= 4U ? hclk : pclk * 4U;
    }
#endif
    return apb_divider == 1U ? pclk : pclk * 2U;
}

hal_status_t hal_timer_start(hal_timer_t *timer)
{
    if (timer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (timer->started) {
        return HAL_STATUS_OK;
    }
    if (timer->irq_priority >= (1UL << __NVIC_PRIO_BITS) ||
        timer->irq_subpriority >= (1UL << __NVIC_PRIO_BITS)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    IRQn_Type irq;
    if (!get_irq(timer->instance, &irq)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint32_t clock = timer_clock();
    if (clock < 1000U || clock % 1000U != 0U) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint32_t ticks = clock / 1000U;
    uint32_t prescaler = (ticks + 65535U) / 65536U;
    while (prescaler <= 65536U && ticks % prescaler != 0U) {
        ++prescaler;
    }
    if (prescaler > 65536U) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    HAL_NVIC_DisableIRQ(irq);
    enable_clock_and_reset(timer->instance);
    timer->driver = (TIM_HandleTypeDef){
        .Instance = timer->instance,
        .Init = {
            .Prescaler = prescaler - 1U, .CounterMode = TIM_COUNTERMODE_UP,
            .Period = ticks / prescaler - 1U, .ClockDivision = TIM_CLOCKDIVISION_DIV1,
            .AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
        },
    };
    HAL_StatusTypeDef status = HAL_TIM_Base_Init(&timer->driver);
    if (status != HAL_OK) {
        return hal_stm32f4_status(status);
    }
    TIM_ClockConfigTypeDef clock_config = { .ClockSource = TIM_CLOCKSOURCE_INTERNAL };
    status = HAL_TIM_ConfigClockSource(&timer->driver, &clock_config);
    if (status != HAL_OK) {
        return hal_stm32f4_status(status);
    }
    __HAL_TIM_SET_COUNTER(&timer->driver, 0U);
    /* Base initialization generates an update event to load the prescaler. */
    __HAL_TIM_CLEAR_IT(&timer->driver, (uint32_t)TIM_IT_UPDATE);
    HAL_NVIC_ClearPendingIRQ(irq);
    HAL_NVIC_SetPriority(irq, timer->irq_priority, timer->irq_subpriority);
    timer->elapsed_millis = 0U;
    status = HAL_TIM_Base_Start_IT(&timer->driver);
    if (status != HAL_OK) {
        return hal_stm32f4_status(status);
    }
    timer->started = true;
    HAL_NVIC_EnableIRQ(irq);
    return HAL_STATUS_OK;
}

hal_status_t hal_timer_get_millis(hal_timer_t *timer, uint32_t *millis)
{
    if (timer == NULL || millis == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!timer->started || (timer->instance->CR1 & TIM_CR1_CEN) == 0U) {
        return HAL_STATUS_ERROR;
    }
    /* Naturally aligned 32-bit loads/stores are atomic on Cortex-M4. */
    *millis = timer->elapsed_millis;
    return HAL_STATUS_OK;
}

void hal_timer_stm32f4_irq(hal_timer_t *timer)
{
    if (timer != NULL && timer->started &&
        __HAL_TIM_GET_FLAG(&timer->driver, TIM_FLAG_UPDATE) != RESET &&
        __HAL_TIM_GET_IT_SOURCE(&timer->driver, TIM_IT_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&timer->driver, (uint32_t)TIM_IT_UPDATE);
        ++timer->elapsed_millis;
    }
}
