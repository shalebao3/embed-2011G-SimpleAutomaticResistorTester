cmake_minimum_required(VERSION 3.22)
project(layering_regression LANGUAGES C)

# 独立主机测试工程：禁止混入 firmware 的 ARM 工具链或硬件头文件。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
enable_testing()
get_filename_component(FIRMWARE "${CMAKE_CURRENT_SOURCE_DIR}/../../firmware" ABSOLUTE)

add_executable(test_modules
    test_modules.c
    "${FIRMWARE}/App/App_ResistorTester.c"
    "${FIRMWARE}/Driver/Driver_ADC.c"
    "${FIRMWARE}/Interface/Interface_LED.c"
)
add_executable(test_time
    test_time.c
    "${FIRMWARE}/User/stm32f10x_it.c"
)
foreach(target test_modules test_time)
    target_include_directories(${target} PRIVATE
        mocks "${FIRMWARE}/App" "${FIRMWARE}/Driver"
        "${FIRMWARE}/Interface" "${FIRMWARE}/Common" "${FIRMWARE}/User"
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
