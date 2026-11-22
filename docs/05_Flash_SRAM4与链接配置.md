# 05 | STM32H723VE Flash、SRAM4、链接脚本与 VTOR

## 1. 使用的是 H723VE，不是 F407

根据 ST STM32H723VE 产品页，该具体型号为 **512 KiB Flash**；RM0468 的 Flash 组织是 **128 KiB/扇区**，512 KiB 版本只有 Sector 0~3。不要用 F407 的 16/64/128 KiB 混合扇区布局推断本工程。

| Sector | 起点 | 末地址（开区间） | 容量 | 此教学布局中的用途 |
|---|---|---|---|---|
| 0 | `0x08000000` | `0x08020000` | 128 KiB | Bootloader |
| 1 | `0x08020000` | `0x08040000` | 128 KiB | APP |
| 2 | `0x08040000` | `0x08060000` | 128 KiB | APP |
| 3 | `0x08060000` | `0x08080000` | 128 KiB | APP |

**不能因为芯片系列有 1 MB 版本，就把 H723VE 的末地址写成 `0x08100000`！**

## 2. 双工程链接配置

### Bootloader

```text
Flash ORIGIN=0x08000000, LENGTH=128K
向量表 0x08000000
目标地址 0x08020000
目标上界 0x08080000
```

### Application

```text
Flash ORIGIN=0x08020000, LENGTH=384K
向量表 0x08020000
目标地址 0x08000000（返回 Boot）
目标上界 0x08020000
```

### STM32CubeIDE / GCC 链接脚本 `MEMORY` 示例

**Bootloader**（只截 Flash 部分）：

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
    DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    /* 其余原始 RAM / 外设定义应保留，不要整体复制替换 */
    SRAM4 (xrw) : ORIGIN = 0x38000000, LENGTH = 0x3FF4
}
```

**APP**：

```ld
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08020000, LENGTH = 384K
    DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    SRAM4 (xrw) : ORIGIN = 0x38000000, LENGTH = 0x3FF4
}
```

> 以上 `MEMORY` 只是**需要更改的段落示意**，不是可直接覆盖你整个 `.ld` 的完整脚本。真实工程还需保留 AXI SRAM、SRAM1/2、堆栈和各段定义。若工程已使用 SRAM4，应该修改相应长度及映射，避免末尾 12 字节被分配；也应检查任何自定义初始化循环没有按 16KB 整片清零。

### Keil / scatter

Keil 工程设置中 `IROM1` 示例：

```text
Bootloader: Start=0x08000000, Size=0x00020000
Application: Start=0x08020000, Size=0x00060000
```

如果通过 scatter 文件单独放置 SRAM4 内容，应限制可分配区域为 `[0x38000000, 0x38003FF4)`，并核查启动文件/调试初始化脚本是否覆盖最后三个字。Keil AC5/AC6 与 GCC 的构造函数和向量表符号可能不同，请勿只改 IROM1 就认为完整移植成功。

## 3. 为什么 VTOR 要分别配置？

`SCB->VTOR` 指明当前异常/中断从何处取中断向量：

```c
SCB->VTOR = 0x08000000UL;  // Bootloader 中断表
SCB->VTOR = 0x08020000UL;  // APP 中断表
__DSB();
__ISB();
```

最容易出错的情况：Bootloader 已经设过 `0x08020000`，但 APP 的 `SystemInit()` 又将 VTOR 设回 `0x08000000`。调试时应该**在 APP main 入口再读一次 VTOR**，不要只看 Bootloader 跳转前的值。

## 4. 为什么保留的是 SRAM4 最后 12 字节？

```text
SRAM4 0x38000000 ─────────────────────────────
      ...          程序可使用的其余 SRAM4 空间
      0x38003FF4   MAGIC        = 0x4A554D50
      0x38003FF8   TARGET       = 0x08020000
      0x38003FFC   TARGET_INV   = 0xF7FDFFFF（例）
      0x38004000   SRAM4 结束（开区间）
```

注意：上例 `0x08020000` 的逐位反码应该由真实程序计算，不能凭经验直接敲入。查 RAM 时也不要混淆 **SRAM4** 与 H7 的 **Backup SRAM（备份 SRAM）**，两者不是同一种存储区域；二者复位/掉电保持条件也不同。

（校对参考：`~0x08020000U` = `0xF7FDFFFFU`。）

### 三大条件缺一不可

1. **存储器物理存在**：芯片是 H723VE，SRAM4 区域 16 KiB。
2. **链接/栈/DMA 不占用**：两个工程都必须从链接可用空间排除最后 12B；不要把数据放进 `.bss` 或任何被启动代码清零的范围。
3. **软件复位路径保持有效**：使用 SYSRESETREQ，若启用 D-Cache write-back 或 MPU 属性不同，需要确认 CPU 写入已到物理 SRAM；测试时结合 MPU/Cache 配置核实。该机制不承诺断电持久化。

## 5. 目标固件 `.bin` 的第一字和第二字

假设 `APP.bin` 是从 `0x08020000` 链接得到的**纯二进制**：

```text
文件偏移 0x00~0x03：初始 MSP（小端）
文件偏移 0x04~0x07：Reset_Handler（小端、bit0=1）
```

运行 `python3 tools/check_vectors.py APP.bin --base 0x08020000 --flash-end 0x08080000` 检查。此工具只做初步向量检查，并不检查 Flash 编程是否完整、CRC 或数字签名。

## 6. 以后做 IAP/OTA 时如何分区？

如果内置 512 KiB Flash 只有 Boot=128 KiB、APP=384 KiB，那么**没有独立的第二份 384 KiB APP 空间**。真正想支持完整 A/B 镜像或升级断电回滚，可能需要：

- 先压缩业务固件，让内部 Flash 能够合理划出两个较小镜像槽（还需考虑 128KiB 擦除颗粒）；
- 或使用外部 SPI/QSPI NOR 保存新的下载镜像和旧固件备份；
- 或采用可靠外部恢复下载模式与断点安装机制。

此外，两个不同地址直接运行的镜像必须考虑**链接位置**：普通 Cortex-M 固件一般不是任意地址可重定位的。必须分别链接，或采用固定运行地址 + 暂存复制方案。详见 06。

## 7. ST 官方资料

- [H723VE](https://www.st.com/en/microcontrollers-microprocessors/stm32h723ve.html)
- [RM0468](https://www.st.com/resource/en/reference_manual/rm0468-stm32h723733-stm32h725735-and-stm32h730-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)（Flash memory organization，128 KiB sectors；512KB 只有 sector0–3）
