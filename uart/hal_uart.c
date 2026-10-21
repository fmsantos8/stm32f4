#include "uart/hal_uart.h"
#include "hal_stm32f4_private.h"

struct hal_uart {
    UART_HandleTypeDef device;
    hal_uart_data_width_t data_width;
    uint32_t timeout;
    bool initialized;
};

_Static_assert(sizeof(hal_uart_t) <= UINT32_MAX, "UART object exceeds arena size range");
_Static_assert(_Alignof(hal_uart_t) <= _Alignof(max_align_t), "UART object requires extended alignment");

static bool claimed_controllers[6] = {0};

static USART_TypeDef *controller(uint8_t instance) {
    switch (instance) {
        case 0:
            return USART1;
        case 1:
            return USART2;
#if defined(STM32F407xx)
        case 2:
            return USART3;
        case 3:
            return UART4;
        case 4:
            return UART5;
#endif
        case 5:
            return USART6;
        default:
            return NULL;
    }
}

static bool pin_matches(hal_pin_t pin, uint8_t port, uint8_t number) {
    return pin.port == port && pin.number == number;
}

static bool valid_route(uint8_t instance, hal_pin_t pin, bool receive) {
    switch (instance) {
        case 0:
            return pin_matches(pin, 0, receive ? 10 : 9) || pin_matches(pin, 1, receive ? 7 : 6)
#if defined(STM32F411xE)
                   || (receive && pin_matches(pin, 1, 3))
#endif
                ;
        case 1:
            return pin_matches(pin, 0, receive ? 3 : 2) || pin_matches(pin, 3, receive ? 6 : 5);
        case 5:
            return pin_matches(pin, 2, receive ? 7 : 6)
#if defined(STM32F407xx)
                   || pin_matches(pin, 6, receive ? 9 : 14)
#endif
                ;
#if defined(STM32F407xx)
        case 2:
            return pin_matches(pin, 1, receive ? 11 : 10) || pin_matches(pin, 2, receive ? 11 : 10) ||
                   pin_matches(pin, 3, receive ? 9 : 8);
        case 3:
            return pin_matches(pin, 0, receive ? 1 : 0) || pin_matches(pin, 2, receive ? 11 : 10);
        case 4:
            return receive ? pin_matches(pin, 3, 2) : pin_matches(pin, 2, 12);
#endif
        default:
            return false;
    }
}

static hal_status_t validate_config(const hal_uart_config_t *config) {
    if (config->instance >= 6 || config->baudrate == 0 ||
        (config->data_width != HAL_UART_DATA_WIDTH_7 && config->data_width != HAL_UART_DATA_WIDTH_8) ||
        (config->parity != HAL_UART_PARITY_NONE && config->parity != HAL_UART_PARITY_EVEN && config->parity != HAL_UART_PARITY_ODD) ||
        (config->stop_count != HAL_UART_STOP_COUNT_1 && config->stop_count != HAL_UART_STOP_COUNT_2) ||
        hal_stm32f4_pin_equal(config->transmit_pin, config->receive_pin)) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    hal_status_t status = hal_stm32f4_timeout_validate(config->timeout);
    if (status == HAL_STATUS_OK) {
        status = hal_stm32f4_pin_validate(config->transmit_pin);
    }
    if (status == HAL_STATUS_OK) {
        status = hal_stm32f4_pin_validate(config->receive_pin);
    }
    if (status != HAL_STATUS_OK) {
        return status;
    }
    if (controller(config->instance) == NULL ||
        !valid_route(config->instance, config->transmit_pin, false) ||
        !valid_route(config->instance, config->receive_pin, true) ||
        (config->data_width == HAL_UART_DATA_WIDTH_7 && config->parity == HAL_UART_PARITY_NONE)) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint32_t clock = config->instance == 0 || config->instance == 5 ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();
    uint64_t divisor = ((uint64_t)clock + config->baudrate / 2U) / config->baudrate;
    if (divisor < 16 || divisor > UINT16_MAX || config->baudrate > clock / 16U) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    uint64_t requested = (uint64_t)config->baudrate * divisor;
    uint64_t error = requested > clock ? requested - clock : clock - requested;
    return error * 100U <= requested * 2U ? HAL_STATUS_OK : HAL_STATUS_NOT_SUPPORTED;
}

static void enable_and_reset(uint8_t instance) {
    switch (instance) {
        case 0:
            __HAL_RCC_USART1_CLK_ENABLE();
            __HAL_RCC_USART1_FORCE_RESET();
            __HAL_RCC_USART1_RELEASE_RESET();
            break;
        case 1:
            __HAL_RCC_USART2_CLK_ENABLE();
            __HAL_RCC_USART2_FORCE_RESET();
            __HAL_RCC_USART2_RELEASE_RESET();
            break;
#if defined(STM32F407xx)
        case 2:
            __HAL_RCC_USART3_CLK_ENABLE();
            __HAL_RCC_USART3_FORCE_RESET();
            __HAL_RCC_USART3_RELEASE_RESET();
            break;
        case 3:
            __HAL_RCC_UART4_CLK_ENABLE();
            __HAL_RCC_UART4_FORCE_RESET();
            __HAL_RCC_UART4_RELEASE_RESET();
            break;
        case 4:
            __HAL_RCC_UART5_CLK_ENABLE();
            __HAL_RCC_UART5_FORCE_RESET();
            __HAL_RCC_UART5_RELEASE_RESET();
            break;
#endif
        case 5:
            __HAL_RCC_USART6_CLK_ENABLE();
            __HAL_RCC_USART6_FORCE_RESET();
            __HAL_RCC_USART6_RELEASE_RESET();
            break;
        default:
            break;
    }
}

static uint32_t map_uart_word_length(hal_uart_data_width_t data_width, hal_uart_parity_t parity) {
    // STM32 word length includes parity, unlike the portable payload width.
    if (data_width == HAL_UART_DATA_WIDTH_8 && parity != HAL_UART_PARITY_NONE) {
        return UART_WORDLENGTH_9B;
    }
    return UART_WORDLENGTH_8B;
}

static uint32_t map_uart_stop_bits(hal_uart_stop_count_t stop_count) {
    switch (stop_count) {
        case HAL_UART_STOP_COUNT_2:
            return UART_STOPBITS_2;
        case HAL_UART_STOP_COUNT_1:
        default:
            return UART_STOPBITS_1;
    }
}

static uint32_t map_uart_parity(hal_uart_parity_t parity) {
    switch (parity) {
        case HAL_UART_PARITY_EVEN:
            return UART_PARITY_EVEN;
        case HAL_UART_PARITY_ODD:
            return UART_PARITY_ODD;
        case HAL_UART_PARITY_NONE:
        default:
            return UART_PARITY_NONE;
    }
}

/** Reserve the controller and pins, then allocate the persistent UART object. */
static hal_status_t allocate_uart(hal_uart_t **out_object, const hal_uart_config_t *config) {
    hal_status_t status = hal_stm32f4_pins_claim(config->transmit_pin, &config->receive_pin);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    if (claimed_controllers[config->instance] || (controller(config->instance)->CR1 & USART_CR1_UE) != 0) {
        hal_stm32f4_pin_release(config->transmit_pin);
        hal_stm32f4_pin_release(config->receive_pin);
        return HAL_STATUS_RESOURCE_BUSY;
    }

    void *storage = NULL;
    status = hal_stm32f4_memory_status(hal_memory_alloc((uint32_t)sizeof(hal_uart_t), &storage));

    if (status != HAL_STATUS_OK) {
        hal_stm32f4_pin_release(config->transmit_pin);
        hal_stm32f4_pin_release(config->receive_pin);
        return status;
    }
    claimed_controllers[config->instance] = true;

    hal_uart_t *object = storage;
    *object = (hal_uart_t){.data_width = config->data_width, .timeout = config->timeout};
    object->device.Instance = controller(config->instance);
    *out_object = object;

    return HAL_STATUS_OK;
}

static hal_status_t configure_uart(hal_uart_t *object, const hal_uart_config_t *config) {

    object->device.Init = (UART_InitTypeDef){
        .BaudRate = config->baudrate,
        .WordLength = map_uart_word_length(config->data_width, config->parity),
        .StopBits = map_uart_stop_bits(config->stop_count),
        .Parity = map_uart_parity(config->parity),
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16,
    };
    enable_and_reset(config->instance);

    hal_status_t status = hal_stm32f4_sdk_status(HAL_UART_Init(&object->device));
    if (status != HAL_STATUS_OK) {
        return status;
    }

    uint32_t alternate = config->instance >= 3 ? 8U : 7U;
    hal_stm32f4_pin_alternate(config->transmit_pin, alternate, false);
    hal_stm32f4_pin_alternate(config->receive_pin, alternate, false);

    return HAL_STATUS_OK;
}

hal_status_t hal_uart_config(hal_uart_t **uart, const hal_uart_config_t *config) {
    if (uart == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    *uart = NULL;
    if (config == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    hal_status_t status = validate_config(config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    hal_uart_t *object = NULL;
    status = allocate_uart(&object, config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    status = configure_uart(object, config);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    object->initialized = true;
    *uart = object;
    return HAL_STATUS_OK;
}

hal_status_t hal_uart_write(hal_uart_t *uart, const uint8_t *buffer, uint32_t size, uint32_t *transmitted_count) {
    if (transmitted_count == NULL) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    *transmitted_count = 0;
    if (uart == NULL || (buffer == NULL && size != 0)) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!uart->initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (size > UINT16_MAX) {
        return HAL_STATUS_NOT_SUPPORTED;
    }
    if (uart->data_width == HAL_UART_DATA_WIDTH_7) {
        for (uint32_t i = 0; i < size; ++i) {
            if (buffer[i] > 0x7F) {
                return HAL_STATUS_INVALID_ARGUMENT;
            }
        }
    }
    if (size == 0) {
        return HAL_STATUS_OK;
    }
    // TxXferCount counts bytes not yet submitted to DR, including on timeout.
    uart->device.TxXferCount = (uint16_t)size;
    HAL_StatusTypeDef result = HAL_UART_Transmit(&uart->device, buffer, (uint16_t)size, uart->timeout);
    *transmitted_count = size - uart->device.TxXferCount;
    if (result == HAL_BUSY) {
        return HAL_STATUS_ERROR;
    }
    return hal_stm32f4_sdk_status(result);
}
