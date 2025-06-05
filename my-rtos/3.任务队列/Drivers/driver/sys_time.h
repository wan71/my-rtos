#ifndef SYS_TIME_H
#define SYS_TIME_H



void Sys_Tick_Count(void);
unsigned short int Get_Sys_Tick();
unsigned short int Is_Timeout(unsigned short int start, unsigned short int timeout);
#endif
