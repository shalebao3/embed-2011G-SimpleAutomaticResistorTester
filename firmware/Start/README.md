# Start：启动与内核支持

这里是 2011G 的**启动支持构建入口和源码导航**，不是应用外设初始化目录。ADC 初始化仍在 `../Driver/Driver_ADC.c`。

## 本目录实际管理什么

| 文件 | 职责 |
| --- | --- |
| [startup.cmake](startup.cmake) | 集中选择 CMSIS、系统初始化和 GNU 启动文件，向顶层 CMake 提供源码与头文件路径 |
| [cmsis-compat.cmake](cmsis-compat.cmake) | 保留已有 CMSIS V1.30 兼容处理，只在构建目录生成完整头文件和三处 STREX 约束修复后的源码 |

顶层 `firmware/CMakeLists.txt` 包含 `Start/startup.cmake`，把 `STM32_STARTUP_SOURCES` 加入固件目标一次。不是只建一个空文件夹，也没有重复编译内核或启动文件。

## 底层源码在哪里

第三方源码仍在固定版本的 `../Libraries/STM32F10x_StdPeriph_Lib` 子模块；不复制一套到 Start，避免来源、版本和构建路径分叉。下面的链接固定到当前审核版本，可在 GitHub 直接查看。

| 源码 | 用途 |
| --- | --- |
| [startup_stm32f10x_md.s（GNU/TrueSTUDIO）](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/TrueSTUDIO/startup_stm32f10x_md.s) | 向量表、复位入口和 C 运行时衔接 |
| [system_stm32f10x.c](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c) | 系统和时钟初始化支持 |
| [system_stm32f10x.h](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.h) | 系统初始化接口 |
| [stm32f10x.h](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/stm32f10x.h) | 芯片寄存器、类型和中断编号定义 |
| [core_cm3.h.old](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/CoreSupport/core_cm3.h.old) | 原版 CMSIS V1.30 头文件，构建时恢复为正确文件名 |
| [core_cm3.c](https://github.com/wajatimur/stm32f10x-stdperiph-lib/blob/afa743577f2784e95be2d5003380fdb84a702519/Libraries/CMSIS/CM3/CoreSupport/core_cm3.c) | 原版内核支持源码，GNU 构建使用兼容副本 |

本地完整目录可从 `startup.cmake` 的 `CMSIS_CORE`、`CMSIS_DEVICE` 找到。Debug 兼容副本仍位于 `firmware/build/Debug/cmsis-compat/`；Release 同理。不要手工编辑或提交这些生成文件。

## 边界与调用关系

`Start` 管启动支持，`User` 管项目入口与中断实现，`App` 管业务组织。保留原来的 GNU 启动流程，不拿 BalancingCar 的 MDK-ARM 启动汇编替换。实际进入 C 的入口是 [User/main.c](../User/main.c)，中断处理在 [User/stm32f10x_it.c](../User/stm32f10x_it.c)。

链接脚本仍为 [STM32F103xx_FLASH.ld](../STM32F103xx_FLASH.ld)，本次不改变 Flash、RAM、向量表、堆栈布局或时钟参数。

兼容处理的来源、限制和验证见 [CMSIS 构建修复](../../docs/cmsis-build-fix.md)；整体分工见 [软件分层](../../docs/software-layering.md)。
