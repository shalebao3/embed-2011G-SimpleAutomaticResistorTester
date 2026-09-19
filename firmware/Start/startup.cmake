# STM32F103C8T6 启动支持的统一构建入口。
# 调用前：STDPERIPH_ROOT 已指向初始化完成的固定版本标准库。
# 输出：STM32_STARTUP_SOURCES、STM32_STARTUP_INCLUDE_DIRS。
# 厂商源码仍由 Libraries 子模块管理，本目录不维护第二套副本。
set(CMSIS_CORE "${STDPERIPH_ROOT}/Libraries/CMSIS/CM3/CoreSupport")
set(CMSIS_DEVICE "${STDPERIPH_ROOT}/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x")
# 保留原工程的 GNU/TrueSTUDIO 启动文件，不替换为 MDK-ARM 汇编。
set(STARTUP_FILE "${CMSIS_DEVICE}/startup/TrueSTUDIO/startup_stm32f10x_md.s")

include("${CMAKE_CURRENT_LIST_DIR}/cmsis-compat.cmake")

# 顺序与目录迁移前保持一致；由顶层可执行目标编译，避免重复注册。
set(STM32_STARTUP_SOURCES
    "${CMSIS_CORE_SOURCE}"
    "${CMSIS_DEVICE}/system_stm32f10x.c"
    "${STARTUP_FILE}"
)
set(STM32_STARTUP_INCLUDE_DIRS
    "${CMSIS_CORE_INCLUDE}"
    "${CMSIS_CORE}"
    "${CMSIS_DEVICE}"
)

foreach(_startup_source IN LISTS STM32_STARTUP_SOURCES)
    if(NOT EXISTS "${_startup_source}")
        message(FATAL_ERROR "启动支持文件不存在：${_startup_source}，请检查标准库子模块。")
    endif()
endforeach()
unset(_startup_source)
