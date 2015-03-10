/************************************************************************

        This code forms the base of the operating system you will
        build.  It has only the barest rudiments of what you will
        eventually construct; yet it contains the interfaces that
        allow test.c and z502.c to be successfully built together.

        Revision History:
        1.0 August 1990
        1.1 December 1990: Portability attempted.
        1.3 July     1992: More Portability enhancements.
                           Add call to sample_code.
        1.4 December 1992: Limit (temporarily) printout in
                           interrupt handler.  More portability.
        2.0 January  2000: A number of small changes.
        2.1 May      2001: Bug fixes and clear STAT_VECTOR
        2.2 July     2002: Make code appropriate for undergrads.
                           Default program start is in test0.
        3.0 August   2004: Modified to support memory mapped IO
        3.1 August   2004: hardware interrupt runs on separate thread
        3.11 August  2004: Support for OS level locking
	4.0  July    2013: Major portions rewritten to support multiple threads
************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"

// declarations
#include 			 <stdlib.h>

#include             "os_pcb.h"
#include             "os_process.h"


extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;



// These loacations are global and define information about the page table
extern UINT16        *Z502_PAGE_TBL_ADDR;
extern INT16         Z502_PAGE_TBL_LENGTH;

extern void          *TO_VECTOR [];

char                 *call_names[] = { "mem_read ", "mem_write",
                            "read_mod ", "get_time ", "sleep    ",
                            "get_pid  ", "create   ", "term_proc",
                            "suspend  ", "resume   ", "ch_prior ",
                            "send     ", "receive  ", "disk_read",
                            "disk_wrt ", "def_sh_ar" };


/************************************************************************
    INTERRUPT_HANDLER
        When the Z502 gets a hardware interrupt, it transfers control to
        this routine in the OS.
************************************************************************/
void    interrupt_handler( void ) {
    INT32              device_id;
    INT32              status;
    INT32              Index = 0;
    static BOOL        remove_this_in_your_code = TRUE;   /** TEMP **/
    static INT32       how_many_interrupt_entries = 0;    /** TEMP **/



    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );


        // Set this device as target of our query
        MEM_WRITE(Z502InterruptDevice, &device_id );
        // Now read the status of this device
        MEM_READ(Z502InterruptStatus, &status );

    if(device_id == TIMER_INTERRUPT){
		timeInterrupt();

	    MEM_WRITE(Z502InterruptClear, &Index );

	    MEM_READ(Z502InterruptDevice, &device_id );
	    return;
    }

    if(device_id >= DISK_INTERRUPT && device_id< (DISK_INTERRUPT+DISK_NUM)){
    	disk_operation_done(device_id-DISK_INTERRUPT+1);

        MEM_WRITE(Z502InterruptClear, &Index );

        MEM_READ(Z502InterruptDevice, &device_id );
        return;
   	}

    printf("other kind of interrupt.\n");


// ***********************************************************************

    // Clear out this device - we're done with it
}
/* End of interrupt_handler */



/************************************************************************
    FAULT_HANDLER
        The beginning of the OS502.  Used to receive hardware faults.
************************************************************************/

void    fault_handler( void )
    {
    INT32       device_id;
    INT32       status;
    INT32       Index = 0;

    // Get cause of interrupt
    MEM_READ(Z502InterruptDevice, &device_id );
    // Set this device as target of our query
    MEM_WRITE(Z502InterruptDevice, &device_id );
    // Now read the status of this device
    MEM_READ(Z502InterruptStatus, &status );


    if(device_id == INVALID_MEMORY){
    		page_fault(status);
    }
    else{
    	printf("other kind of fault.\n");
    }
    // Clear out this device - we're done with it
    MEM_WRITE(Z502InterruptClear, &Index );


}                                       /* End of fault_handler */

/************************************************************************
    SVC
        The beginning of the OS502.  Used to receive software interrupts.
        All system calls come to this point in the code and are to be
        handled by the student written code here.
        The variable do_print is designed to print out the data for the
        incoming calls, but does so only for the first ten calls.  This
        allows the user to see what's happening, but doesn't overwhelm
        with the amount of data.
************************************************************************/

void    svc( SYSTEM_CALL_DATA *SystemCallData ) {
    short               call_type;
    short               i;
    int                current_time;
    int                disk_write_read;


    call_type = (short)SystemCallData->SystemCallNumber;
    if ( base_print > 0 ) {
        printf( "SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++ ){
        	 //Value = (long)*SystemCallData->Argument[i];
             printf( "Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
             (unsigned long )SystemCallData->Argument[i],
             (unsigned long )SystemCallData->Argument[i]);
        }
    base_print--;
    }

    switch(call_type){
    //Get time service call
     case SYSNUM_GET_TIME_OF_DAY:
     	CALL(MEM_READ(Z502ClockStatus, &current_time));
     	*SystemCallData->Argument[0] = current_time;
     	break;

     //Sleep system call
     case SYSNUM_SLEEP:
     	CALL(os_sleep((long)SystemCallData->Argument[0]));
     	break;

     case SYSNUM_CREATE_PROCESS:
        	CALL(os_create_process((char *)SystemCallData->Argument[0], (void *)SystemCallData->Argument[1],  (long)SystemCallData->Argument[2], (long*)SystemCallData->Argument[3], NOTRUN,SystemCallData->Argument[4]));
        	break;

    //terminate system call
     case SYSNUM_TERMINATE_PROCESS:
     	CALL(os_terminate_process((long)SystemCallData->Argument[0], SystemCallData->Argument[1]));
     	break;

     case SYSNUM_GET_PROCESS_ID:
     	CALL(os_get_pid_by_name((char *)SystemCallData->Argument[0],  SystemCallData->Argument[1], SystemCallData->Argument[2]));
     	break;

     case SYSNUM_SUSPEND_PROCESS:
     	CALL(os_suspend_process((long)SystemCallData->Argument[0],SystemCallData->Argument[1]));
     	break;

     case SYSNUM_RESUME_PROCESS:
     	CALL(os_resume_process((long)SystemCallData->Argument[0],SystemCallData->Argument[1]));
     	break;

     case SYSNUM_CHANGE_PRIORITY:
     	CALL(os_change_priority((long)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],SystemCallData->Argument[2]));
     	break;

     case SYSNUM_SEND_MESSAGE:
     	CALL(os_send_message((long)SystemCallData->Argument[0],(char *)SystemCallData->Argument[1],(long)SystemCallData->Argument[2],SystemCallData->Argument[3]));
     	break;
     case SYSNUM_RECEIVE_MESSAGE:
     	CALL(os_receive_message((long)SystemCallData->Argument[0],(char *)SystemCallData->Argument[1],(long)SystemCallData->Argument[2],(long*)SystemCallData->Argument[3],(long*)SystemCallData->Argument[4],SystemCallData->Argument[5]));
     	break;

 	case SYSNUM_DISK_WRITE:
 		disk_write_read=DISK_WRITE;
 		CALL(os_disk_write_read((long)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],(char *)SystemCallData->Argument[2], disk_write_read));
 		break;
 	case SYSNUM_DISK_READ:
 		disk_write_read=DISK_READ;
 		CALL(os_disk_write_read((long)SystemCallData->Argument[0],(long)SystemCallData->Argument[1],(char *)SystemCallData->Argument[2],disk_write_read));
 		break;

     default:
     	printf("ERROR! call_type not recognized! \n");
     	printf("Call_type is = %i\n", call_type);
    }
//******************************************************************************

}                                               // End of svc



/************************************************************************
    osInit
        This is the first routine called after the simulation begins.  This
        is equivalent to boot code.  All the initial OS components can be
        defined and initialized here.
************************************************************************/

void    osInit( int argc, char *argv[]  ) {
    void                *next_context;
    INT32               i;
    long                error;

    long				init_pid=0;

        timerQ_head=NULL;
        readyQ_head=NULL;

        frame_count=0;

        current_pid=(int *)&init_pid;
        totalQ_size=0;
        pid_num=0;
        msg_num=0;
        totalQ = malloc(sizeof(PCB*) * MAX_PCB_NUM);

        //initial queue and tables
        for(i=0;i<=MAX_PCB_NUM;i++){
        	totalQ[i]=malloc(sizeof(PCB*));
        	totalQ[i]->pid=-100;
        }

        for(i=0; i<PHYS_MEM_PGS; i++){
        	frame_table[i]=FRAME_EMPTY;
        }

        for(i=0; i<DISK_NUM; i++){
        	disk_table[i]=NULL;
        }

        int j;
        for(i=0; i<MAX_NUMBER_OF_DISKS; i++){
        	for(j=0; j<NUM_LOGICAL_SECTORS;j++){
        		shadow_page_table[i][j]=0;
        	}
        }

        for(i=0; i<MAX_NUMBER_OF_DISKS; i++){
        	frame_count_table[i]=0;
        }


    /* Demonstrates how calling arguments are passed thru to here       */

    printf( "Program called with %d arguments:", argc );
    for ( i = 0; i < argc; i++ )
        printf( " %s", argv[i] );
    printf( "\n" );
    printf( "Calling with argument 'sample' executes the sample program.\n" );

    /*          Setup so handlers will come to code in base.c           */

    TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR]   = (void *)interrupt_handler;
    TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR] = (void *)fault_handler;
    TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR]  = (void *)svc;

    /*  Determine if the switch was set, and if so go to demo routine.  */

    if (( argc > 1 ) && ( strcmp( argv[1], "sample" ) == 0 ) ) {
        Z502MakeContext( &next_context, (void *)sample_code, KERNEL_MODE );
        Z502SwitchContext( SWITCH_CONTEXT_KILL_MODE, &next_context );
    }                   /* This routine should never return!!           */


    /*  This should be done by a "os_make_process" routine, so that
        test0 runs on a process recognized by the operating system.    */

    if (( argc > 1 ) && ( strcmp( argv[1], "test0" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
    	CALL(os_create_process("test0", (void *)test0, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
       }

    if (( argc > 1 ) && ( strcmp( argv[1], "test1a" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
    	CALL(os_create_process("test1a", (void *)test1a, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
       }

    if (( argc > 1 ) && ( strcmp( argv[1], "test1b" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
        CALL(os_create_process("test1b", (void *)test1b, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
           }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1c" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
        CALL(os_create_process("test1c", (void *)test1c, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
    }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1d" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
        CALL(os_create_process("test1d", (void *)test1d, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
        }

    if (( argc > 1 ) && ( strcmp( argv[1], "test1e" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
        CALL(os_create_process("test1e", (void *)test1e, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
          }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1f" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
		CALL(os_create_process("test1f", (void *)test1f, HIGHEST_PRIORITY, (long*)current_pid, RUN, &error));
	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1g" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
		CALL(os_create_process("test1g", (void *)test1g, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
	}
    if (( argc > 1 ) && ( strcmp( argv[1], "test1h" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
	    CALL(os_create_process("test1h", (void *)test1h, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
		   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1i" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
		CALL(os_create_process("test1i", (void *)test1i, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1j" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
		CALL(os_create_process("test1j", (void *)test1j, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1k" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
   		CALL(os_create_process("test1k", (void *)test1k, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
   	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test1l" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = FULL_PRINTING;
	    CALL(os_create_process("test1l", (void *)test1l, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test2a" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
    	memory_print = FULL_PRINTING;
    	CALL(os_create_process("test2a", (void *)test2a, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
       }

    if (( argc > 1 ) && ( strcmp( argv[1], "test2b" ) == 0 ) ) {
    	base_print = FULL_PRINTING;
    	scheduler_print = NO_PRINTING;
    	memory_print = FULL_PRINTING;
        CALL(os_create_process("test2b", (void *)test2b, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
           }
    if (( argc > 1 ) && ( strcmp( argv[1], "test2c" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = FULL_PRINTING;
    	memory_print = NO_PRINTING;
        CALL(os_create_process("test2c", (void *)test2c, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
    }
    if (( argc > 1 ) && ( strcmp( argv[1], "test2d" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = LIMITED_PRINTING;
    	memory_print = NO_PRINTING;
        CALL(os_create_process("test2d", (void *)test2d, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
        }

    if (( argc > 1 ) && ( strcmp( argv[1], "test2e" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = LIMITED_PRINTING;
    	memory_print = LIMITED_PRINTING;
        CALL(os_create_process("test2e", (void *)test2e, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
          }
    if (( argc > 1 ) && ( strcmp( argv[1], "test2f" ) == 0 ) ) {
    	base_print = LIMITED_PRINTING;
    	scheduler_print = NO_PRINTING;
    	memory_print = LIMITED_PRINTING;
		CALL(os_create_process("test2f", (void *)test2f, HIGHEST_PRIORITY, (long*)current_pid, RUN, &error));
	   }
    if (( argc > 1 ) && ( strcmp( argv[1], "test2g" ) == 0 ) ) {
    	base_print = NO_PRINTING;
    	scheduler_print = NO_PRINTING;
    	memory_print = NO_PRINTING;
        CALL(os_create_process("test2g", (void *)test2g, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
        }

    if (( argc > 1 ) && ( strcmp( argv[1], "test2h" ) == 0 ) ) {
        CALL(os_create_process("test2h", (void *)test2h, HIGH_PRIORITY, (long*)current_pid, RUN, &error));
          }



    CALL(os_create_process("test1c", (void *)test2d, HIGH_PRIORITY, (long*)current_pid, RUN, &error));

//*******************************************************************************************
}                                               // End of osInit
