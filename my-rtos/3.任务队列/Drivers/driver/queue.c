#include "queue.h"
#include "stm32f1xx_hal.h"
#include "cpu.h"
extern TaskControlBlock *currentTask;
extern TaskControlBlock *nextTask;
extern TaskControlBlock *taskLists[MAX_PRIORITY]; 
extern TaskControlBlock *delayList;
extern TaskHandle_t task_1;
extern TaskHandle_t task_2;
extern TaskHandle_t task_3;

Queue_t *xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    UBaseType_t xQueueSizeInBytes = uxQueueLength * uxItemSize;
    Queue_t *pxNewQueue = (Queue_t *)malloc(sizeof(Queue_t) + xQueueSizeInBytes);

    if (pxNewQueue == NULL) {
        return NULL;  // �ڴ����ʧ��
    }

    // ��ʼ������ָ��
    pxNewQueue->pcHead = (int8_t *)(pxNewQueue + 1); // ���д洢�ռ�λ�ڽṹ��֮��
    pxNewQueue->pcTail = pxNewQueue->pcHead + xQueueSizeInBytes;
    pxNewQueue->pcWriteTo = pxNewQueue->pcHead;
    pxNewQueue->pcReadFrom = pxNewQueue->pcHead;

    // ��ʼ����������
    pxNewQueue->uxLength = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;
    pxNewQueue->uxMessagesWaiting = 0;
    pxNewQueue->cRxLock = 0;
    pxNewQueue->cTxLock = 0;

    pxNewQueue->xTasksWaitingToSend = NULL;
    pxNewQueue->xTasksWaitingToReceive = NULL;

    return pxNewQueue;
}

/* ���к�����������д����� */
BaseType_t xQueueSend(Queue_t *pxQueue, const void *pvItemToQueue) {
    if (pxQueue == NULL || pvItemToQueue == NULL) {
        return pdFAIL; // ��ֹ��ָ�����
    }

    // �������Ƿ�����
    if (pxQueue->uxMessagesWaiting >= pxQueue->uxLength) {
        return pdFAIL;
    }

    // �������ݵ���ǰд��λ��
    memcpy(pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize);

    // ����дָ�룬ѭ��ʹ�ö��пռ�
    pxQueue->pcWriteTo += pxQueue->uxItemSize;
    if (pxQueue->pcWriteTo >= pxQueue->pcTail) {
        pxQueue->pcWriteTo = pxQueue->pcHead; // ���Ƶ���ͷ
    }

    // ��Ϣ������һ
    pxQueue->uxMessagesWaiting++;

    return pdPASS;
}

/* ���к������Ӷ����ж�ȡ���� */
BaseType_t xQueueReceive(Queue_t *pxQueue, void *pvBuffer) {
    if (pxQueue == NULL || pvBuffer == NULL) {
        return pdFAIL; // ��ֹ��ָ�����
    }

    // �������Ƿ�Ϊ��
    if (pxQueue->uxMessagesWaiting == 0) {
        return pdFAIL;
    }

    // �������ݵ�Ŀ�껺����
    memcpy(pvBuffer, pxQueue->pcReadFrom, pxQueue->uxItemSize);

    // ���¶�ָ�룬ѭ��ʹ�ö��пռ�
    pxQueue->pcReadFrom += pxQueue->uxItemSize;
    if (pxQueue->pcReadFrom >= pxQueue->pcTail) {
        pxQueue->pcReadFrom = pxQueue->pcHead; // ����
    }

    // ��Ϣ������һ
    pxQueue->uxMessagesWaiting--;

    return pdPASS;
}


/*�ú������ڴ�ӡ�����е�Ԫ�أ��������*/
void vQueuePrint(Queue_t *pxQueue) {
    printf("Queue Length: %d\r\n", pxQueue->uxLength);
    printf("Messages Waiting: %d\r\n", pxQueue->uxMessagesWaiting);

    int8_t *pTempReadFrom = pxQueue->pcReadFrom;
    for (UBaseType_t i = 0; i < pxQueue->uxMessagesWaiting; i++) {
        printf("Item %d: %d\r\n", i, *((int *)pTempReadFrom));

        // ѭ�����¶�ָ��
        pTempReadFrom += pxQueue->uxItemSize;
        if (pTempReadFrom >= pxQueue->pcTail) {
            pTempReadFrom = pxQueue->pcHead;
        }
    }
}

// ���������е���������
static void WakeUpAll(TaskControlBlock **list) {
    while (*list != NULL) {
        TaskControlBlock *task = *list;
        removeTaskFromList(list, task);
        removeTaskFromList(&delayList, task);
        TaskList_Add(task, &taskLists[task->priority]);
    }
}

BaseType_t xQueueGenericSend(Queue_t *pxQueue, const void *pvItemToQueue, TickType_t delay) {
    BaseType_t xReturn;

    set_task_delay(NULL, delay);  // ���õ�ǰ�����ӳ٣�����Ϊ 0��

    while (1) {
        enterCritical();

        xReturn = xQueueSend(pxQueue, pvItemToQueue);

        if (xReturn == pdPASS) {
            set_task_delay(NULL, 0);  // ���ͳɹ�������ӳ�
            WakeUpAll(&(pxQueue->xTasksWaitingToReceive));  // ���ѽ�����

            exitCritical();
            return pdPASS;
        }

        // �������� delay == 0������ʧ���˳�
        if (currentTask->delay == 0) {
            removeTaskFromList(&(pxQueue->xTasksWaitingToSend), (TaskHandle_t)&currentTask);
            exitCritical();
            return pdFAIL;
        }

        // ���򣬽�����������״̬
        removeTaskFromList(&taskLists[currentTask->priority], NULL);
        TaskList_Add(NULL, &(pxQueue->xTasksWaitingToSend));
        TaskList_Add(NULL, &delayList);

        exitCritical();
        restart_pendsv();  // �ֶ����������л�
    }
}
BaseType_t xQueueGenericReceive(Queue_t *pxQueue, void *pvBuffer, TickType_t delay) {
    BaseType_t xReturn;

    set_task_delay(NULL, delay);  // ���õ�ǰ�����ӳ٣�����Ϊ 0��

    while (1) {
        enterCritical();

        xReturn = xQueueReceive(pxQueue, pvBuffer);

        if (xReturn == pdPASS) {
            set_task_delay(NULL, 0);  // �ɹ����գ�����ӳ�
            WakeUpAll(&(pxQueue->xTasksWaitingToSend));  // ���ѷ�����

            exitCritical();
            return pdPASS;
        }

        // ���п��� delay == 0������ʧ���˳�
        if (currentTask->delay == 0) {
            removeTaskFromList(&(pxQueue->xTasksWaitingToReceive), (TaskHandle_t)&currentTask);
            exitCritical();
            return pdFAIL;
        }

        // ���򣬽�����������״̬
        removeTaskFromList(&taskLists[currentTask->priority], NULL);
        TaskList_Add(NULL, &(pxQueue->xTasksWaitingToReceive));
        TaskList_Add(NULL, &delayList);

        exitCritical();
        restart_pendsv();
    }
}
