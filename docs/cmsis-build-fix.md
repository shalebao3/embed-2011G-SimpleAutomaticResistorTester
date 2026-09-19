# CMSIS V1.30 缺失头文件与 GNU 构建修复

## 来源已核对

在 `shalebao3/ElectronicsCompetition` 的提交 `40eb4008bee78e254e72c33701a2309d9492fed9` 中找到：

- [BalancingCar/Start/core_cm3.h](https://github.com/shalebao3/ElectronicsCompetition/blob/40eb4008bee78e254e72c33701a2309d9492fed9/project/BalancingCar/Start/core_cm3.h)
- `common/software/c8t6/Start/core_cm3.h` 中也有相同副本。

文件为 ARM CMSIS V1.30，保留原版权和分发声明。它与项目固定子模块 `afa743577f2784e95be2d5003380fdb84a702519` 中的 `Libraries/CMSIS/CM3/CoreSupport/core_cm3.h.old` 具有相同 Git blob SHA：`7ab7b4b43685d3ab2facb53d326c48eeb2bdfda1`。因此不需要混用新版 CMSIS，也不需要配置时联网下载另一套文件。

## 两个独立问题

1. 干净检出只有 `core_cm3.h.old`，没有被 include 的正常文件名 `core_cm3.h`。基线构建已证实失败。
2. 恢复头文件后，GNU 13.2.1 构建继续暴露旧 `core_cm3.c` 的独占写汇编约束错误：`registers may not be the same -- strexb r3,r2,[r3]`，半字版本也报错。原代码使用 `"=r"` 状态输出约束，允许输出与输入地址复用寄存器；构建副本在 STREXB/H/W 三处改为 `"=&r"`，阻止复用。依据：[GCC early-clobber 约束说明](https://gcc.gnu.org/onlinedocs/gcc/Modifiers.html)。

核心源文件审核版本的 Git blob SHA 为 `fcff0d133ca83a837ea4a2076d4fc629e14d75b9`；本修复不是对 CMSIS 全部核心函数的重新实现或完整功能认证。

## 实现位置与边界

`firmware/cmake/cmsis-compat.cmake` 在构建目录 `cmsis-compat/` 恢复头文件，并为 GNU 生成只含上述三处约束调整的 `core_cm3.c` 副本。CMake 仅编译这一份核心源码。原文件存在时优先使用原 `core_cm3.h`；两个名字都缺失则报错。汇编语句不匹配审核版本或已知修复版本时停止，禁止对未知版本盲目替换。

例如 Debug 输出：

```text
firmware/build/Debug/cmsis-compat/
├── core_cm3.h
└── core_cm3.c
```

这是实际生成的完整配套文件，不是空占位头文件。无需手工复制，每次全新构建都可恢复；原子模块和 ElectronicsCompetition 仓库不写入任何变化。没有替换启动代码、链接脚本、STM32 标准外设驱动或 ADC 配置，没有调低检查、删除核心源码或改变优化选项来隐藏错误。

## 验证

现有工作流在 Debug/Release 下分别运行原 15 项主机分层测试和完整 ARM 编译链接。构建后再执行：

```bash
python3 tests/check_cmsis_compat.py firmware/build/Debug
```

新增检查验证头文件的固定 Git 哈希和逐字节一致性、核心源码差异仅限三处约束、只编译一份核心源码、子模块提交与工作区保持不变，以及 BIN/HEX/MAP 产物非空。验证失败不会被忽略。具体运行结果以对应提交的 Actions 为准。

本修复仍不代表真实 ADC 测量、硬件连接、校准精度或全部 CMSIS 指令已经上板验收。当前容器没有 ARM 工具链，完整固件编译验证在 GitHub Actions 执行，不把主机测试误报为 ARM 构建。

## 分支与回滚

继续使用 `refactor/app-driver-layering`，不修改 main，不自动合并，不修改另一份旧分层分支。回滚时撤销本次构建修复提交，保留分层代码；干净构建会恢复原先的缺失依赖错误。生成目录是构建产物，不是需要手动提交到标准库子模块的文件。
