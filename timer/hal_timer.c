#include "timer/hal_timer.h"
#include "hal_stm32f4_private.h"

#include <stdatomic.h>

/** Four dedicated resources shared by F411 and F407: TIM2, TIM3, TIM4, TIM5. */
struct hal_timer {
    TIM_HandleTypeDef device;
    hal_timer_callback_t callback;
    IRQn_Type interrupt;
    atomic_bool running;
    bool initialized;
};

_Static_assert(sizeof(hal_timer_t) <= UINT32_MAX, "Timer object exceeds arena size range");
_Static_assert(_Alignof(hal_timer_t) <= _Alignof(max_align_t), "Timer object requires extended alignment");
_Static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "Timer ISR requires lock-free atomic bool");

static hal_timer_t *timers[4] = {0};
static TIM_TypeDef *const controllers[] = {TIM2, TIM3, TIM4, TIM5};
static const IRQn_Type interrupts[] = {TIM2_IRQn, TIM3_IRQn, TIM4_IRQn, TIM5_IRQn};

static uint32_t timer_clock(void) {
    uint32_t prescaler = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    uint32_t divider = prescaler < 4 ? 1U : 1U << (prescaler - 3U);
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
#if defined(RCC_DCKCFGR_TIMPRE)
    if ((RCC->DCKCFGR & RCC_DCKCFGR_TIMPRE) != 0) {
        return divider <= 4 ? HAL_RCC_GetHCLKFreq() : clock * 4U;
    }
#endif
    return divider == 1 ? clock : clock * 2U;
}

/** Only exact integer divisors are accepted; never approximate a period. */
static hal_status_t calculate_period(uint8_t instance, uint32_t period, uint32_t *prescaler, uint32_t *reload) {
    uint64_t cycles = (uint64_t)timer_clock() * period;
    if (cycles == 0 || cycles % 1000U != 0) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    cycles /= 1000U;
    uint64_t maximum_reload = instance == 0 || instance == 3 ? UINT64_C(0x100000000) : UINT64_C(0x10000);
    uint64_t first_divisor = (cycles + maximum_reload - 1U) / maximum_reload;
    for (uint64_t divisor = first_divisor; divisor <= 65536U; ++divisor) {
        if (cycles % divisor == 0) {
            *prescaler = (uint32_t)(divisor - 1U);
            *reload = (uint32_t)(cycles / divisor - 1U);
            return HAL_STATUS_OK;
        }
    }
    return HAL_STATUS_NOT_SUPPORTED;
}

static hal_status_t validate_config(const hal_timer_config_t *config, uint32_t *prescaler, uint32_t *reload) {
    if (config->instance >= 4 || config->period == 0 || config->callback == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    hal_memory_info_t info;
    hal_status_t status = hal_stm32f4_memory_status(hal_memory_get_info(&info));
    if (status != HAL_STATUS_OK) {
        return status;
    }
    if (info.sealed) {
        return HAL_STATUS_INVALID_STATE;
    }

    return calculate_period(config->instance, config->period, prescaler, reload);
}

static void enable_and_reset(uint8_t instance) {
    switch (instance) {
        case 0:
            __HAL_RCC_TIM2_CLK_ENABLE();
            __HAL_RCC_TIM2_FORCE_RESET();
            __HAL_RCC_TIM2_RELEASE_RESET();
            break;
        case 1:
            __HAL_RCC_TIM3_CLK_ENABLE();
            __HAL_RCC_TIM3_FORCE_RESET();
            __HAL_RCC_TIM3_RELEASE_RESET();
            break;
        case 2:
            __HAL_RCC_TIM4_CLK_ENABLE();
            __HAL_RCC_TIM4_FORCE_RESET();
            __HAL_RCC_TIM4_RELEASE_RESET();
            break;
        case 3:
            __HAL_RCC_TIM5_CLK_ENABLE();
            __HAL_RCC_TIM5_FORCE_RESET();
            __HAL_RCC_TIM5_RELEASE_RESET();
            break;
        default:
            break;
    }
}

/** Reserve the hardware timer and allocate its persistent object. */
static hal_status_t allocate_timer(hal_timer_t **out_object, const hal_timer_config_t *config) {
    if (timers[config->instance] != NULL || (controllers[config->instance]->CR1 & TIM_CR1_CEN) != 0) {
        return HAL_STATUS_RESOURCE_BUSY;
    }

    void *storage = NULL;
    hal_status_t status = hal_stm32f4_memory_status(hal_memory_alloc((uint32_t)sizeof(hal_timer_t), &storage));
    if (status != HAL_STATUS_OK) {
        return status;
    }

    hal_timer_t *object = storage;
    // Reserve even if subsequent SDK configuration fails: arena has no rollback.
    timers[config->instance] = object;
    object->device = (TIM_HandleTypeDef){.Instance = controllers[config->instance]};
    object->callback = config->callback;
    object->interrupt = interrupts[config->instance];
    object->initialized = false;
    atomic_init(&object->running, false);
    *out_object = object;

    return HAL_STATUS_OK;
}

static hal_status_t configure_timer(hal_timer_t *object, uint8_t instance, uint32_t prescaler, uint32_t reload) {
    object->device.Init = (TIM_Base_InitTypeDef){
        .Prescaler = prescaler,
        .CounterMode = TIM_COUNTERMODE_UP,
        .Period = reload,
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
    };
    enable_and_reset(instance);
    HAL_StatusTypeDef result = HAL_TIM_Base_Init(&object->device);
    if (result == HAL_OK) {
        TIM_ClockConfigTypeDef clock = {.ClockSource = TIM_CLOCKSOURCE_INTERNAL};
        result = HAL_TIM_ConfigClockSource(&object->device, &clock);
    }
    if (result != HAL_OK) {
        return hal_stm32f4_sdk_status(result);
    }
    __HAL_TIM_DISABLE_IT(&object->device, TIM_IT_UPDATE);
    __HAL_TIM_DISABLE(&object->device);
    __HAL_TIM_CLEAR_FLAG(&object->device, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(object->interrupt);
    HAL_NVIC_SetPriority(object->interrupt, 5, 0);
    return HAL_STATUS_OK;
}

static void service_interrupt(uint8_t instance) {
    hal_timer_t *timer = timers[instance];
    if (timer != NULL && (timer->device.Instance->SR & TIM_SR_UIF) != 0 &&
        (timer->device.Instance->DIER & TIM_DIER_UIE) != 0) {
        __HAL_TIM_CLEAR_IT(&timer->device, TIM_IT_UPDATE);
        if (atomic_load_explicit(&timer->running, memory_order_acquire)) {
            timer->callback();
        }
    }
}

void TIM2_IRQHandler(void) {
    service_interrupt(0);
}
void TIM3_IRQHandler(void) {
    service_interrupt(1);
}
void TIM4_IRQHandler(void) {
    service_interrupt(2);
}
void TIM5_IRQHandler(void) {
    service_interrupt(3);
}

hal_status_t hal_timer_config(hal_timer_t **timer, const hal_timer_config_t *config) {
    if (timer == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    *timer = NULL;
    if (config == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    uint32_t prescaler;
    uint32_t reload;
    hal_status_t status = validate_config(config, &prescaler, &reload);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    hal_timer_t *object = NULL;
    status = allocate_timer(&object, config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    status = configure_timer(object, config->instance, prescaler, reload);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    object->initialized = true;
    HAL_NVIC_EnableIRQ(object->interrupt);
    *timer = object;
    return HAL_STATUS_OK;
}

hal_status_t hal_timer_start(hal_timer_t *timer) {
    if (timer == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!timer->initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (atomic_load_explicit(&timer->running, memory_order_acquire)) {
        return HAL_STATUS_INVALID_STATE;
    }
    uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    // Reload the prescaler and reset its phase before the first complete period.
    __HAL_TIM_SET_COUNTER(&timer->device, 0);
    timer->device.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&timer->device, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(timer->interrupt);
    atomic_store_explicit(&timer->running, true, memory_order_release);
    HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(&timer->device);
    if (result != HAL_OK) {
        __HAL_TIM_DISABLE_IT(&timer->device, TIM_IT_UPDATE);
        __HAL_TIM_DISABLE(&timer->device);
        __HAL_TIM_CLEAR_FLAG(&timer->device, TIM_FLAG_UPDATE);
        HAL_NVIC_ClearPendingIRQ(timer->interrupt);
        atomic_store_explicit(&timer->running, false, memory_order_release);
    }
    __set_PRIMASK(interrupt_mask);
    return result == HAL_OK ? HAL_STATUS_OK : HAL_STATUS_ERROR;
}
