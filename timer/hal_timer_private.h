#ifndef HAL_TIMER_STM32F4_PRIVATE_H
#define HAL_TIMER_STM32F4_PRIVATE_H

#include "hal_timer_stm32f4.h"

/* Internal operations: public entry points validate pointers and start state. */
hal_status_t hal_timer_stm32f4_start(hal_timer_t *timer);
hal_status_t hal_timer_stm32f4_get_millis(hal_timer_t *timer, uint32_t *millis);

#endif
