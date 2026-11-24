#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""纯 Python 模拟上传原代码的双向跳转流程。

不访问 STM32 外设、不写入 Flash、不保证硬件行为。
假设默认软件复位从 Boot (0x08000000) 启动。
"""
from __future__ import annotations

import argparse

BASES = {"boot": 0x08000000, "app": 0x08020000}
MAGIC = 0x4A554D50


def simulate(origin: str, target: str, *, target_valid: bool = True) -> list[str]:
    """返回带事件日志的原始源码控制流模型。"""
    if origin not in BASES or target not in BASES:
        raise ValueError("origin/target must be boot or app")
    if origin == target:
        raise ValueError("本仓库双工程配置只允许请求跳向另一份镜像")
    log = [f"正在运行: {origin} @0x{BASES[origin]:08X}"]
    if not target_valid:
        log.append("Request(): 目标向量表初检不通过 -> return（没有软件复位）")
        return log

    record = {"magic": MAGIC, "target": BASES[target], "inv": (~BASES[target]) & 0xFFFFFFFF}
    log.append(f"Request(): SRAM4 写入 MAGIC / TARGET=0x{record['target']:08X} / ~TARGET")
    log.append("NVIC_SystemReset(): 模拟系统复位")
    running = "boot"  # 明确是模拟前提，不是无条件的硬件结论
    log.append(f"上电默认映射 -> {running} Reset_Handler -> constructor")
    if record["magic"] != MAGIC or record["inv"] != ((~record["target"]) & 0xFFFFFFFF):
        log.append("constructor: 校验失败 -> 当前 main")
        return log
    requested = record["target"]
    record = {"magic": 0, "target": 0, "inv": 0}
    log.append("constructor: 请求合法 -> 清 SRAM4 一次性记录")
    if requested == BASES[running]:
        log.append(f"请求目标==当前 {running} -> 直接进入 {running} main；不执行 JumpNow")
    else:
        log.append("请求目标!=当前 Boot -> 校验 APP 向量表")
        log.append("JumpNow(): 关中断/更新VTOR、MSP -> 进入 APP Reset_Handler")
        log.append("APP 的 constructor 读到清空标志 -> APP main")
    return log


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--from", dest="origin", choices=BASES, required=True)
    parser.add_argument("--target", choices=BASES, required=True)
    parser.add_argument("--invalid-target", action="store_true", help="模拟 Request() 在入口检查失败")
    args = parser.parse_args()
    for line in simulate(args.origin, args.target, target_valid=not args.invalid_target):
        print("- " + line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
