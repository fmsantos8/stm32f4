#ifndef STM32F4_TEST_FIXTURE_H
#define STM32F4_TEST_FIXTURE_H

#include "stm32f4xx_hal.h"

/* Optional application checks run with the same emulated memory and drivers. */
void stm32f4_test_set_clocks(uint32_t hclk_hz, uint32_t pclk1_hz, uint32_t pclk2_hz);
void stm32f4_test_set_status(HAL_StatusTypeDef status);
void stm32f4_test_application(void);

#endif
