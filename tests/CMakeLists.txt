cmake_minimum_required(VERSION 3.20)
project(stm32f4_port_tests LANGUAGES C CXX)
enable_testing()

# Linux tests use the actual SDK headers and map emulated peripheral memory
# at the STM32 addresses. SDK functions are test doubles; no hardware is used.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The register-mapping tests require Linux")
endif()
set(STM32CUBE_F4_PATH "" CACHE PATH "External STM32CubeF4 package")
set(STM32F4_DEVICE STM32F411xE CACHE STRING "Device tested using actual CMSIS headers")
set(STM32F4_TEST_CALLBACKS 0 CACHE STRING "SDK callback registration setting")
if(NOT EXISTS "${STM32CUBE_F4_PATH}/Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal.h")
    message(FATAL_ERROR "Set STM32CUBE_F4_PATH to the external SDK")
endif()

add_library(test_sdk INTERFACE)
target_include_directories(test_sdk SYSTEM INTERFACE
    "${STM32CUBE_F4_PATH}/Drivers/STM32F4xx_HAL_Driver/Inc"
    "${STM32CUBE_F4_PATH}/Drivers/CMSIS/Include"
    "${STM32CUBE_F4_PATH}/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
)
target_include_directories(test_sdk INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_definitions(test_sdk INTERFACE USE_HAL_DRIVER "${STM32F4_DEVICE}"
    USE_HAL_I2C_REGISTER_CALLBACKS=${STM32F4_TEST_CALLBACKS}
    USE_HAL_UART_REGISTER_CALLBACKS=${STM32F4_TEST_CALLBACKS}
    USE_HAL_TIM_REGISTER_CALLBACKS=${STM32F4_TEST_CALLBACKS})
set(STM32F4_HAL_SDK_TARGET test_sdk)
add_subdirectory(.. port)
target_compile_options(hal_stm32f4 PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(test_port test_port.c header_smoke.cpp)
target_link_libraries(test_port PRIVATE hal::stm32f4)
target_compile_options(test_port PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME stm32f4_port_contracts COMMAND test_port)
