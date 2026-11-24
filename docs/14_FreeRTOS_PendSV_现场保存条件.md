# 14｜FreeRTOS PendSV 任务现场保存：前置条件、每条指令、栈变化

> 本章以 **STM32H723 Cortex-M7 + 使用 PSP 的 FreeRTOS 移植惯例**为背景，解释“任务现场保存之前需要哪些条件”。汇编为**教学简化示例**，不保证与你正在使用的 FreeRTOS 版本或 FPU 选项逐字一致。

## 1. 先排除一个常见误会

**PendSV 不是直接把整个 CPU 状态一次压到一个栈上。**异常进入时 Cortex-M 硬件先保存基本异常帧；PendSV 处理函数再用软件指令保存其余非易失寄存器，并把任务 PSP 写入 TCB。之后通过同一个机制恢复另一个任务。

```text
Task A 执行（Thread + PSP）
    │ SysTick 或某操作使 PendSV 挂起
    ▼
CPU 选择何时真正响应 PendSV
    │ [硬件异常进入] 在 A 的 PSP 栈压基本异常帧
    │                    切到 Handler + MSP
    ▼
xPortPendSVHandler
    │ 软件用 A 的 PSP 压 R4-R11、EXC_RETURN（及需要的 FP 状态）
    │ 当前 PSP 写入 TCB_A->pxTopOfStack
    │ 调用 vTaskSwitchContext 选择任务 B
    │ 从 TCB_B 取 PSP，软件恢复 R4-R11 等
    ▼
BX LR = EXC_RETURN
    │ [硬件异常返回] 从 B 的 PSP 栈自动恢复 R0-R3,R12,LR,PC,xPSR
    ▼
Task B 从之前的 PC 继续执行
```

## 2. 到底需要哪些条件？

| 条件 | 重要原因 | 不满足的表现 |
|---|---|---|
| 正常调度阶段已建立当前 TCB | `pxCurrentTCB` 必须可解引用 | 读到非法栈顶、HardFault |
| 当前任务栈是有效的 PSP | 基本帧和软件保存的帧都写在任务栈上 | 压栈出错、栈溢出 |
| 异常能在正确优先级执行 | PendSV 常配置最低优先级，等高优先级 ISR 结束 | 切换不及时、优先级问题 |
| 不处于无限期中断屏蔽中 | `PRIMASK`/`BASEPRI` 可延后 PendSV 响应 | PendSV 一直 Pending |
| 运行的内核端口与 FPU 配置一致 | M7 可能有扩展异常栈帧和浮点寄存器 | FP 计算异常、现场损坏 |
| `TCB` 中栈指针布局与汇编一致 | `TCB` 栈顶成员通常处在移植代码预期偏移 | 恢复到错误栈地址 |
| ISR 使用 RTOS API 满足优先级约束 | FreeRTOS 管理的临界区只保护相应优先级范围 | 隐蔽竞态、断言失败 |

**注意例外：**“调度器已运行”是**正常运行中切换任务**的条件；**首次启动任务**则通过专门启动路径（通常 SVC）恢复预先构造的初始栈帧，不能强行套用“已有运行任务的保存过程”。

## 3. CPU 自动保存哪些寄存器？

典型基本栈帧（**地址从低到高排列**）：

```text
新 PSP（低地址） ──┬─ R0
                  ├─ R1
                  ├─ R2
                  ├─ R3
                  ├─ R12
                  ├─ 旧 LR（任务函数原 LR）
                  ├─ PC（从哪条指令恢复）
                  └─ xPSR
原 PSP（高地址） ──┘
```

8 × 4 = **32 字节**，如果要求 8 字节栈对齐可能存在填充；若使用硬件 FPU，可能出现扩展帧和 lazy stacking，**不能永远假设只有 32 字节**。

为什么没有 R4-R11？ARM ABI 将它们列为**被调用者保存**寄存器，异常硬件保存最小通用集合；操作系统需要软件保存任务独立的 R4-R11。

为什么没有 R2 地址？**被保存的是 R2 当前的 32 位值**；如果 R2 恰好存放一个指针，保存这个值就等于保存该指针数值，不会递归把指针指向的所有 RAM 数据一起复制。指向的数据原本就在 RAM 中。

## 4. PendSV 到达真正处理入口时，寄存器发生了什么？

- `IPSR != 0`，CPU 在 Handler Mode；
- Handler 使用 **MSP** 作为当前栈；
- 原任务的基本栈帧已在**PSP** 上；
- `LR` 通常装有 `EXC_RETURN` 标记（例如 `0xFFFFFFFD`，取决于 FP 上下文）；
- `R0-R3` 等可能被异常处理使用，所以已由硬件保存在原任务栈里。

这是为什么处理函数会读 PSP，而不是读 MSP：

```asm
mrs r0, psp          ; R0 = 刚保存基本帧后的任务 PSP
stmdb r0!, {r4-r11}  ; 再向低地址软件压 R4-R11
```

**真正的“保存目标”是 PSP 指向的任务栈内存，不是 R0 寄存器。** R0 只是临时保存并移动任务栈指针。

## 5. 包含 EXC_RETURN 的教学版伪汇编

```asm
xPortPendSVHandler:            ; 已处于 Handler Mode，硬件 basic frame 已入 PSP
    mrs   r0, psp             ; 取得 Task A 的栈顶
    isb
    stmdb r0!, {r4-r11, r14}  ; 存 R4..R11 + EXC_RETURN（教学化端口示例）
    ldr   r3, =pxCurrentTCB   ; R3 = 全局变量 pxCurrentTCB 的地址
    ldr   r2, [r3]            ; R2 = TCB_A 的地址
    str   r0, [r2]            ; TCB_A->pxTopOfStack = 新 PSP

    ; 保护临界区，再选择下一个任务；真实代码涉及 BASEPRI/ABI 对齐
    bl    vTaskSwitchContext

    ldr   r3, =pxCurrentTCB
    ldr   r2, [r3]            ; R2 = TCB_B 的地址
    ldr   r0, [r2]            ; R0 = B 先前保存的 PSP
    ldmia r0!, {r4-r11, r14}  ; 还原 B 的软件保存寄存器/EXC_RETURN
    msr   psp, r0             ; PSP 指向 B 的硬件基本异常帧
    isb
    bx    r14                 ; EXC_RETURN：CPU 从 B 的 PSP 自动弹基本帧
```

⚠️ **伪汇编不可复制成生产级 PendSV_Handler**：省略 FPU、保存 BASEPRI、寄存器保护、栈 ABI 对齐、端口宏和错误路径。真实源码请看你项目 FreeRTOS 端口文件 `portable/.../ARM_CM7.../port.c`（实际位置可能不同）。

## 6. 用真实数值理解 PSP 如何移动

假设异常进入前 `PSP=0x20010000`，忽略额外对齐与 FPU：

| 操作 | 新 PSP | PSP 指向内容 |
|---|---|---|
| Task A 正常执行 | `0x20010000` | 栈顶 |
| 硬件压 8 个寄存器 | `0x2000FFE0` | 从低地址起 R0 ... xPSR |
| 软件压 9 个寄存器 | `0x2000FFBC` | 从低地址起 R4...R11、EXC_RETURN（示意） |
| 保存到 TCB | 数值仍是 `0x2000FFBC` | `TCB_A->pxTopOfStack` 接管恢复依据 |

9×4=36 字节。为什么不一定刚好 36？因为具体端口可能按需存浮点寄存器，还可能使用不同的栈对齐和布局。

## 7. `pxCurrentTCB` 为什么要解引用两次？

```asm
ldr r3, =pxCurrentTCB   ; r3 = 全局指针变量自己的地址
ldr r2, [r3]            ; r2 = 全局指针变量的值，也就是 TCB 地址
str r0, [r2]            ; 把当前 PSP 存到 TCB 起始成员
```

```text
全局变量 pxCurrentTCB（例如位于 RAM 地址 0x20000080）
    内存内容 = 0x20002000（TCB_A 的地址）
TCB_A 首字段 pxTopOfStack
    内存内容 = 0x2000FFBC（A 保存的 PSP）
```

`LDR R3, =变量名` 与 `LDR R2, [R3]` 不是重复操作；前者形成地址，后者从地址取数据。

## 8. 中断优先级：PendSV Pending 不等于马上执行

例子：

```text
Task A (Thread)
  → SysTick (ISR) 请求任务调度
  → PendSV 被置 Pending
  → 另一个高优先级 UART ISR 仍活跃
  → UART 完成并返回
  → 若无更高优先级活跃且屏蔽条件允许，CPU 响应 PendSV
  → Task A 现场保存、Task B 恢复
```

`BASEPRI` 设置阈值时可阻止某些优先级的异常响应；`PRIMASK` 会屏蔽可配置异常；`NMI` 和部分 Fault 属于特殊情况。PendSV 一般最低优先级，但**仅最低优先级不是保存现场所需的唯一条件**。

## 9. 常见易错说法逐条纠正

- ❌ “OSDelay 一调用马上执行 PendSV”：它会操作任务阻塞状态并请求调度；实际切换依赖调度器机制和异常响应时机。
- ❌ “PendSV 进来再把 PC 保存一次”：**硬件**进入异常时已经把任务 PC 放入基本帧。
- ❌ “要保存所有 RAM”：寄存器内容/栈上下文需要保存，任务使用的堆/全局 RAM 数据原本仍在相应内存中。
- ❌ “TCB 保存所有寄存器”：TCB 常只保存一个栈顶指针；寄存器内容主要在该任务栈。
- ❌ “R14 始终是普通 LR”：异常处理期间 R14 可能是 EXC_RETURN。
- ❌ “`BX LR` 就是普通函数返回”：LR 为 EXC_RETURN 时，触发硬件异常返回与自动出栈。

## 10. 复习题（答案位于文末）

1. PendSV 执行前 PC 被谁保存？
2. 为什么软件只保存 R4-R11（以及其他必要状态）？
3. R2 保存一个地址，为何不用复制这个地址指向的内存？
4. `r0 = PSP` 后 `STMDB r0!` 为什么能改变任务栈保存位置？
5. `pxCurrentTCB` 与 `TCB->pxTopOfStack` 是同一个东西吗？
6. PendSV 一旦 pending，为什么有时不立刻执行？

**答案**：1 硬件异常入栈；2 硬件已有基本帧，软件补未保存状态；3 指针数值保存即可，RAM 数据并未因换任务消失；4 写回更新 R0 所保存的任务栈顶数值；5 前者指向当前 TCB，后者是 TCB 内保存的 PSP；6 异常优先级与屏蔽状态决定响应时机。

下一章：[15 异常进入和 EXC_RETURN](15_异常进入_异常返回_EXC_RETURN.md)。
