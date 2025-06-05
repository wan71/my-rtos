#include "sys_time.h"

int sys_tick = 0;



/**
 * @brief 系统时基
 * 
 */
void Sys_Tick_Count(void)
{
    sys_tick += 1;
	  
}



/**
 * @description: 获取系统滴答计时
 * @param {*}
 * @return {*}
 */
unsigned short int Get_Sys_Tick()
{
    return sys_tick;
}

/**
 * @description: 判断是否超时
 * @param {unsigned long int} start 计算开始的时间
 * @param {unsigned long int} timeout   超时时长
 * @return {*}
 */
unsigned short int Is_Timeout(unsigned short int start, unsigned short int timeout)
{
    return ((unsigned short int)(Get_Sys_Tick() - start)) > timeout ?  1: 0;
}


