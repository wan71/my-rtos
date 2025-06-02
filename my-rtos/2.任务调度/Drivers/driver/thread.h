#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <string.h>
#define MAX_TASKS 4   			// 最大任务个数
#define MAX_PRIORITY 3        // 最大优先级数
#define TASK_STACK_SIZE 128

typedef uint32_t stack_task;
typedef void * TaskHandle_t;


typedef struct TaskControlBlock {
    uint32_t *stackPointer;          // 任务的堆栈指针
    uint32_t *stack;                 // 指向任务的栈空间
    int stackSize;                   // 栈大小
    int priority;                    // 任务优先级
	  uint32_t delay;                  // 延时时间
    int yield;                       // 是否设置礼让（1: 礼让, 0: 不礼让）
    struct TaskControlBlock *next;   // 指向下一个任务的指针
} TaskControlBlock;

//任务初始化函数.
void schedule(void);
void os_schedule_start(void);
int createTask(void (*taskFunc)(void *), void *param, int priority, int yield, int stackSize,TaskHandle_t * pxCreatedTask);
int AddTaskptob(TaskHandle_t p,TaskHandle_t b,int a) ;
int removeTaskFromList(TaskHandle_t a,TaskHandle_t p);
int set_task_delay(TaskHandle_t p,uint32_t delay);
void Tsakdelay(uint32_t delay);
#endif

