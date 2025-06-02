#include "cpu.h"
#include <stdint.h>
#include "main.h"
#define NVIC_INT_CTRL 			0xE000Ed04	//寄存器地址
#define NVIC_PENDSVSET 		  0x10000000
#define NVIC_SYSPRI2			  0xE000ED22	//寄存器地址
#define NVIC_PENDSV_PRI		  0x000000FF

#define MEM32(addr)				*(volatile unsigned long *)(addr)
#define MEM8(addr)				 *(volatile unsigned char *)(addr)

#define configPRIO_BITS         4
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
//extern TaskControlBlock *currentTask;
//extern TaskControlBlock *nextTask;


void PendSV_init(void){
	//设置pendsv异常 
    __set_PSP(0);
	MEM8(NVIC_SYSPRI2) = NVIC_PENDSV_PRI;
	MEM32(NVIC_INT_CTRL) = NVIC_PENDSVSET;
}


void triggerPendSV()  // 触发PendSV进行任务切换
{
	 MEM32(NVIC_INT_CTRL) = NVIC_PENDSVSET;  //触发异常
	
}


//__asm void PendSV_Handler(void) {
//    IMPORT currentTask
//    IMPORT nextTask
//		extern schedule;
//    // 获取 PSP 寄存器值（当前任务堆栈指针）
//    MRS R0,PSP
//    CBZ R0,PendSVHandler_nosave
//	  STMDB R0!,{R4-R11}
//   //把currentTask的地址 - > R1
//    LDR R1,=currentTask
//    //R1地址的内容 ->R1
//    LDR R1,[R1]
//    //让R1的内容保存栈顶的内容
//    //即R1保存的是currentTask栈顶内容(压栈后的地址).
//    STR R0,[R1]

//PendSVHandler_nosave
//		
//			// 保存 LR 寄存器
//			mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
//			msr basepri, r0
//			dsb
//			isb
//			MOV     R6, LR  //寄存器6是随意选的
//			BL schedule	
//			MOV     LR, R6
//			mov r0, #0
//			msr basepri, r0
//			
//		
//			//交换
//			LDR R0,=currentTask
//			LDR R1,=nextTask
//			LDR R2,[R1]
//			STR R2,[R0]
//		
//		 //取出堆栈地址
//			LDR R0,[R2]
//			LDMIA R0!,{R4-R11}
//			
//			
////			MOV R3,LR
//			//切换
//			MSR PSP,R0
//			
//			// 为了在回到任务后，确保继续使用PSP堆栈
//			ORR LR,LR,#0x04
//			
//			BX LR   //退出堆栈

//}



#include <stdint.h>

extern void schedule(void);
extern uint32_t *currentTask;
extern uint32_t *nextTask;

__asm void PendSV_Handler(void)
{
    IMPORT schedule
    IMPORT currentTask
    IMPORT nextTask

    MRS     R0, PSP
    CBZ     R0, no_save
    STMDB   R0!, {R4-R11}

    LDR     R1, =currentTask
    LDR     R1, [R1]
    STR     R0, [R1]

no_save
    // 临界区，屏蔽中断
    MOV     R0, #0x20
    MSR     BASEPRI, R0
    DSB
    ISB

    MOV     R6, LR
    BL      schedule
    MOV     LR, R6

    // 开中断
    MOV     R0, #0
    MSR     BASEPRI, R0

    // 切换任务
    LDR     R0, =currentTask
    LDR     R1, =nextTask
    LDR     R2, [R1]
    STR     R2, [R0]

    // 恢复下一任务上下文
    LDR     R0, [R2]
    LDMIA   R0!, {R4-R11}
    MSR     PSP, R0

    ORR     LR, LR, #0x04
    BX      LR
}



uint32_t basepri;  // 保存 BASEPRI 寄存器的值

// 进入临界区，屏蔽低优先级中断
void enterCritical(void) {
    basepri = __get_BASEPRI();    // 获取当前 BASEPRI 值
    __set_BASEPRI(0x10);          // 屏蔽优先级 16 及以下的中断
}

// 退出临界区并恢复中断优先级
void exitCritical(void) {
    __set_BASEPRI(basepri);       // 恢复之前的 BASEPRI 值
}


