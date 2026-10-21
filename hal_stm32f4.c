#include "hal_stm32f4_private.h"

static uint16_t claimed_pins[9] = {0};

hal_status_t hal_stm32f4_memory_status(hal_memory_status_t status) {
    switch (status) {
        case HAL_MEMORY_OK:
            return HAL_STATUS_OK;
        case HAL_MEMORY_INVALID_ARGUMENT:
            return HAL_STATUS_INVALID_ARGUMENT;
        case HAL_MEMORY_NOT_INITIALIZED:
            return HAL_STATUS_NOT_INITIALIZED;
        case HAL_MEMORY_INVALID_STATE:
            return HAL_STATUS_INVALID_STATE;
        case HAL_MEMORY_NO_MEMORY:
            return HAL_STATUS_NO_MEMORY;
        default:
            return HAL_STATUS_ERROR;
    }
}

hal_status_t hal_stm32f4_sdk_status(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:
            return HAL_STATUS_OK;
        case HAL_TIMEOUT:
            return HAL_STATUS_TIMEOUT;
        case HAL_BUSY:
            return HAL_STATUS_RESOURCE_BUSY;
        default:
            return HAL_STATUS_ERROR;
    }
}

hal_status_t hal_stm32f4_pin_validate(hal_pin_t pin) {
    if (pin.port >= 9 || pin.number >= 16) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (hal_stm32f4_pin_port(pin) == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
#if defined(STM32F411xE)
    if (pin.port == 7 && pin.number > 1) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
#else
    if (pin.port == 8 && pin.number > 11) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
#endif
    return HAL_STATUS_OK;
}

GPIO_TypeDef *hal_stm32f4_pin_port(hal_pin_t pin) {
    switch (pin.port) {
        case 0:
            return GPIOA;
        case 1:
            return GPIOB;
        case 2:
            return GPIOC;
        case 3:
            return GPIOD;
        case 4:
            return GPIOE;
#if defined(STM32F407xx)
        case 5:
            return GPIOF;
        case 6:
            return GPIOG;
        case 8:
            return GPIOI;
#endif
        case 7:
            return GPIOH;
        default:
            return NULL;
    }
}

bool hal_stm32f4_pin_equal(hal_pin_t first, hal_pin_t second) {
    return first.port == second.port && first.number == second.number;
}

hal_status_t hal_stm32f4_pins_claim(hal_pin_t first, const hal_pin_t *second) {
    hal_status_t status = hal_stm32f4_pin_validate(first);
    if (status == HAL_STATUS_OK && second != NULL) {
        status = hal_stm32f4_pin_validate(*second);
        if (hal_stm32f4_pin_equal(first, *second)) {
            return HAL_STATUS_INVALID_ARGUMENT;
        }
    }
    if (status != HAL_STATUS_OK) {
        return status;
    }
    hal_memory_info_t info = {0};
    status = hal_stm32f4_memory_status(hal_memory_get_info(&info));
    if (status != HAL_STATUS_OK) {
        return status;
    }
    if (info.sealed) {
        return HAL_STATUS_INVALID_STATE;
    }
    if ((claimed_pins[first.port] & (1U << first.number)) != 0 ||
        (second != NULL && (claimed_pins[second->port] & (1U << second->number)) != 0)) {
        return HAL_STATUS_RESOURCE_BUSY;
    }
    claimed_pins[first.port] |= (uint16_t)(1U << first.number);
    if (second != NULL) {
        claimed_pins[second->port] |= (uint16_t)(1U << second->number);
    }
    return HAL_STATUS_OK;
}

void hal_stm32f4_pin_release(hal_pin_t pin) {
    claimed_pins[pin.port] &= (uint16_t)~(1U << pin.number);
}

void hal_stm32f4_pin_clock_enable(hal_pin_t pin) {
    switch (pin.port) {
        case 0:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
#if defined(STM32F407xx)
        case 5:
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
        case 6:
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
        case 8:
            __HAL_RCC_GPIOI_CLK_ENABLE();
            break;
#endif
        case 7:
            __HAL_RCC_GPIOH_CLK_ENABLE();
            break;
        default:
            break;
    }
}

void hal_stm32f4_pin_alternate(hal_pin_t pin, uint32_t alternate, bool open_drain) {
    hal_stm32f4_pin_clock_enable(pin);
    GPIO_TypeDef *port = hal_stm32f4_pin_port(pin);
    HAL_GPIO_WritePin(port, (uint16_t)(1U << pin.number), GPIO_PIN_SET);
    GPIO_InitTypeDef config = {
        .Pin = 1U << pin.number,
        .Mode = open_drain ? GPIO_MODE_AF_OD : GPIO_MODE_AF_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = alternate,
    };
    HAL_GPIO_Init(port, &config);
}

hal_status_t hal_stm32f4_timeout_validate(uint32_t timeout) {
    if (timeout == 0) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    return timeout == HAL_MAX_DELAY ? HAL_STATUS_NOT_SUPPORTED : HAL_STATUS_OK;
}
