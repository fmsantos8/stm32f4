/**
 * @file hal_stm32f4_private.h
 * @brief Internal services shared by the STM32F411/STM32F407 port.
 */
#ifndef HAL_STM32F4_PRIVATE_H
#define HAL_STM32F4_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/hal_pin.h"
#include "common/hal_status.h"
#include "hal_memory.h"
#include "stm32f4xx_hal.h"

#if !defined(STM32F411xE) && !defined(STM32F407xx)
#error "This port supports STM32F411xE and STM32F407xx"
#endif

/**
 * @brief Translate the memory module's independent status into the HAL contract.
 *
 * @param[in] status The memory module's status.
 *
 * @return hal_status_t The corresponding HAL status, or HAL_STATUS_ERROR for
 * an unrecognized memory status.
 */
hal_status_t hal_stm32f4_memory_status(hal_memory_status_t status);

/**
 * @brief Translate an STM32Cube HAL result into the framework's HAL contract.
 *
 * @param[in] status The STM32Cube HAL status.
 *
 * @retval HAL_STATUS_OK status is HAL_OK.
 * @retval HAL_STATUS_TIMEOUT status is HAL_TIMEOUT.
 * @retval HAL_STATUS_RESOURCE_BUSY status is HAL_BUSY.
 * @retval HAL_STATUS_ERROR status is HAL_ERROR or unrecognized.
 */
hal_status_t hal_stm32f4_sdk_status(HAL_StatusTypeDef status);

/**
 * @brief Validate a physical GPIO bank and pin for the selected STM32 device.
 *
 * @details Bank identifiers are zero-based: A=0 through I=8. F411 supports
 * banks A..E and PH0/PH1; F407 supports A..H and PI0..PI11. The BSP determines
 * package availability and board wiring. Peripheral-specific routing and
 * capabilities must be validated separately by each constructor.
 *
 * @param[in] pin The physical bank and pin to validate.
 *
 * @retval HAL_STATUS_OK The bank and pin are supported by this device.
 * @retval HAL_STATUS_INVALID_ARGUMENT The bank exceeds 8 or the pin exceeds 15.
 * @retval HAL_STATUS_NOT_SUPPORTED The bank or pin is unavailable on this MCU.
 */
hal_status_t hal_stm32f4_pin_validate(hal_pin_t pin);

/**
 * @brief Map a physical GPIO bank identifier to its STM32 register block.
 *
 * @details Only the bank is examined; pin-number validation belongs to
 * hal_stm32f4_pin_validate(). This lookup does not enable the bank's clock.
 *
 * @param[in] pin Pin identifier containing the bank to map.
 *
 * @return GPIO_TypeDef* The bank's register block, or NULL if the bank is
 * unavailable on the selected MCU or its identifier is invalid.
 */
GPIO_TypeDef *hal_stm32f4_pin_port(hal_pin_t pin);

/**
 * @brief Compare two physical pin identifiers.
 *
 * @details Compares bank and pin numbers without validating availability.
 *
 * @param[in] first The first pin identifier.
 * @param[in] second The second pin identifier.
 *
 * @return bool True when both identifiers refer to the same bank and pin.
 */
bool hal_stm32f4_pin_equal(hal_pin_t first, hal_pin_t second);

/**
 * @brief Reserve one or two distinct pins for a HAL resource.
 *
 * @details Validates the pins, arena initialization and sealing state, and the
 * port's pin reservations before claiming either pin. A failed claim leaves
 * reservations unchanged. Peripheral-specific capabilities must be checked
 * before claiming.
 * Construction and SDK pin configuration must be sequential; this function
 * provides no synchronization for concurrent callers or interrupt context.
 *
 * @param[in] first The required pin to claim.
 * @param[in] second Optional second pin to claim; NULL requests only first.
 *
 * @retval HAL_STATUS_OK All requested pins were reserved.
 * @retval HAL_STATUS_INVALID_ARGUMENT A pin identifier is invalid or the two
 * requested pins are identical.
 * @retval HAL_STATUS_NOT_SUPPORTED A requested bank or pin is unavailable.
 * @retval HAL_STATUS_NOT_INITIALIZED The arena has not been initialized.
 * @retval HAL_STATUS_INVALID_STATE The construction phase has ended.
 * @retval HAL_STATUS_RESOURCE_BUSY At least one pin is already reserved.
 */
hal_status_t hal_stm32f4_pins_claim(hal_pin_t first, const hal_pin_t *second);

/**
 * @brief Release a pin reservation after construction fails before hardware setup.
 *
 * @details Only clears the port's reservation. The caller must supply a valid,
 * previously claimed pin and coordinate sequential construction. Hardware
 * configuration and arena storage are unaffected.
 *
 * @param[in] pin The valid, previously claimed pin to release.
 */
void hal_stm32f4_pin_release(hal_pin_t pin);

/**
 * @brief Enable a GPIO bank's AHB1 clock before accessing its registers.
 *
 * @details Uses the STM32Cube clock-enable macro, including its readback delay.
 * Repeated calls keep the clock enabled. The caller must validate the pin first.
 *
 * @param[in] pin A validated pin identifying the bank whose clock is required.
 */
void hal_stm32f4_pin_clock_enable(hal_pin_t pin);

/**
 * @brief Enable and configure a claimed pin for a peripheral's alternate function.
 *
 * @details Enables the bank clock, preloads the output latch high and configures
 * the selected AF with no internal pull and very-high GPIO speed. The constructor
 * must validate and claim the pin and validate the peripheral's AF route first.
 *
 * @param[in] pin The validated, claimed pin to configure.
 * @param[in] alternate Validated STM32 alternate-function number from 0 to 15.
 * @param[in] open_drain True selects open drain; false selects push-pull.
 */
void hal_stm32f4_pin_alternate(hal_pin_t pin, uint32_t alternate, bool open_drain);

/**
 * @brief Validate a positive, finite STM32Cube transfer timeout in milliseconds.
 *
 * @details Accepts 1 through UINT32_MAX - 1 without conversion. HAL_MAX_DELAY
 * equals UINT32_MAX and is reserved by STM32Cube for an infinite wait. The port
 * uses a one-millisecond HAL tick; this validation does not establish a strict
 * whole-call deadline.
 *
 * @param[in] timeout Requested transfer timeout in milliseconds.
 *
 * @retval HAL_STATUS_OK The timeout is positive and finite.
 * @retval HAL_STATUS_INVALID_ARGUMENT timeout is zero.
 * @retval HAL_STATUS_NOT_SUPPORTED timeout is HAL_MAX_DELAY.
 */
hal_status_t hal_stm32f4_timeout_validate(uint32_t timeout);

#endif
