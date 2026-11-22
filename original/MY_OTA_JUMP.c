#include "MY_OTA_JUMP.h"
#include "stm32h723xx.h"
#include "stm32h7xx_hal.h"
#include "stdio.h"

/*
 * 使用 SRAM4 最后 12 字节保存一次性跳转请求。
 * SYSRESETREQ 软件复位不会清除 SRAM，启动代码也不会使用这些固定地址。
 * 除目标地址外，同时保存目标地址反码，避免 SRAM 随机值导致误跳转。
 */
#define JUMP_MAGIC_ADDRESS       0x38003FF4UL
#define JUMP_TARGET_ADDRESS      0x38003FF8UL
#define JUMP_TARGET_INV_ADDRESS  0x38003FFCUL
#define JUMP_MAGIC_VALUE         0x4A554D50UL /* ASCII: "JUMP" */

#define JUMP_MAGIC       (*(volatile uint32_t *)JUMP_MAGIC_ADDRESS)
#define JUMP_TARGET      (*(volatile uint32_t *)JUMP_TARGET_ADDRESS)
#define JUMP_TARGET_INV  (*(volatile uint32_t *)JUMP_TARGET_INV_ADDRESS)

/* 启动文件导出的向量表符号，其地址就是当前程序的链接起始地址。 */
extern const uint32_t g_pfnVectors[];

/* Application 的 Reset_Handler 函数指针类型。 */
typedef void (*MY_OTA_JUMP_EntryFunction)(void);

static uint8_t MY_OTA_JUMP_IsStackValid(uint32_t address)
{
    /* DTCM RAM */
    if ((address >= 0x20000000UL) && (address <= 0x20020000UL)) {
        return 1U;
    }

    /* AXI SRAM */
    if ((address >= 0x24000000UL) && (address <= 0x24050000UL)) {
        return 1U;
    }

    /* SRAM1/SRAM2 */
    if ((address >= 0x30000000UL) && (address <= 0x30008000UL)) {
        return 1U;
    }

    /* SRAM4；末尾 12 字节保留给跳转信息，不作为栈顶。 */
    if ((address >= 0x38000000UL) && (address <= JUMP_MAGIC_ADDRESS)) {
        return 1U;
    }

    return 0U;
}

uint8_t MY_OTA_JUMP_IsApplicationValid(void)
{
    uint32_t app_stack = *(volatile uint32_t *)MY_OTA_APP_ADDRESS;
    uint32_t app_reset = *(volatile uint32_t *)(MY_OTA_APP_ADDRESS + 4UL);
    uint32_t reset_address = app_reset & ~1UL;

    if (MY_OTA_JUMP_IsStackValid(app_stack) == 0U) {
        return 0U;
    }

    if ((reset_address < MY_OTA_APP_ADDRESS) ||
        (reset_address >= MY_OTA_APP_FLASH_END)) {
        return 0U;
    }

    /* Cortex-M7 只执行 Thumb 指令，入口地址 bit0 必须为 1。 */
    if ((app_reset & 1UL) == 0U) {
        return 0U;
    }

    return 1U;
}

void MY_OTA_JUMP_JumpNow(void)
{
    uint32_t app_stack;
    uint32_t app_reset;
    uint32_t index;
    MY_OTA_JUMP_EntryFunction app_entry;

    /* 跳转前先检查 APP 的栈顶地址和复位入口地址。 */
    if (MY_OTA_JUMP_IsApplicationValid() == 0U) {
        return;
    }

    /*
     * APP 向量表的第 1 个字是初始 MSP；
     * APP 向量表的第 2 个字是 Reset_Handler 地址。
     */
    app_stack = *(volatile uint32_t *)MY_OTA_APP_ADDRESS;
    app_reset = *(volatile uint32_t *)(MY_OTA_APP_ADDRESS + 4UL);
    app_entry = (MY_OTA_JUMP_EntryFunction)app_reset;

    /* 清理过程中不允许任何中断插入。 */
    __disable_irq();

    /* 停止 Bootloader 的系统节拍，避免跳转后产生 SysTick 中断。 */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* 关闭并清除所有已经挂起的外部中断。 */
    for (index = 0U; index < 8U; index++) {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /* 将中断向量表切换到 APP 的向量表。 */
    SCB->VTOR = MY_OTA_APP_ADDRESS;

    /* APP 从特权级 Thread 模式启动，并使用 MSP。 */
    __set_CONTROL(0U);
    __DSB();
    __ISB();

    /*
     * 装载 APP 的初始主栈指针。app_entry 已经提前保存在函数指针中，
     * 设置 MSP 后不要再声明局部变量，也不要再调用其他普通函数。
     */
    __set_MSP(app_stack);
    __DSB();
    __ISB();

    /* 恢复全局中断状态，随后进入 APP 的 Reset_Handler。 */
    __enable_irq();
    app_entry();

    /* 正常情况下 Reset_Handler 永远不会返回。 */
    while (1) {
        
    }
}

/*
 * Reset_Handler 在调用 main() 前会执行 .init_array 中的构造函数。
 * 此时已退出 UART IRQ，CPU 位于 Thread 模式，可以安全切换 MSP。
 */
__attribute__((constructor))
static void MY_OTA_JUMP_CheckRequestAfterReset(void)
{
    uint32_t requested_target;
    uint32_t current_address = (uint32_t)g_pfnVectors;

    /* 检查魔数以及目标地址和反码是否匹配。 */
    if ((JUMP_MAGIC != JUMP_MAGIC_VALUE) ||
        (JUMP_TARGET_INV != ~JUMP_TARGET)) {
        return;
    }

    requested_target = JUMP_TARGET;

    /* 读取后立即清除，使这次请求只执行一次。 */
    JUMP_MAGIC = 0U;
    JUMP_TARGET = 0U;
    JUMP_TARGET_INV = 0U;
    __DSB();
    __ISB();

    if (requested_target == current_address) {
        /* 软件复位后已经位于请求地址，继续进入当前程序的 main()。 */
        return;
    }

    if (requested_target == MY_OTA_APP_ADDRESS) {
        /* 请求的是本工程配置的另一个程序，执行安全跳转。 */
        MY_OTA_JUMP_JumpNow();
    }
}

void MY_OTA_JUMP_Request(void)
{
    /* 先检查，避免 APP 无效时陷入无意义的软件复位。 */
    if (MY_OTA_JUMP_IsApplicationValid() == 0U) {
        return;
    }

    /* 保存明确的目标地址，而不是只保存一个无方向的 JUMP 标志。 */
    JUMP_MAGIC = JUMP_MAGIC_VALUE;
    JUMP_TARGET = MY_OTA_APP_ADDRESS;
    JUMP_TARGET_INV = ~MY_OTA_APP_ADDRESS;
    __DSB();
    __ISB();

    NVIC_SystemReset();

    while (1) {
        /* NVIC_SystemReset 正常不会返回。 */
    }
}
