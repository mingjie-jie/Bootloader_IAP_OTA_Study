# 15｜Cortex-M7 异常进入与异常返回：SVC / PendSV / EXC_RETURN 到底是什么？

> 这是把 **STM32 启动 → FreeRTOS 首任务 → 普通任务切换 → Bootloader 换固件** 串成一套知识体系的关键章节。官方依据：Arm《Cortex-M7 Devices Generic User Guide》Exception model、ST/CMSIS 启动代码。

## 1. 两种“返回”：普通函数返回 ≠ 异常返回

普通函数（示意）：

```asm
bl  Function       ; LR 获取普通代码返回地址
...
Function:
    ...
    bx lr          ; PC 跳回普通代码地址
```

异常（示意）：

```text
Thread 执行 Task A
  ↓ 硬件响应 PendSV
硬件压基本栈帧 + 写 LR = EXC_RETURN
  ↓
PendSV_Handler 执行...
  ↓ BX LR，且 LR = 0xFFFFFFFD（示例）
CPU **识别特殊编码**，自动从 PSP 出栈恢复 PC/xPSR 等，返回 Thread
```

相同的 `BX LR` 指令，含义取决于 LR 的值：普通代码地址 → 普通分支；特定 EXC_RETURN 编码 → 硬件异常返回流程。

## 2. EXC_RETURN 不是一段 Flash 函数地址

异常入口时，CPU 将特定**标记值**放在 Handler 的 LR 中：

| 值（无安全扩展的常见 M7 情况） | 返回模式 | 使用哪个栈恢复 | 帧类型 |
|---|---|---|---|
| `0xFFFFFFF1` | Handler | MSP | 基本帧 |
| `0xFFFFFFF9` | Thread | MSP | 基本帧 |
| `0xFFFFFFFD` | Thread | PSP | 基本帧 |
| `0xFFFFFFE1` | Handler | MSP | 浮点扩展帧 |
| `0xFFFFFFE9` | Thread | MSP | 浮点扩展帧 |
| `0xFFFFFFED` | Thread | PSP | 浮点扩展帧 |

最常见的：`0xFFFFFFFD` 代表“恢复 **PSP** 上的基本帧，返回 **Thread Mode**”。这也是为何 FreeRTOS 常把它随 R4~R11 一起保存，以便恢复任务时让硬件知道从何处出栈。

**注意：**硬件对 EXC_RETURN 有合法性检查；不是可以随便在任意 C 代码里把 LR 设置成这些数然后 `BX LR`。必须拥有正确异常上下文和栈帧，否则会产生异常返回错误。

## 3. 进入异常的完整时间线

假设 Task A 以 Thread + PSP 运行：

```text
t0: Task A：PSP=0x20010000，PC=0x08012340
    | CPU 执行 Task A
    | PendSV 变为 pending
    |
t1: CPU 判断异常优先级/屏蔽允许响应
    | 硬件将 R0 R1 R2 R3 R12 旧LR PC xPSR 压入 PSP
    | （有时存在对齐填充、浮点扩展/惰性压栈）
    | IPSR 变为 PendSV 异常号
    | Handler 模式开始使用 MSP
    | LR 获取 EXC_RETURN（例如 FFFFFF FD）
    |
t2: PC 取 VTOR + 4*(16+PendSV_IRQn) 的向量值（系统异常有固定编号）
    | 注意 PendSV 是系统异常，不属于外部 IRQn 编号体系
    |
t3: 在 PendSV_Handler 中运行软件保存/切换代码
    |
t4: BX LR = EXC_RETURN
    | 检查合法性，从目标栈弹出基本帧
    | 恢复 PC、xPSR、LR、通用寄存器
    ▼
t5: Task B 从保存的 PC 位置继续执行（Thread + PSP）
```

**修正精度：**异常向量读取和其他异常进入微步骤的硬件内部先后顺序由架构规定，不要把上面的步骤当作精确逐周期流水线。PendSV 的异常编号是 14，向量偏移 `14*4 = 0x38`，SysTick 编号 15，对应 `0x3C`；SVC 编号 11，对应 `0x2C`。

## 4. PendSV、SysTick、SVC 三者分工

| 异常 | 典型职责 | 为什么 |
|---|---|---|
| SysTick | 提供 RTOS tick 计时，更新时间/阻塞唤醒 | 周期事件源 |
| PendSV | 在安全优先级时切换任务上下文 | 可延迟执行，适合软件调度 |
| SVC | 通过异常进入特权处理路径，启动首任务/系统调用（依端口） | 使用异常返回建立第一个任务上下文 |

### 首任务启动的一个常见链路

```text
main()
    ↓ 创建 Task1、Task2
vTaskStartScheduler()
    ↓ 初始化堆栈、系统节拍和 SVC/PendSV 优先级
触发 SVC
    ↓ SVC_Handler 运行在 Handler+MSP
从选中 TCB 读取第一个任务预先构造的栈帧
    ↓ 恢复 R4-R11、EXC_RETURN 等
MSR PSP,new_psp
    ↓ BX LR (EXC_RETURN)
CPU 通过 PSP 中的硬件帧恢复到 Task1 的入口（Thread+PSP）
```

首任务**并没有先真正运行过一次**，所以它的栈帧是在创建任务时由 `pxPortInitialiseStack` 等移植函数预先构建的。让 CPU **像从 PendSV 返回到一个之前被中断的任务一样**执行这个任务。

## 5. 逐条看经典 SVC 恢复示意

下面贴近你之前学习的写法：

```asm
ldr   r3, =pxCurrentTCB
ldr   r1, [r3]
ldr   r0, [r1]             ; 取目标任务 TCB 的 saved PSP
ldmia r0!, {r4-r11, r14}   ; 恢复软件压栈的 R4-R11、EXC_RETURN
msr   psp, r0             ; 设 PSP 指向硬件基本帧
isb
movs  r0, #0
msr   basepri, r0         ; 清优先级屏蔽（端口依赖）
bx    r14                 ; 触发异常返回，硬件继续恢复 PC 等
```

- `R14` 不是一个普通业务函数的返回地址，而是 EXC_RETURN。
- 还原 PC 的关键不在 `LDMIA` 中看到 PC，而是**后面的异常返回由硬件从 PSP 栈帧取 PC**。
- 这解释了你曾经问的“**它怎么知道 PC 是多少**”：PC 早就在任务初始化帧或者异常入口时压进该任务栈了。
- `LDMIA R0!` 的 `!` 是写回，读取后更新 R0；`MSR PSP,R0` 使 CPU 之后从正确位置弹硬件帧。

## 6. 为什么 PendSV 要低优先级？

如果 PendSV 抢占处理更紧急外设的中断，任务状态与 ISR 操作的同步可能更加复杂；FreeRTOS 通常将 PendSV 设为最低优先级，并在更高优先级中断处理完成后进行任务切换。**这不是硬件要求 PendSV 必须最低，而是端口设计的调度策略。**

## 7. MSP 在 RTOS 中还负责什么？

FreeRTOS 任务一般各有 PSP；Handler Mode 都走 MSP。意味着：

- MSP 需要足够的**中断嵌套栈空间**；
- **任务栈 PSP** 溢出与**异常栈 MSP** 溢出是两个不同来源；
- 不应把每个 FreeRTOS 任务的栈理解为“它运行期间包括所有 ISR 的全部栈空间”。

## 8. 例题：为什么不在 ISR 里直接修改 VTOR 然后 BX APP？

在 ISR 内 `IPSR != 0` 且有活动异常状态。普通 `BX target` 不会触发 **EXC_RETURN**，无法自动清除活动异常/恢复 Thread，目标代码可能继续在 Handler Mode 执行。即使你把 MSP 修改了，先前压栈的异常状态也没有被正确处理。这是你的工程采用**ISR 请求→SRAM4→SYSRESETREQ→启动路径检查**的核心动机。

## 9. 进阶：FPU 与浮点惰性压栈

Cortex-M7 带 FPU 的配置可能使用 FP 扩展帧；如果配置了 lazy stacking，部分 FP 保存可能延迟完成。FreeRTOS 的具体移植会判断 EXC_RETURN 相应位并视情保存 S16~S31 等寄存器。**绝不能把无 FPU 的 PendSV 教学例子不加修改地替换进带 FPU 的 H723 生产工程。**

## 10. 错误定位思路

- 进入 PendSV 前异常出错：PSP 有效性、任务栈越界、浮点上下文、NVIC 屏蔽。
- PendSV 内 HardFault：`pxCurrentTCB`、TCB 布局、`pxTopOfStack`、R0 地址有效性。
- `BX LR` 后异常：EXC_RETURN 值、恢复的 PSP 对齐、基本帧 PC/xPSR、浮点帧布局。
- 第一个任务始终起不来：SVC 向量/优先级、首任务初始栈构造、PRIMASK、BASEPRI。

**参考来源**：Arm Cortex-M7 Generic User Guide（https://support.arm.com/documentation/dui0646/a/The-Cortex-M7-Processor/Exception-model/Exception-entry-and-return）；ST 官方 STM32H723 GCC 启动文件（https://github.com/STMicroelectronics/cmsis-device-h7/blob/master/Source/Templates/gcc/startup_stm32h723xx.s）。
