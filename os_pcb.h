/*
 * os_pcb.h
 *
 *  Created on: Oct 14, 2014
 *      Author: yc
 */

#ifndef OS_PCB_H_
#define OS_PCB_H_


#include         "global.h"


//*****************PCB state***********************************************
#define			WAITING  		                         (int)0
#define			READY                 	 	             (int)1
#define			MSG_SUSPEND              		         (int)2
#define			SUSPEND               		             (int)3

//*****************priorities**********************************************
#define			HIGHEST_PRIORITY                         (int)1
#define			HIGH_PRIORITY                            (int)10

//***********************run or not run************************************
#define			RUN								          TRUE
#define			NOTRUN							          FALSE

//***********************errors********************************************
#define			SUCCESS	                             	  0L
#define			NAME_ERROR         		                  1L
#define			PRIORITY_ERROR                            2L
#define			EXCEED_MAX_SIZE                           3L
#define			ID_ERROR         		                  4L
#define			EXCEED_MSG_ERROR         		          5L
#define         SUSPEND_SELF                              6L
#define         SUSPEND_ID_ERROR                          7L
#define         SUSPEND_AGAIN_ERROR                       8L
#define         RESUME_ID_ERROR                           9L
#define			MSG_ERROR         		                  10L
#define			MSG_LENGTH_ERROR         		          11L
#define			MSG_ID_ERROR         		              12L


//**********************LOCK*********************************************
#define			TIMERQ_LOCK						 MEMORY_INTERLOCK_BASE
#define			READYQ_LOCK						 MEMORY_INTERLOCK_BASE+1
#define			SUSPENDQ_LOCK				     MEMORY_INTERLOCK_BASE+2
#define         DISK_LOCK                        MEMORY_INTERLOCK_BASE+3
#define         FRAME_LOCK                       MEMORY_INTERLOCK_BASE+13



#define         LOCK                                  (int)1
#define         UNLOCK                                (int)0
#define         SUSPEND_UNTIL_LOCKED                  (int)1

//************************define the max***********************************
#define			MAX_PCB_NUM			     		      (int)10
#define         MAX_MSG_LENGTH         			      (int)500
#define         MAX_MSG_NUM        			          (int)10
#define			MAX_WAKEUP_TIME					      (int)10000
#define         MAX_PRIORITY                          (int)800

#define         MIN_PRIORITY                          (int)0

//***********************print*********************************************
#define 		FULL_PRINTING						  (int)1000
#define 		LIMITED_PRINTING					  (int)10
#define 		NO_PRINTING							  (int)0

//**********************phase2**********************************************
#define         PCB_PAGE_TABLE_LENGTH              		  (INT32)1024
#define         TOTAL_FRAME_NUMBER              		  (INT32)64

#define         FRAME_EMPTY                                 0x80000000
#define         FRAME_FULL                                  0xffffffff
#define         FRAME_ERROR                                 66

#define         DISK_WRITE                                  1
#define         DISK_READ                                   0
#define         DISK_NUM                                    8

#define         FRAME_PID_MASK                              0x000f0000
#define         FRAME_PAGE_MASK                             0x0000ffff

#define         MODIFIED_MASK                               0x00004000
#define         REFERENCE_MASK                              0x00002000
#define         VALID_MASK                                  0x00008000



//*********************pcb struct*************************************
typedef struct pcb{
	int pid;
	char process_name[20];
	int priority;
	int process_state;
	int wakeup_time;
	void *context;
	BOOL wait_msg;
	int wait_msg_from;
	int wait_msg_length;
	struct pcb  *next;

	UINT16  pcb_page_table[PCB_PAGE_TABLE_LENGTH];

	int disk_id;
	int disk_sector;
	char *disk_operation_data;
	int disk_operation;

} PCB;


//********************** Message struct******************************
typedef struct msg{
		int   msg_id;
		int   source_pid;
		int	  target_pid;
		int   send_length;
		int   receive_length;
		char   msg_buffer[MAX_MSG_LENGTH];
		struct msg *next;
} message;

//*******************extern variables*******************************
PCB * timerQ_head;
PCB * readyQ_head;
PCB * suspendQ_head;
message * msgQ_head;

int *current_pid;
int pid_num;
int  totalQ_size;
PCB	 ** totalQ;
int msg_num;
int base_print;
int scheduler_print;
int memory_print;


PCB * disk_table[DISK_NUM+1];      //from 0 to 8, 0 is not used

INT32 frame_table[PHYS_MEM_PGS];    //from 0 to 63

int shadow_page_table[MAX_NUMBER_OF_DISKS][NUM_LOGICAL_SECTORS];

int frame_count;

int frame_count_table[MAX_NUMBER_OF_DISKS];

#endif /* OS_PCB_H_ */
