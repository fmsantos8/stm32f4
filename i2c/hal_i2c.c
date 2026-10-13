#include "hal_i2c_stm32f4.h"
#include "port_internal.h"
#include <limits.h>

static bool is_supported_instance(I2C_TypeDef *instance)
{
    return
#if defined(I2C1)
        instance == I2C1 ||
#endif
#if defined(I2C2)
        instance == I2C2 ||
#endif
#if defined(I2C3)
        instance == I2C3 ||
#endif
        false;
}

static void enable_clock_and_reset(I2C_TypeDef *instance)
{
#if defined(I2C1)
    if (instance == I2C1) {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_I2C1_FORCE_RESET(); __HAL_RCC_I2C1_RELEASE_RESET(); return;
    }
#endif
#if defined(I2C2)
    if (instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_I2C2_FORCE_RESET(); __HAL_RCC_I2C2_RELEASE_RESET(); return;
    }
#endif
#if defined(I2C3)
    if (instance == I2C3) {
        __HAL_RCC_I2C3_CLK_ENABLE();
        __HAL_RCC_I2C3_FORCE_RESET(); __HAL_RCC_I2C3_RELEASE_RESET(); return;
    }
#endif
}

static hal_status_t transfer_status(hal_i2c_t *i2c, HAL_StatusTypeDef status)
{
    if (status == HAL_ERROR && (HAL_I2C_GetError(&i2c->driver) & HAL_I2C_ERROR_TIMEOUT)) {
        return HAL_STATUS_TIMEOUT;
    }
    return hal_stm32f4_status(status);
}

hal_status_t hal_i2c_config(hal_i2c_t *i2c, const hal_i2c_config_t *config)
{
    if (i2c == NULL || config == NULL || config->clock_speed == 0U ||
        !hal_stm32f4_timeout_valid(i2c->timeout_ms)) {
        return HAL_STATUS_INVALID_PARAM;
    }
    if (!is_supported_instance(i2c->instance) ||
        !hal_stm32f4_pin_supported(&i2c->scl) ||
        !hal_stm32f4_pin_supported(&i2c->sda) ||
        (i2c->scl.port == i2c->sda.port && i2c->scl.pin == i2c->sda.pin) ||
        config->clock_speed > 400000U) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t coefficient = config->clock_speed <= 100000U ? 2U : 3U;
    uint64_t divisor = (uint64_t)config->clock_speed * coefficient;
    uint64_t ccr = ((uint64_t)pclk + divisor - 1U) / divisor;
    if (I2C_MIN_PCLK_FREQ(pclk, config->clock_speed) ||
        pclk / 1000000U > (I2C_CR2_FREQ >> I2C_CR2_FREQ_Pos) ||
        ccr > I2C_CCR_CCR) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    i2c->configured = false;
    enable_clock_and_reset(i2c->instance);
    hal_stm32f4_pin_init(&i2c->scl, GPIO_MODE_AF_OD);
    hal_stm32f4_pin_init(&i2c->sda, GPIO_MODE_AF_OD);
    i2c->driver = (I2C_HandleTypeDef){
        .Instance = i2c->instance,
        .Init = {
            .ClockSpeed = config->clock_speed, .DutyCycle = I2C_DUTYCYCLE_2,
            .OwnAddress1 = 0U, .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
            .DualAddressMode = I2C_DUALADDRESS_DISABLE, .OwnAddress2 = 0U,
            .GeneralCallMode = I2C_GENERALCALL_DISABLE,
            .NoStretchMode = I2C_NOSTRETCH_DISABLE,
        },
    };
    HAL_StatusTypeDef status = HAL_I2C_Init(&i2c->driver);
    i2c->configured = status == HAL_OK;
    return hal_stm32f4_status(status);
}

hal_status_t hal_i2c_write(hal_i2c_t *i2c, uint8_t address,
                           const uint8_t *data, uint32_t size)
{
    if (i2c == NULL || data == NULL || size == 0U || address > 0x7fU) {
        return HAL_STATUS_INVALID_PARAM;
    }
    /* Splitting would create separate I2C transactions and change device semantics. */
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (!i2c->configured) {
        return HAL_STATUS_ERROR;
    }
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(
        &i2c->driver, (uint16_t)address << 1U, (uint8_t *)data,
        (uint16_t)size, i2c->timeout_ms);
    return transfer_status(i2c, status);
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
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(
        &i2c->driver, (uint16_t)address << 1U, data, (uint16_t)size, i2c->timeout_ms);
    return transfer_status(i2c, status);
}
