# CMSIS V1.30 构建兼容：只写构建目录，不修改标准库子模块。
# 调用前：CMSIS_CORE 指向已初始化的 CoreSupport 目录。
# 输出：CMSIS_CORE_INCLUDE、CMSIS_CORE_SOURCE。
# 版本来源与变更依据见 docs/cmsis-build-fix.md。
set(CMSIS_COMPAT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cmsis-compat")
set(CMSIS_CORE_INCLUDE "${CMSIS_CORE}")
set(CMSIS_CORE_SOURCE "${CMSIS_CORE}/core_cm3.c")

if(NOT EXISTS "${CMSIS_CORE_SOURCE}")
    message(FATAL_ERROR "CMSIS 缺少 core_cm3.c，请检查固定版本的标准库子模块。")
endif()

# .old 与 ElectronicsCompetition/BalancingCar 的 core_cm3.h 内容一致。
# 已有正常文件名时优先使用它；仅在缺失时恢复备份，保留原版权声明。
if(NOT EXISTS "${CMSIS_CORE}/core_cm3.h")
    if(NOT EXISTS "${CMSIS_CORE}/core_cm3.h.old")
        message(FATAL_ERROR "CMSIS 缺少 core_cm3.h 和 core_cm3.h.old，请检查标准库子模块。")
    endif()
    file(MAKE_DIRECTORY "${CMSIS_COMPAT_DIR}")
    configure_file("${CMSIS_CORE}/core_cm3.h.old"
                   "${CMSIS_COMPAT_DIR}/core_cm3.h" COPYONLY)
    set(CMSIS_CORE_INCLUDE "${CMSIS_COMPAT_DIR}")
    message(STATUS "Restored CMSIS V1.30 core_cm3.h in build directory")
endif()

# GNU 旧实现的 STREXB/H/W 缺少 early-clobber，可能令状态输出和地址
# 共用寄存器，汇编器报 registers may not be the same。只替换三条已知语句。
# 不删除核心源码，不改变优化等级，不禁用错误，也不升级 CMSIS。
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    file(READ "${CMSIS_CORE_SOURCE}" _cmsis_source)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMSIS_CORE_SOURCE}")
    foreach(_instruction IN ITEMS strexb strexh strex)
        set(_old "__ASM volatile (\"${_instruction} %0, %2, [%1]\" : \"=r\" (result) : \"r\" (addr), \"r\" (value) );")
        set(_new "__ASM volatile (\"${_instruction} %0, %2, [%1]\" : \"=&r\" (result) : \"r\" (addr), \"r\" (value) );")
        string(FIND "${_cmsis_source}" "${_old}" _old_position)
        if(_old_position EQUAL -1)
            # 允许本地副本已经做过同一处修复；其他实现必须重新审核。
            string(FIND "${_cmsis_source}" "${_new}" _new_position)
            if(_new_position EQUAL -1)
                message(FATAL_ERROR "CMSIS ${_instruction} 实现与 V1.30 预期不符，停止自动适配，请人工核对。")
            endif()
        else()
            string(REPLACE "${_old}" "${_new}" _cmsis_source "${_cmsis_source}")
        endif()
    endforeach()
    file(MAKE_DIRECTORY "${CMSIS_COMPAT_DIR}")
    # 文件内容不变时不刷新时间戳，避免每次配置都重编核心源码。
    file(CONFIGURE OUTPUT "${CMSIS_COMPAT_DIR}/core_cm3.c"
         CONTENT "${_cmsis_source}" @ONLY)
    set(CMSIS_CORE_SOURCE "${CMSIS_COMPAT_DIR}/core_cm3.c")
    message(STATUS "Applied CMSIS V1.30 GNU STREX constraints in build copy")
endif()
