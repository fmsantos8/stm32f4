#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "gpio/hal_gpio_stm32f4.h"
#include "i2c/hal_i2c_stm32f4.h"
#include "uart/hal_uart_stm32f4.h"
#include "timer/hal_timer_stm32f4.h"
#ifdef STM32F4_TEST_APPLICATION
#include "test_fixture.h"
#endif

static HAL_StatusTypeDef sdk_status;
static uint32_t hclk, pclk1, pclk2, tick, advance_per_write;
static uint32_t gpio_writes, gpio_inits, gpio_output_inits, uart_writes, timer_starts, i2c_calls;
static uint32_t timeouts[4];
static uint16_t sizes[4], device_address;
static const uint8_t *buffers[4];
static GPIO_InitTypeDef last_gpio_config;
static GPIO_PinState input_level;
static bool irq_enabled;
static IRQn_Type selected_irq;
static uint32_t selected_priority;

static void reset_state(void)
{
    memset((void *)(uintptr_t)PERIPH_BASE, 0, 0x30000U);
    sdk_status = HAL_OK;
    hclk = 100000000U; pclk1 = 50000000U; pclk2 = 100000000U;
    tick = 0U; advance_per_write = 0U;
    gpio_writes = gpio_inits = uart_writes = timer_starts = i2c_calls = 0U;
    gpio_output_inits = 0U;
    memset(timeouts, 0, sizeof(timeouts));
    irq_enabled = false;
}

static void assert_gpio_clock(GPIO_TypeDef *port)
{
    uint32_t index = (uint32_t)(((uintptr_t)port - GPIOA_BASE) / 0x400U);
    assert((RCC->AHB1ENR & (1U << index)) != 0U);
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    assert_gpio_clock(port);
    if (config->Mode == GPIO_MODE_OUTPUT_PP) {
        assert(gpio_writes > gpio_output_inits);
        ++gpio_output_inits;
    }
    last_gpio_config = *config;
    ++gpio_inits;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level)
{
    assert_gpio_clock(port);
    if (level == GPIO_PIN_SET) { port->ODR |= pin; }
    else { port->ODR &= ~pin; }
    ++gpio_writes;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    assert_gpio_clock(port);
    assert(pin != 0U);
    return input_level;
}

uint32_t HAL_RCC_GetPCLK1Freq(void) { return pclk1; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return pclk2; }
uint32_t HAL_RCC_GetHCLKFreq(void) { return hclk; }
uint32_t HAL_GetTick(void) { return tick; }

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *driver)
{
    assert(driver->Instance == I2C1);
    assert((RCC->APB1ENR & RCC_APB1ENR_I2C1EN) != 0U);
    assert((RCC->APB1RSTR & RCC_APB1RSTR_I2C1RST) == 0U);
    assert(gpio_inits >= 2U);
    assert(last_gpio_config.Mode == GPIO_MODE_AF_OD);
    return sdk_status;
}

uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *driver) { return driver->ErrorCode; }

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *driver, uint16_t address,
                                       uint8_t *data, uint16_t size, uint32_t timeout)
{
    assert(driver->Instance == I2C1 && data != NULL);
    device_address = address;
    sizes[0] = size; timeouts[0] = timeout;
    ++i2c_calls;
    return sdk_status;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *driver, uint16_t address,
                                      uint8_t *data, uint16_t size, uint32_t timeout)
{
    data[0] = 0xa5U;
    return HAL_I2C_Master_Transmit(driver, address, data, size, timeout);
}

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *driver)
{
    if (driver->Instance == USART1) { assert(RCC->APB2ENR & RCC_APB2ENR_USART1EN); }
    else if (driver->Instance == USART2) { assert(RCC->APB1ENR & RCC_APB1ENR_USART2EN); }
#if defined(UART4)
    else if (driver->Instance == UART4) { assert(RCC->APB1ENR & RCC_APB1ENR_UART4EN); }
#endif
    else { assert(false); }
    assert(last_gpio_config.Mode == GPIO_MODE_AF_PP);
    return sdk_status;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *driver, const uint8_t *data,
                                  uint16_t size, uint32_t timeout)
{
    assert(driver->Instance != NULL && uart_writes < 4U);
    sizes[uart_writes] = size; timeouts[uart_writes] = timeout;
    buffers[uart_writes] = data;
    ++uart_writes;
    tick += advance_per_write;
    return sdk_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *driver)
{
    assert(!irq_enabled);
    if (driver->Instance == TIM2) { assert(RCC->APB1ENR & RCC_APB1ENR_TIM2EN); }
    else if (driver->Instance == TIM3) { assert(RCC->APB1ENR & RCC_APB1ENR_TIM3EN); }
    else { assert(false); }
    driver->Instance->PSC = driver->Init.Prescaler;
    driver->Instance->ARR = driver->Init.Period;
    driver->Instance->SR |= TIM_FLAG_UPDATE;
    return sdk_status;
}

HAL_StatusTypeDef HAL_TIM_ConfigClockSource(TIM_HandleTypeDef *driver,
                                          const TIM_ClockConfigTypeDef *config)
{
    assert(driver->Instance != NULL && config->ClockSource == TIM_CLOCKSOURCE_INTERNAL);
    return sdk_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *driver)
{
    assert((driver->Instance->SR & TIM_FLAG_UPDATE) == 0U);
    driver->Instance->DIER |= TIM_IT_UPDATE;
    driver->Instance->CR1 |= TIM_CR1_CEN;
    ++timer_starts;
    return sdk_status;
}

void HAL_NVIC_DisableIRQ(IRQn_Type irq) { selected_irq = irq; irq_enabled = false; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { selected_irq = irq; irq_enabled = true; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { assert(irq == selected_irq); }
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t priority, uint32_t subpriority)
{
    assert(irq == selected_irq && subpriority == 0U);
    selected_priority = priority;
}

#ifdef STM32F4_TEST_APPLICATION
void stm32f4_test_set_clocks(uint32_t hclk_hz, uint32_t pclk1_hz, uint32_t pclk2_hz)
{
    hclk = hclk_hz;
    pclk1 = pclk1_hz;
    pclk2 = pclk2_hz;
}

void stm32f4_test_set_status(HAL_StatusTypeDef status)
{
    sdk_status = status;
}
#endif

static void test_gpio(void)
{
    reset_state();
    hal_gpio_t gpio = { .port = GPIOB, .pin = GPIO_PIN_3, .speed = GPIO_SPEED_FREQ_LOW };
    hal_gpio_config_t config = { HAL_GPIO_DIRECTION_OUTPUT, HAL_GPIO_PULL_UP };
    bool level = true;
    assert(hal_gpio_read(&gpio, &level) == HAL_STATUS_ERROR && level);
    assert(hal_gpio_write(&gpio, true) == HAL_STATUS_ERROR);
    assert(hal_gpio_config(NULL, &config) == HAL_STATUS_INVALID_PARAM);
    assert(hal_gpio_config(&gpio, NULL) == HAL_STATUS_INVALID_PARAM);
    config.direction = (hal_gpio_direction_t)99;
    assert(hal_gpio_config(&gpio, &config) == HAL_STATUS_INVALID_PARAM);
    config.direction = HAL_GPIO_DIRECTION_OUTPUT;
    gpio.port->ODR = gpio.pin;
    assert(hal_gpio_config(&gpio, &config) == HAL_STATUS_OK);
    assert((gpio.port->ODR & gpio.pin) == 0U && last_gpio_config.Pull == GPIO_PULLUP);
    assert(hal_gpio_write(&gpio, true) == HAL_STATUS_OK);
    assert(hal_gpio_config(&gpio, &config) == HAL_STATUS_OK);
    assert((gpio.port->ODR & gpio.pin) == 0U);
    config.direction = HAL_GPIO_DIRECTION_INPUT;
    config.pull = HAL_GPIO_PULL_DOWN;
    assert(hal_gpio_config(&gpio, &config) == HAL_STATUS_OK);
    assert(hal_gpio_write(&gpio, true) == HAL_STATUS_ERROR);
    input_level = GPIO_PIN_SET;
    assert(hal_gpio_read(&gpio, &level) == HAL_STATUS_OK && level);
    assert(hal_gpio_read(&gpio, NULL) == HAL_STATUS_INVALID_PARAM);
    hal_gpio_t invalid = { .port = GPIOB, .pin = GPIO_PIN_0 | GPIO_PIN_1 };
    assert(hal_gpio_config(&invalid, &config) == HAL_STATUS_NOT_SUPPORTED);
    hal_gpio_t invalid_port = { .port = NULL, .pin = GPIO_PIN_0 };
    assert(hal_gpio_config(&invalid_port, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.pull = (hal_gpio_pull_t)99;
    assert(hal_gpio_config(&gpio, &config) == HAL_STATUS_INVALID_PARAM);
}

#define TEST_I2C_FIELDS \
    .instance = I2C1, .timeout_ms = 17U, \
    .scl = { GPIOB, GPIO_PIN_6, GPIO_AF4_I2C1, GPIO_PULLUP, GPIO_SPEED_FREQ_HIGH }, \
    .sda = { GPIOB, GPIO_PIN_7, GPIO_AF4_I2C1, GPIO_PULLUP, GPIO_SPEED_FREQ_HIGH }

static void test_i2c(void)
{
    reset_state();
    hal_i2c_t bus = { TEST_I2C_FIELDS };
    hal_i2c_config_t config = { 100000U };
    uint8_t data[2] = { 0U };
    assert(hal_i2c_write(&bus, 0x44U, data, 2U) == HAL_STATUS_ERROR);
    assert(hal_i2c_config(NULL, &config) == HAL_STATUS_INVALID_PARAM);
    assert(hal_i2c_config(&bus, NULL) == HAL_STATUS_INVALID_PARAM);
    config.clock_speed = 0U;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_INVALID_PARAM);
    config.clock_speed = 400001U;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.clock_speed = 1U;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.clock_speed = 400000U; pclk1 = 2000000U;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_NOT_SUPPORTED);
    pclk1 = 50000000U;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_OK);
    assert(bus.driver.Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT);
    assert(hal_i2c_write(&bus, 0x44U, data, 2U) == HAL_STATUS_OK);
    assert(device_address == 0x88U && sizes[0] == 2U && timeouts[0] == 17U);
    assert(hal_i2c_read(&bus, 0x44U, data, 2U) == HAL_STATUS_OK && data[0] == 0xa5U);
    assert(hal_i2c_write(&bus, 0x80U, data, 2U) == HAL_STATUS_INVALID_PARAM);
    assert(hal_i2c_read(&bus, 0U, NULL, 2U) == HAL_STATUS_INVALID_PARAM);
    assert(hal_i2c_write(&bus, 0U, data, 0U) == HAL_STATUS_INVALID_PARAM);
    assert(hal_i2c_write(&bus, 0U, data, 65536U) == HAL_STATUS_NOT_SUPPORTED);
    assert(hal_i2c_read(&bus, 0U, data, 65536U) == HAL_STATUS_NOT_SUPPORTED);
    assert(i2c_calls == 2U);
    sdk_status = HAL_TIMEOUT;
    assert(hal_i2c_write(&bus, 0U, data, 1U) == HAL_STATUS_TIMEOUT);
    sdk_status = HAL_ERROR; bus.driver.ErrorCode = HAL_I2C_ERROR_TIMEOUT;
    assert(hal_i2c_read(&bus, 0U, data, 1U) == HAL_STATUS_TIMEOUT);
    sdk_status = HAL_BUSY;
    assert(hal_i2c_write(&bus, 0U, data, 1U) == HAL_STATUS_ERROR);
    sdk_status = HAL_ERROR; bus.driver.ErrorCode = HAL_I2C_ERROR_AF;
    assert(hal_i2c_write(&bus, 0U, data, 1U) == HAL_STATUS_ERROR);
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_ERROR && !bus.configured);
    assert(hal_i2c_read(&bus, 0U, data, 1U) == HAL_STATUS_ERROR);
    sdk_status = HAL_OK;
    assert(hal_i2c_config(&bus, &config) == HAL_STATUS_OK);
    hal_i2c_t invalid_timeout = { .instance = I2C1 };
    assert(hal_i2c_config(&invalid_timeout, &config) == HAL_STATUS_INVALID_PARAM);
    hal_i2c_t invalid_pins = {
        .instance = I2C1, .timeout_ms = 1U,
        .scl = { GPIOB, GPIO_PIN_6, 4U, GPIO_NOPULL, 0U },
        .sda = { GPIOB, GPIO_PIN_6, 4U, GPIO_NOPULL, 0U },
    };
    assert(hal_i2c_config(&invalid_pins, &config) == HAL_STATUS_NOT_SUPPORTED);
}

#define TEST_UART_FIELDS(controller) \
    .instance = controller, .timeout_ms = 7U, \
    .tx = { GPIOA, GPIO_PIN_9, GPIO_AF7_USART1, GPIO_NOPULL, GPIO_SPEED_FREQ_HIGH }

static void test_uart(void)
{
    reset_state();
    hal_uart_t uart = { TEST_UART_FIELDS(USART1) };
    hal_uart_config_t config = { 115200U, HAL_UART_PARITY_NONE, HAL_UART_STOP_BITS_1 };
    static uint8_t data[65540];
    assert(hal_uart_write(&uart, data, 1U) == HAL_STATUS_ERROR);
    assert(hal_uart_config(NULL, &config) == HAL_STATUS_INVALID_PARAM);
    assert(hal_uart_config(&uart, NULL) == HAL_STATUS_INVALID_PARAM);
    config.baud_rate = 1U;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.baud_rate = 10000000U;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.baud_rate = 115200U;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_OK);
    assert(uart.driver.Init.WordLength == UART_WORDLENGTH_8B);
    assert(uart.driver.Init.Mode == UART_MODE_TX);
    config.parity = HAL_UART_PARITY_EVEN; config.stop_bits = HAL_UART_STOP_BITS_2;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_OK);
    assert(uart.driver.Init.WordLength == UART_WORDLENGTH_9B);
    assert(uart.driver.Init.Parity == UART_PARITY_EVEN && uart.driver.Init.StopBits == UART_STOPBITS_2);
    config.parity = HAL_UART_PARITY_ODD;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_OK);
    assert(uart.driver.Init.Parity == UART_PARITY_ODD);
    config.parity = (hal_uart_parity_t)99;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_INVALID_PARAM && uart.configured);
    advance_per_write = 4U; tick = UINT32_MAX - 2U;
    assert(hal_uart_write(&uart, data, sizeof(data)) == HAL_STATUS_OK);
    assert(uart_writes == 2U && sizes[0] == 65535U && sizes[1] == 5U);
    assert(buffers[0] == data && buffers[1] == data + 65535U);
    assert(timeouts[0] == 7U && timeouts[1] == 3U);
    uart_writes = 0U; advance_per_write = 8U;
    assert(hal_uart_write(&uart, data, sizeof(data)) == HAL_STATUS_TIMEOUT && uart_writes == 1U);
    sdk_status = HAL_TIMEOUT;
    assert(hal_uart_write(&uart, data, 1U) == HAL_STATUS_TIMEOUT);
    sdk_status = HAL_BUSY;
    assert(hal_uart_write(&uart, data, 1U) == HAL_STATUS_ERROR);
    assert(hal_uart_write(&uart, NULL, 1U) == HAL_STATUS_INVALID_PARAM);
    assert(hal_uart_write(&uart, data, 0U) == HAL_STATUS_INVALID_PARAM);
    config.parity = HAL_UART_PARITY_NONE;
    sdk_status = HAL_ERROR;
    assert(hal_uart_config(&uart, &config) == HAL_STATUS_ERROR && !uart.configured);
    sdk_status = HAL_OK;
    hal_uart_t apb1 = { TEST_UART_FIELDS(USART2) };
    config.baud_rate = 4000000U;
    assert(hal_uart_config(&apb1, &config) == HAL_STATUS_NOT_SUPPORTED);
    config.baud_rate = 115200U;
    assert(hal_uart_config(&apb1, &config) == HAL_STATUS_OK);
#if defined(UART4)
    hal_uart_t extra = { TEST_UART_FIELDS(UART4) };
    assert(hal_uart_config(&extra, &config) == HAL_STATUS_OK);
#endif
}

static void test_timer(void)
{
    reset_state();
    hal_timer_t timer = { .instance = TIM3, .irq_priority = 5U };
    uint32_t millis = 42U;
    assert(hal_timer_start(NULL) == HAL_STATUS_INVALID_PARAM);
    assert(hal_timer_get_millis(&timer, &millis) == HAL_STATUS_ERROR && millis == 42U);
    assert(hal_timer_get_millis(&timer, NULL) == HAL_STATUS_INVALID_PARAM);
    assert(hal_timer_start(&timer) == HAL_STATUS_OK);
    assert(irq_enabled && selected_irq == TIM3_IRQn && selected_priority == 5U);
    assert((timer.driver.Init.Prescaler + 1U) * (timer.driver.Init.Period + 1U) == 100000U);
    assert(hal_timer_get_millis(&timer, &millis) == HAL_STATUS_OK && millis == 0U);
    timer.instance->SR |= TIM_FLAG_UPDATE;
    hal_timer_stm32f4_irq(&timer);
    assert(hal_timer_get_millis(&timer, &millis) == HAL_STATUS_OK && millis == 1U);
    hal_timer_stm32f4_irq(&timer);
    assert(timer.elapsed_millis == 1U);
    assert(hal_timer_start(&timer) == HAL_STATUS_OK && timer_starts == 1U);
    timer.elapsed_millis = UINT32_MAX;
    timer.instance->SR |= TIM_FLAG_UPDATE;
    hal_timer_stm32f4_irq(&timer);
    assert(hal_timer_get_millis(&timer, &millis) == HAL_STATUS_OK && millis == 0U);
    timer.instance->DIER = 0U;
    timer.instance->SR |= TIM_FLAG_UPDATE;
    hal_timer_stm32f4_irq(&timer);
    assert(timer.elapsed_millis == 0U);
    timer.instance->CR1 = 0U; millis = 99U;
    assert(hal_timer_get_millis(&timer, &millis) == HAL_STATUS_ERROR && millis == 99U);
    hal_timer_stm32f4_irq(NULL);
    hal_timer_t unsupported = { .instance = TIM1 };
    assert(hal_timer_start(&unsupported) == HAL_STATUS_NOT_SUPPORTED);
    hal_timer_t invalid_priority = { .instance = TIM2, .irq_priority = 16U };
    assert(hal_timer_start(&invalid_priority) == HAL_STATUS_INVALID_PARAM);
    reset_state();
    hal_timer_t second = { .instance = TIM2, .irq_priority = 3U };
    pclk1 = hclk;
    sdk_status = HAL_ERROR;
    assert(hal_timer_start(&second) == HAL_STATUS_ERROR && !second.started && !irq_enabled);
    sdk_status = HAL_OK;
    pclk1 = 33333U;
    assert(hal_timer_start(&second) == HAL_STATUS_NOT_SUPPORTED);
    pclk1 = 25000000U;
#if defined(RCC_DCKCFGR_TIMPRE)
    RCC->DCKCFGR |= RCC_DCKCFGR_TIMPRE;
#endif
    assert(hal_timer_start(&second) == HAL_STATUS_OK && selected_irq == TIM2_IRQn);
    uint32_t expected_ticks = 50000U;
#if defined(RCC_DCKCFGR_TIMPRE)
    expected_ticks = 100000U;
#endif
    assert((second.driver.Init.Prescaler + 1U) * (second.driver.Init.Period + 1U) == expected_ticks);
}

int main(void)
{
    void *memory = mmap((void *)(uintptr_t)PERIPH_BASE, 0x30000U,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                        -1, 0);
    assert(memory != MAP_FAILED);
    test_gpio(); test_i2c(); test_uart(); test_timer();
#ifdef STM32F4_TEST_APPLICATION
    reset_state();
    stm32f4_test_application();
#endif
    assert(munmap(memory, 0x30000U) == 0);
    puts("GPIO, I2C, UART and timer contract tests passed");
    return 0;
}
