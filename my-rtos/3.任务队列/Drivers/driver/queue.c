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
        return NULL;  // 内存分配失败
    }

    // 初始化队列指针
    pxNewQueue->pcHead = (int8_t *)(pxNewQueue + 1); // 队列存储空间位于结构体之后
    pxNewQueue->pcTail = pxNewQueue->pcHead + xQueueSizeInBytes;
    pxNewQueue->pcWriteTo = pxNewQueue->pcHead;
    pxNewQueue->pcReadFrom = pxNewQueue->pcHead;

    // 初始化其他参数
    pxNewQueue->uxLength = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;
    pxNewQueue->uxMessagesWaiting = 0;
    pxNewQueue->cRxLock = 0;
    pxNewQueue->cTxLock = 0;

    pxNewQueue->xTasksWaitingToSend = NULL;
    pxNewQueue->xTasksWaitingToReceive = NULL;

    return pxNewQueue;
}

/* 入列函数：将数据写入队列 */
BaseType_t xQueueSend(Queue_t *pxQueue, const void *pvItemToQueue) {
    if (pxQueue == NULL || pvItemToQueue == NULL) {
        return pdFAIL; // 防止空指针访问
    }

    // 检查队列是否已满
    if (pxQueue->uxMessagesWaiting >= pxQueue->uxLength) {
        return pdFAIL;
    }

    // 拷贝数据到当前写入位置
    memcpy(pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize);

    // 更新写指针，循环使用队列空间
    pxQueue->pcWriteTo += pxQueue->uxItemSize;
    if (pxQueue->pcWriteTo >= pxQueue->pcTail) {
        pxQueue->pcWriteTo = pxQueue->pcHead; // 回绕到队头
    }

    // 消息数量加一
    pxQueue->uxMessagesWaiting++;

    return pdPASS;
}

/* 出列函数：从队列中读取数据 */
BaseType_t xQueueReceive(Queue_t *pxQueue, void *pvBuffer) {
    if (pxQueue == NULL || pvBuffer == NULL) {
        return pdFAIL; // 防止空指针访问
    }

    // 检查队列是否为空
    if (pxQueue->uxMessagesWaiting == 0) {
        return pdFAIL;
    }

    // 拷贝数据到目标缓冲区
    memcpy(pvBuffer, pxQueue->pcReadFrom, pxQueue->uxItemSize);

    // 更新读指针，循环使用队列空间
    pxQueue->pcReadFrom += pxQueue->uxItemSize;
    if (pxQueue->pcReadFrom >= pxQueue->pcTail) {
        pxQueue->pcReadFrom = pxQueue->pcHead; // 回绕
    }

    // 消息数量减一
    pxQueue->uxMessagesWaiting--;

    return pdPASS;
}


/*该函数用于打印队列中的元素，方便调试*/
void vQueuePrint(Queue_t *pxQueue) {
    printf("Queue Length: %d\r\n", pxQueue->uxLength);
    printf("Messages Waiting: %d\r\n", pxQueue->uxMessagesWaiting);

    int8_t *pTempReadFrom = pxQueue->pcReadFrom;
    for (UBaseType_t i = 0; i < pxQueue->uxMessagesWaiting; i++) {
        printf("Item %d: %d\r\n", i, *((int *)pTempReadFrom));

        // 循环更新读指针
        pTempReadFrom += pxQueue->uxItemSize;
        if (pTempReadFrom >= pxQueue->pcTail) {
            pTempReadFrom = pxQueue->pcHead;
        }
    }
}

// 唤醒链表中的所有任务
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

    set_task_delay(NULL, delay);  // 配置当前任务延迟（可以为 0）

    while (1) {
        enterCritical();

        xReturn = xQueueSend(pxQueue, pvItemToQueue);

        if (xReturn == pdPASS) {
            set_task_delay(NULL, 0);  // 发送成功，清除延迟
            WakeUpAll(&(pxQueue->xTasksWaitingToReceive));  // 唤醒接收者

            exitCritical();
            return pdPASS;
        }

        // 队列满且 delay == 0，立即失败退出
        if (currentTask->delay == 0) {
            removeTaskFromList(&(pxQueue->xTasksWaitingToSend), (TaskHandle_t)&currentTask);
            exitCritical();
            return pdFAIL;
        }

        // 否则，进入阻塞挂起状态
        removeTaskFromList(&taskLists[currentTask->priority], NULL);
        TaskList_Add(NULL, &(pxQueue->xTasksWaitingToSend));
        TaskList_Add(NULL, &delayList);

        exitCritical();
        restart_pendsv();  // 手动触发任务切换
    }
}
BaseType_t xQueueGenericReceive(Queue_t *pxQueue, void *pvBuffer, TickType_t delay) {
    BaseType_t xReturn;

    set_task_delay(NULL, delay);  // 配置当前任务延迟（可以为 0）

    while (1) {
        enterCritical();

        xReturn = xQueueReceive(pxQueue, pvBuffer);

        if (xReturn == pdPASS) {
            set_task_delay(NULL, 0);  // 成功接收，清除延迟
            WakeUpAll(&(pxQueue->xTasksWaitingToSend));  // 唤醒发送者

            exitCritical();
            return pdPASS;
        }

        // 队列空且 delay == 0，立即失败退出
        if (currentTask->delay == 0) {
            removeTaskFromList(&(pxQueue->xTasksWaitingToReceive), (TaskHandle_t)&currentTask);
            exitCritical();
            return pdFAIL;
        }

        // 否则，进入阻塞挂起状态
        removeTaskFromList(&taskLists[currentTask->priority], NULL);
        TaskList_Add(NULL, &(pxQueue->xTasksWaitingToReceive));
        TaskList_Add(NULL, &delayList);

        exitCritical();
        restart_pendsv();
    }
}
