# STM32F103C8T6 标准库 CMake 模板

用于后续电子设计竞赛题目的 STM32F103C8T6 基础工程。

> 当前 `main` 保存 STM32F103C8T6 + 标准库 + CMake 模板。后续这个仓库可继续沉淀其它 STM32 模板。

## 技术路线

- MCU：STM32F103C8T6
- 标准库：STM32F10x Standard Peripheral Library V3.5.0
- CMSIS：标准库自带 CMSIS CM3
- 编译器：arm-none-eabi-gcc
- 构建：CMake + Ninja
- 编辑器：VSCode
- 调试器：ST-Link
- 不使用 CubeMX
- 不使用 HAL

这个模板的目标是保留标准库的结构体初始化方式，同时让每个配置都还能继续追到参考手册和寄存器。

## 目录

```text
.
├── .vscode
├── firmware
│   ├── Core
│   │   ├── Inc
│   │   └── Src
│   ├── Libraries
│   │   └── STM32F10x_StdPeriph_Lib
│   ├── cmake
│   │   └── gcc-arm-none-eabi.cmake
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   └── STM32F103xx_FLASH.ld
└── README.md
```

## 首次使用

该仓库把 STM32F10x 标准库作为 Git submodule 固定到明确 commit。

```bash
git submodule update --init --recursive
```

macOS 需要能够执行：

```bash
arm-none-eabi-gcc --version
cmake --version
ninja --version
```

VSCode 推荐扩展已经写入 `.vscode/extensions.json`：

- C/C++
- CMake Tools
- Cortex-Debug

## 构建

VSCode 打开仓库后，CMake Tools 会自动把 `firmware` 作为源码目录，并使用 `CMakePresets.json`。

命令行也可以直接从仓库根目录构建：

```bash
cmake -S firmware -B firmware/build/Debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake

cmake --build firmware/build/Debug
```

输出：

```text
firmware/build/Debug/
├── stm32f103_std_template.elf
├── stm32f103_std_template.hex
├── stm32f103_std_template.bin
└── stm32f103_std_template.map
```

## 默认验证程序

`Core/Src/main.c` 使用标准库初始化 GPIOC Pin 13，并按常见 Blue Pill 板卡的 PC13 低电平点亮方式每 500 ms 闪烁一次。

核心写法：

```c
GPIO_InitTypeDef gpio_init;

GPIO_StructInit(&gpio_init);
gpio_init.GPIO_Pin = GPIO_Pin_13;
gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
GPIO_Init(GPIOC, &gpio_init);
```

如果你的板子 PC13 没接 LED，只需要修改验证引脚，不影响模板本身。

## 为什么保留标准库

建议以后按下面这条链学习：

```text
参考手册寄存器
    ↓
标准库结构体 / 枚举 / 函数
    ↓
GPIO_InitTypeDef / TIM_TimeBaseInitTypeDef / DMA_InitTypeDef ...
    ↓
标准库内部实现
    ↓
寄存器
    ↓
外设硬件
```

标准库比直接寄存器开发少一层重复劳动，但不会像 CubeMX + HAL 那样把初始化细节隐藏得太深。

## 新电赛题怎么使用

1. 从这个仓库创建新分支或复制为新的题目仓库。
2. 保留 `Libraries`、linker、CMake 和 VSCode 配置。
3. 修改 CMake 工程名。
4. 删除或替换 PC13 LED 验证代码。
5. 题目复杂后再增加 `App/`、`Driver/`、`Bsp/` 等业务目录。
6. 不要修改标准库子模块里的源码，除非明确需要修库。
