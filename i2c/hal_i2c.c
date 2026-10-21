#include "i2c/hal_i2c.h"
#include "hal_stm32f4_private.h"

struct hal_i2c {
    I2C_HandleTypeDef device;
    uint32_t timeout;
    bool initialized;
};

_Static_assert(sizeof(hal_i2c_t) <= UINT32_MAX, "I2C object exceeds arena size range");
_Static_assert(_Alignof(hal_i2c_t) <= _Alignof(max_align_t), "I2C object requires extended alignment");

static bool claimed_controllers[3];
static I2C_TypeDef *const controllers[] = {I2C1, I2C2, I2C3};

/** Return the actual AF number, or -1 for an unsupported signal route. */
static int32_t alternate_function(uint8_t instance, hal_pin_t pin, bool data) {
    if (instance == 0 && pin.port == 1 &&
        (pin.number == (data ? 7 : 6) || pin.number == (data ? 9 : 8))) {
        return 4;
    }
    if (instance == 1) {
        if (pin.port == 1 && pin.number == (data ? 11 : 10)) {
            return 4;
        }
#if defined(STM32F411xE)
        if (data && pin.port == 1 && (pin.number == 3 || pin.number == 9)) {
            return 9;
        }
#else
        if (pin.port == 5 && pin.number == (data ? 0 : 1)) {
            return 4;
        }
        if (pin.port == 7 && pin.number == (data ? 5 : 4)) {
            return 4;
        }
#endif
    }
    if (instance == 2) {
        if ((!data && pin.port == 0 && pin.number == 8) || (data && pin.port == 2 && pin.number == 9)) {
            return 4;
        }
#if defined(STM32F411xE)
        if (data && pin.port == 1 && (pin.number == 4 || pin.number == 8)) {
            return 9;
        }
#else
        if (pin.port == 7 && pin.number == (data ? 8 : 7)) {
            return 4;
        }
#endif
    }
    return -1;
}

static hal_status_t validate_config(const hal_i2c_config_t *config) {
    if (config->instance >= 3 || config->frequency == 0 ||
        hal_stm32f4_pin_equal(config->clock_pin, config->data_pin)) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    hal_status_t status = hal_stm32f4_timeout_validate(config->timeout);
    if (status == HAL_STATUS_OK) {
        status = hal_stm32f4_pin_validate(config->clock_pin);
    }
    if (status == HAL_STATUS_OK) {
        status = hal_stm32f4_pin_validate(config->data_pin);
    }
    if (status != HAL_STATUS_OK) {
        return status;
    }
    if (config->frequency < 10000 || config->frequency > 400000 ||
        alternate_function(config->instance, config->clock_pin, false) < 0 ||
        alternate_function(config->instance, config->data_pin, true) < 0) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    return hal_stm32f4_construction_status();
}

static void enable_and_reset(uint8_t instance) {
    switch (instance) {
        case 0:
            __HAL_RCC_I2C1_CLK_ENABLE();
            __HAL_RCC_I2C1_FORCE_RESET();
            __HAL_RCC_I2C1_RELEASE_RESET();
            break;
        case 1:
            __HAL_RCC_I2C2_CLK_ENABLE();
            __HAL_RCC_I2C2_FORCE_RESET();
            __HAL_RCC_I2C2_RELEASE_RESET();
            break;
        case 2:
            __HAL_RCC_I2C3_CLK_ENABLE();
            __HAL_RCC_I2C3_FORCE_RESET();
            __HAL_RCC_I2C3_RELEASE_RESET();
            break;
        default:
            break;
    }
}

/** Reserve the controller and pins, then allocate the persistent bus object. */
static hal_status_t allocate_bus(hal_i2c_t **out_object, const hal_i2c_config_t *config) {

    if (claimed_controllers[config->instance] || (controllers[config->instance]->CR1 & I2C_CR1_PE) != 0) {
        return HAL_STATUS_RESOURCE_BUSY;
    }

    hal_status_t status = hal_stm32f4_pins_claim(config->clock_pin, &config->data_pin);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    void *storage = NULL;
    status = hal_stm32f4_memory_status(hal_memory_alloc((uint32_t)sizeof(hal_i2c_t), &storage));
    if (status != HAL_STATUS_OK) {
        hal_stm32f4_pin_release(config->clock_pin);
        hal_stm32f4_pin_release(config->data_pin);
        return status;
    }

    claimed_controllers[config->instance] = true;

    hal_i2c_t *object = storage;
    *object = (hal_i2c_t){.timeout = config->timeout};
    object->device.Instance = controllers[config->instance];
    *out_object = object;

    return HAL_STATUS_OK;
}

static hal_status_t configure_bus(hal_i2c_t *object, const hal_i2c_config_t *config) {

    object->device.Init = (I2C_InitTypeDef){
        .ClockSpeed = config->frequency,
        .DutyCycle = I2C_DUTYCYCLE_2,
        .AddressingMode = I2C_ADDRESSINGMODE_7BIT,
        .DualAddressMode = I2C_DUALADDRESS_DISABLE,
        .GeneralCallMode = I2C_GENERALCALL_DISABLE,
        .NoStretchMode = I2C_NOSTRETCH_DISABLE,
    };
    enable_and_reset(config->instance);

    uint32_t clock_alternate = (uint32_t)alternate_function(config->instance, config->clock_pin, false);
    uint32_t data_alternate = (uint32_t)alternate_function(config->instance, config->data_pin, true);
    hal_stm32f4_pin_alternate(config->clock_pin, clock_alternate, true);
    hal_stm32f4_pin_alternate(config->data_pin, data_alternate, true);

    HAL_StatusTypeDef result = HAL_I2C_Init(&object->device);
#if defined(I2C_FLTR_ANOFF) && defined(I2C_FLTR_DNF)
    if (result == HAL_OK) {
        result = HAL_I2CEx_ConfigAnalogFilter(&object->device, I2C_ANALOGFILTER_ENABLE);
    }
    if (result == HAL_OK) {
        result = HAL_I2CEx_ConfigDigitalFilter(&object->device, 0);
    }
#endif

    return hal_stm32f4_sdk_status(result);
}

static hal_status_t validate_transfer(hal_i2c_t *i2c, uint8_t address, const void *buffer, uint32_t size) {
    if (i2c == NULL || buffer == NULL || size == 0 || address < 0x08 || address > 0x77) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!i2c->initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return size > UINT16_MAX ? HAL_STATUS_NOT_SUPPORTED : HAL_STATUS_OK;
}

static hal_status_t transfer_status(hal_i2c_t *i2c, HAL_StatusTypeDef result) {
    if (result == HAL_OK) {
        return HAL_STATUS_OK;
    }
    // Some blocking HAL paths return HAL_ERROR/HAL_BUSY with the timeout bit.
    if (result == HAL_TIMEOUT || (HAL_I2C_GetError(&i2c->device) & (HAL_I2C_ERROR_TIMEOUT | HAL_I2C_WRONG_START)) != 0) {
        return HAL_STATUS_TIMEOUT;
    }
    return HAL_STATUS_ERROR;
}

hal_status_t hal_i2c_config(hal_i2c_t **i2c, const hal_i2c_config_t *config) {
    if (i2c == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    *i2c = NULL;
    if (config == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    hal_status_t status = validate_config(config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    hal_i2c_t *object = NULL;
    status = allocate_bus(&object, config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    status = configure_bus(object, config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    object->initialized = true;
    *i2c = object;
    return HAL_STATUS_OK;
}

hal_status_t hal_i2c_write(hal_i2c_t *i2c, uint8_t address, const uint8_t *buffer, uint32_t size) {
    hal_status_t status = validate_transfer(i2c, address, buffer, size);
    if (status != HAL_STATUS_OK) {
        return status;
    }
    // The ST API predates const-correct transmit buffers; it does not modify them.
    HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(&i2c->device, (uint16_t)(address << 1), (uint8_t *)buffer, (uint16_t)size, i2c->timeout);
    return transfer_status(i2c, result);
}

hal_status_t hal_i2c_read(hal_i2c_t *i2c, uint8_t address, uint8_t *buffer, uint32_t size) {
    hal_status_t status = validate_transfer(i2c, address, buffer, size);
    if (status != HAL_STATUS_OK) {
        return status;
    }
    HAL_StatusTypeDef result = HAL_I2C_Master_Receive(&i2c->device, (uint16_t)(address << 1), buffer, (uint16_t)size, i2c->timeout);
    return transfer_status(i2c, result);
}
