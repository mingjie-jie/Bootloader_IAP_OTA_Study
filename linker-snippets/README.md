# SRAM4 末尾 12 字节的链接器预留示例

> 原代码直接使用 `0x38003FF4 / 0x38003FF8 / 0x38003FFC`。**必须在 Boot 和 APP 两个工程中同时预留**，否则 `.data/.bss/heap/stack`、DMA 或别的对象可能覆盖一次性跳转请求。

## GCC 的 MEMORY 长度修改思路

如果原工程有一块独立的 SRAM4 区：

```ld
/* 原始示意：SRAM4 全部 16 KiB 可用 */
RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 16K
```

要让普通段不使用 SRAM4 最后 12B，可以改成：

```ld
/* 注意：仅供示意，实际段布局和 STACK/HEAP 都要检查 */
RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 0x3FF4
/* 0x38003FF4–0x38003FFF 给 SRAM4 跳转请求固定保留 */
```

**这不是完整可直接链接的 `.ld`**，只说明如何对已有内存区域进行裁剪。若工程有其他区域/段仍覆盖这 12 字节，仍需修改。也不能让 MSP 的初始栈顶落在被排除的区域。

## Keil scatter 或配置界面

同样需要把 SRAM4 中给 RW/ZI/heap/stack 使用的区域终点限制在 `0x38003FF4` 以下，保留区的访问者才是你的跳转协议。项目中如果从未使用 SRAM4 也应检查 MPU/Cache/调试器访问该地址的效果。

## 为什么不建议直接 `__attribute__((section(".noinit")))` 就算结束？

`noinit` 可以避免某一变量被启动代码清零，但**不自动保证它恰好放在 0x38003FF4**，也不保证不会与其他应用/Boot 使用的内存重叠。需要链接脚本显式定位或限制内存区。

## 清理缓存的额外条件

H7 在启用 MPU/D-Cache 时要明确 SRAM4 的缓存属性：如果写入是在 write-back cacheable 内存里，写入 `volatile` 并不等于数据已经回写到 SRAM。考虑把协议区域设为 non-cacheable、或在复位前对齐清理对应缓存行（按你当前 CMSIS API 的对齐/长度要求）。软件复位可能改变 Cache 状态，在实际硬件上验证能否读到跳转记录。

## 测试建议

- 两边编译后阅读 `.map`，搜索 SRAM4 的全部 RW/ZI 段、栈、heap；
- 用调试器读 `0x38003FF4`、`0x38003FF8`、`0x38003FFC`；
- 写入一次后 `NVIC_SystemReset`，在 constructor 第一条检查分支前观察记录是否还在；
- 重复启用/禁用 Cache 与不同优化级别条件，不要靠一次实验代表稳定性。
