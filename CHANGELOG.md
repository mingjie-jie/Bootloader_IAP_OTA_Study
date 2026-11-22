# 更新记录

## v1.0 — 2026-11-22

- 原样备份 `MY_OTA_JUMP.c/.h` 两份原始代码；
- 针对 STM32H723VE，完成启动、互跳、Flash 链接、SRAM4、寄存器/汇编及 OTA 扩展设计章节；
- 提供 Boot/App 双头文件配置和更稳妥的末级汇编跳转参考；
- 提供 `.bin` 前两个向量项检测工具与 Python 单元测试；
- 增加 Mermaid 时序图、GitHub 上传教程和 UTF-8 编码提示。

**明确不包含**：完整 `.ioc`/HAL/Keil 工程、实际 Flash 编程 OTA 功能、量产级 Secure Boot 或已实机验证报告。
