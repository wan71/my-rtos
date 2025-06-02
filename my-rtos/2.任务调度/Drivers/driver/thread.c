#include <thread.h>
#include "stm32f1xx_hal.h"

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



/*
 * 将任务 p 从链表 a 中移除。
 * 返回值：1 表示移除成功，0 表示任务不存在或链表为空。
 */
int removeTaskFromList(TaskHandle_t a, TaskHandle_t p) {
    if (p == NULL) {
        p = (void *)currentTask;
    }

    TaskControlBlock *task = (TaskControlBlock *)p;
    TaskControlBlock **listHead = NULL;

    enterCritical();

    if (a == (void *)taskLists) {
        listHead = &taskLists[task->priority];
    } else if (a == (void *)delayList) {
        listHead = &delayList;
    } else {
        exitCritical();
        return 0; // 不认识的链表
    }

    TaskControlBlock *curr = *listHead;
    TaskControlBlock *prev = NULL;

    while (curr != NULL) {
        if (curr == task) {
            if (prev == NULL) {
                *listHead = curr->next;  // 移除头节点
            } else {
                prev->next = curr->next; // 移除中间/尾部节点
            }
            task->next = NULL; // 清空 next 指针
            exitCritical();
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }

    exitCritical();
    return 0; // 没找到
}

/*
 * 将任务 p 加入链表 b，如果 a 非 0，则触发 PendSV。
 * 返回值：1 表示添加成功。
 */
int AddTaskToList(TaskHandle_t p, TaskHandle_t b, int a) {
    if (p == NULL) {
        p = (void *)currentTask;
    }

    TaskControlBlock *task = (TaskControlBlock *)p;
    TaskControlBlock **listHead = NULL;

    // 选定链表头
    if (b == (void *)taskLists) {
        listHead = &taskLists[task->priority];
    } else if (b == (void *)delayList) {
        listHead = &delayList;
    } else {
        return 0; // 非法链表
    }

    enterCritical();

    if (*listHead == NULL) {
        *listHead = task;
    } else {
        TaskControlBlock *curr = *listHead;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = task;
    }

    task->next = NULL;

    exitCritical();

    // 是否触发 PendSV
    if (a != 0) {
        enterCritical();
        HAL_SYSTICK_Config(SystemCoreClock / (1000U / uwTickFreq));
        exitCritical();
        triggerPendSV();
    }

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
//	TaskControlBlock *current = currentTask;
		set_task_delay(NULL,delay);
		removeTaskFromList(taskLists,NULL);
		AddTaskToList(NULL,delayList,1);
	
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


//void schedule(void) {
//    // 遍历所有优先级的任务列表，从高优先级到低优先级
//    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
//        if (taskLists[i] != NULL) {
//           
//            if (currentTask != NULL && currentTask->priority <= i) {    // 如果当前任务是同优先级任务
//                TaskControlBlock *firstTask = taskLists[i]; // 取出队列首任务
//							  
//                if (firstTask->next != NULL && currentTask == firstTask) {  
//									// 如果该任务队列只有一个任务 或者 这是一个刚进入就绪态的任务，直接进入默认
//									// 如果都不是，证明当前任务就是该任务队列的头部，并且该任务队列不唯一，我们将头部移到末尾该任务队列就完成了同级别任务的调度
//									while(jump_wei(taskLists[i],i))
//									{
//										if(currentTask == taskLists[i]) //如果currentTask == taskLists[i],说明该任务队列都礼让，就
//										{
//											jump_wei(taskLists[i],i);
//											break;
//										}
//										
//									}

//                		 // 将当前任务移到末尾，并处理链表指针
//            }
//					}

//            // 默认下个任务，是优先级任务队列的头部
//            nextTask = taskLists[i];
//						
//						

//						
//            return;
//        }
//    }

//    // 如果没有其他任务可运行，切换到空闲任务
////    nextTask = idleTask;
//}

void rotateTaskList(TaskControlBlock **listHead) {
    if (*listHead == NULL || (*listHead)->next == NULL) return; // 空或单节点

    TaskControlBlock *head = *listHead;
    TaskControlBlock *tail = head;

    // 找到最后一个节点
    while (tail->next != NULL) {
        tail = tail->next;
    }

    // 头部任务移到尾部
    *listHead = head->next;
    head->next = NULL;
    tail->next = head;
}



void schedule(void) {
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        TaskControlBlock *head = taskLists[i];
        if (head == NULL) continue;

        // 如果当前任务处于当前优先级
        if (currentTask != NULL && currentTask->priority == i) {
            if (head == currentTask && head->next != NULL) {
                // 当前任务是该优先级队列的头部且有同级任务，让出（转至末尾）
                rotateTaskList(&taskLists[i]);
            }
        }

        // 设置下一个要运行的任务
        nextTask = taskLists[i];
        return;
    }

    // 如果没有任务可运行（全空），默认可以切换到 idle 任务（如果定义了）
    // nextTask = idleTask;
}


void every_delay(void) {
    TaskControlBlock *current = delayList;
    while (current != NULL) {
        TaskControlBlock *next = current->next;  // 先保存 next

        if (current->delay > 0) {
            current->delay--;
        }

        if (current->delay == 0) {
            // 从 delayList 删除并添加到对应优先级的 taskLists
            removeTaskFromList((TaskHandle_t)delayList, (void *)current);
            AddTaskToList((void *)current, (TaskHandle_t)taskLists, 0);
        }

        current = next;  // 移动到下一个任务
    }
}


void os_schedule_start(void)
{
//	schedule(1);
	PendSV_init();
}



