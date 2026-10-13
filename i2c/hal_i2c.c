#include "hal_i2c_private.h"
#include "port_internal.h"
#include <limits.h>

hal_status_t hal_i2c_config(hal_i2c_t *i2c, const hal_i2c_config_t *config)
{
    if (i2c == NULL || config == NULL || config->clock_speed == 0U ||
        !hal_stm32f4_timeout_valid(i2c->timeout_ms)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    return hal_i2c_stm32f4_configure(i2c, config);
}

hal_status_t hal_i2c_write(hal_i2c_t *i2c, uint8_t address,
                          const uint8_t *data, uint32_t size)
{
    if (i2c == NULL || data == NULL || size == 0U || address > 0x7fU) {
        return HAL_STATUS_INVALID_PARAM;
    }
    /* Splitting would create separate transactions and change device semantics. */
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (!i2c->configured) {
        return HAL_STATUS_ERROR;
    }
    return hal_i2c_stm32f4_write(i2c, address, data, (uint16_t)size);
}

hal_status_t hal_i2c_read(hal_i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t size)
{
    if (i2c == NULL || data == NULL || size == 0U || address > 0x7fU) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (!i2c->configured) {
        return HAL_STATUS_ERROR;
    }
    return hal_i2c_stm32f4_read(i2c, address, data, (uint16_t)size);
}
