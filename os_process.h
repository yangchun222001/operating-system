/*
 * os_process.h
 *
 *  Created on: Oct 14, 2014
 *      Author: yc
 */

#ifndef OS_PROCESS_H_
#define OS_PROCESS_H_

#include   "os_pcb.h"


//**********************basic part*********************************
void os_create_process( char  [], void * , long , long *, BOOL, long *);
void os_sleep(long);
void os_terminate_process(long, long*);
void start_timer();
void timeInterrupt();
void dispatcher();


//********************suspend & resume part*************************
void os_suspend_process(long, long*);
void os_resume_process(long,long*);


//*******************message part***********************************
void os_send_message(long , char * ,long , long*);
void os_receive_message(long, char* , long , long * , long *, long *);
void broadcast_msg(message * ,long*);
void send_msg_to_target(message * , int, long*);
//message * find_msg_by_sender_receiver(int, int);
//message * find_msg_by_receiver(int);
message * find_msg(int, int);
PCB *pcb_wait_msg();


//****************other system call*******************************
void os_change_priority(long, long, long*);
void os_get_pid_by_name(char [] ,  long *, long * );


//****************Q operation************************************
void put_in_msgQ(message * );
message* delete_msgQ(INT32);

void put_in_readyQ(PCB * );
PCB * delete_readyQ(int );

void put_in_timerQ(PCB * );
PCB * delete_timerQ(int);

void put_in_suspendQ(PCB *);
PCB * delete_suspendQ(int);

BOOL is_in_Q(int, PCB*, int);


//******************totalQ operations****************************
void delete_totalQ(int );
PCB * get_pcb_in_totalQ(int);
//BOOL check_name(char  []);

void sp_print();

//change-----------------------------------------
//-----------------------------------------------
/*virtual page management and schedule*/
void page_fault(INT32);
int get_empty_frame_id();
void os_disk_write_read(long, long, char*,int);
//void os_disk_read(long, long, char*);
void disk_operation_done(int);
PCB* delete_diskQ(int);
void put_in_diskQ(PCB* pcb, int);
void set_disk(int);

void replace_page(int, int, int);
int find_victim_page();


void MP_print(void);

//-----------------------------------------------



#endif /* OS_PROCESS_H_ */
