/*
 * UART 收到“进入另一镜像”的命令时使用原模块的示例。
 * 此文件是需要嵌入用户工程的片段，不提供 main / UART 句柄。
 * 不建议同时复制已有 HAL_UART_RxCpltCallback 实现，避免重复定义。
 */
#include "MY_OTA_JUMP.h"
#include <stdint.h>

void Example_OnUartCommand(uint8_t cmd)
{
    if (cmd == (uint8_t)'J') {
        /* 若目标镜像前两个向量项非法，此函数返回；
         * 否则写 SRAM4 并请求软件复位，一般不会返回。 */
        MY_OTA_JUMP_Request();
    }
}

/*
 * 集成位置示意：
 *
 * void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
 * {
 *     if (huart->Instance == USART1) {
 *         Example_OnUartCommand(rx_byte);
 *         HAL_UART_Receive_IT(huart, &rx_byte, 1);
 *     }
 * }
 *
 * 注意：真正传输 OTA 固件时，不应在 UART IRQ 内做长时间
 * Flash 扇区擦除/编程；推荐 ISR -> 消息队列 -> 升级任务。
 */
