#ifndef __QUEUE_H
#define __QUEUE_H
#include <thread.h>
typedef   signed          char int8_t;
typedef unsigned long     UBaseType_t;
typedef long BaseType_t;
typedef void * QueueHandle_t;
#define pdFALSE			( ( BaseType_t ) 0 )
#define pdTRUE			( ( BaseType_t ) 1 )

#define pdPASS			( pdTRUE )
#define pdFAIL			( pdFALSE )

typedef struct QueueDefinition{
	int8_t *pcHead;                  /* 队列存储区的起始地址 */
  int8_t *pcTail;                  /* 队列存储区的结束标志 */
  int8_t *pcWriteTo;               /* 下一个写入的位置 */
  int8_t *pcReadFrom;              /* 当前读的位置 */
	
	TaskControlBlock *xTasksWaitingToSend;
	TaskControlBlock *xTasksWaitingToReceive;
	
	volatile UBaseType_t uxMessagesWaiting;/*< The number of items currently in the queue. *//* 当前队列中的消息数量 */
	UBaseType_t uxLength;			/*< The length of the queue defined as the number of items it will hold, not the number of bytes. *//* 队列的长度，即队列可以容纳的项数 */
	UBaseType_t uxItemSize;			/*< The size of each items that the queue will hold. *//* 每个队列项的大小 */

	/* 接收锁，用于多任务调度 */
	volatile int8_t cRxLock;		/*< Stores the number of items received from the queue (removed from the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
	volatile int8_t cTxLock;		/*< Stores the number of items transmitted to the queue (added to the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
	/* 发送锁，用于多任务调度 */
}xQUEUE;

typedef xQUEUE Queue_t;
typedef uint32_t TickType_t;
/******************************************************************自己定义*******************************************************************/



Queue_t *xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t xQueueReceive(Queue_t *pxQueue, void *pvBuffer);
BaseType_t xQueueSend(Queue_t *pxQueue, const void *pvItemToQueue);
void vQueuePrint(Queue_t *pxQueue);
BaseType_t xQueueGenericSend(Queue_t *pxQueue, const void * const pvItemToQueue, TickType_t delay);
BaseType_t xQueueGenericReceive(Queue_t *pxQueue, void * const pvItemToQueue, TickType_t delay);
#endif

