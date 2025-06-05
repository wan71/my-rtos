#include <thread.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "cpu.h"


//TaskControlBlock *currentTask;
//TaskControlBlock *nextTask;
TaskControlBlock *currentTask = NULL;  // ��ǰ��������
TaskControlBlock *nextTask = NULL;     // ��һ������

TaskControlBlock *taskLists[MAX_PRIORITY];  // ���������ͷָ��
TaskControlBlock *delayList = NULL;         // ��ʱ��������
int taskCount = 0;  // ��ǰ��������


void addTaskToList(TaskControlBlock *task) {
    int priority = task->priority;
    if (priority >= MAX_PRIORITY || priority < 0) {
        printf("Invalid priority!\n");
        return;
    }

    task->next = NULL;

    if (taskLists[priority] == NULL) {
        taskLists[priority] = task;  // �������Ϊ�գ�ֱ�Ӽ���
    } else {
        TaskControlBlock *current = taskLists[priority];
        while (current->next != NULL) {
            current = current->next;  // ����ĩβ�������
        }
        current->next = task;
    }
}


// ������ list ���Ƴ����� task����� task Ϊ NULL�����Ƴ� currentTask��
// �����Ƿ�ɹ��Ƴ���1 Ϊ�ɹ���0 Ϊδ�ҵ���-1 Ϊ������
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
            // �ҵ�Ŀ������ִ��ɾ��
            if (prev == NULL) {
                // ɾ��ͷ�ڵ�
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

    // û���ҵ�Ŀ������
    return 0;
}



// ������ task ���뵽���� list �У����δ�ظ��������� 1 ��ʾ��ӳɹ���0 ��ʾ�Ѵ���
int TaskList_Add(TaskHandle_t taskHandle, TaskHandle_t listHandle) {
    if (taskHandle == NULL) {
        if (currentTask == NULL) return -1;
        taskHandle = currentTask;
    }

    TaskControlBlock *task = (TaskControlBlock *)taskHandle;
    TaskControlBlock **listHead = (TaskControlBlock **)listHandle;

    if (listHead == NULL) {
        return -1; // ������Ϊ NULL������
    }

    // ��������Ƿ�����������
    TaskControlBlock *curr = *listHead;
    while (curr != NULL) {
        if (curr == task) return 0; // �Ѵ���
        curr = curr->next;
    }

    // ��ӵ�����ĩβ
    if (*listHead == NULL) {
        *listHead = task;
    } else {
        curr = *listHead;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = task;
    }

    task->next = NULL; // ����Ϊβ�ڵ�
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
���������ڴ�����̬����delay̬��Ĭ���ǵ�ǰ����
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

/*ÿ��ʱ���ж�ʱ������ delayList��������ʱʱ�䣬�����ʱ�������������ƻض�Ӧ���ȼ��� taskLists*/

void every_delay(void) {
	
		enterCritical();
    TaskControlBlock *prev = NULL;
    TaskControlBlock *current = delayList;
	 while (current != NULL) {
		  if (current->delay > 0) {
            current->delay--;  // ������ʱʱ��
        }
		 if (current->delay == 0) {
				  // ��������뵽��Ӧ���ȼ��� taskLists
					
					removeTaskFromList(&delayList,(void *)current);
					TaskList_Add((void *)current,&taskLists[current->priority]);
			 }
//			 
			 // ��������
        prev = current;
        current = current->next;
			 
	 }
	exitCritical();
}

int createTask(void (*taskFunc)(void *), void *param, int priority, int yield, int stackSize,TaskHandle_t * pxCreatedTask ) {
    if (taskCount < MAX_TASKS) {
        // ��̬���� TaskControlBlock
        TaskControlBlock *tcb = (TaskControlBlock *)malloc(sizeof(TaskControlBlock));
        if (tcb == NULL) {
            printf("Memory allocation for TaskControlBlock failed\r\n");
            return 0;  // �ڴ����ʧ��
        }

        // ��̬����ջ�ռ�
        tcb->stack = (uint32_t *)malloc(stackSize * sizeof(uint32_t));
        if (tcb->stack == NULL) {
            printf("Memory allocation for stack failed\r\n");
            free(tcb);  // �ͷ��ѷ���� TaskControlBlock
            return 0;  // �ڴ����ʧ��
        }
        // ָ������ջ�Ķ���
        uint32_t *stack = &(tcb->stack[stackSize]);

        // ��ʼ������Ķ�ջ (xPSR, PC, LR, R12, R3-R0, R11-R4)
        *(--stack) = (uint32_t)0x01000000;  // xPSR: Thumb״̬
        *(--stack) = (uint32_t)taskFunc;    // PC: ������ں�����ַ
        *(--stack) = (uint32_t)0xFFFFFFFD;  // LR: ���ص��߳�ģʽ
        *(--stack) = (uint32_t)0x12121212;  // R12: ����ֵ
        *(--stack) = (uint32_t)0x03030303;  // R3
        *(--stack) = (uint32_t)0x02020202;  // R2
        *(--stack) = (uint32_t)0x01010101;  // R1
        *(--stack) = (uint32_t)param;       // R0: �������

        // ��ʼ��R11-R4��ֵ
        *(--stack) = (uint32_t)0x11111111;  // R11
        *(--stack) = (uint32_t)0x10101010;  // R10
        *(--stack) = (uint32_t)0x09090909;  // R9
        *(--stack) = (uint32_t)0x08080808;  // R8
        *(--stack) = (uint32_t)0x07070707;  // R7
        *(--stack) = (uint32_t)0x06060606;  // R6
        *(--stack) = (uint32_t)0x05050505;  // R5
        *(--stack) = (uint32_t)0x04040404;  // R4

        // ����TCB�Ķ�ջָ�����������
        tcb->stackPointer = stack;
        tcb->stackSize = stackSize;  // ����ջ��С
        tcb->priority = priority;
        tcb->yield = yield;
				tcb->delay=0;
        tcb->next = NULL;
				taskCount++;

        // ��������ӵ���������
        addTaskToList(tcb);
				
					if(pxCreatedTask != NULL)
				{	
				*pxCreatedTask=(TaskHandle_t)tcb;
				}
				return 1;
    }
			return 0;  // ������������������� NULL
}






//�ú�����������е�ͷ��ַ�������β��
//������һ��������е�ͷ����yield�����Ϊ1���������á�
int jump_wei(TaskControlBlock *firstTask,int i)
{
	    TaskControlBlock *lastTask = firstTask;
//      // ����������ĩβ

	 if (firstTask->next != NULL) {
      while (lastTask->next != NULL) {
      lastTask = lastTask->next;
      }
      // ����ǰ�����Ƶ�ĩβ������������ָ��
      lastTask->next = firstTask;
      taskLists[i] = firstTask->next;  // ����ͷ�Ƶ���һ������
      firstTask->next = NULL;          // ��ǰ����� next �ÿ�	
			
    }
	return taskLists[i]->yield;
}


void schedule(void) {
    // �����������ȼ��������б��Ӹ����ȼ��������ȼ�
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        if (taskLists[i] != NULL) {
           
            if (currentTask != NULL && currentTask->priority <= i) {    // �����ǰ������ͬ���ȼ�����
                TaskControlBlock *firstTask = taskLists[i]; // ȡ������������
							  
                if (firstTask->next != NULL && currentTask == firstTask) {  
									// ������������ֻ��һ������ ���� ����һ���ս������̬������ֱ�ӽ���Ĭ��
									// ��������ǣ�֤����ǰ������Ǹ�������е�ͷ�������Ҹ�������в�Ψһ�����ǽ�ͷ���Ƶ�ĩβ��������о������ͬ��������ĵ���
									while(jump_wei(taskLists[i],i))
									{
										if(currentTask == taskLists[i]) //���currentTask == taskLists[i],˵����������ж����ã���
										{
											jump_wei(taskLists[i],i);
											break;
										}
										
									}

                		 // ����ǰ�����Ƶ�ĩβ������������ָ��
            }
					}

            // Ĭ���¸����������ȼ�������е�ͷ��
            nextTask = taskLists[i];
					

						
            return;
        }
    }

    // ���û��������������У��л�����������
//    nextTask = idleTask;
}



void os_schedule_start(void)
{
//	schedule(1);
	PendSV_init();
}



