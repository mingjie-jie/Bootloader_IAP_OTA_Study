#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查 Cortex-M 二进制镜像前 8 字节的初始 MSP 和 Reset_Handler。

仅离线读取 bin，不烧录、不修改文件、不代替 Hash/签名/整包验证。
用法：
    python3 tools/check_vectors.py app.bin --base 0x08020000 --flash-end 0x08080000
"""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys

RAM_REGIONS = (
    (0x20000000, 0x20020000, "DTCM"),
    (0x24000000, 0x24050000, "AXI SRAM (需核对实际配置)"),
    (0x30000000, 0x30008000, "SRAM1/2"),
    (0x38000000, 0x38003FF4, "SRAM4 (末 12 字节保留)"),
)


def inspect_vector_bytes(data: bytes, base: int, flash_end: int) -> dict:
    """对纯二进制开头 MSP/PC 执行静态检查（不证明固件完整性）。"""
    if len(data) < 8:
        raise ValueError("bin 不足 8 字节，不能构成向量表前两项")
    if base < 0 or flash_end <= base:
        raise ValueError("目标 Flash 地址范围无效")
    msp, reset = struct.unpack_from("<II", data)
    reset_instruction_address = reset & ~1
    ram = next((name for lo, hi, name in RAM_REGIONS if lo < msp <= hi), None)
    issues = []
    if (msp & 7) != 0:
        issues.append("MSP 未按 8 字节对齐")
    if ram is None:
        issues.append("MSP 不在本示例允许的 SRAM 地址范围内")
    if not (reset & 1):
        issues.append("Reset_Handler 最低位不是 1（不是合法的 Thumb 函数指针）")
    if not (base <= reset_instruction_address < flash_end):
        issues.append("Reset_Handler 指令地址不在指定固件 Flash 分区")
    if len(data) > flash_end - base:
        issues.append("bin 文件总长度超过指定分区")
    if len(data) < 16:
        issues.append("镜像非常短，虽有 8 字节向量头，但不像完整程序")
    return {
        "msp": msp,
        "reset": reset,
        "reset_instruction_address": reset_instruction_address,
        "ram": ram,
        "issues": issues,
        "size": len(data),
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("binary", type=pathlib.Path, help="APP/Bootloader .bin 路径")
    ap.add_argument("--base", type=lambda v: int(v, 0), required=True,
                    help="此镜像链接起始地址，例如 0x08020000")
    ap.add_argument("--flash-end", type=lambda v: int(v, 0), required=True,
                    help="镜像末地址（不含），例如 0x08080000")
    ns = ap.parse_args(argv)
    try:
        data = ns.binary.read_bytes()
        result = inspect_vector_bytes(data, ns.base, ns.flash_end)
    except (OSError, ValueError) as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        return 2

    print(f"镜像: {ns.binary}  ({result['size']} 字节)")
    print(f"分区: 0x{ns.base:08X} ~ 0x{ns.flash_end:08X} (开区间)")
    print(f"初始 MSP:    0x{result['msp']:08X}  RAM: {result['ram'] or 'unknown'}")
    print(f"Reset 向量:  0x{result['reset']:08X}  (Thumb位={result['reset'] & 1})")
    print(f"真实指令址:  0x{result['reset_instruction_address']:08X}")
    if result["issues"]:
        for issue in result["issues"]:
            print("[FAIL] " + issue)
        return 1
    print("[PASS] 前两个向量项初步符合要求；不代表完整固件已验证。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
