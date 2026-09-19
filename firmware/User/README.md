# User：项目入口与中断

本目录由原 `Core/Inc`、`Core/Src` 中的五个项目文件迁入，统一使用 `User` 大小写。迁移保持文件内容不变，`.c/.h` 放在一起。

| 文件 | 职责 |
| --- | --- |
| [main.c](main.c) | 更新时钟信息、先初始化时基再初始化应用、循环调用应用任务、处理启动失败 |
| [main.h](main.h) | 项目入口相关声明 |
| [stm32f10x_it.c](stm32f10x_it.c) | 唯一的项目中断实现，SysTick 转发给 `Com_Time_Tick()` |
| [stm32f10x_it.h](stm32f10x_it.h) | 中断函数声明 |
| [stm32f10x_conf.h](stm32f10x_conf.h) | 项目选择的标准外设库头文件和断言配置，不是第三方内核头文件 |

不要把全部自定义代码重新堆进 User：ADC 在 `Driver`，LED 在 `Interface`，时基在 `Common`，仪器业务在 `App`。这里的“入口”不是说上电后 CPU 首先执行 main；复位和启动支持见 [Start](../Start/README.md)。

初始化顺序不变：

```text
main
 ├─ SystemCoreClockUpdate
 ├─ Com_Time_Init
 ├─ App_ResistorTester_Init
 │    ├─ Interface_LED_Init
 │    └─ Driver_ADC1_Init
 └─ while (1) → App_ResistorTester_Task

SysTick_Handler → Com_Time_Tick
```

ADC 的延时和超时依赖已工作的 SysTick，因此不能把时基初始化移到 ADC 初始化之后。更多运行边界见 [软件分层说明](../../docs/software-layering.md)。
