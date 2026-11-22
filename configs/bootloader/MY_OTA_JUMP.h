#ifndef MY_OTA_JUMP_H
#define MY_OTA_JUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 烧录到 0x08000000 的工程：希望跳转到 0x08020000 的 APP。 */
#define MY_OTA_APP_ADDRESS    0x08020000UL
/* STM32H723VE：512 KiB Flash 最后地址（不包含）。 */
#define MY_OTA_APP_FLASH_END  0x08080000UL

void MY_OTA_JUMP_JumpNow(void);
void MY_OTA_JUMP_Request(void);
uint8_t MY_OTA_JUMP_IsApplicationValid(void);

#ifdef __cplusplus
}
#endif
#endif
