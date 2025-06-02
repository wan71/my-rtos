#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

typedef uint32_t stack_task;

typedef struct _losTask{
	stack_task *stack;
}losTask;

extern losTask *currentTask;
extern losTask *nextTask;
extern losTask *taskTable[2];
//任务初始化函数.
void los_task_create();
void los_task_init(losTask * task,void (*taskEntry)(void *),void *param,stack_task *stack);
void los_task_run();
void los_task_switch();
void los_task_sched();
#endif

