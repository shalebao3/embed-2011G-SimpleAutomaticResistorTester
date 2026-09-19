# 2011G 软件分层与回归验证

> 构建状态更新（2026-09-19）：在 `refactor/app-driver-layering` 的修复提交 `479b723b4aa013972b1143a5018c37157fc68e3b` 上，GitHub Actions 的 Debug、Release 主机测试与完整 ARM 编译链接均已通过，CMSIS 来源/差异/产物/子模块不变性校验也已通过。[对应运行记录](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35441242606)。缺失文件来源和最小构建修复见 [CMSIS 构建修复说明](cmsis-build-fix.md)。尚未做真实上板验收，main 未修改。

## 改动范围

业务代码基线为 `3a560d62d18681d53d8dc65e3baea5a19014146d`；复用现有分支中 `a949f7c737c7d1149a386caa7329d117fe0882f1` 的只读构建工作流并追加主机测试。做职责拆分时，参考 BalancingCar 的目录组织，但不复制其寄存器驱动、FreeRTOS、Keil 工程或业务代码。

这次不新增 ADC 读取、电阻换算、自动量程、继电器、DMA、TIM 触发或显示功能。ADC 配置、上电稳定等待 2ms、两阶段校准各 10ms 超时、错误后关闭 ADC、LED 亮灭各 500ms 均保持原实现。

## 文件职责

| 位置 | 职责 |
| --- | --- |
| `firmware/Core/Src/main.c` | 更新系统时钟信息，先初始化时基，再初始化应用，循环调用应用任务；保留 Error_Handler |
| `firmware/App/App_ResistorTester.c/.h` | 组织 LED、ADC 初始化和当前验证任务；以后放测量调度、阻值换算与换档策略 |
| `firmware/Driver/Driver_ADC.c/.h` | ADC1/PA0 的标准库初始化和校准；以后扩展读取原始值，不处理参考电阻与量程业务 |
| `firmware/Interface/Interface_LED.c/.h` | GPIOC 时钟、PC13 初始化及低电平有效的亮灭操作 |
| `firmware/Common/Com_Time.c/.h` | 使用 CMSIS SysTick 的毫秒服务；此公共服务仍依赖 STM32，不是纯平台无关算法 |
| `firmware/Core/Src/stm32f10x_it.c` | 保留唯一的中断入口；SysTick 只调用 Com_Time_Tick |
| `tests/layering` | 独立主机回归测试，使用硬件接口替身，不进入固件编译 |

同一模块的 `.c/.h` 放在一起；现有 `Core/Inc`、`Core/Src`、标准库子模块、启动代码和链接脚本的位置不变。分层提交为 CMake 增加四个源文件和四个头文件搜索路径，固件编译参数不变；后续 CMSIS 构建兼容处理独立记录在 [构建修复说明](cmsis-build-fix.md)。

## 调用顺序

```text
main
 ├─ SystemCoreClockUpdate
 ├─ Com_Time_Init                         必须先启动 SysTick
 ├─ App_ResistorTester_Init
 │    ├─ Interface_LED_Init              保留原有 LED 配置顺序
 │    └─ Driver_ADC1_Init
 │         └─ Com_Time_DelayMs / GetMs    依赖已运行的时基
 └─ while (1)
      └─ App_ResistorTester_Task         当前保留阻塞 LED 示例

SysTick_Handler → Com_Time_Tick → 私有 s_ms_ticks
```

`Driver` 与 `Interface` 不反向调用 `App` 或 `main`；需要时间服务时可以使用 `Common`。App 可以直接调用 ADC 驱动，不创建只转发调用的接口层。后续真正接入继电器时才增加 `Interface_Range`，不提前堆空模块。

## 时间与中断约束

- 计数唯一存放在 `Com_Time.c` 的 `static volatile uint32_t s_ms_ticks`，不再通过 `main.h` 暴露可写全局量。
- `Com_Time_Init()` 只在启动阶段调用一次，使用已更新的 `SystemCoreClock`，不重置已运行的计数。
- 超时仍使用无符号差值 `(uint32_t)(now - start)`，保留回绕处理。
- `Com_Time_DelayMs()` 和 ADC 初始化要求 SysTick 能继续运行，不可在中断中或关闭中断时调用。此次没有把它们改为非阻塞实现，也没有增加“中断被冻结”时的第二时钟源。
- 当前 `App_ResistorTester_Task()` 仍一次阻塞约 1 秒；这是等价拆分，不是测量调度已完成。后续加入采样时，再单独设计非阻塞任务节奏。
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

每个构建类型包含 15 个用例：ADC 参数与调用顺序、两阶段校准超时、超时跨计数回绕、应用初始化及错误传播、LED 极性与闪烁节奏、SysTick 配置成功/失败、Tick 和 ISR 转发、计数回绕以及零延时。

测试没有使用会被 Release 的 `NDEBUG` 关闭的断言。`test_time.c` 在测试翻译单元中纳入真实时间模块实现，仅为设置其私有计数到回绕边界；不向生产接口增加测试专用 setter。非零延时依赖的真实中断时序仍需上板验证。

真正的 ARM 固件构建需要初始化子模块并安装完整的 `arm-none-eabi-gcc` 与 newlib：

```bash
set -e
git submodule update --init --recursive
for config in Debug Release; do
  cmake -S firmware -B "firmware/build/$config" -G Ninja \
    -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake
  cmake --build "firmware/build/$config"
done
```

云端工作流先执行主机测试，再执行 ARM 固件构建及 CMSIS 兼容副本校验，使用只读仓库权限，不烧录、不部署、不提交代码，没有定时任务。

## 历史构建阻塞（已在分层分支修复）

分层前的业务代码在 CI 提交 `a949f7c737c7d1149a386caa7329d117fe0882f1` 下已出现 Debug/Release 编译失败：[基线运行记录](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35435104761)。Debug 日志首先报 `core_cm3.h: No such file or directory`。固定子模块提交 `afa743577f2784e95be2d5003380fdb84a702519` 的 `Libraries/CMSIS/CM3/CoreSupport/` 只有 `core_cm3.c` 和 `core_cm3.h.old`，缺少正常文件名的 `core_cm3.h`。

后续在 ElectronicsCompetition 找到同版本、同 Git blob SHA 的头文件，通过 CMake 在构建目录恢复完整文件；继续构建后发现的 GNU STREX 寄存器约束问题也只在生成副本中进行了三处最小修复。子模块原文件和提交保持不变，未借用新版 CMSIS。修复提交 `479b723b4aa013972b1143a5018c37157fc68e3b` 的 [Actions](https://github.com/shalebao3/embed-2011G-SimpleAutomaticResistorTester/actions/runs/35441242606) 已验证 Debug/Release 全部步骤成功。以上原始失败记录保留作根因证据，不再作为本分支当前阻塞。

用户原先的本地编译为何通过仍未核实；本修复保证固定子模块的云端干净检出可构建，不据此推断或覆盖用户本地曾做过的修改。主机测试及构建成功不能代替上板测量。

## 上板验收与回滚

主机测试和交叉编译均不能验证真实 ADC 校准、模拟电压、电阻精度或晶振是否正常。上板时至少检查：进入主循环而不是 Error_Handler；SysTick 每次仅更新一次计数；LED 仍亮 500ms、灭 500ms；ADC1 初始化返回 SUCCESS，参数与拆分前一致。

改动在独立分支审查，未合并 main 时可直接切回 main。合入后如需撤回，用 `git revert` 撤销对应分层或构建修复提交，避免 `reset --hard` 丢弃其他工作。此次不涉及数据库、生产部署或标准库子模块更新。
