/*
 * 教学片段：在 Bootloader 的末级 BX 保持 PRIMASK=1 后，APP 早期需要
 * 在合适、可证明安全的时机重新允许可配置中断。
 *
 * 非完整程序！APP 的 SystemInit/VTOR、DMA、NVIC 与时钟启动顺序必须
 * 根据具体 CubeMX 项目确定，不能盲目把 __enable_irq 放到任意位置。
 */
#include "stm32h7xx_hal.h"

void Study_APP_UnmaskIRQsAtChosenPoint(void)
{
    /* 前提：当前是 APP Thread 特权状态；VTOR 已指向 APP 正确向量表；
     * Boot 已禁用了旧外设的中断源且清 pending；APP 自己的 IRQ
     * 处理程序和必要的时钟环境已经准备就绪。
     *
     * 注意如果 HAL_Init/SystemClock_Config 在此之前依赖 tick 或 timeout，
     * 需要重新安排这些初始化的顺序，使它们不会因 PRIMASK=1 卡住。
     */
    __set_BASEPRI(0U);
    __DSB();
    __ISB();
    __enable_irq();
}
