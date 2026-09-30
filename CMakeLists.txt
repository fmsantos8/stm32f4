# The parent supplies stm32cube_f4_hal (SDK sources, device and HAL config).
add_library(stm32f4_port STATIC
    gpio/hal_gpio.c
    i2c/hal_i2c.c
    timer/hal_timer.c
    uart/hal_uart.c
)

target_include_directories(stm32f4_port PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../interface"
)
target_link_libraries(stm32f4_port PUBLIC stm32cube_f4_hal)
target_compile_features(stm32f4_port PUBLIC c_std_11)
target_compile_options(stm32f4_port PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(stm32f4_port PRIVATE
    $<$<CONFIG:Debug>:-Og;-g3>
    $<$<NOT:$<CONFIG:Debug>>:-Os>
)
