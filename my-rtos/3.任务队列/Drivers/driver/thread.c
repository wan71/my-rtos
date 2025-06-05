#include <thread.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "cpu.h"


//TaskControlBlock *currentTask;
//TaskControlBlock *nextTask;
TaskControlBlock *currentTask = NULL;  // 当前运行任务
TaskControlBlock *nextTask = NULL;     // 下一个任务

TaskControlBlock *taskLists[MAX_PRIORITY];  // 任务链表的头指针
TaskControlBlock *delayList = NULL;         // 延时任务链表
int taskCount = 0;  // 当前任务数量


void addTaskToList(TaskControlBlock *task) {
    int priority = task->priority;
    if (priority >= MAX_PRIORITY || priority < 0) {
        printf("Invalid priority!\n");
        return;
    }

    task->next = NULL;

    if (taskLists[priority] == NULL) {
        taskLists[priority] = task;  // 如果链表为空，直接加入
    } else {
        TaskControlBlock *current = taskLists[priority];
        while (current->next != NULL) {
            current = current->next;  // 链表末尾添加任务
        }
        current->next = task;
    }
}


// 从链表 list 中移除任务 task，如果 task 为 NULL，则移除 currentTask。
// 返回是否成功移除（1 为成功，0 为未找到，-1 为出错）。
int removeTaskFromList(TaskHandle_t list, TaskHandle_t task) {
    if (task == NULL) {
        if (currentTask == NULL) {
            printf("Error: currentTask is NULL!\r\n");
            return -1;
        }
        task = currentTask;
    }

    TaskControlBlock *target = (TaskControlBlock *)task;
    TaskControlBlock **head = (TaskControlBlock **)list;
    TaskControlBlock *curr = *head;
    TaskControlBlock *prev = NULL;

    while (curr != NULL) {
        if (curr == target) {
            // 找到目标任务，执行删除
            if (prev == NULL) {
                // 删除头节点
                *head = curr->next;
            } else {
                prev->next = curr->next;
            }

            target->next = NULL;
            return 1;
        }

        prev = curr;
        curr = curr->next;
    }

    // 没有找到目标任务
    return 0;
}



// 将任务 task 加入到链表 list 中（如果未重复），返回 1 表示添加成功，0 表示已存在
int TaskList_Add(TaskHandle_t taskHandle, TaskHandle_t listHandle) {
    if (taskHandle == NULL) {
        if (currentTask == NULL) return -1;
        taskHandle = currentTask;
    }

    TaskControlBlock *task = (TaskControlBlock *)taskHandle;
    TaskControlBlock **listHead = (TaskControlBlock **)listHandle;

    if (listHead == NULL) {
        return -1; // 链表本身为 NULL，错误
    }

    // 检查任务是否已在链表中
    TaskControlBlock *curr = *listHead;
    while (curr != NULL) {
        if (curr == task) return 0; // 已存在
        curr = curr->next;
    }

    // 添加到链表末尾
    if (*listHead == NULL) {
        *listHead = task;
    } else {
        curr = *listHead;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = task;
    }

    task->next = NULL; // 设置为尾节点
    return 1;
}




int set_task_delay(TaskHandle_t p,uint32_t delay)
{
	if(p==NULL)
		{
			p=(void *)currentTask;
		}
		TaskControlBlock *task=(TaskControlBlock *) p;
		if(task!=NULL)
		{
			task->delay=delay;
			return 1;
		}
		return 0;
}

/*
该任务用于从运行态进入delay态，默认是当前任务
*/
void Tsakdelay(uint32_t delay)
{
		enterCritical();
		set_task_delay(NULL,delay);
		removeTaskFromList(&taskLists[currentTask->priority],NULL);
		TaskList_Add(NULL,&delayList);
		exitCritical();
		restart_pendsv();
}

/*每次时钟中断时，遍历 delayList，减少延时时间，如果延时结束，则将任务移回对应优先级的 taskLists*/

void every_delay(void) {
	
		enterCritical();
    TaskControlBlock *prev = NULL;
    TaskControlBlock *current = delayList;
	 while (current != NULL) {
		  if (current->delay > 0) {
            current->delay--;  // 减少延时时间
        }
		 if (current->delay == 0) {
				  // 将任务加入到对应优先级的 taskLists
					
					removeTaskFromList(&delayList,(void *)current);
					TaskList_Add((void *)current,&taskLists[current->priority]);
			 }
//			 
			 // 继续遍历
        prev = current;
        current = current->next;
			 
	 }
	exitCritical();
}

int createTask(void (*taskFunc)(void *), void *param, int priority, int yield, int stackSize,TaskHandle_t * pxCreatedTask ) {
    if (taskCount < MAX_TASKS) {
        // 动态分配 TaskControlBlock
        TaskControlBlock *tcb = (TaskControlBlock *)malloc(sizeof(TaskControlBlock));
        if (tcb == NULL) {
            printf("Memory allocation for TaskControlBlock failed\r\n");
            return 0;  // 内存分配失败
        }

        // 动态分配栈空间
        tcb->stack = (uint32_t *)malloc(stackSize * sizeof(uint32_t));
        if (tcb->stack == NULL) {
            printf("Memory allocation for stack failed\r\n");
            free(tcb);  // 释放已分配的 TaskControlBlock
            return 0;  // 内存分配失败
        }
        // 指向任务栈的顶端
        uint32_t *stack = &(tcb->stack[stackSize]);

        // 初始化任务的堆栈 (xPSR, PC, LR, R12, R3-R0, R11-R4)
        *(--stack) = (uint32_t)0x01000000;  // xPSR: Thumb状态
        *(--stack) = (uint32_t)taskFunc;    // PC: 任务入口函数地址
        *(--stack) = (uint32_t)0xFFFFFFFD;  // LR: 返回到线程模式
        *(--stack) = (uint32_t)0x12121212;  // R12: 测试值
        *(--stack) = (uint32_t)0x03030303;  // R3
        *(--stack) = (uint32_t)0x02020202;  // R2
        *(--stack) = (uint32_t)0x01010101;  // R1
        *(--stack) = (uint32_t)param;       // R0: 任务参数

        // 初始化R11-R4的值
        *(--stack) = (uint32_t)0x11111111;  // R11
        *(--stack) = (uint32_t)0x10101010;  // R10
        *(--stack) = (uint32_t)0x09090909;  // R9
        *(--stack) = (uint32_t)0x08080808;  // R8
        *(--stack) = (uint32_t)0x07070707;  // R7
        *(--stack) = (uint32_t)0x06060606;  // R6
        *(--stack) = (uint32_t)0x05050505;  // R5
        *(--stack) = (uint32_t)0x04040404;  // R4

        // 更新TCB的堆栈指针和其他参数
        tcb->stackPointer = stack;
        tcb->stackSize = stackSize;  // 设置栈大小
        tcb->priority = priority;
        tcb->yield = yield;
				tcb->delay=0;
        tcb->next = NULL;
				taskCount++;

        // 将任务添加到任务链表
        addTaskToList(tcb);
				
					if(pxCreatedTask != NULL)
				{	
				*pxCreatedTask=(TaskHandle_t)tcb;
				}
				return 1;
    }
			return 0;  // 如果任务数已满，返回 NULL
}






//该函数将任务队列的头地址放入队列尾部
//返回下一个任务队列的头部的yield，如果为1代表了礼让。
int jump_wei(TaskControlBlock *firstTask,int i)
{
	    TaskControlBlock *lastTask = firstTask;
//      // 遍历到队列末尾

	 if (firstTask->next != NULL) {
      while (lastTask->next != NULL) {
      lastTask = lastTask->next;
      }
      // 将当前任务移到末尾，并处理链表指针
      lastTask->next = firstTask;
      taskLists[i] = firstTask->next;  // 队列头移到下一个任务
      firstTask->next = NULL;          // 当前任务的 next 置空	
			
    }
	return taskLists[i]->yield;
}


void schedule(void) {
    // 遍历所有优先级的任务列表，从高优先级到低优先级
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        if (taskLists[i] != NULL) {
           
            if (currentTask != NULL && currentTask->priority <= i) {    // 如果当前任务是同优先级任务
                TaskControlBlock *firstTask = taskLists[i]; // 取出队列首任务
							  
                if (firstTask->next != NULL && currentTask == firstTask) {  
									// 如果该任务队列只有一个任务 或者 这是一个刚进入就绪态的任务，直接进入默认
									// 如果都不是，证明当前任务就是该任务队列的头部，并且该任务队列不唯一，我们将头部移到末尾该任务队列就完成了同级别任务的调度
									while(jump_wei(taskLists[i],i))
									{
										if(currentTask == taskLists[i]) //如果currentTask == taskLists[i],说明该任务队列都礼让，就
										{
											jump_wei(taskLists[i],i);
											break;
										}
										
									}

                		 // 将当前任务移到末尾，并处理链表指针
            }
					}

            // 默认下个任务，是优先级任务队列的头部
            nextTask = taskLists[i];
					

						
            return;
        }
    }

    // 如果没有其他任务可运行，切换到空闲任务
//    nextTask = idleTask;
}



void os_schedule_start(void)
{
//	schedule(1);
	PendSV_init();
}



