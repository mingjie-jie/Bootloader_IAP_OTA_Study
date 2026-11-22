/*
 * STM32H723VE Cortex-M7: 学习用“最终汇编分支”示意。
 * 该文件不是自动替换原模块的完整 IAP/OTA 工程。
 *
 * 使用前：
 * 1. 添加到现有 STM32CubeIDE / GCC 或 ArmClang 工程；
 * 2. 调用仅限特权 Thread + MSP 环境（如 Bootloader Reset 后 main 早期）；
 * 3. 校对地址和编译器生成的汇编，完善外设/Cache/MPU/看门狗处理；
 * 4. 从 APP 内部、FreeRTOS PSP 任务、UART ISR 不要直接调用它，
 *    而应使用“保存请求 + NVIC_SystemReset()”机制。
 */
#include "stm32h7xx_hal.h"
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
__attribute__((naked, noreturn))
static void Study_BranchToReset(uint32_t initial_msp, uint32_t reset_handler)
{
    __asm volatile(
        "msr msp, r0  \n"  /* ABI: r0 = new MSP */
        "bx  r1       \n"  /* ABI: r1 = Reset_Handler|1 */
    );
}
#else
#error "此示例只适用于 GCC/Clang 风格编译器。请为你的编译器重写纯汇编末级分支。"
#endif

static uint8_t Study_StackPlausible(uint32_t sp)
{
    /* Cortex-M 栈初始化通常要求 8 字节对齐；起点不应位于 SRAM 下边界。 */
    if ((sp & 7UL) != 0UL) return 0U;
    if ((sp > 0x20000000UL) && (sp <= 0x20020000UL)) return 1U;
    /* AXI SRAM 实际容量与工程内存配置须另外核对 */
    if ((sp > 0x24000000UL) && (sp <= 0x24050000UL)) return 1U;
    if ((sp > 0x30000000UL) && (sp <= 0x30008000UL)) return 1U;
    if ((sp > 0x38000000UL) && (sp <= 0x38003FF4UL)) return 1U;
    return 0U;
}

/*
 * 只用于“当前确实运行在 Thread+privileged+MSP”的场景。
 * 这个例子没有实现所有外设撤销，不能直接认作量产级。
 */
void Study_JumpToImage(uint32_t image_base, uint32_t image_end)
{
    uint32_t sp;
    uint32_t pc;
    uint32_t i;

    if ((image_base & 0x7FUL) != 0U || image_base >= image_end)
        return;

    /* IPSR=0 表示 Thread；CONTROL bit0/bit1=0 为特权/MSP。 */
    if ((__get_IPSR() != 0U) || ((__get_CONTROL() & 3UL) != 0U))
        return;

    sp = *(volatile const uint32_t *)(image_base + 0UL);
    pc = *(volatile const uint32_t *)(image_base + 4UL);

    if (!Study_StackPlausible(sp)) return;
    if ((pc & 1UL) == 0U) return;
    if (((pc & ~1UL) < image_base) || ((pc & ~1UL) >= image_end))
        return;

    /* 设备层：这里还应先停止 Bootloader 使用的 DMA/UART/TIM 等外设。 */
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

    for (i = 0U; i < 8U; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = image_base;
    __set_BASEPRI(0U);
    __DSB();
    __ISB();

    /* 这是最后一个 C 函数调用。之后“设置 MSP + BX”纯汇编，不再返回旧栈。 */
    /* 注意：PRIMASK 仍屏蔽中断，APP 初始化必须在合适时机 __enable_irq()。 */
    Study_BranchToReset(sp, pc);
}
