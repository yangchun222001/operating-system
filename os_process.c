/*
 * chunyang_os.c
 *
 *  Created on: Oct 14, 2014
 *      Author: yc
 */



#include             "string.h"
#include             "stdlib.h"

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include 			 "z502.h"

#include             "os_pcb.h"
#include             "os_process.h"


//********my define extern variables**************
extern PCB * timerQ_head;
extern PCB * readyQ_head;

extern PCB	** totalQ;
extern int 	totalQ_size;
extern int   pid_num;

extern int   *current_pid;

extern int msg_num;

extern INT32 frame_table[PHYS_MEM_PGS];
extern PCB* disk_table[DISK_NUM+1];
extern int shadow_page_table[MAX_NUMBER_OF_DISKS][NUM_LOGICAL_SECTORS];

extern int frame_count;

extern int frame_count_table[8];

INT32  LockResult;

//*************************************************

/***********************basic part*********************************
 * 1. in the operating system, run a process need create a process
 * 2. sleep the process, wakeup_time add sleep_time to be pcb's new wakeup_time
 * 3. put the pcb into timerQ, delete from readyQ, call start_timer
 * 4. check pcb in timerQ, its wakeup_time less then current_time, if it is move it to readyQ
 * 5. the left pcb in timerQ, wakeup_time bigger then current_time,
 *    set timer interrupt by timerQ_head wakeup_time minus current_time
 * 6. terminate process, -2 means terminate all, -1 means terminate current process
 *    else desided by pid, if terminate process is in timerQ, call start_timer again
 * 7. dispatcher, change the current_pcb to a new one in readyQ
 *********************************************************/

/********************************************
 * the user calls create process
 * the beginning of the process, choose runProcess
 * 1. check the condition
 * 2. make process context
 * 3. run the main process
 ********************************************/

void os_create_process( char process_name[], void *process_address, long priority, long * pid_address, BOOL run, long* error){

	// check the condition
	if(strlen(process_name)==0){
		*error=NAME_ERROR;
		return;
	}

	if(priority < 0){
		*error=PRIORITY_ERROR;
		return;
	}

	if(totalQ_size >= MAX_PCB_NUM  ){
		*error=EXCEED_MAX_SIZE;
		return;
	}

	//create a PCB
	PCB *pcb_new;
	pcb_new = malloc(sizeof(PCB));
	pcb_new->pid=pid_num;
	pcb_new->wakeup_time = 0;
	pcb_new->process_state =READY;
	pcb_new->priority = priority;
	pcb_new->wait_msg=FALSE;
	pcb_new->wait_msg_from=0;
	pcb_new->wait_msg_length=0;
	strcpy(pcb_new->process_name,  process_name);
	pcb_new->next = NULL;

	//change---------------------------------
	int i;
	//pcb_new->page_table_len=SYS_PAGE_TABLE_LENGTH;


	for(i=0;i<PCB_PAGE_TABLE_LENGTH;i++){
	   pcb_new->pcb_page_table[i] = 0;
	}

	pcb_new->disk_id=0;
	pcb_new->disk_sector=0;
	pcb_new->disk_operation_data=NULL;
	pcb_new->disk_operation=-1;
	//pcb_new->availabe_sector=1;
//----------------------------------------------------

	pid_num++;

	//create process
	//if the process with the same name, fail the second one and terminate the first one
	//return to register
	*pid_address=(long)pcb_new->pid;

	CALL(Z502MakeContext(&(pcb_new->context), process_address, USER_MODE));

	//change------------------------------------
	/*create a context for testX*/
	((Z502CONTEXT *)pcb_new->context) ->page_table_ptr=pcb_new->pcb_page_table;
	((Z502CONTEXT *)pcb_new->context) ->page_table_len=PCB_PAGE_TABLE_LENGTH;

	//---------------------------------

	//put created process into ready queue and the total pcb table
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	put_in_readyQ(pcb_new);
	totalQ[totalQ_size]=pcb_new;
	totalQ_size++;

	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	*error=SUCCESS;

	//switch the context
	//run the main process
	if(run==TRUE){
		CALL(Z502SwitchContext(SWITCH_CONTEXT_KILL_MODE, &(pcb_new->context)));
	}
}



/********************************************************************
 * when the user calls terminate
 * 1.  if the signal is -2, stop all process
 * 2. if the signal is -1, stop current process
 * 3. terminate the process by pid
*********************************************************************/
void os_terminate_process(long terminate_signal, long * error){
	//terminate all process
	if(terminate_signal==-2){
		Z502Halt();
	}

	//terminate signal is -1, terminate current pcb
	if(terminate_signal==-1){
		terminate_signal=*current_pid;
	}

	//terminate the process by pid
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	CALL(delete_readyQ((int)terminate_signal));
	CALL(delete_timerQ((int)terminate_signal));
	CALL(delete_totalQ((int)terminate_signal));
	BOOL in_timerQ=is_in_Q(terminate_signal, timerQ_head,0);  //is_in_timerQ(terminate_signal);
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);


	//after delete, there is no process, terminate all
	if(totalQ_size==0){
	 	Z502Halt();
	}

	//if the pcb is in the timerQ, start_timer again
	if(in_timerQ==TRUE){
		start_timer();
	}

	//dispatch the new process
 	dispatcher();

	*error=SUCCESS;
}

/******************************************************
 * when the user calls sleep, this method starts to run
 * 1. delete from readyQ
 * 2. put in timerQ by wakeup_time
 * 3. start_timer
 * 4. dispatcher
 ******************************************************/
void os_sleep(long sleep_time){
	int current_time;

	CALL(MEM_READ(Z502ClockStatus, &current_time));

	//get the new wakeup_time
	PCB *current_pcb=get_pcb_in_totalQ(*current_pid);

	current_pcb->wakeup_time=current_time+sleep_time;

    READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
    READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	CALL(delete_readyQ(*current_pid));
	CALL(put_in_timerQ(current_pcb));
	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);


	start_timer();
	dispatcher();
}

/*************************************
 * this method start timer by the delay time
 * 1. check the timerQ_head wakeup_time
 * 2. wakeup_time is less than the current time
 * 3. call move_to_readyQ() move it to readyQ
 * 4. check the head again
 * 5. until the timerQ_head is after the current time
 * 6. set the timer
 *************************************/
void start_timer(){
///*
	int delay_time, current_time, wakeup_time;
	CALL(MEM_READ(Z502ClockStatus, &current_time));

	if(timerQ_head==NULL){
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	wakeup_time=timerQ_head->wakeup_time;
	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	//check the timerQ_head wakeup_time, delete it until new timerQ_head wakeup_time less then current time
	while(wakeup_time<=current_time){

	 	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(SUSPENDQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	 	//delete the timerQ_head
		PCB *pcb_temp=delete_timerQ(timerQ_head->pid);

		if(pcb_temp->process_state==SUSPEND){
			put_in_suspendQ(pcb_temp);
		}
		else{
			put_in_readyQ(pcb_temp);
		}

		pcb_temp=timerQ_head;

		READ_MODIFY(SUSPENDQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);


	 	if(pcb_temp==NULL){
			return;
		}
		wakeup_time = pcb_temp->wakeup_time;
	}

	delay_time=wakeup_time-current_time;

	CALL (MEM_WRITE( Z502TimerStart, &delay_time ));
}


//-------------------------------------
//better for debug, have a timeInterrupt

/************************************************************************
 *this method handle the time interrupt
 *when the time is come, the time interrupt will begin at other thread
 *the function is same as start_timer, for debug,
 *put them seperate because they maybe run at the same time
 *1. check the timerQ head wakeup_time
 *2. if wakeup_time less then current time, move it to other Q
 *3. until timerQ_head wakeup_time is bigger then current time
 *4. set timer again
 ***********************************************************************/
void timeInterrupt(){
	int delay_time, current_time, wakeup_time;
	CALL(MEM_READ(Z502ClockStatus, &current_time));


	if(timerQ_head==NULL){
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	wakeup_time=timerQ_head->wakeup_time;
	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	while(wakeup_time<=current_time){

	 	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(SUSPENDQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

		PCB *pcb_temp=delete_timerQ(timerQ_head->pid);
		if(pcb_temp->process_state==SUSPEND){
			put_in_suspendQ(pcb_temp);
		}
		else{
			put_in_readyQ(pcb_temp);
		}

		pcb_temp=timerQ_head;

		READ_MODIFY(SUSPENDQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	 	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);


		if(pcb_temp==NULL){
			return;
		}
		wakeup_time = pcb_temp->wakeup_time;
	}

	delay_time=wakeup_time-current_time;

	CALL (MEM_WRITE( Z502TimerStart, &delay_time ));

}

/************************************************************************
 * this method
 * 1. if no process stop all
 * 2. have process but readyQ is empty, waste time, until time interrupt
 * 3. get new current process from readyQ head
 * 4. change context
 ************************************************************************/

void dispatcher(){
	//if no process stop all
	if(totalQ_size==0){
		 Z502Halt();
	}

	//have process, but readyQ is emply, waste time, until time interrupt
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	while(readyQ_head==NULL){
		READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	    CALL();
	    READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	}
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	//get new current process from readyQ head
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	*current_pid=readyQ_head->pid;
	PCB * pcb= get_pcb_in_totalQ(*current_pid);
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	if (scheduler_print > 0 ) {
		sp_print();
		scheduler_print--;
	}
	//change context, to run new current process
	CALL(Z502SwitchContext(SWITCH_CONTEXT_SAVE_MODE, &(pcb->context)));
}


/**************************suspend part**********************************
 *1. pcb in readyQ, change state and move to suspendQ
 *2. pcb is in suspendQ, state is MSG_SUSPEND, change state to SUSPEND;
 *3. pcb is in suspendQ and state is suspend, error
 *4. pcb is in timerQ, change state to suspendQ, after time, move to suspendQ
 *5. cannot suspend itself
 *6. user wants to resume pcb
 *7. pcb is in timerQ,  change state to waiting
 *8. pcb is in suspendQ, state is MSG_SUSPEND, do noting
 *9. pcb is in suspendQ, state is suspend, if wait_msg true, change state to
 *   MSG_SUSPEND, and check the msg, if there is what it want, there is, move to readyQ
 *10. else pcb is not in suspendQ, error
 *************************************************************************/

/**************************************************************************
 * user calls suspend process
 * 1. pid is -1, suspend itself, error, in this os
 * 2. pid is current pid, error
 * 3. pid is illegal
 * 4. pcb is in readyQ, move to suspendQ
 * 5. pcb is in suspendQ, state is suspend, error, suspend again
 * 6. change the pcb state to suspend, even in timerQ or suspendQ, readyQ move to suspendQ
 *************************************************************************/
void os_suspend_process(long pid,long *error){
    //suspend self
    if(pid==-1){
    	*error = SUSPEND_SELF;
    	return;
    }

    if(pid==*current_pid){
    	*error=SUSPEND_SELF;
    	return;
    }

    PCB *pcb_temp = get_pcb_in_totalQ(pid);

    if(pcb_temp==NULL){
    	*error = SUSPEND_ID_ERROR;
    	return;
    }

    BOOL in_readyQ, in_suspendQ;

    //in suspendQ, process_state is suspend, error
    in_readyQ=is_in_Q(pid, readyQ_head,0);
    in_suspendQ=is_in_Q(pid, suspendQ_head,0);

    if(in_suspendQ==TRUE && pcb_temp->process_state==SUSPEND){
    	*error=SUSPEND_AGAIN_ERROR;
    	return;
    }

	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

    // in readyQ, move to suspendQ
    if(in_readyQ==TRUE){
    	delete_readyQ(pid);
    	put_in_suspendQ(pcb_temp);
    	*error=SUCCESS;
    }

	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

    //change state to suspend
    pcb_temp->process_state=SUSPEND;

}

/*************************************************************************
 * user calls resume process
 * 1. check the pcb null, id illegal
 * 2. is in suspendQ, wait msg, state change to MSG_SUSPEND
 * 3. is in suspendQ, wait msg is false, move to readyQ
 * 4. is in timerQ, change state to waiting
 * 5. is not in suspendQ, id error
 *************************************************************************/
void os_resume_process(long pid, long *error){
	PCB *pcb_temp = get_pcb_in_totalQ(pid);
	//check null
	if(pcb_temp == NULL){
		*error=RESUME_ID_ERROR;
		return;
	}

	BOOL in_timerQ, in_suspendQ;

	in_timerQ=is_in_Q(pid, timerQ_head,0);
	in_suspendQ=is_in_Q(pid, suspendQ_head,0);

	//not in suspendQ, id  error
	if(in_suspendQ==FALSE){
		*error=RESUME_ID_ERROR;
		return;
	}

	//in timerQ, change state to waiting
	if(in_timerQ==TRUE){
		pcb_temp->process_state=WAITING;
		*error = SUCCESS;
		return;
	}

	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	//in suspendQ, wait_msg is true, change state
	//wait_msg is false, move to readyQ
	if(in_suspendQ==TRUE){
		if(pcb_temp->wait_msg==TRUE){
			pcb_temp->process_state=MSG_SUSPEND;
			message *msg_temp1=find_msg(pcb_temp->wait_msg_from, pcb_temp->pid);
			message *msg_temp2=find_msg(pcb_temp->wait_msg_from,-1);
			message *msg_temp3 =NULL;
			if(pcb_temp->wait_msg_from==-1){
				msg_temp3=find_msg(-2, pcb_temp->pid);
			}
			if(msg_temp1!=NULL || msg_temp2!=NULL || msg_temp3!=NULL){

				delete_suspendQ(pid);
				put_in_readyQ(pcb_temp);

			}
			*error=SUCCESS;
		}
		else{
			delete_suspendQ(pid);
			put_in_readyQ(pcb_temp);
			*error=SUCCESS;
		}
	}

	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}



/******************************************************************************
 * message part
 * for this part, there are two user calls
 * 1. os_send_message
 * 2. os_receive_message
 * for my operating system
 * 1.1 if pcb A waits a message, change the pcb state to MSG_SUSPEND, put it in suspendQ
 * 1.2 at this state, if the user suspend A, change state to suspend
 * 1.3 user resume A, change state to MSG_SUSPEND
 * 1.4 the pcbs cannot listen to the message send means that they don't know there
 *     is message send to them until call them to receive
 * 1.5 the system will listen the message
 * 1.6 some process send message, put the message in msgQ
 * 1.7 system check whether is message is what the pcb in suspendQ with MSG_SUSPEND wait for
 * 1.8 if is, go to readQ
 * 1.9 since the condition, pcb A waits massage, state, MSG_SUSPEND, user suspend it, state SUSPEND
 *     , system will not listen message for it(only for MSG_SUSPEND)
 * 1.10 every time, resume pcb, if wait_msg is true, change state to MSG_SUSPEND
 *      check, whether the msg is in msgQ
 * 1.11 is here, move to readyQ
 **********************************************************************************************/

/********************************************************************************
 *user calls send message
 *1. set up a new msg
 *1. put in msgQ
 *1. if target pid is -1, broadcast
 *1. send to target
 *******************************************************************************/
void os_send_message(long target_pid, char * msg_buffer, long send_length, long *error){

	//check the illegal condition
	if(send_length>MAX_MSG_LENGTH){
		*error=MSG_ERROR;
		return;
	}

	if(is_in_Q(target_pid, NULL, 1)==FALSE && target_pid!=-1){
		*error=ID_ERROR;
		return;
	}

	if(msg_num>=MAX_MSG_NUM){
		*error=EXCEED_MSG_ERROR;
		return;
	}

	//create new message
	message *new_msg = malloc(sizeof(message));
	new_msg->msg_id= msg_num++;
	new_msg->target_pid=target_pid;
	new_msg->source_pid=*current_pid;
	new_msg-> send_length=send_length;
	strcpy(new_msg-> msg_buffer,msg_buffer);
	new_msg->next=NULL;
	put_in_msgQ(new_msg);


	if(target_pid==-1){
		//send message to everyone
	    broadcast_msg(new_msg, error);
	}
	else{
		//send message to specific one
	    send_msg_to_target(new_msg, target_pid, error);
	}
	*error=SUCCESS;


}


/********************************************************************
 *user calls recerive message, the current pid want to receive a msg
 *1. check the illegal condition
 *1. source_pid is -1, broadcast, everyone can receive
 *1. else the target pcb can receive
 **********************************************************************/
void os_receive_message(long source_pid, char* msg_buffer, long receive_length, long * send_length, long *sender_pid, long *error){
	PCB * pcb_temp = get_pcb_in_totalQ(source_pid);
	//illegal condition
	if(pcb_temp==NULL && source_pid!=-1){
		*error=ID_ERROR;
		return;
	}

	if(receive_length>MAX_MSG_LENGTH){
		*error=EXCEED_MSG_ERROR;
		return;
	}


	PCB *pcb_msg = get_pcb_in_totalQ(*current_pid);
	message *msg_temp;

	if(source_pid==-1){
		//check the msg, just by receiver, anyone is source pcb is ok
		msg_temp=find_msg(-2, pcb_msg->pid);

		//check if there is msg, one pcb send to anyone
		if(msg_temp == NULL){
			msg_temp=find_msg(-2, -1);
		}
	}
	else{
		//the msg is source_pid to target_pid(current_pid)
		msg_temp=find_msg(source_pid, pcb_msg->pid);

		//the msg is source_pid to everyone include target_pid
		if(msg_temp==NULL){
			msg_temp=find_msg(source_pid, -1);
		}
	}
	//if there is no such pcb, move to suspendQ, state MSG_SUSPEND
	//dispatcher,new process run
	if(msg_temp == NULL){
		pcb_msg->wait_msg=TRUE;
		pcb_msg->wait_msg_from=source_pid;
		pcb_msg->wait_msg_length=receive_length;
		pcb_msg->process_state=MSG_SUSPEND;

		delete_readyQ(*current_pid);
		put_in_suspendQ(pcb_msg);
		dispatcher();
		*error=SUCCESS;
	}

	//get the msg again
	if(source_pid==-1){
		msg_temp=find_msg(-2, pcb_msg->pid);
		if(msg_temp==NULL){
			msg_temp=find_msg(-2, -1);
		}
	}
	else{
		msg_temp=find_msg(source_pid, pcb_msg->pid);
		if(msg_temp==NULL){
			msg_temp=find_msg(source_pid, -1);
		}
	}

	//receive the msg
	*send_length=msg_temp->send_length;
	*sender_pid=msg_temp->source_pid;

	//change the pcb state to initial
	pcb_msg->wait_msg=FALSE;
	pcb_msg->wait_msg_from=0;
	pcb_msg->wait_msg_length=0;

	if(msg_temp->send_length>receive_length){
		*error=MSG_LENGTH_ERROR;
		return;
	}

	//delete the msg, since one pcb receive msg, delete it even it is broadcasted
	strcpy(msg_buffer, msg_temp->msg_buffer);
	delete_msgQ(msg_temp->msg_id);
	msg_num--;
	*error=SUCCESS;

}

/**************************************************************************
 *user calls send message to target pcb not broadcast
 *1. since pcb cannot listen, they don't know there is msg for them, do nothing
 *2. system listen the msg, check whether it is the MSG_SUSPEND pcb waits
 *3. it is, move to readyQ
 *4. do not need to change state, and pcb value, will change letter
 **************************************************************************/
void send_msg_to_target(message * msg, int target_pid,long *error){
	//os_send_message checked target_pid
	PCB *pcb_temp=get_pcb_in_totalQ(target_pid);

	//check whether the msg is the MSG_SUSPEND pcb wait
	if(is_in_Q(target_pid, suspendQ_head,0)==TRUE
			&&pcb_temp->process_state==MSG_SUSPEND
			&&(pcb_temp->wait_msg_from == *current_pid
					||pcb_temp->wait_msg_from == -1)){
			//it is move to readyQ
			delete_suspendQ(target_pid);
			put_in_readyQ(pcb_temp);
	}
	*error=SUCCESS;
}

/****************************************************************************
 *user calls send message to everyone
 *1. same as to target
 *1. if there are pcbs wait message and state is MSG_SUSPEND, whether msg is it want
 *1. it is move to readyQ
 ***************************************************************************/
void broadcast_msg(message * msg, long *error ){
	PCB *pcb_temp = pcb_wait_msg();
	//whether some pcb wait
	if(pcb_temp==NULL){
		return;
	}

	//there is pcb wait, whether the message it what it need
	if(pcb_temp->wait_msg_from==*current_pid || pcb_temp->wait_msg_from == -1){
		delete_suspendQ(pcb_temp->pid);
		put_in_readyQ(pcb_temp);

	}
	*error=SUCCESS;
}

/*******************************************************************
 * find_msg, if just use the target_pid set the source pid is -2
 * if any source pid, the source pid is -1
 * if any target pid, the target pid is -1
 *
 *******************************************************************/
message * find_msg(int source_pid, int target_pid){

	message *msg_temp=msgQ_head;

	if(source_pid==-2){
		while(msg_temp!=NULL){
			if(msg_temp->target_pid==target_pid)
				return msg_temp;
			msg_temp=msg_temp->next;
		}
	}
	else{
		while(msg_temp!=NULL){
			if(msg_temp->source_pid==source_pid && msg_temp->target_pid==target_pid){
				return msg_temp;
			}
			msg_temp=msg_temp->next;
		}
	}
	return NULL;
}

/***************************************************************************
 *this method check whether there is pcb waits message
 **************************************************************************/
PCB *pcb_wait_msg(){
	PCB *pcb_temp=suspendQ_head;
	if(suspendQ_head==NULL){
		return NULL;
	}

	while(pcb_temp!=NULL){
		if(pcb_temp->process_state==MSG_SUSPEND){
			return pcb_temp;
		}
		pcb_temp=pcb_temp->next;
	}
	return NULL;
}

/*********************************************************************
 * user calls change priority
 * 1. if pid is -1, pid is current_pid
 * 2. check the pid is legal
 * 3. check the priority is legal
 * 4. change priority
 * 5. is in readyQ, reschedule the pcb
 *********************************************************************/
void os_change_priority(long pid, long priority, long *error){
	//pid is -1, means current pcb
	if(pid==-1){
		pid=*current_pid;
	}

	PCB *pcb_temp = get_pcb_in_totalQ(pid);
	//check the pcb is here
	if(pcb_temp==NULL){
		*error = ID_ERROR;
		return;
	}

	//check the priority is legal
	if(priority>MAX_PRIORITY || priority<MIN_PRIORITY){
		*error = PRIORITY_ERROR;
		return;
	}
	//change priority
	pcb_temp->priority = priority;

	//if pcb is in readyQ
	//reschedule it
	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	if(is_in_Q(pid, readyQ_head,0)==TRUE){   //is_in_readyQ
		delete_readyQ(pid);
		put_in_readyQ(pcb_temp);
	}
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	*error=SUCCESS;

}

/**********************************************************
 *when user calls get pid by name
 *1. if the name is null, return current pid
 *2. check the name in pcb table
 **********************************************************/
void os_get_pid_by_name(char name[], long * pid_address, long * error ){

 	int i;

	//originally set error
	*error=NAME_ERROR;

	//name is empty, return the current pid
 	if(strlen(name)==0){
		*pid_address=(long)*current_pid;
		*error=SUCCESS;
		return;
	}

 	//check the name in the pcb table
 	for(i=0;i<totalQ_size;i++){
 		if(strcmp(totalQ[i]->process_name,name)==0){
 			*pid_address=(long)totalQ[i]->pid;
 			*error=SUCCESS;
 			return;
 		}
 	}

}


/*******************************************************************
 *get pcb in totalQ by pid
 *****************************************************************/
PCB* get_pcb_in_totalQ(int pid){
	int i;
	PCB * pcb_temp;
	pcb_temp = NULL;
	//seach all the pcb
	for(i=0;i<totalQ_size; i++){

		if( totalQ[i]->pid==pid){
			pcb_temp = totalQ[i];
		}
	}
	//no pcb with this pid
	return pcb_temp;
}

/*************************************************************************
 *put the msg in msgQ
 *************************************************************************/
void put_in_msgQ(message *msg){
	message* msg_temp= msgQ_head;
	if(msgQ_head==NULL){
			msgQ_head=msg;
			return;
		}

	while(msg_temp->next!=NULL){
		msg_temp=msg_temp->next;
	}
	msg_temp->next=msg;
}

/**********************************************************************
 *delete msg from msgQ same as other delete use id
 ***********************************************************************/
message *delete_msgQ(INT32 mid){
	message* msg_temp= msgQ_head;

	//check head
	if(msg_temp==NULL){
		return NULL;
	}

	//compare head
	if(msgQ_head->msg_id==mid){
		msgQ_head=msg_temp->next;
		msg_temp->next=NULL;
		return msg_temp;
	}

	//check in the middle
	while(msg_temp->next!=NULL){
		if(msg_temp->next->msg_id==mid){
			message *msg_delete=msg_temp->next;
			msg_temp->next = msg_delete->next;
			msg_delete->next=NULL;
			return msg_delete;
		}
		msg_temp=msg_temp->next;
	}
	return NULL;

}




/*********************************************
 * this method put the pcb into readyQ
 * by priority from small to large
 * 1. check the head is null
 * 2. compare the head and the pcb
 * 3. find a place insert the pcb
 * 4. the pcb is at the end of the Q
 **********************************************/
void put_in_readyQ(PCB * pcb){

    pcb->process_state = READY;

    //check head is null
	if(readyQ_head==NULL ){
		readyQ_head=pcb;

		return;
	}

	long priority_temp=pcb->priority;

	//check head priority with the pcb
	if (readyQ_head->priority > priority_temp){
		pcb->next=readyQ_head;
		readyQ_head=pcb;
		return;
	}

	PCB* pcb_temp= readyQ_head;

	//insert into the Q
	while(pcb_temp->next!=NULL){
		if(pcb_temp->next->priority > pcb->priority){
			pcb->next = pcb_temp->next;
			pcb_temp->next = pcb;
			return;
		}
		pcb_temp = pcb_temp->next;
	}

	//at the end of the Q
	pcb_temp->next = pcb;

}


/***********************************************************
 * this method delete pcb from readyQ
 * 1. check the head null
 * 2. compare the head and pcb
 * 3. delete at the middle or the end
 ***********************************************************/
PCB * delete_readyQ(int pid){
	PCB* pcb_temp= readyQ_head;
	PCB* pcb_delete;

	//check the head
	if(pcb_temp==NULL){
		return NULL;
	}

	//compare the head
	if(pcb_temp->pid==pid){
		readyQ_head=pcb_temp->next;
		pcb_temp->next=NULL;
		return pcb_temp;
	}

	//delete from middle and end
	while(pcb_temp->next!=NULL){
		if(pcb_temp->next->pid == pid){
			pcb_delete = pcb_temp->next;
			pcb_temp->next = pcb_delete->next;
			pcb_delete->next = NULL;
			return pcb_delete;
		}
		pcb_temp = pcb_temp->next;
	}

	return NULL;
}

/*****************************************
 * This method put the pcb into timerQ
 * by wakeup_time from small to large
 * 1. check the head is null
 * 2. compare the pcb with head
 * 3. insert the pcb in the middle
 * 4. put the pcb at the end of Q
 *******************************************/
void put_in_timerQ(PCB * pcb){
	pcb->process_state = WAITING;

	//check the head
	if(timerQ_head==NULL){
		timerQ_head=pcb;
		return;
	}

	//compare the head with pcb
	if(timerQ_head->wakeup_time > pcb->wakeup_time){
		pcb->next = timerQ_head;
		timerQ_head=pcb;
		return;
	}

	//insert the pcb in the middle
	PCB* pcb_temp= timerQ_head;
	while(pcb_temp->next!=NULL){
		if(pcb_temp->next->wakeup_time > pcb->wakeup_time){
			pcb->next = pcb_temp->next;
			pcb_temp->next = pcb;
			return;
		}
		pcb_temp=pcb_temp->next;
	}

	//put the pcb at the end of the Q
	pcb_temp->next = pcb;
}

/**********************************************************************
 * this method delete process from timerQ by its pid
 *1. chech the head null
 *2. compare the head
 *3. delete from middle or end
 *4. no, return NULL
 *********************************************************************/
PCB * delete_timerQ(int pid){
	PCB* pcb_temp= timerQ_head;
	PCB* pcb_delete;

	//check the head
	if(pcb_temp==NULL){
		return NULL;
	}

	//compare the head
	if(pcb_temp->pid==pid){
		timerQ_head=pcb_temp->next;
		pcb_temp->next=NULL;
		return pcb_temp;
	}

	//delete from middle and end
	while(pcb_temp->next!=NULL){
		if(pcb_temp->next->pid == pid){
			pcb_delete = pcb_temp->next;
			pcb_temp->next = pcb_delete->next;
			pcb_delete->next = NULL;
			return pcb_delete;
		}
		pcb_temp = pcb_temp->next;
	}

	return NULL;

}

/*******************************************************************
 *put the pcb in suspendQ
 *******************************************************************/
void put_in_suspendQ(PCB * pcb){
	PCB *pcb_temp=suspendQ_head;

	if(pcb_temp==NULL){
		suspendQ_head = pcb;
		return;
	}

	while(pcb_temp->next!=NULL){
		pcb_temp=pcb_temp->next;
	}

	pcb_temp->next=pcb;

}

/*******************************************************************
 *delete_suspendQ
 *******************************************************************/
PCB * delete_suspendQ(INT32 pid){

	PCB* pcb_temp= suspendQ_head;
	PCB* pcb_delete;

	//check the head
	if(pcb_temp==NULL){
		return NULL;
	}

	//compare the head
	if(pcb_temp->pid==pid){
		suspendQ_head=pcb_temp->next;
		pcb_temp->next=NULL;
		return pcb_temp;
	}

	//delete from middle and end
	while(pcb_temp->next!=NULL){
		if(pcb_temp->next->pid == pid){
			pcb_delete = pcb_temp->next;
			pcb_temp->next = pcb_delete->next;
			pcb_delete->next = NULL;
			return pcb_delete;
		}
		pcb_temp = pcb_temp->next;
	}

	return NULL;
}


/************************************************************
 * this method is used for readyQ, timerQ, suspendQ
 * check the pid is in the readyQ, timerQ, suspendQ or not
 * when state is number except 0 , check for totalQ
 ************************************************************/

BOOL is_in_Q(int pid, PCB *pcb, int state){
	PCB *pcb_temp=pcb;

	if(state == 0){
		while(pcb_temp!=NULL){
			if(pcb_temp->pid==pid){
				return TRUE;
			}
			pcb_temp=pcb_temp->next;
		}
		return FALSE;
	}

	else{
		int i;
		for(i=0;i<totalQ_size;i++){
			if( totalQ[i]->pid==pid){
				return TRUE;
			}
		}
		return FALSE;
	}
}



/******************************************************************
 *delete the pcb from totalQ
 **********************************************************************/
void delete_totalQ(int pid){
	int i,j;
	for(i=0; i<totalQ_size; i++){
		if(totalQ[i]->pid==pid){
			for(j=i; j<totalQ_size-1; j++){
				totalQ[j]=totalQ[j+1];
			}
			totalQ[totalQ_size-1]=NULL;
			totalQ_size--;
		}
	}
}


/********************************************************************
 *scheduler printer function
 ********************************************************************/

void sp_print(){

	INT32 current_time;

	SP_setup_action(SP_ACTION_MODE, "DISPATCH");

	MEM_READ(Z502ClockStatus, &current_time);

	SP_setup(SP_TIME_MODE, current_time);
	SP_setup(SP_RUNNING_MODE, *current_pid);

	READ_MODIFY(READYQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(TIMERQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	PCB * pcb_temp;

	//print ready queue
	pcb_temp = readyQ_head;
	while(pcb_temp != NULL){
		SP_setup(SP_READY_MODE,pcb_temp->pid);
		pcb_temp = pcb_temp->next;
	}

	// print timer queue
	pcb_temp = timerQ_head;
	while(pcb_temp != NULL){
		SP_setup(SP_TIMER_SUSPENDED_MODE,pcb_temp->pid);
		pcb_temp = pcb_temp->next;
	}

	// print suspend queue
	pcb_temp = suspendQ_head;
	while(pcb_temp != NULL){
		SP_setup(SP_PROCESS_SUSPENDED_MODE,pcb_temp->pid);
		pcb_temp = pcb_temp->next;
	}

	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(TIMERQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(SUSPENDQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	int i;
	for(i=1; i<=DISK_NUM; i++){
		READ_MODIFY(DISK_LOCK+i, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
		pcb_temp=disk_table[i];
		while(pcb_temp!=NULL){
			SP_setup(SP_DISK_SUSPENDED_MODE,pcb_temp->pid);
			pcb_temp=pcb_temp->next;
		}
		READ_MODIFY(DISK_LOCK+i, UNLOCK,  SUSPEND_UNTIL_LOCKED, &LockResult);
	}

	SP_print_line();

}


/*************************************************************************
 * the z502 translate the address into VirtualPageNumber & page_offset
 * when there is no mapping from that VirtualPage to Frame, interrupt occurs
 * and lead to this method
 * 1. z502.c already checked the VirtualPageNumber is valid or not
 * 2. get pcb by current_pid
 * 3. get pcb_page_table_item of the current pcb's pcb_page_table on VirtualPageNumber position
 * 4. if there is empty frame, map the page and the frame
 * 5. if there isn't empty frame, replace or page
 ****************************************************************************/

void page_fault(INT32 virtual_page_number){
	PCB *current_pcb;
	int victim_frame;
	int victim_frame_id;
	int victim_pid;
	int victim_page_id;
	int frame_id;
	char data [PGSIZE];

	if(virtual_page_number == PCB_PAGE_TABLE_LENGTH){
		printf("virtual_page_number error \n");
		Z502Halt();
	}

	current_pcb = get_pcb_in_totalQ(*current_pid);

	frame_id = get_empty_frame_id();

	//when frame full recur a page replacement may happen
	if(frame_id==FRAME_FULL){
		//get a victim frame which stores information of frame id, pid and page id
		victim_frame=find_victim_page();

	    //get frame id,the page id of which page current take this frame and pid
		victim_frame_id = (victim_frame & 0x0ff00000)>>20;
		victim_page_id = victim_frame&FRAME_PAGE_MASK;
		victim_pid = (victim_frame & FRAME_PID_MASK)>>16;

		//printf("current pid is %d, victim frame id is %d \n", *current_pid, victim_frame_id);
		//printf("current pid is %d, victim page id is %d \n", *current_pid, victim_page_id);
		//printf("current pid is %d, victim pid is %d \n", *current_pid, victim_pid);

		//replace page action begin
		replace_page(victim_pid,victim_page_id,victim_frame_id);

		//get the frame which this pid has requested
		frame_id = frame_count_table[*current_pid];
	}

	//if there is a free frame original or produced by page replacement
	if(frame_id != FRAME_FULL){
		//check the page has been moved to disk
		if(shadow_page_table[*current_pid][virtual_page_number]!=0){

			//get the page back to memory from disk
			os_disk_write_read((*current_pid+1), virtual_page_number , data, DISK_READ) ;
			Z502WritePhysicalMemory(frame_id, (char*) data);

			//set the status back
			shadow_page_table[*current_pid][virtual_page_number]=0;
		}

		//set page status
		current_pcb->pcb_page_table[virtual_page_number] = PTBL_VALID_BIT +frame_id;
		frame_table[frame_id]=current_pcb->pid<<16;
		frame_table[frame_id]+=virtual_page_number;
		//printf("current pid is %d, frame id is %d \n", *current_pid, frame_id);
	}


	else{
		printf("the frame_id error");
		Z502Halt();
	}

	if (memory_print > 0) {
		MP_print();
		memory_print--;
	}

	return;

}

/********************************************************************
 * in this method, the os looking for a page to replace based on FIFO
 ********************************************************************/
INT32 find_victim_page(){

	int victim_frame;
	victim_frame=frame_table[frame_count];
	//add frame id into it
	victim_frame+=(frame_count<<20);

	//frame_table[frame_count]=FRAME_EMPTY;  //if have this line, will mess the disk operation

	//record which frame it request
	frame_count_table[*current_pid]=frame_count;

	//from 0 to 63
	frame_count++;
	if(frame_count==64){
		frame_count=0;
	}
	return victim_frame;

}

void replace_page(int victim_pid, int victim_page_id, int victim_frame_id){
	PCB * victim_pcb;
	char data [PGSIZE];

	//get the pcb which has a page to be replaced
	victim_pcb=get_pcb_in_totalQ(victim_pid);

	//change the status of replaced page of that pcb
	victim_pcb->pcb_page_table[victim_page_id]=(victim_pcb->pcb_page_table[victim_page_id])&0xffff7fff;

	//move the replaced page to disk to store
	Z502ReadPhysicalMemory(victim_frame_id, (char*) data);
	os_disk_write_read((victim_pid+1), victim_page_id , data, DISK_WRITE) ;

	//record the page has been replaced
	shadow_page_table[victim_pid][victim_page_id]=1;
}

/****************************************************************
 * check frame_table, return the empty frame number
 ****************************************************************/
int get_empty_frame_id(){
	int i;
	for(i=0; i<TOTAL_FRAME_NUMBER; i++){
		if(frame_table[i]==FRAME_EMPTY){
			return i;
		}
	}
	return FRAME_FULL;
}

/******************************************************************
 * SYSTEM CALL DISK_WRITE & DISK_READ
 * 1. write the pcb information
 * 2. put the pcb from readyQ to diskQ
 * 3. get the pcb need to access the disk from diskQ
 * 4. if the disk is free, write it
 * 5. if is busy, return
 *****************************************************************/
void os_disk_write_read(long disk_id, long disk_sector, char* data_write_read, int disk_write_read){
	PCB *pcb_temp;

	READ_MODIFY(READYQ_LOCK, LOCK,  SUSPEND_UNTIL_LOCKED, &LockResult);

	// get the pcb temp
	pcb_temp=get_pcb_in_totalQ(*current_pid);

	//set pcb disk part
	pcb_temp->disk_id=disk_id;
	pcb_temp->disk_sector=disk_sector;
	pcb_temp->disk_operation_data=data_write_read;
	pcb_temp->disk_operation=disk_write_read;

	READ_MODIFY(DISK_LOCK+disk_id, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	//move the pcb from readyQ to diskQ
	delete_readyQ(pcb_temp->pid);
	put_in_diskQ(pcb_temp, disk_id);

	//start the disk operation
	set_disk(disk_id);

	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(DISK_LOCK+disk_id, UNLOCK,  SUSPEND_UNTIL_LOCKED, &LockResult);

	dispatcher();
}


/*********************************************************************
 *this method/interrupt will work when the operation on disk write or read is done
 *1. put the pcb from diskQ to readyQ
 *2. get the next pcb in that diskQ
 *3. do the new disk operation
 *******************************************************************/
void disk_operation_done(int disk_id){
	PCB *pcb_temp;

	READ_MODIFY(DISK_LOCK+disk_id, LOCK, SUSPEND_UNTIL_LOCKED, &LockResult);

	//interrrupt, the pcb in this disk_id is done
	pcb_temp=delete_diskQ(disk_id);

	//set back the status in pcb
	pcb_temp->disk_id=0;
	pcb_temp->disk_sector=0;
	pcb_temp->disk_operation_data=NULL;
	pcb_temp->disk_operation=-1;

	READ_MODIFY(READYQ_LOCK, LOCK,  SUSPEND_UNTIL_LOCKED, &LockResult);

	//move the pcb back to readyQ
	put_in_readyQ(pcb_temp);

	//start the disk operation
	set_disk(disk_id);

	READ_MODIFY(DISK_LOCK+disk_id, UNLOCK,  SUSPEND_UNTIL_LOCKED, &LockResult);
	READ_MODIFY(READYQ_LOCK, UNLOCK, SUSPEND_UNTIL_LOCKED, &LockResult);
}

/******************************************************************
 * this method is about the operation on disk
 * 1. get the first pcb in that diskQ
 * 2. check the pcb is null / the diskQ is empty or not
 * 3. if is not empty
 * 4. check the disk is busy or not
 * 5. if is not busy
 * 6. set the disk and begin the disk read write operation
 ******************************************************************/
void set_disk(int disk_id){
	int disk_status;

	//get the first pcb in that diskQ
	PCB *pcb_temp = disk_table[disk_id];

	//if is not null, means the diskQ is not empty
	if(pcb_temp!=NULL){
		//set disk id and check the status
		MEM_WRITE(Z502DiskSetID, &pcb_temp->disk_id);
		MEM_READ(Z502DiskStatus, &disk_status);

		//if the disk is free
		if (disk_status == DEVICE_FREE)
		{
			//disk operation read or write begin
			MEM_WRITE(Z502DiskSetAction, &pcb_temp->disk_operation);
			MEM_WRITE(Z502DiskSetSector, &pcb_temp->disk_sector);
			MEM_WRITE(Z502DiskSetBuffer, pcb_temp->disk_operation_data);

			// status is set to 0 to start the disk
			disk_status = 0;
			MEM_WRITE(Z502DiskStart, &disk_status);
		}
	}
}

PCB* delete_diskQ(int disk_id){
	//in my design the disk id starts from 1 to 8
	if(disk_id == 0){
		printf("error disk_id occurs!\n");
		Z502Halt();
	}

	//get the first pcb in this diskQ
	PCB *pcb_temp=disk_table[disk_id];
	disk_table[disk_id]=pcb_temp->next;
	pcb_temp->next=NULL;

	return pcb_temp;
}

void put_in_diskQ(PCB* pcb, int disk_id){
	PCB *pcb_temp;

	//in my design the disk id starts from 1 to 8
	if(disk_id == 0){
		printf("error disk_id occurs!\n");
		Z502Halt();
	}

	//get the fist pcb in that diskQ
	pcb_temp=disk_table[disk_id];

	//if the diskQ is empty
	if(pcb_temp==NULL){
		disk_table[disk_id]=pcb;
		return;
	}

	//if the diskQ is not empty, put the new pcb at the end of the Q
	while(pcb_temp->next != NULL){
		pcb_temp=pcb_temp->next;
	}

	pcb_temp->next = pcb;
	pcb->next = NULL;
}

// print memory information
void MP_print(void){

	int frame_item;
	int page_item;
	int pid, frame_id, page_id;
	int print_info;
	PCB * pcb_temp;

	// loop through each frame
	for(frame_id=0;frame_id<64;frame_id++)
	{
		print_info = 0;

		frame_item = frame_table[frame_id];

		// get process ID
		pid = frame_item & FRAME_PID_MASK;

		// get page ID
		page_id = frame_item & FRAME_PAGE_MASK;

		// get process
		pcb_temp = get_pcb_in_totalQ(pid);

		page_item = pcb_temp->pcb_page_table[page_id];

		//print VMR, so v is 4, m is 2, r is 1
		// reference bit
		if ((page_item & REFERENCE_MASK) == 1) {
			print_info += 1;
		}

		// modified bit
		if ((page_item & MODIFIED_MASK) == 1) {
			print_info += 2;
		}

		// valid bit
		if ((page_item & VALID_MASK) == 1) {
			print_info += 4;
		}

		MP_setup(frame_id,pid,page_id,print_info);
	}

	// print out memory status
	MP_print_line();

}


