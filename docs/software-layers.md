# 2011G 软件分层与回归验收

## 范围

基于 `3a560d62d18681d53d8dc65e3baea5a19014146d` 拆分现有代码，参考 `ElectronicsCompetition/project/BalancingCar` 的职责分工，不复制它的寄存器驱动或 FreeRTOS。

本次只做等价分层：仍是 ADC1 初始化与 LED 验证程序，没有开始 ADC 采样，没有电阻换算、继电器或自动量程。README 中的测量方案仍属于设计记录；当前代码组织以本文为准。

## 文件与职责

| 位置 | 职责 | 禁止混入的内容 |
| --- | --- | --- |
| `firmware/Core/Src/main.c` | 更新时钟信息、启动时基、启动应用、循环调度、致命错误处理 | ADC/GPIO 具体初始化、阻值计算 |
| `firmware/App/App_ResistorTester.c/.h` | 组织 LED 与 ADC 初始化，安排当前 500ms 亮/500ms 灭验证流程 | 直接填写外设结构体或改寄存器 |
| `firmware/Driver/Driver_ADC.c/.h` | PA0/ADC1 通道0的初始化、校准和超时；以后承接原始采样 | 档位阻值、自动量程和显示规则 |
| `firmware/Interface/Interface_LED.c/.h` | GPIOC 时钟、PC13 配置和低有效 LED 操作 | 闪烁调度 |
| `firmware/Common/Com_Time.c/.h` | 1ms SysTick 配置、私有计数、读取时间、阻塞延时 | 测量业务 |
| `firmware/Core/Src/stm32f10x_it.c` | 保留中断入口，SysTick 只调用 `Com_Time_Tick()` | 应用测量流程 |

每个新模块的 `.c` 和 `.h` 放在同一目录。公共头文件可独立包含；除 Core 入口外，各层不包含 `main.h`。`Common` 目前的时间模块依赖 CMSIS/SPL，不宣称为与芯片无关的库。

## 调用顺序

```text
main
  ├─ SystemCoreClockUpdate
  ├─ Com_Time_Init                 # 先建立时基
  ├─ App_ResistorTester_Init
  │    ├─ Interface_LED_Init       # 初始熄灭
  │    └─ Driver_ADC1_Init         # 依赖时基进行延时与超时检查
  └─ while (1)
       └─ App_ResistorTester_Task
            ├─ LED 亮 → 等待 500ms
            └─ LED 灭 → 等待 500ms

SysTick_Handler → Com_Time_Tick → 私有 s_ms_ticks
```

`Com_Time_Init()` 仅在启动阶段调用。所有等待函数必须在线程/主循环、中断已开启且 SysTick 正常工作的前提下使用，不能在 ISR 或关中断临界区调用。计数自然回绕，超时继续用 `uint32_t` 无符号差判断。

当前 `App_ResistorTester_Task()` 仍阻塞约 1 秒，这是为了保留既有行为，而不是最终的非阻塞调度设计。引入采样和显示时再单独调整，不能在本次分层中混入行为变更。

## 保持不变的内容

- ADC1、PA0/通道0、PCLK2 六分频。
- 独立模式、关闭扫描、关闭连续转换、软件触发、右对齐、序列长度1。
- 第1个规则序列位置，239.5周期采样。
- ADC 开启后等待2ms，再复位校准、再校准；两个阶段分别保持10ms软件超时。
- 超时关闭 ADC 并返回 ERROR；main 最终调用原有 Error_Handler。
- PC13、推挽2MHz、低有效LED；初始化先熄灭，随后亮500ms、灭500ms。
- 标准库子模块版本、库源码、启动汇编、链接脚本、CMake 工程名和工具链。

CMake 仅添加4个自定义源文件及4个目录的头文件搜索路径。不得让测试桩参与固件编译。未添加 `Interface_Range` 空壳、RTOS、DMA、中断采样或新的业务状态机。

## 主机回归测试

在项目根目录运行，需要 Python 3 与主机 `cc`（也可以设置 `CC=clang` 或 `CC=gcc`）：

```bash
python3 tests/test_layers.py
```

测试使用真正的模块 `.c` 文件，在临时目录生成 SPL/CMSIS 桩，编译时开启 `-std=c11 -Wall -Wextra -Werror -O2`，运行后自动清理。不会改写标准库或仓库源码。

11项测试覆盖 ADC 参数和初始化顺序、成功路径、两个校准阶段超时及跨计数回绕、LED极性与完整初始化、应用层错误传递与500ms节奏、时间计数回绕与中断转发、main初始化顺序与错误路径、头文件独立包含、分层依赖与时基唯一性。

测试桩只能证明软件调用/边界行为，不能证明 ADC 校准真实成功、模拟精度、晶振稳定、硬件时序或供电安全。非零阻塞延时的实时时长仍需上板确认。

## 固件构建与已发现的基线阻塞

```bash
cmake -S firmware -B firmware/build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build firmware/build/Debug
cmake -S firmware -B firmware/build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
cmake --build firmware/build/Release
```

本次新增 `.github/workflows/firmware-build.yml`：Linux 运行器、主机回归测试、Debug/Release 交叉编译，只有读取权限；固件/测试/构建配置变化时触发，无定时任务、不部署、不自动提交。

**2026-09-19 基线验证：尚未分层的固件在 GitHub Actions 干净检出中，Debug 和 Release 都遇到 `fatal error: core_cm3.h: No such file or directory`。**

基线运行：[CMake Firmware Build #1](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35435104761)。运行提交 `a949f7c` 只增加了CI，固件与原 `3a560d6` 相同。工具链为 GCC Arm 13.2.1、CMake 3.31.6。

固定标准库子模块 `afa743577f2784e95be2d5003380fdb84a702519` 的 `Libraries/CMSIS/CM3/CoreSupport/` 实际只有 `core_cm3.c` 和 `core_cm3.h.old`，没有被 `stm32f10x.h` 包含的 `core_cm3.h`。该问题先于本次分层存在。

本次不重命名、修改、替换或升级标准库以绕过这个问题；也不把失败标记成成功。用户报告的本地编译通过与干净检出构建结果分别记录，本地具体依赖差异尚未核实。后续应单独处理可重复构建问题，不能把头文件缺失归因于本次分层，也不能把主机桩测试通过视为完整固件验收。

## 上板验收（待执行）

在修复/确认工具链依赖后，编译并烧录，检查：仅一个 SysTick_Handler，毫秒计数持续变化，ADC 初始化返回 SUCCESS，PC13 保持原闪烁节奏，Error_Handler 不被意外触发。通过后再写 ADC 原始值读取。

不要照旧 PNG 接线；README 中已有电气连线错误警告，保持不变。

## 回滚

当前在独立分支开发，未合并 main。未合并时直接切回 main；合并后若只回退结构，单独 revert 分层提交，CI 提交可以独立保留或回退。无数据库迁移、无生产部署、未改 BalancingCar 或模板仓库。
