#!/usr/bin/env python3
"""验证干净检出下 CMSIS 补全的来源、差异范围和构建产物。

在 GNU 固件构建完成后运行：
    python3 tests/check_cmsis_compat.py firmware/build/Debug
同时检查 Start/User 目录迁移后的实际编译单元；不代表 Cortex-M3 上板验证。
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def git_blob_sha(data: bytes) -> str:
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_dir", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build_dir.resolve()
    library = root / "firmware/Libraries/STM32F10x_StdPeriph_Lib"
    core = library / "Libraries/CMSIS/CM3/CoreSupport"
    compat = build / "cmsis-compat"

    # 校验固定子模块确实没有被改写、升级或添加临时文件。
    head = subprocess.check_output(["git", "-C", str(library), "rev-parse", "HEAD"], text=True).strip()
    require(head == "afa743577f2784e95be2d5003380fdb84a702519", "标准库提交与审核版本不符")
    status = subprocess.check_output(["git", "-C", str(library), "status", "--porcelain", "--untracked-files=all"], text=True)
    require(not status.strip(), "构建修改了标准库子模块：\n" + status)

    header = (core / "core_cm3.h.old").read_bytes()
    require(git_blob_sha(header) == "7ab7b4b43685d3ab2facb53d326c48eeb2bdfda1", "头文件与 ElectronicsCompetition 的审核副本不一致")
    require((compat / "core_cm3.h").read_bytes() == header, "生成头文件不是原样复制")

    source_bytes = (core / "core_cm3.c").read_bytes()
    require(git_blob_sha(source_bytes) == "fcff0d133ca83a837ea4a2076d4fc629e14d75b9", "核心源文件与审核版本不符")
    expected = source_bytes.decode("utf-8").replace("\r\n", "\n")
    for instruction in ("strexb", "strexh", "strex"):
        old = '__ASM volatile ("' + instruction + ' %0, %2, [%1]" : "=r" (result) : "r" (addr), "r" (value) );'
        new = old.replace('"=r"', '"=&r"')
        require(expected.count(old) == 1, "无法唯一定位原始 " + instruction + " 语句")
        expected = expected.replace(old, new)
    require((compat / "core_cm3.c").read_text(encoding="utf-8") == expected, "核心源文件改动超出三处汇编约束")

    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    core_units = [entry for entry in commands if Path(entry["file"]).name == "core_cm3.c"]
    require(len(core_units) == 1, "core_cm3.c 必须且只能编译一份")
    require(Path(core_units[0]["file"]).resolve() == (compat / "core_cm3.c").resolve(), "编译未使用适配后的核心源码")

    # 不能只看到文件夹就认为迁移完成：校验实际参与固件编译的来源与唯一性。
    firmware = root / "firmware"
    device = library / "Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x"
    require(not (firmware / "Core").exists(), "仍遗留旧 Core 目录")
    for filename in ("main.c", "main.h", "stm32f10x_it.c", "stm32f10x_it.h", "stm32f10x_conf.h"):
        require((firmware / "User" / filename).is_file(), "User 缺少 " + filename)
    for filename in ("startup.cmake", "cmsis-compat.cmake"):
        require((firmware / "Start" / filename).is_file(), "Start 缺少 " + filename)
    expected_units = {
        "main.c": firmware / "User/main.c",
        "stm32f10x_it.c": firmware / "User/stm32f10x_it.c",
        "system_stm32f10x.c": device / "system_stm32f10x.c",
        "startup_stm32f10x_md.s": device / "startup/TrueSTUDIO/startup_stm32f10x_md.s",
    }
    for filename, expected_path in expected_units.items():
        units = [entry for entry in commands if Path(entry["file"]).name == filename]
        require(len(units) == 1, filename + " 必须且只能编译一份")
        unit = Path(units[0]["file"])
        if not unit.is_absolute():
            unit = Path(units[0]["directory"]) / unit
        require(unit.resolve() == expected_path.resolve(), filename + " 编译来源不正确")
    for entry in commands:
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        require("/Core/Inc" not in command and "/Core/Src" not in command, "编译命令仍引用旧 Core 路径")
        require("tests/layering/mocks" not in command, "主机测试桩混入固件构建")

    project = "embed-2011G-SimpleAutomaticResistorTester"
    for suffix in (".bin", ".hex", ".map"):
        artifact = build / (project + suffix)
        require(artifact.is_file() and artifact.stat().st_size > 0, "缺少构建产物：" + str(artifact))
    print("PASS: original header hash, byte-identical restore, only three STREX edits, single core source, clean pinned submodule, BIN/HEX/MAP artifacts")
    print("PASS: Start/User layout, unique main/ISR/system/GNU startup units, no legacy Core paths or host mocks")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as exc:
        raise SystemExit("CMSIS compatibility verification failed: " + str(exc)) from exc
