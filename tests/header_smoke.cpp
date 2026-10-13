#include "gpio/hal_gpio_stm32f4.h"
#include "i2c/hal_i2c_stm32f4.h"
#include "uart/hal_uart_stm32f4.h"
#include "timer/hal_timer_stm32f4.h"

static_assert(sizeof(hal_gpio_t) > 0, "Concrete handles must be usable in C++ BSPs");
