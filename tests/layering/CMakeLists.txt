cmake_minimum_required(VERSION 3.22)
project(layering_regression LANGUAGES C)

# 独立主机测试工程：禁止混入 firmware 的 ARM 工具链或硬件头文件。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()
get_filename_component(FIRMWARE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/src" ABSOLUTE)

add_executable(test_modules
    test_modules.c
    "${FIRMWARE_SRC}/App/App_ResistorTester.c"
    "${FIRMWARE_SRC}/Driver/Driver_ADC.c"
    "${FIRMWARE_SRC}/Interface/Interface_LED.c"
)
add_executable(test_time
    test_time.c
    "${FIRMWARE_SRC}/User/stm32f10x_it.c"
)
foreach(target test_modules test_time)
    target_include_directories(${target} PRIVATE
        mocks "${FIRMWARE_SRC}/App" "${FIRMWARE_SRC}/Driver"
        "${FIRMWARE_SRC}/Interface" "${FIRMWARE_SRC}/Common" "${FIRMWARE_SRC}/User"
    )
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
    endif()
endforeach()

foreach(case adc_ok adc_reset_timeout adc_cal_timeout adc_reset_timeout_wrap
             adc_cal_timeout_wrap app_ok app_reset_timeout app_cal_timeout led app_cycle)
    add_test(NAME ${case} COMMAND test_modules ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 5)
endforeach()
foreach(case init_ok init_error tick wrap delay_zero)
    add_test(NAME time_${case} COMMAND test_time ${case})
    set_tests_properties(time_${case} PROPERTIES TIMEOUT 5)
endforeach()

# ADC 单次读取的纯主机握手测试；不替代真实芯片验证。
foreach(case read_before_init read_null read_after_init_failure read_midscale
             read_zero read_fullscale read_immediate read_stale_eoc
             read_timeout read_timeout_wrap read_success_wrap read_repeat
             read_reinit_after_timeout)
    add_test(NAME ${case} COMMAND test_modules ${case})
    set_tests_properties(${case} PROPERTIES TIMEOUT 5)
endforeach()
