/*
 * Bootloader main 默认启动策略示意（伪代码，不是可编译项目）。
 * 重要：上传的原代码没有默认自动跳 APP！
 *
 * 警惕：如果 APP 请求“进入 Bootloader”后，Bootloader main 又
 * 立即无条件 JumpNow()，则会直接回 APP，导致无法停留在升级模式。
 * 必须设计独立的 enter_update_mode 条件，例如按键/串口握手/持久化标志。
 */

#if 0
int main(void)
{
    HAL_Init();
    Board_Init();

    if (UpgradeModePinPressed() || PersistentUpdateModeRequested()) {
        EnterUpdateMode();   /* 等待 UART / CAN / USB 命令 */
    }
    else if (MY_OTA_JUMP_IsApplicationValid()) {
        /* 只在 Thread+MSP、外设已被妥善清理时跳 APP。 */
        MY_OTA_JUMP_JumpNow();
    }

    while (1) {
        RunRecoveryProtocol();
    }
}
#endif
