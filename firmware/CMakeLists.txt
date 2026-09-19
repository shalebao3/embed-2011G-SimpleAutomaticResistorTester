cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_PROJECT_NAME embed-2011G-SimpleAutomaticResistorTester)
project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_FLAGS
    -mcpu=cortex-m3
    -mthumb
)

set(STDPERIPH_ROOT "${CMAKE_SOURCE_DIR}/Libraries/STM32F10x_StdPeriph_Lib")
set(STDPERIPH_DRIVER "${STDPERIPH_ROOT}/Libraries/STM32F10x_StdPeriph_Driver")
set(CMSIS_CORE "${STDPERIPH_ROOT}/Libraries/CMSIS/CM3/CoreSupport")
set(CMSIS_DEVICE "${STDPERIPH_ROOT}/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x")
set(STARTUP_FILE "${CMSIS_DEVICE}/startup/TrueSTUDIO/startup_stm32f10x_md.s")

if(NOT EXISTS "${STDPERIPH_DRIVER}/src/stm32f10x_gpio.c")
    message(FATAL_ERROR
        "STM32F10x 标准库子模块未初始化。请在仓库根目录执行: "
        "git submodule update --init --recursive"
    )
endif()

# 在构建目录恢复配套头文件并适配旧 CMSIS 的 GNU 汇编约束。
# 不修改第三方子模块，具体来源和修复范围见 docs/cmsis-build-fix.md。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmsis-compat.cmake")

file(GLOB STDPERIPH_SOURCES CONFIGURE_DEPENDS
    "${STDPERIPH_DRIVER}/src/*.c"
)

add_library(stm32_stdperiph STATIC
    ${STDPERIPH_SOURCES}
)

target_include_directories(stm32_stdperiph PUBLIC
    "${CMAKE_SOURCE_DIR}/Core/Inc"
    "${STDPERIPH_DRIVER}/inc"
    "${CMSIS_CORE_INCLUDE}"
    "${CMSIS_CORE}"
    "${CMSIS_DEVICE}"
)

target_compile_definitions(stm32_stdperiph PUBLIC
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
)

target_compile_options(stm32_stdperiph PRIVATE
    ${MCU_FLAGS}
    -Wall
    -ffunction-sections
    -fdata-sections
)

add_executable(${CMAKE_PROJECT_NAME}
    Core/Src/main.c
    Core/Src/stm32f10x_it.c
    App/App_ResistorTester.c
    Driver/Driver_ADC.c
    Interface/Interface_LED.c
    Common/Com_Time.c
    "${CMSIS_CORE_SOURCE}"
    "${CMSIS_DEVICE}/system_stm32f10x.c"
    "${STARTUP_FILE}"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    App
    Driver
    Interface
    Common
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -O0 -g3)
    target_compile_options(stm32_stdperiph PRIVATE -O0 -g3)
else()
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Os)
    target_compile_options(stm32_stdperiph PRIVATE -Os)
endif()

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${MCU_FLAGS}
    "-T${CMAKE_SOURCE_DIR}/STM32F103xx_FLASH.ld"
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map"
    -Wl,--print-memory-usage
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stm32_stdperiph
    m
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
    COMMAND ${CMAKE_SIZE}
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating HEX/BIN and printing firmware size"
)
