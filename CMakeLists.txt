cmake_minimum_required(VERSION 3.20)

# This directory is a reusable dependency: the consuming project owns the
# toolchain, device selection, startup/linker files and HAL configuration.
if(NOT DEFINED STM32F4_HAL_INTERFACE_DIR)
    set(STM32F4_HAL_INTERFACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../interface")
endif()
set(STM32F4_HAL_INTERFACE_DIR "${STM32F4_HAL_INTERFACE_DIR}"
    CACHE PATH "Directory containing the portable HAL interfaces")
set(STM32F4_HAL_SDK_TARGET "${STM32F4_HAL_SDK_TARGET}" CACHE STRING "Existing target supplying STM32CubeF4 HAL")
set(STM32CUBE_F4_PATH "${STM32CUBE_F4_PATH}" CACHE PATH "External STM32CubeF4 package root")
set(STM32F4_HAL_CONFIG_DIR "${STM32F4_HAL_CONFIG_DIR}" CACHE PATH "Directory containing stm32f4xx_hal_conf.h")
set(STM32F4_DEVICE "${STM32F4_DEVICE}" CACHE STRING "CMSIS device macro selected by the consuming project")

if(NOT EXISTS "${STM32F4_HAL_INTERFACE_DIR}/status/hal_status.h")
    message(FATAL_ERROR "Set STM32F4_HAL_INTERFACE_DIR to the portable HAL headers")
endif()

if(STM32F4_HAL_SDK_TARGET)
    if(NOT TARGET "${STM32F4_HAL_SDK_TARGET}")
        message(FATAL_ERROR "STM32F4_HAL_SDK_TARGET must name an existing CMake target")
    endif()
    set(_stm32f4_sdk_target "${STM32F4_HAL_SDK_TARGET}")
else()
    set(_stm32f4_driver "${STM32CUBE_F4_PATH}/Drivers/STM32F4xx_HAL_Driver")
    set(_stm32f4_cmsis "${STM32CUBE_F4_PATH}/Drivers/CMSIS")
    set(_stm32f4_conf "${STM32F4_HAL_CONFIG_DIR}/stm32f4xx_hal_conf.h")
    if(NOT EXISTS "${_stm32f4_driver}/Inc/stm32f4xx_hal.h" OR
       NOT EXISTS "${_stm32f4_cmsis}/Include/core_cm4.h" OR
       NOT EXISTS "${_stm32f4_cmsis}/Device/ST/STM32F4xx/Include/stm32f4xx.h")
        message(FATAL_ERROR "Set STM32CUBE_F4_PATH to an external STM32CubeF4 package with HAL and CMSIS")
    endif()
    if(NOT EXISTS "${_stm32f4_conf}" OR NOT STM32F4_DEVICE)
        message(FATAL_ERROR "Set STM32F4_HAL_CONFIG_DIR and STM32F4_DEVICE in the consuming project")
    endif()

    add_library(stm32f4_sdk STATIC)
    foreach(_module hal hal_cortex hal_dma hal_dma_ex hal_flash hal_flash_ex
                    hal_gpio hal_i2c hal_i2c_ex hal_pwr hal_pwr_ex hal_rcc
                    hal_rcc_ex hal_tim hal_tim_ex hal_uart)
        target_sources(stm32f4_sdk PRIVATE "${_stm32f4_driver}/Src/stm32f4xx_${_module}.c")
    endforeach()
    target_include_directories(stm32f4_sdk SYSTEM PUBLIC
        "${_stm32f4_driver}/Inc"
        "${_stm32f4_cmsis}/Include"
        "${_stm32f4_cmsis}/Device/ST/STM32F4xx/Include"
    )
    target_include_directories(stm32f4_sdk PUBLIC "${STM32F4_HAL_CONFIG_DIR}")
    target_compile_definitions(stm32f4_sdk PUBLIC USE_HAL_DRIVER "${STM32F4_DEVICE}")
    target_compile_features(stm32f4_sdk PUBLIC c_std_11)
    set(_stm32f4_sdk_target stm32f4_sdk)
endif()

add_library(hal_stm32f4 STATIC
    port_internal.c
    gpio/hal_gpio.c
    i2c/hal_i2c.c
    uart/hal_uart.c
    timer/hal_timer.c
)
add_library(hal::stm32f4 ALIAS hal_stm32f4)
target_include_directories(hal_stm32f4 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}" "${STM32F4_HAL_INTERFACE_DIR}"
)
target_compile_features(hal_stm32f4 PUBLIC c_std_11)
target_link_libraries(hal_stm32f4 PUBLIC "${_stm32f4_sdk_target}")
