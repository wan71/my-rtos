#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include <string.h>
#define MAX_TASKS 4   			// ����������
#define MAX_PRIORITY 3        // ������ȼ���
#define TASK_STACK_SIZE 128

typedef uint32_t stack_task;
typedef void * TaskHandle_t;


typedef struct TaskControlBlock {
    uint32_t *stackPointer;          // ����Ķ�ջָ��
    uint32_t *stack;                 // ָ�������ջ�ռ�
    int stackSize;                   // ջ��С
    int priority;                    // �������ȼ�
	  uint32_t delay;                  // ��ʱʱ��
    int yield;                       // �Ƿ��������ã�1: ����, 0: �����ã�
    struct TaskControlBlock *next;   // ָ����һ�������ָ��
} TaskControlBlock;

//�����ʼ������.
void schedule(void);
void os_schedule_start(void);
int createTask(void (*taskFunc)(void *), void *param, int priority, int yield, int stackSize,TaskHandle_t * pxCreatedTask);
int AddTaskptob(TaskHandle_t p,TaskHandle_t b,int a) ;
int removeTaskFromList(TaskHandle_t a,TaskHandle_t p);
int set_task_delay(TaskHandle_t p,uint32_t delay);
void Tsakdelay(uint32_t delay);
#endif

