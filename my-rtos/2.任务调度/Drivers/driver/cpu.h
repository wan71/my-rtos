#ifndef __CPU_H
#define __CPU_H
void PendSV_init(void);
void triggerPendSV();
void enterCritical(void);
void exitCritical(void);
#endif

