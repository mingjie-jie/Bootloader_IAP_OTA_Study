#ifndef MY_OTA_JUMP_H
#define MY_OTA_JUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * 本工程请求跳转的目标地址。
 * 0x08000000 工程中设置为 0x08020000；
 * 0x08020000 工程中设置为 0x08000000。
 */
#define MY_OTA_APP_ADDRESS    0x08000000UL

/* STM32H723VE 512 KiB Flash 的末地址（开区间）。 */
#define MY_OTA_APP_FLASH_END  0x08020000UL

void MY_OTA_JUMP_JumpNow(void);

/**
 * @brief 请求跳转到 Application。
 *
 * 可以直接在 UART 中断回调中调用。本函数保存目标地址后触发
 * 软件复位；复位后的启动阶段会自动跳转到 Application。
 * Application 无效时不会跳转，而会正常进入 Bootloader。
 *
 * 成功触发复位后本函数不会返回。
 */
void MY_OTA_JUMP_Request(void);


/**
 * @brief 检查 0x08020000 处的 Application 向量表是否合法。
 * @return 1 合法，0 非法。
 */
uint8_t MY_OTA_JUMP_IsApplicationValid(void);

#ifdef __cplusplus
}
#endif

#endif /* MY_OTA_JUMP_H */
