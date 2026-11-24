#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""STM32H723VE 512 KiB Flash 与 SRAM4 地址可视化（离线，不烧录）。"""
from __future__ import annotations

FLASH_START = 0x08000000
FLASH_END = 0x08080000
SECTOR_SIZE = 0x20000
SRAM4_START = 0x38000000
SRAM4_END = 0x38004000
REQUEST_START = 0x38003FF4


def sectors() -> list[tuple[int, int, int]]:
    return [(i, FLASH_START + i * SECTOR_SIZE, FLASH_START + (i + 1) * SECTOR_SIZE)
            for i in range((FLASH_END - FLASH_START) // SECTOR_SIZE)]


def validate_ranges(boot_start=0x08000000, boot_end=0x08020000,
                    app_start=0x08020000, app_end=0x08080000) -> list[str]:
    """只作 sector/重叠检查，并非固件安全审查。"""
    errors = []
    bounds = (boot_start, boot_end, app_start, app_end)
    if any(v < FLASH_START or v > FLASH_END for v in bounds):
        errors.append("分区越过 STM32H723VE 512 KiB Flash 物理范围")
    if boot_start >= boot_end or app_start >= app_end:
        errors.append("分区长度不合法")
    if boot_start < app_end and app_start < boot_end:
        errors.append("Boot 和 APP 分区发生重叠")
    if any((v - FLASH_START) % SECTOR_SIZE for v in bounds):
        errors.append("分区边界未对齐 128 KiB Flash 扇区")
    return errors


def main() -> int:
    print("STM32H723VE: FLASH 0x08000000..0x0807FFFF, 512 KiB")
    for number, lo, hi in sectors():
        print(f"Sector {number}: 0x{lo:08X}..0x{hi-1:08X}  (128 KiB)")
    print(f"SRAM4: 0x{SRAM4_START:08X}..0x{SRAM4_END-1:08X}")
    print(f"请求记录: 0x{REQUEST_START:08X}..0x{SRAM4_END-1:08X}（共 12 字节）")
    problems = validate_ranges()
    print("[PASS] 示例 Boot/APP Flash 分区无重叠且扇区对齐" if not problems else "[FAIL] " + "; ".join(problems))
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
