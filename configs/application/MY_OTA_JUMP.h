#ifndef MY_OTA_JUMP_H
#define MY_OTA_JUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 烧录到 0x08020000 的工程：希望请求返回 0x08000000 Bootloader。 */
#define MY_OTA_APP_ADDRESS    0x08000000UL
#define MY_OTA_APP_FLASH_END  0x08020000UL

void MY_OTA_JUMP_JumpNow(void);
void MY_OTA_JUMP_Request(void);
uint8_t MY_OTA_JUMP_IsApplicationValid(void);

#ifdef __cplusplus
}
#endif
#endif
