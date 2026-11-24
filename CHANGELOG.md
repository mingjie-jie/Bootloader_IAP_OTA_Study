# 更新记录

## v2.0 — 2026-11-24

- 在 v1.0 原 11 章的基础上新增 **12–21 共 10 章**，重点讲解函数指针控制流、Bootloader 跳转前置条件、PendSV 硬件/软件保存、SVC/EXC_RETURN、源码逐行寄存器状态。
- 根据 ST 官方 STM32H723 GCC 模板补充 `ExitRun0Mode → SystemInit → .data/.bss → __libc_init_array → main` 的具体链路（不同工具链需核对）。
- 深化 OTA 状态机、固件安全验证、Flash 128KiB Sector 边界、掉电中断恢复和旧固件回滚条件，严格标注为 **扩展方案**，非原源码已有功能。
- 新增离线流程模拟 `tools/boot_flow_sim.py`、Flash 扇区检查 `tools/flash_layout.py`、覆盖相应单元测试。
- 新增 `linker-snippets/README.md`，解释为何要在两个工程同时预留 SRAM4 末 12 字节。
- 新增如何更新已存在 GitHub 仓库的详细步骤与编码处理。
- 保留 `original/` 中上传的源码原样不变，安全汇编示例单独存储，**没有伪称已实现可烧录量产 OTA**。

## v1.0 — 2026-11-22

- 原样备份 `MY_OTA_JUMP.c/.h` 两份原始代码；
- 针对 STM32H723VE，完成启动、互跳、Flash 链接、SRAM4、寄存器/汇编及 OTA 扩展设计章节；
- 提供 Boot/App 双头文件配置和更稳妥的末级汇编跳转参考；
- 提供 `.bin` 前两个向量项检测工具与 Python 单元测试；
- 增加 Mermaid 时序图、GitHub 上传教程和 UTF-8 编码提示。

**明确不包含**：完整 `.ioc`/HAL/Keil 工程、实际 Flash 编程 OTA 功能、量产级 Secure Boot 或已实机验证报告。
