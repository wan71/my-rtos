/*  Copyright (s) 2019 深圳百问网科技有限公司
 *  All rights reserved
 * 
 * 文件名称：driver_uart.c
 * 摘要：
 *  
 * 修改历史     版本号        Author       修改内容
 *--------------------------------------------------
 * 2023.8.05      v01         百问科技      创建文件
 *--------------------------------------------------
*/

#include "driver_uart.h"
#include "usart.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

#define DEBUG_UART_TIMEOUT 500

extern UART_HandleTypeDef huart1;




/**********************************************************************
 * 函数名称： UART_Init
 * 功能描述： UART初始化函数
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2023/08/05	     V1.0	  韦东山	      创建
 ***********************************************************************/

int fputc(int c, FILE *f) 
{
    (void)f;
	
    HAL_UART_Transmit(&huart1, (const uint8_t *)&c, 1, DEBUG_UART_TIMEOUT);
    return c;
}

int fgetc(FILE *f)
{
 uint8_t ch = 0;
  HAL_UART_Receive(&huart1, &ch, 1, 0xffff);
  return ch;
}
