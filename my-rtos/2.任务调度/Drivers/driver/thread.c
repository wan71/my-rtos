#include <thread.h>
#include "stm32f1xx_hal.h"

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



/*
 * ������ p ������ a ���Ƴ���
 * ����ֵ��1 ��ʾ�Ƴ��ɹ���0 ��ʾ���񲻴��ڻ�����Ϊ�ա�
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
        return 0; // ����ʶ������
    }

    TaskControlBlock *curr = *listHead;
    TaskControlBlock *prev = NULL;

    while (curr != NULL) {
        if (curr == task) {
            if (prev == NULL) {
                *listHead = curr->next;  // �Ƴ�ͷ�ڵ�
            } else {
                prev->next = curr->next; // �Ƴ��м�/β���ڵ�
            }
            task->next = NULL; // ��� next ָ��
            exitCritical();
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }

    exitCritical();
    return 0; // û�ҵ�
}

/*
 * ������ p �������� b����� a �� 0���򴥷� PendSV��
 * ����ֵ��1 ��ʾ��ӳɹ���
 */
int AddTaskToList(TaskHandle_t p, TaskHandle_t b, int a) {
    if (p == NULL) {
        p = (void *)currentTask;
    }

    TaskControlBlock *task = (TaskControlBlock *)p;
    TaskControlBlock **listHead = NULL;

    // ѡ������ͷ
    if (b == (void *)taskLists) {
        listHead = &taskLists[task->priority];
    } else if (b == (void *)delayList) {
        listHead = &delayList;
    } else {
        return 0; // �Ƿ�����
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

    // �Ƿ񴥷� PendSV
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
���������ڴ�����̬����delay̬��Ĭ���ǵ�ǰ����
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


//void schedule(void) {
//    // �����������ȼ��������б��Ӹ����ȼ��������ȼ�
//    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
//        if (taskLists[i] != NULL) {
//           
//            if (currentTask != NULL && currentTask->priority <= i) {    // �����ǰ������ͬ���ȼ�����
//                TaskControlBlock *firstTask = taskLists[i]; // ȡ������������
//							  
//                if (firstTask->next != NULL && currentTask == firstTask) {  
//									// ������������ֻ��һ������ ���� ����һ���ս������̬������ֱ�ӽ���Ĭ��
//									// ��������ǣ�֤����ǰ������Ǹ�������е�ͷ�������Ҹ�������в�Ψһ�����ǽ�ͷ���Ƶ�ĩβ��������о������ͬ��������ĵ���
//									while(jump_wei(taskLists[i],i))
//									{
//										if(currentTask == taskLists[i]) //���currentTask == taskLists[i],˵����������ж����ã���
//										{
//											jump_wei(taskLists[i],i);
//											break;
//										}
//										
//									}

//                		 // ����ǰ�����Ƶ�ĩβ������������ָ��
//            }
//					}

//            // Ĭ���¸����������ȼ�������е�ͷ��
//            nextTask = taskLists[i];
//						
//						

//						
//            return;
//        }
//    }

//    // ���û��������������У��л�����������
////    nextTask = idleTask;
//}

void rotateTaskList(TaskControlBlock **listHead) {
    if (*listHead == NULL || (*listHead)->next == NULL) return; // �ջ򵥽ڵ�

    TaskControlBlock *head = *listHead;
    TaskControlBlock *tail = head;

    // �ҵ����һ���ڵ�
    while (tail->next != NULL) {
        tail = tail->next;
    }

    // ͷ�������Ƶ�β��
    *listHead = head->next;
    head->next = NULL;
    tail->next = head;
}



void schedule(void) {
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        TaskControlBlock *head = taskLists[i];
        if (head == NULL) continue;

        // �����ǰ�����ڵ�ǰ���ȼ�
        if (currentTask != NULL && currentTask->priority == i) {
            if (head == currentTask && head->next != NULL) {
                // ��ǰ�����Ǹ����ȼ����е�ͷ������ͬ�������ó���ת��ĩβ��
                rotateTaskList(&taskLists[i]);
            }
        }

        // ������һ��Ҫ���е�����
        nextTask = taskLists[i];
        return;
    }

    // ���û����������У�ȫ�գ���Ĭ�Ͽ����л��� idle ������������ˣ�
    // nextTask = idleTask;
}


void every_delay(void) {
    TaskControlBlock *current = delayList;
    while (current != NULL) {
        TaskControlBlock *next = current->next;  // �ȱ��� next

        if (current->delay > 0) {
            current->delay--;
        }

        if (current->delay == 0) {
            // �� delayList ɾ������ӵ���Ӧ���ȼ��� taskLists
            removeTaskFromList((TaskHandle_t)delayList, (void *)current);
            AddTaskToList((void *)current, (TaskHandle_t)taskLists, 0);
        }

        current = next;  // �ƶ�����һ������
    }
}


void os_schedule_start(void)
{
//	schedule(1);
	PendSV_init();
}



