# 2011G 软件分层与回归验证

## 改动范围

最初分层的业务代码基线为 `3a560d62d18681d53d8dc65e3baea5a19014146d`；参考 BalancingCar 的职责分工，但不复制其寄存器驱动、FreeRTOS、Keil 工程或业务代码。后续在已合入分层及 CMSIS 构建修复的 `77eac95a2273eb84c4342f3ec12c7ce39ae4cb4d` 基础上落实 `Start / User` 目录。

此前以 `cd88988334f7dd3b746ebbbeb622d03bfcead534` 为基线，将 `App / Driver / Interface / Common / User` 原样收进 `firmware/src/`，将链接脚本原样移入 `firmware/Start/`。目录迁移仅同步构建、校验和文档路径，没有改变已有模块分工。

当前在 `deff7da6f48bfedf73a622d143500675338ba561` 基础上移除板载 LED 示例：删除其 `.c/.h`、应用调用、500ms 延时以及构建和测试依赖。不修改已有 ADC 初始化、单次读取、超时保护、时基、中断或启动支持；不新增周期采样、电阻换算、自动量程、继电器、DMA、TIM 触发或显示功能。

## 文件职责

| 位置 | 职责 |
| --- | --- |
| `firmware/Start/startup.cmake` | 启动支持统一构建入口，集中选择 GNU 启动文件、系统初始化和 CMSIS 源码/头文件路径 |
| `firmware/Start/cmsis-compat.cmake` | 已有 CMSIS V1.30 兼容处理，脚本内容、生成目录和三处修复不变 |
| `firmware/Start/STM32F103xx_FLASH.ld` | 链接脚本，Flash、RAM、段和堆栈布局不变 |
| `firmware/src/User/main.c` | 更新系统时钟信息，先初始化时基，再初始化应用，循环调用应用任务；保留 Error_Handler |
| `firmware/src/User/stm32f10x_it.c/.h` | 保留唯一的项目中断实现与声明；SysTick 只调用 Com_Time_Tick |
| `firmware/src/User/stm32f10x_conf.h` | 项目级标准外设库包含和断言配置 |
| `firmware/src/App/App_ResistorTester.c/.h` | 组织 ADC 初始化；任务当前为空入口，后续再加入测量调度、阻值换算与换档策略 |
| `firmware/src/Driver/Driver_ADC.c/.h` | ADC1/PA0 初始化、校准和软件触发单次原始值读取，不处理参考电阻与量程业务 |
| `firmware/src/Interface/README.md` | 保留具体硬件模块接口的目录约定，当前没有参与编译的源码，不保留空 LED 驱动 |
| `firmware/src/Common/Com_Time.c/.h` | 使用 CMSIS SysTick 的毫秒服务；此公共服务仍依赖 STM32，不是纯平台无关算法 |
| `tests/layering` | 独立主机回归测试，使用硬件接口替身，不进入固件编译 |

`firmware` 是工程根目录，`src` 是自己维护的项目源码区，不等同于纯业务层。同一模块的 `.c/.h` 放在一起；不同时保留两套入口或中断文件。第三方源码仍由原位置的 `Libraries` 子模块管理；`Start` 是实际接入构建的配置、链接支持和源码导航，不在工作树复制另一套内核支持。启动文件、系统初始化和 CMSIS 来源不变。

`CMakeLists.txt`、`CMakePresets.json` 继续放在 firmware 根目录，工具链保留在 `cmake/`，生成文件继续放在已忽略的 `build/`。LED 清理只移除源文件登记和不再需要的 Interface 头文件路径，不改变工程名、构建预设、编译选项、产物名称或子模块路径和版本。

详细导航：[Start](../firmware/Start/README.md)、[User](../firmware/src/User/README.md)。

## 调用顺序

```text
main
 ├─ SystemCoreClockUpdate
 ├─ Com_Time_Init                         必须先启动 SysTick
 ├─ App_ResistorTester_Init
 │    └─ Driver_ADC1_Init
 │         └─ Com_Time_DelayMs / GetMs    依赖已运行的时基
 └─ while (1)
      └─ App_ResistorTester_Task         空入口，无硬件操作和延时

SysTick_Handler → Com_Time_Tick → 私有 s_ms_ticks
```

`Driver` 与后续的 `Interface` 模块不反向调用 `App` 或 `main`；需要时间服务时可以使用 `Common`。App 可以直接调用 ADC 驱动，不创建只转发调用的接口层。后续真正接入继电器时才增加 `Interface_Range`，不提前堆空模块。

## 时间与中断约束

- 计数唯一存放在 `Com_Time.c` 的 `static volatile uint32_t s_ms_ticks`，不通过 `main.h` 暴露可写全局量。
- `Com_Time_Init()` 只在启动阶段调用一次，使用已更新的 `SystemCoreClock`，不重置已运行的计数。
- 超时仍使用无符号差值 `(uint32_t)(now - start)`，保留回绕处理。
- `Com_Time_DelayMs()`、ADC 初始化和读取要求 SysTick 能继续运行，不可在中断中或关闭中断时调用。没有增加“中断被冻结”时的第二时钟源。
- LED 的两次 500ms 延时已删除；ADC 初始化所需的 2ms 稳定等待和两个校准阶段的 10ms 超时均保留，单次读取的 10ms 超时保护也不变。
- 当前 `App_ResistorTester_Task()` 是立即返回的空入口，主循环仍持续调用它；没有加入休眠、低功耗或采样调度，不能把这次清理当成完整测量任务已经完成。
- ADC 初始化失败向上传递 `ERROR`，最后由 main 的 `Error_Handler()` 按原策略停机。

## 本地验证命令

在仓库根目录运行主机测试，不使用 ARM 交叉工具链：

```bash
set -e
for config in Debug Release; do
  cmake -S tests/layering -B "firmware/build/host-$config" -G Ninja -DCMAKE_BUILD_TYPE="$config"
  cmake --build "firmware/build/host-$config"
  ctest --test-dir "firmware/build/host-$config" --output-on-failure
done
```

每个构建类型现有 27 个用例。保留全部 ADC 初始化、读取和时基回归；删除已失去被测对象的 LED 专用用例，将原应用闪烁用例替换为 `app_idle`。新用例在应用初始化后连续调用任务 100 次，确认不操作 GPIO、不延时、不启动转换，也不破坏 ADC 已初始化状态。ADC 的 13 项读取测试函数保持不变，不以跳过或禁用测试代替验证。

主机测试直接使用 `firmware/src/` 中的真实模块源码与 `User/stm32f10x_it.c`，不是旧路径或复制品。测试没有使用会被 Release 的 `NDEBUG` 关闭的断言。`test_time.c` 在测试翻译单元中纳入真实时间模块实现，仅为设置其私有计数到回绕边界；不向生产接口增加测试专用 setter。非零延时依赖的真实中断时序仍需上板验证。

真正的 ARM 固件构建需要初始化子模块并安装完整的 `arm-none-eabi-gcc` 与 newlib：

```bash
set -e
git submodule update --init --recursive
for config in Debug Release; do
  cmake -S firmware -B "firmware/build/$config" -G Ninja \
    -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
  cmake --build "firmware/build/$config"
  python3 tests/check_cmsis_compat.py "firmware/build/$config"
done
```

源码列表变更后先重新运行配置命令，让编译数据库刷新；不要仅复用旧可执行文件判断构建通过。独立验收应使用全新构建目录，GitHub Actions 使用干净检出完成验证。

构建校验检查 `src` 下的目录约定、Start 中的脚本和链接文件、旧顶层源码目录及旧链接脚本已移除、现有项目源码/中断/系统/启动编译单元各只有一份、实际使用 GNU/TrueSTUDIO 启动文件、编译命令没有旧 Core 路径或主机测试桩。新增检查确认 LED 的两个源文件已删除，且不再登记为编译单元。CMSIS 原文件哈希、兼容副本差异和干净子模块检查仍保留。

云端工作流先执行主机测试，再执行 ARM 固件构建及构建产物检查，使用只读仓库权限，不烧录、不部署、不提交代码，没有定时任务。本次清理不修改工作流触发方式，结果以本次提交对应的 Actions 为准。

## 历史构建问题

分层前的业务代码在 CI 提交 `a949f7c737c7d1149a386caa7329d117fe0882f1` 下已出现 Debug/Release 编译失败：[基线运行记录](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35435104761)。固定子模块曾因只有 `core_cm3.h.old`、没有正常文件名的 `core_cm3.h` 而失败；补全后又暴露旧 GNU 汇编输出约束问题。

这些问题已由 [CMSIS 构建修复](cmsis-build-fix.md) 处理，并在合并提交 `77eac95` 的 [Actions](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35441308285) 中通过。之后将兼容脚本移入 Start；LED 清理不改兼容处理逻辑，不能用历史通过代替当前验收。

## 上板验收与回滚

主机测试和交叉编译均不能验证真实 ADC 校准、模拟电压、电阻精度或晶振是否正常。上板时至少检查：进入主循环而不是 Error_Handler；SysTick 每次仅更新一次计数；程序不再初始化或翻转 PC13；ADC1 初始化返回 SUCCESS，参数与清理前一致。删除驱动不等于主动把灯设成熄灭，也不影响板子的硬件电源指示灯，实际亮灭取决于接线和引脚状态。

按授权直接在 main 以独立提交完成清理；如需撤回，用 `git revert` 撤销本次 LED 清理提交，再重新运行 CMake 配置和现有测试，避免 `reset --hard` 丢弃其他工作。回滚不需要修改或升级标准库子模块，也不撤销此前已生效的 CMSIS 修复。此次不涉及数据库、生产部署或标准库更新。
