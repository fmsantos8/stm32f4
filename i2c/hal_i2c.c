#include "hal_i2c_stm32f4.h"
#include "port_status.h"

#include <stddef.h>

#define I2C_TRANSFER_TIMEOUT_MS 1000U
#define I2C_STANDARD_CLOCK_HZ   100000U
#define I2C_MAX_CLOCK_HZ        400000U

static bool is_supported_clock(uint32_t clock_speed) {
    if (clock_speed > I2C_MAX_CLOCK_HZ) {
        return false;
    }

    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t minimum_pclk = clock_speed <= I2C_STANDARD_CLOCK_HZ ? 2000000U : 4000000U;
    if (pclk < minimum_pclk || pclk / 1000000U > I2C_CR2_FREQ) {
        return false;
    }

    /* Cube masks CCR to 12 bits. Reject slow clocks before that can wrap. */
    uint32_t coefficient = clock_speed <= I2C_STANDARD_CLOCK_HZ ? 2U : 3U;
    uint32_t divider = (pclk - 1U) / (clock_speed * coefficient) + 1U;
    return divider <= I2C_CCR_CCR;
}

static bool is_ready(const hal_i2c_t *i2c) {
    return i2c->configured && i2c->clock_speed != 0U &&
           i2c->driver.Instance == i2c->instance &&
           i2c->driver.State == HAL_I2C_STATE_READY &&
           i2c->driver.Lock == HAL_UNLOCKED;
}

static hal_status_t transfer_status(const hal_i2c_t *i2c, HAL_StatusTypeDef status) {
    /* Cube can return HAL_ERROR or HAL_BUSY for a polling timeout. */
    if (status != HAL_OK && (i2c->driver.ErrorCode & HAL_I2C_ERROR_TIMEOUT) != 0U) {
        return HAL_STATUS_TIMEOUT;
    }
    return hal_stm32_status(status);
}

hal_status_t hal_i2c_config(hal_i2c_t *i2c, const hal_i2c_config_t *config) {
    if (i2c == NULL || config == NULL || config->clock_speed == 0U) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!IS_I2C_ALL_INSTANCE(i2c->instance) || !is_supported_clock(config->clock_speed)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    i2c->configured = false;
    i2c->clock_speed = 0U;
    i2c->driver.Instance = i2c->instance;
    i2c->driver.Init.ClockSpeed = config->clock_speed;
    i2c->driver.Init.DutyCycle = I2C_DUTYCYCLE_2;
    i2c->driver.Init.OwnAddress1 = 0U;
    i2c->driver.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c->driver.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c->driver.Init.OwnAddress2 = 0U;
    i2c->driver.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c->driver.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    HAL_StatusTypeDef status = HAL_I2C_Init(&i2c->driver);
    if (status != HAL_OK) {
        return hal_stm32_status(status);
    }
    i2c->clock_speed = config->clock_speed;
    i2c->configured = true;
    return HAL_STATUS_OK;
}

hal_status_t hal_i2c_write(hal_i2c_t *i2c, uint8_t address, const uint8_t *data, uint32_t size) {
    if (i2c == NULL || data == NULL || size == 0U || address > 0x7fU) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (!is_ready(i2c)) {
        return HAL_STATUS_ERROR;
    }

    /* The Cube transmit API does not modify data despite its non-const type. */
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &i2c->driver, (uint16_t)((uint16_t)address << 1U), (uint8_t *)data,
        (uint16_t)size, I2C_TRANSFER_TIMEOUT_MS);
    return transfer_status(i2c, status);
}

hal_status_t hal_i2c_read(hal_i2c_t *i2c, uint8_t address, uint8_t *data, uint32_t size) {
    if (i2c == NULL || data == NULL || size == 0U || address > 0x7fU) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (!is_ready(i2c)) {
        return HAL_STATUS_ERROR;
    }

    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(
        &i2c->driver, (uint16_t)((uint16_t)address << 1U), data,
        (uint16_t)size, I2C_TRANSFER_TIMEOUT_MS);
    return transfer_status(i2c, status);
}
