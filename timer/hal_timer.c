#include "hal_timer_private.h"

hal_status_t hal_timer_start(hal_timer_t *timer)
{
    if (timer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (timer->started) {
        return HAL_STATUS_OK;
    }
    return hal_timer_stm32f4_start(timer);
}

hal_status_t hal_timer_get_millis(hal_timer_t *timer, uint32_t *millis)
{
    if (timer == NULL || millis == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }
    return hal_timer_stm32f4_get_millis(timer, millis);
}
