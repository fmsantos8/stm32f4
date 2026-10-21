#include "system/hal_system.h"
#include "hal_stm32f4_private.h"

static bool initialized = false;

static HAL_StatusTypeDef configure_clock(void) {
    RCC_OscInitTypeDef oscillator = {
        .OscillatorType = RCC_OSCILLATORTYPE_HSI,
        .HSIState = RCC_HSI_ON,
        .HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT,
        .PLL.PLLState = RCC_PLL_NONE,
    };
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        return HAL_ERROR;
    }
    // Leave an existing PLL before changing it. Increasing Flash latency first
    // is safe even if a bootloader entered with a faster clock than reset HSI.
    RCC_ClkInitTypeDef clocks = {
        .ClockType = RCC_CLOCKTYPE_SYSCLK,
        .SYSCLKSource = RCC_SYSCLKSOURCE_HSI,
    };
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_5) != HAL_OK) {
        return HAL_ERROR;
    }
    oscillator.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        return HAL_ERROR;
    }
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    oscillator.PLL.PLLM = 16;
    oscillator.PLL.PLLP = RCC_PLLP_DIV2;
#if defined(STM32F411xE)
    oscillator.PLL.PLLN = 200;
    oscillator.PLL.PLLQ = 5;
    const uint32_t flash_latency = FLASH_LATENCY_3;
    clocks.APB1CLKDivider = RCC_HCLK_DIV2;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
#else
    oscillator.PLL.PLLN = 336;
    oscillator.PLL.PLLQ = 7;
    const uint32_t flash_latency = FLASH_LATENCY_5;
    clocks.APB1CLKDivider = RCC_HCLK_DIV4;
    clocks.APB2CLKDivider = RCC_HCLK_DIV2;
#endif
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        return HAL_ERROR;
    }
    uint32_t start = HAL_GetTick();
    while (__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY) == RESET) {
        if ((uint32_t)(HAL_GetTick() - start) > 100U) {
            return HAL_ERROR;
        }
    }
    clocks.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    return HAL_RCC_ClockConfig(&clocks, flash_latency);
}

hal_status_t hal_system_init(const hal_system_config_t *config) {
    hal_memory_info_t info;
    if (initialized || hal_memory_get_info(&info) == HAL_MEMORY_OK) {
        return HAL_STATUS_INVALID_STATE;
    }
    if (config == NULL || config->memory_buffer == NULL || config->memory_capacity == 0 ||
        (uintptr_t)config->memory_buffer % _Alignof(max_align_t) != 0) {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    SystemCoreClockUpdate();
    if (HAL_Init() != HAL_OK) {
        return HAL_STATUS_ERROR;
    }
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    if (configure_clock() != HAL_OK) {
        return HAL_STATUS_ERROR;
    }
    hal_status_t status = hal_stm32f4_memory_status(hal_memory_init(config->memory_buffer, config->memory_capacity));
    if (status == HAL_STATUS_OK) {
        initialized = true;
    }
    return status;
}

hal_status_t hal_system_seal(void) {
    if (!initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return hal_stm32f4_memory_status(hal_memory_seal());
}

void SysTick_Handler(void) {
    HAL_IncTick();
}
