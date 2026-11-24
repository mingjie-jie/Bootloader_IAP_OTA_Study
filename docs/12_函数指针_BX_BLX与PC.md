# 12｜函数指针为何能启动另一份程序？从 C 声明到 BX/BLX 和 PC

> 对应上传源码 `original/MY_OTA_JUMP.c` 中的 `typedef`、`app_entry`、`app_reset` 和 `app_entry()`。**这里描述的是原代码实际意图**；后文关于安全分支是**改进建议**。

## 1. 先区分四个不同的东西

```c
typedef void (*MY_OTA_JUMP_EntryFunction)(void);
MY_OTA_JUMP_EntryFunction app_entry;
app_entry = (MY_OTA_JUMP_EntryFunction)app_reset;
app_entry();
```

1. `void (*...)(void)`：指向“无参数、无返回值函数”的**指针类型**，不是函数本体。
2. `typedef ... MY_OTA_JUMP_EntryFunction`：为该**类型**起别名；不产生数据、也没有函数调用。
3. `app_entry`：一个函数指针变量，存储一个可调用入口的地址值。
4. `app_entry()`：让 CPU 对这个入口做**间接调用**。类型转换不会检查该地址是否真是函数，更不会自动初始化目标程序。

区别：

```c
void (*p)(void);  // p 是函数指针变量
void *p(void);    // p 是一个函数：不接收参数、返回 void*
```

括号 `(*p)` 的作用是让 `*` 与 `p` 结合；否则 `p(void)` 会先被解析成函数声明。

## 2. 普通函数调用：编译器已经知道目标地址

```c
void led_on(void) { /* ... */ }
void demo(void) { led_on(); }
```

在某次链接结果中，`led_on` 的机器指令可能位于 `0x08001480`（仅为举例），链接器把函数符号解析成地址，调用指令让 PC 改变。编译器有时用 `BL 目标地址`；也可能内联，不能断言每次都存在 `BL`。

## 3. 间接函数调用：运行时从寄存器中取目标地址

```c
void led_on(void) { /* ... */ }
void (*p)(void) = led_on;
p();
```

在 Cortex-M 上，编译器可能生成（**示意，不是你实际编译得到的反汇编**）：

```asm
ldr  r3, [sp, #offset]    ; 从变量位置取出函数指针
blx  r3                  ; 保存普通返回地址到 LR，然后跳转 R3
```

`BLX` 是带链接的分支（设置 LR），`BX` 是不带链接的分支（不为调用留下普通返回地址）。两者都可能按寄存器目标执行分支。需要以 **Keil/STM32CubeIDE 的反汇编窗口**核实生成结果，优化器可能完全改变这段实现。

## 4. 为什么 `app_reset` 是一串数字，却能变成函数指针？

原代码：

```c
uint32_t app_reset = *(volatile uint32_t *)(MY_OTA_APP_ADDRESS + 4UL);
app_entry = (MY_OTA_JUMP_EntryFunction)app_reset;
```

假设 APP 链接起点为 `0x08020000`，向量表开头是：

| 地址 | 内容（示例） | 由谁写入 |
|---|---|---|
| `0x08020000` | `0x20020000`（初始 MSP） | 编译/链接时由向量表产生 |
| `0x08020004` | `0x08020439`（Reset_Handler 入口值） | 编译/链接时由向量表产生 |
| `0x08020008` | NMI_Handler 入口值 | 编译/链接时由向量表产生 |
| `0x0802000C` | HardFault_Handler 入口值 | 编译/链接时由向量表产生 |

`*(uint32_t *)(base+4)` 读取的是**存放在向量表里的 32 位内容**，不是 `base+4` 这个位置本身。读出来的 `0x08020439` 经过显式转换，就被解释为“函数入口值”。

```text
0x08020004 这个地址里的 4 字节
           ↓ 读取
app_reset = 0x08020439
           ↓ 转换成函数指针类型（不改变目标代码）
app_entry  = 0x08020439
           ↓ 间接分支
PC 指向 0x08020438 附近的 Thumb 指令流
```

**重要：**“整数转函数指针”在 ISO C 中有实现相关的部分。在 ARM 32 位裸机 ABI/CMSIS 的约定下，这种写法是常见的嵌入式实践；不能把它当作跨平台 C 语言保证任意整数均可调用。

## 5. Thumb 位 bit0 为什么是 1？

Cortex-M7 只执行 Thumb 指令，入口值最低 bit 通常置 1，表示 Thumb 状态：

```text
函数的物理指令地址：0x08020438（半字对齐）
作为函数指针存储：0x08020439（bit0 = 1）
校验实际范围时使用：(app_reset & ~1UL)
```

跳到 bit0 为 0 的无效目标可能产生 INVSTATE UsageFault，并可能升级成 HardFault。这个 bit 不是“地址偏移一个字节”，更不意味着指令真的从 `0x...439` 奇地址开始。

## 6. 为什么用 Reset_Handler，而不跳 `main()`？

直接跳 `main()` 会绕过：

- 处理器与系统初始化（工程的 `SystemInit` 路径）；
- `.data` 已初始化变量的 Flash→RAM 搬运；
- `.bss` 清零、C/C++ 运行时构造函数；
- 启动文件约定的异常向量环境。

真实时序（GCC 风格，以**具体启动文件为准**）：

```text
向量表第 2 个字  → Reset_Handler
                         ↓
                 ExitRun0Mode()（H723 GCC 模板，版本依赖）
                         ↓
                 SystemInit()
                         ↓
              初始化 .data / .bss
                         ↓
                __libc_init_array()
                         ↓
                   main()
```

**源码中的 `app_entry()` 只是控制流转移。** `VTOR`、`MSP`、`CONTROL`、中断和外设状态还需要单独准备。

## 7. 为什么建议最后使用 `BX`，而不是普通 `app_entry()`？

你的原代码：

```c
__set_MSP(app_stack);
__DSB();
__ISB();
__enable_irq();
app_entry();
```

风险不是“函数指针不能调用”，而是**在 C 函数调用过程中手动改变当前调用栈**：编译器可能在修改 MSP 后仍通过原有栈帧取回变量/准备调用/恢复寄存器。`__enable_irq()` 还可能令中断在 APP 初始化之前进入。即便示例一次成功，也不构成跨优化等级的安全保证。

更稳妥的末级汇编（GCC / ArmClang 风格示意）：

```c
__attribute__((naked, noreturn))
static void branch_to_reset(uint32_t msp, uint32_t entry)
{
    __asm volatile (
        "msr msp, r0\n"  // 第一个参数 msp 在 R0
        "bx r1\n"        // 第二个参数 entry 在 R1
    );
}
```

前提：调用点为**特权 Thread Mode + 使用 MSP**、已清理旧执行环境，且编译器 ABI 与实现一致。`branch_to_reset` 是本函数的**最后一次 C 调用**，内部纯汇编设置 MSP 后不再返回。因为中断保持屏蔽，目标 APP 必须在合适的初始化阶段主动 `__enable_irq()`。

## 8. `BL`/`BLX`/`BX`/`POP {PC}`/异常返回的区别

| 指令/机制 | 主要作用 | 是否正常写 LR | 常见使用 |
|---|---|---|---|
| `BL label` | 直接带链接分支 | 是 | 调普通已知地址函数 |
| `BLX Rn` | 寄存器间接带链接分支 | 是 | 调函数指针 |
| `BX Rn` | 寄存器间接分支 | 否 | 无返回跳转、尾调用、函数返回 |
| `POP {PC}` | 从栈取值到 PC | 否（返回地址来自栈） | 函数尾声，需有效当前栈 |
| `BX LR` 且 LR 为 EXC_RETURN | 触发**异常返回语义**，不是普通函数返回 | 不适用 | FreeRTOS PendSV/SVC 返回任务 |

**面试/复习要点：**`app_entry()` 与 FreeRTOS 的 `BX LR` 都能改变 PC，但前者通常是**函数控制流**；后者当 `LR=EXC_RETURN` 时会触发**硬件自动出栈恢复异常现场**，机制完全不同。

## 9. 自检题

1. `typedef void (*F)(void)` 和 `void (*f)(void)` 哪个产生变量？
2. `*(uint32_t*)0x08020004` 的值，与整数 `0x08020004` 有什么不同？
3. 解释 `0x08020439` 的最低位 1。
4. `BLX` 为什么通常不适合 Bootloader 最后一条切换指令？
5. 为什么不推荐在修改 MSP 后再正常调用 `app_entry()`？

关联：[13 跳转前置条件](13_跳转前的严格条件与安全顺序.md)、[15 异常返回](15_异常进入_异常返回_EXC_RETURN.md)。
