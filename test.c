/************************************************************************

 test.c

 These programs are designed to test the OS502 functionality

 Read Appendix B about test programs and Appendix C concerning
 system calls when attempting to understand these programs.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Tests cleaned up; 1b, 1e - 1k added
                    Portability attempted.
 1.2 December 1991: Still working on portabililty.
 1.3 July     1992: tests1i/1j made re-entrant.
 1.4 December 1992: Minor changes - portability
                    tests2c/2d added.  2f/2g rewritten
 1.5 August   1993: Produced new test2g to replace old
                    2g that did a swapping test.
 1.6 June     1995: Test Cleanup.
 1.7 June     1999: Add test0, minor fixes.
 2.0 January  2000: Lots of small changes & bug fixes
 2.1 March    2001: Minor Bugfixes.
                    Rewrote GetSkewedRandomNumber
 2.2 July     2002: Make appropriate for undergrads
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 3.13 November 2004: Minor fix defining USER
 3.41 August  2009: Additional work for multiprocessor + 64 bit
 3.53 November 2011: Changed test2c so data structure used
                     ints (4 bytes) rather than longs.
 3.61 November 2012: Fixed a number of bugs in test2g and test2gx
                     (There are probably many more)
 3.70 December 2012: Rename test2g to test2h - it still does
                     shared memory.  Define a new test2g that runs
                     multiple copies of test2f.
 4.03 December 2013: Changes to test 2e and 2f.
 4.10 December 2013: Changes to test 2g regarding a clean ending.
      July     2014: Numerous small changes to tests.
                     Major rwrite of Test2h
 4.11 November 2014: Bugfix to test2c and test2e
 4.12 November 2014: Bugfix to test2e
 ************************************************************************/

#define          USER
#include         "global.h"
#include         "protos.h"
#include         "syscalls.h"

#include         "stdio.h"
#include         "string.h"
#include         "stdlib.h"
#include         "math.h"

INT16 Z502_PROGRAM_COUNTER;

extern long Z502_REG1;
extern long Z502_REG2;
extern long Z502_REG3;
extern long Z502_REG4;
extern long Z502_REG5;
extern long Z502_REG6;
extern long Z502_REG7;
extern long Z502_REG8;
extern long Z502_REG9;
extern INT16 Z502_MODE;

/*      Prototypes for internally called routines.                  */

void   test1x(void);
void   test1j_echo(void);
void   test2hx(void);
void   ErrorExpected(INT32, char[]);
void   SuccessExpected(INT32, char[]);
void   GetRandomNumber( long *, long );
void   GetSkewedRandomNumber( long *, long );
void   Test2f_Statistics(int Pid);

/**************************************************************************

 Test0

 Exercises GET_TIME_OF_DAY and TERMINATE_PROCESS

 Z502_REG1              Time returned from call
 Z502_REG9              Error returned

 **************************************************************************/

void test0(void) {
    printf("This is Release %s:  Test 0\n", CURRENT_REL);
    GET_TIME_OF_DAY(&Z502_REG1);

    printf("Time of day is %ld\n", Z502_REG1);
    TERMINATE_PROCESS(-1, &Z502_REG9);

    // We should never get to this line since the TERMINATE_PROCESS call
    // should cause the program to end.
    printf("ERROR: Test should be terminated but isn't.\n");
}                                               // End of test0

/**************************************************************************
 Test1a

 Exercises GET_TIME_OF_DAY and SLEEP and TERMINATE_PROCESS
 What should happen here is that the difference between the time1 and time2
 will be GREATER than SleepTime.  This is because a timer interrupt takes
 AT LEAST the time specified.

 Z502_REG9              Error returned

 **************************************************************************/

void test1a(void) {
    static long    SleepTime = 100;
    static INT32   time1, time2;

    printf("This is Release %s:  Test 1a\n", CURRENT_REL);
    GET_TIME_OF_DAY(&time1);

    SLEEP(SleepTime);

    GET_TIME_OF_DAY(&time2);

    printf("Sleep Time = %ld, elapsed time= %d\n", SleepTime, time2 - time1);
    TERMINATE_PROCESS(-1, &Z502_REG9);

    printf("ERROR: Test should be terminated but isn't.\n");

} /* End of test1a    */

/**************************************************************************
 Test1b

 Exercises the CREATE_PROCESS and GET_PROCESS_ID  commands.

 This test tries lots of different inputs for create_process.
 In particular, there are tests for each of the following:

 1. use of illegal priorities
 2. use of a process name of an already existing process.
 3. creation of a LARGE number of processes, showing that
 there is a limit somewhere ( you run out of some
 resource ) in which case you take appropriate action.

 Test the following items for get_process_id:

 1. Various legal process id inputs.
 2. An illegal/non-existant name.

 Z502_REG1, _2          Used as return of process id's.
 Z502_REG3              Cntr of # of processes created.
 Z502_REG9              Used as return of error code.
 **************************************************************************/

#define         ILLEGAL_PRIORITY                -3
#define         LEGAL_PRIORITY                  10

void test1b(void) {
    static char process_name[16];

    // Try to create a process with an illegal priority.
    printf("This is Release %s:  Test 1b\n", CURRENT_REL);
    CREATE_PROCESS("test1b_a", test1x, ILLEGAL_PRIORITY, &Z502_REG1,
            &Z502_REG9);
    ErrorExpected(Z502_REG9, "CREATE_PROCESS");

    // Create two processes with same name - 1st succeeds, 2nd fails
    // Then terminate the process that has been created
    CREATE_PROCESS("two_the_same", test1x, LEGAL_PRIORITY, &Z502_REG2,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");
    CREATE_PROCESS("two_the_same", test1x, LEGAL_PRIORITY, &Z502_REG1,
            &Z502_REG9);
    ErrorExpected(Z502_REG9, "CREATE_PROCESS");
    TERMINATE_PROCESS(Z502_REG2, &Z502_REG9);
    SuccessExpected(Z502_REG9, "TERMINATE_PROCESS");

    // Loop until an error is found on the create_process.
    // Since the call itself is legal, we must get an error
    // because we exceed some limit.
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        Z502_REG3++; /* Generate next unique program name*/
        sprintf(process_name, "Test1b_%ld", Z502_REG3);
        printf("Creating process \"%s\"\n", process_name);
        CREATE_PROCESS(process_name, test1x, LEGAL_PRIORITY, &Z502_REG1,
                &Z502_REG9);
    }

    //  When we get here, we've created all the processes we can.
    //  So the OS should have given us an error
    ErrorExpected(Z502_REG9, "CREATE_PROCESS");
    printf("%ld processes were created in all.\n", Z502_REG3);

    //      Now test the call GET_PROCESS_ID for ourselves
    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);     // Legal
    SuccessExpected(Z502_REG9, "GET_PROCESS_ID");
    printf("The PID of this process is %ld\n", Z502_REG2);

    // Try GET_PROCESS_ID on another existing process
    strcpy(process_name, "Test1b_1");
    GET_PROCESS_ID(process_name, &Z502_REG1, &Z502_REG9); /* Legal */
    SuccessExpected(Z502_REG9, "GET_PROCESS_ID");
    printf("The PID of target process is %ld\n", Z502_REG1);

    // Try GET_PROCESS_ID on a non-existing process
    GET_PROCESS_ID("bogus_name", &Z502_REG1, &Z502_REG9); // Illegal
    ErrorExpected(Z502_REG9, "GET_PROCESS_ID");

    GET_TIME_OF_DAY(&Z502_REG4);
    printf("Test1b, PID %ld, Ends at Time %ld\n", Z502_REG2, Z502_REG4);
    TERMINATE_PROCESS(-2, &Z502_REG9)

}                                                  // End of test1b

/**************************************************************************
 Test1c

 Tests multiple copies of test1x running simultaneously.
 Test1c runs these with the same priority in order to show FCFS scheduling
 behavior; Test1d uses different priorities in order to show priority
 scheduling.

 WARNING:  This test assumes tests 1a - 1b run successfully

 Z502_REG1, 2, 3, 4, 5  Used as return of process id's.
 Z502_REG6              Return of PID on GET_PROCESS_ID
 Z502_REG9              Used as return of error code.

 **************************************************************************/
#define         PRIORITY1C              10

void test1c(void) {
    static long   sleep_time = 1000;

    printf("This is Release %s:  Test 1c\n", CURRENT_REL);
    CREATE_PROCESS("test1c_a", test1x, PRIORITY1C, &Z502_REG1, &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    CREATE_PROCESS("test1c_b", test1x, PRIORITY1C, &Z502_REG2, &Z502_REG9);

    CREATE_PROCESS("test1c_c", test1x, PRIORITY1C, &Z502_REG3, &Z502_REG9);

    CREATE_PROCESS("test1c_d", test1x, PRIORITY1C, &Z502_REG4, &Z502_REG9);

    CREATE_PROCESS("test1c_e", test1x, PRIORITY1C, &Z502_REG5, &Z502_REG9);

    // Now we sleep, see if one of the five processes has terminated, and
    // continue the cycle until one of them is gone.  This allows the test1x
    // processes to exhibit scheduling.
    // We know that the process terminated when we do a GET_PROCESS_ID and
    // receive an error on the system call.

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test1c_e", &Z502_REG6, &Z502_REG9);
    }

    TERMINATE_PROCESS(-2, &Z502_REG9); /* Terminate all */

}                                                     // End test1c

/**************************************************************************
 Test 1d

 Tests multiple copies of test1x running simultaneously.
 Test1c runs these with the same priority in order to show
 FCFS scheduling behavior; Test1d uses different priorities
 in order to show priority scheduling.

 WARNING:  This test assumes tests 1a - 1b run successfully

 Z502_REG1, 2, 3, 4, 5  Used as return of process id's.
 Z502_REG6              Return of PID on GET_PROCESS_ID
 Z502_REG9              Used as return of error code.

 **************************************************************************/

#define         PRIORITY1               10
#define         PRIORITY2               11
#define         PRIORITY3               11
#define         PRIORITY4               90
#define         PRIORITY5               40

void test1d(void) {
    static long   sleep_time = 1000;

    printf("This is Release %s:  Test 1d\n", CURRENT_REL);
    CREATE_PROCESS("test1d_1", test1x, PRIORITY1, &Z502_REG1, &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    CREATE_PROCESS("test1d_2", test1x, PRIORITY2, &Z502_REG2, &Z502_REG9);

    CREATE_PROCESS("test1d_3", test1x, PRIORITY3, &Z502_REG3, &Z502_REG9);

    CREATE_PROCESS("test1d_4", test1x, PRIORITY4, &Z502_REG4, &Z502_REG9);

    CREATE_PROCESS("test1d_5", test1x, PRIORITY5, &Z502_REG5, &Z502_REG9);

    // Now we sleep, see if one of the five processes has terminated, and
    // continue the cycle until one of them is gone.  This allows the test1x
    // processes to exhibit scheduling.
    // We know that the process terminated when we do a GET_PROCESS_ID and
    // receive an error on the system call.

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test1d_4", &Z502_REG6, &Z502_REG9);
    }

    TERMINATE_PROCESS(-2, &Z502_REG9);

}                                                 // End test1d

/**************************************************************************
 Test 1e

 Exercises the SUSPEND_PROCESS and RESUME_PROCESS commands

 This test should try lots of different inputs for suspend and resume.
 In particular, there should be tests for each of the following:

 1. use of illegal process id.
 2. what happens when you suspend yourself - is it legal?  The answer
 to this depends on the OS architecture and is up to the developer.
 3. suspending an already suspended process.
 4. resuming a process that's not suspended.

 there are probably lots of other conditions possible.

 Z502_REG1              Target process ID
 Z502_REG2              OUR process ID
 Z502_REG9              Error returned

 **************************************************************************/
#define         LEGAL_PRIORITY_1E               10

void test1e(void) {

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);
    printf("Release %s:Test 1e: Pid %ld\n", CURRENT_REL, Z502_REG2);

    // Make a legal target process
    CREATE_PROCESS("test1e_a", test1x, LEGAL_PRIORITY_1E, &Z502_REG1,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    // Try to Suspend an Illegal PID
    SUSPEND_PROCESS((INT32 )9999, &Z502_REG9);
    ErrorExpected(Z502_REG9, "SUSPEND_PROCESS");

    // Try to Resume an Illegal PID
    RESUME_PROCESS((INT32 )9999, &Z502_REG9);
    ErrorExpected(Z502_REG9, "RESUME_PROCESS");

    // Suspend alegal PID
    SUSPEND_PROCESS(Z502_REG1, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SUSPEND_PROCESS");

    // Suspend already suspended PID
    SUSPEND_PROCESS(Z502_REG1, &Z502_REG9);
    ErrorExpected(Z502_REG9, "SUSPEND_PROCESS");

    // Do a legal resume of the process we have suspended
    RESUME_PROCESS(Z502_REG1, &Z502_REG9);
    SuccessExpected(Z502_REG9, "RESUME_PROCESS");

    // Resume an already resumed process
    RESUME_PROCESS(Z502_REG1, &Z502_REG9);
    ErrorExpected(Z502_REG9, "RESUME_PROCESS");

    // Try to resume ourselves
    RESUME_PROCESS(Z502_REG2, &Z502_REG9);
    ErrorExpected(Z502_REG9, "RESUME_PROCESS");

    // It may or may not be legal to suspend ourselves;
    // architectural decision.   It can be a useful technique
    // as a way to pass off control to another process.
    SUSPEND_PROCESS(-1, &Z502_REG9);

    /* If we returned "SUCCESS" here, then there is an inconsistency;
     * success implies that the process was suspended.  But if we
     * get here, then we obviously weren't suspended.  Therefore
     * this must be an error.                                    */
    ErrorExpected(Z502_REG9, "SUSPEND_PROCESS");

    GET_TIME_OF_DAY(&Z502_REG4);
    printf("Test1e, PID %ld, Ends at Time %ld\n", Z502_REG2, Z502_REG4);

    TERMINATE_PROCESS(-2, &Z502_REG9);
}                                                // End of test1e

/**************************************************************************
 Test1f

 Successfully suspend and resume processes. This assumes that Test1e
 runs successfully.

 In particular, show what happens to scheduling when processes
 are temporarily suspended.

 This test works by starting up a number of processes at different
 priorities.  Then some of them are suspended.  Then some are resumed.

 Z502_REG1              Loop counter
 Z502_REG2              OUR process ID
 Z502_REG3,4,5,6,7      Target process ID
 Z502_REG9              Error returned

 **************************************************************************/
#define         PRIORITY_1F1             5
#define         PRIORITY_1F2            10
#define         PRIORITY_1F3            15
#define         PRIORITY_1F4            20
#define         PRIORITY_1F5            25

void test1f(void) {

    static long   sleep_time = 300;
    int           iterations;

    // Get OUR PID
    Z502_REG1 = 0; // Initialize
    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);

// Make legal targets
    printf("Release %s:Test 1f: Pid %ld\n", CURRENT_REL, Z502_REG2);
    CREATE_PROCESS("test1f_a", test1x, PRIORITY_1F1, &Z502_REG3, &Z502_REG9);

    CREATE_PROCESS("test1f_b", test1x, PRIORITY_1F2, &Z502_REG4, &Z502_REG9);

    CREATE_PROCESS("test1f_c", test1x, PRIORITY_1F3, &Z502_REG5, &Z502_REG9);

    CREATE_PROCESS("test1f_d", test1x, PRIORITY_1F4, &Z502_REG6, &Z502_REG9);

    CREATE_PROCESS("test1f_e", test1x, PRIORITY_1F5, &Z502_REG7, &Z502_REG9);

    // Let the 5 processes go for a while
    SLEEP(sleep_time);

    // Do a set of suspends/resumes four times
    for (iterations = 0; iterations < 4; iterations++) {
        // Suspend 3 of the pids and see what happens - we should see
        // scheduling behavior where the processes are yanked out of the
        // ready and the waiting states, and placed into the suspended state.

        SUSPEND_PROCESS(Z502_REG3, &Z502_REG9);
        SUSPEND_PROCESS(Z502_REG5, &Z502_REG9);
        SUSPEND_PROCESS(Z502_REG7, &Z502_REG9);

        // Sleep so we can watch the scheduling action
        SLEEP(sleep_time);

        RESUME_PROCESS(Z502_REG3, &Z502_REG9);
        RESUME_PROCESS(Z502_REG5, &Z502_REG9);
        RESUME_PROCESS(Z502_REG7, &Z502_REG9);
    }

    //   Wait for children to finish, then quit
    SLEEP((INT32 )10000);
    TERMINATE_PROCESS(-2, &Z502_REG9);

}                        // End of test1f

/**************************************************************************
 Test1g

 Generate lots of errors for CHANGE_PRIORITY

 Try lots of different inputs: In particular, some of the possible
 inputs include:

 1. use of illegal priorities
 2. use of an illegal process id.


 Z502_REG1              Target process ID
 Z502_REG2              OUR process ID
 Z502_REG9              Error returned

 **************************************************************************/
#define         LEGAL_PRIORITY_1G               10
#define         ILLEGAL_PRIORITY_1G            999

void test1g(void) {

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);
    printf("Release %s:Test 1g: Pid %ld\n", CURRENT_REL, Z502_REG2);

    // Make a legal target
    CREATE_PROCESS("test1g_a", test1x, LEGAL_PRIORITY_1G, &Z502_REG1,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    // Target Illegal PID
    CHANGE_PRIORITY((INT32 )9999, LEGAL_PRIORITY_1G, &Z502_REG9);
    ErrorExpected(Z502_REG9, "CHANGE_PRIORITY");

    // Use illegal priority
    CHANGE_PRIORITY(Z502_REG1, ILLEGAL_PRIORITY_1G, &Z502_REG9);
    ErrorExpected(Z502_REG9, "CHANGE_PRIORITY");

    // Use legal priority on legal process
    CHANGE_PRIORITY(Z502_REG1, LEGAL_PRIORITY_1G, &Z502_REG9);
    SuccessExpected(Z502_REG9, "CHANGE_PRIORITY");
    // Terminate all existing processes
    TERMINATE_PROCESS(-2, &Z502_REG9);

}                                          // End of test1g

/**************************************************************************

 Test1h  Successfully change the priority of a process

 There are TWO ways wee can see that the priorities have changed:
 1. When you change the priority, it should be possible to see
 the scheduling behaviour of the system change; processes
 that used to be scheduled first are no longer first.  This will be
 visible in the ready Q of shown by the scheduler printer.
 2. The processes with more favorable priority should schedule first so
 they should finish first.


 Z502_REG2              OUR process ID
 Z502_REG3 - 5          Target process IDs
 Z502_REG9              Error returned

 **************************************************************************/

#define         MOST_FAVORABLE_PRIORITY         1
#define         FAVORABLE_PRIORITY             10
#define         NORMAL_PRIORITY                20
#define         LEAST_FAVORABLE_PRIORITY       30

void test1h(void) {
    long     ourself;

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);

    // Make our priority high
    printf("Release %s:Test 1h: Pid %ld\n", CURRENT_REL, Z502_REG2);
    ourself = -1;
    CHANGE_PRIORITY(ourself, MOST_FAVORABLE_PRIORITY, &Z502_REG9);

    // Make legal targets
    CREATE_PROCESS("test1h_a", test1x, NORMAL_PRIORITY, &Z502_REG3,
            &Z502_REG9);
    CREATE_PROCESS("test1h_b", test1x, NORMAL_PRIORITY, &Z502_REG4,
            &Z502_REG9);
    CREATE_PROCESS("test1h_c", test1x, NORMAL_PRIORITY, &Z502_REG5,
            &Z502_REG9);

    //      Sleep awhile to watch the scheduling
    SLEEP(200);

    //  Now change the priority - it should be possible to see
    //  that the priorities have been changed for processes that
    //  are ready and for processes that are sleeping.

    CHANGE_PRIORITY(Z502_REG3, FAVORABLE_PRIORITY, &Z502_REG9);

    CHANGE_PRIORITY(Z502_REG5, LEAST_FAVORABLE_PRIORITY, &Z502_REG9);

    //      Sleep awhile to watch the scheduling
    SLEEP(200);

    //  Now change the priority - it should be possible to see
    //  that the priorities have been changed for processes that
    //  are ready and for processes that are sleeping.

    CHANGE_PRIORITY(Z502_REG3, LEAST_FAVORABLE_PRIORITY, &Z502_REG9);

    CHANGE_PRIORITY(Z502_REG4, FAVORABLE_PRIORITY, &Z502_REG9);

    //     Sleep awhile to watch the scheduling
    SLEEP(600);

    // Terminate everyone
    TERMINATE_PROCESS(-2, &Z502_REG9);

}                                               // End of test1h  

/**************************************************************************

 Test1i   SEND_MESSAGE and RECEIVE_MESSAGE with errors.

 This has the same kind of error conditions that previous tests did;
 bad PIDs, bad message lengths, illegal buffer addresses, etc.
 Your imagination can go WILD on this one.

 This is a good time to mention an important aspect of the OS and
 scheduling.
 As you know, after doing a switch_context, the hardware passes
 control to the code after SwitchContext.  In other words, a process
 that is being rescheduled "disappears" into SwitchContext.  But
 it "reappears" after some other process causes that "disappeared"
 process to be scheduled.
 So at that "reappearing" location is a good place to put your message 
 code so as to do the rendevous work that's necessary to match up sends
 and receives.

 Why do this:        Suppose process A has sent a message to
 process B.  It so happens that you may well want
 to do some preparation in process B once it's
 registers are in memory, but BEFORE it executes
 the test.  In other words, it allows you to
 complete the work for the send to process B.

 We use test1x as our target process; but since it doesn't have any
 messaging code, it never actually sends or receives messages so it
 will never actually be scheduled.

 Z502_REG1         Pointer to data private to each process
 running this routine.
 Z502_REG2         OUR process ID
 Z502_REG3         Target process IDs
 Z502_REG9         Error returned

 **************************************************************************/

#define         LEGAL_MESSAGE_LENGTH           (INT16)64
#define         ILLEGAL_MESSAGE_LENGTH         (INT16)1000

#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct {
    long    target_pid;
    long    source_pid;
    long    actual_source_pid;
    long    send_length;
    long    receive_length;
    long    actual_send_length;
    long    loop_count;
    char    msg_buffer[LEGAL_MESSAGE_LENGTH ];
} TEST1I_DATA;

void test1i(void) {
    TEST1I_DATA *td; // Use as ptr to data */

    // Here we maintain the data to be used by this process when running
    // on this routine.  This code should be re-entrant.              

    td = (TEST1I_DATA *) calloc(1, sizeof(TEST1I_DATA));
    if (td == 0) {
        printf("Something screwed up allocating space in test1i\n");
    }

    td->loop_count = 0;

    // Get OUR PID
    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);
    printf("Release %s:Test 1i: Pid %ld\n", CURRENT_REL, Z502_REG2);

    // Make our priority high 
    CHANGE_PRIORITY(-1, MOST_FAVORABLE_PRIORITY, &Z502_REG9);

    // Make a legal target
    CREATE_PROCESS("test1i_a", test1x, NORMAL_PRIORITY, &Z502_REG3,
            &Z502_REG9);

    // Send a message to illegal process
    td->target_pid = 9999;
    td->send_length = 8;
    SEND_MESSAGE(td->target_pid, "message", td->send_length, &Z502_REG9);
    ErrorExpected(Z502_REG9, "SEND_MESSAGE");

    // Try an illegal message length                        
    td->target_pid = Z502_REG3;
    td->send_length = ILLEGAL_MESSAGE_LENGTH;
    SEND_MESSAGE(td->target_pid, "message", td->send_length, &Z502_REG9);
    ErrorExpected(Z502_REG9, "SEND_MESSAGE");

    //      Receive from illegal process                    
    td->source_pid = 9999;
    td->receive_length = LEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    ErrorExpected(Z502_REG9, "RECEIVE_MESSAGE");

    //      Receive with illegal buffer size                
    td->source_pid = Z502_REG3;
    td->receive_length = ILLEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    ErrorExpected(Z502_REG9, "RECEIVE_MESSAGE");

    //      Send a legal ( but long ) message to self       
    td->target_pid = Z502_REG2;
    td->send_length = LEGAL_MESSAGE_LENGTH;
    SEND_MESSAGE(td->target_pid, "a long but legal message", td->send_length,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "SEND_MESSAGE");
    td->loop_count++;      // Count the number of legal messages sent

    //   Receive this long message, which should error because the receive buffer is too small         
    td->source_pid = Z502_REG2;
    td->receive_length = 10;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    ErrorExpected(Z502_REG9, "RECEIVE_MESSAGE");

    // Keep sending legal messages until the architectural
    // limit for buffer space is exhausted.  In order to pass
    // the  test1j, this number should be at least EIGHT     

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        td->target_pid = Z502_REG3;
        td->send_length = LEGAL_MESSAGE_LENGTH;
        SEND_MESSAGE(td->target_pid, "Legal Test1i Message", td->send_length, &Z502_REG9);
        td->loop_count++;
    }  // End of while

    printf("A total of %ld messages were enqueued.\n", td->loop_count - 1);
    TERMINATE_PROCESS(-2, &Z502_REG9);
}                                              // End of test1i     

/**************************************************************************

 Test1j   SEND_MESSAGE and RECEIVE_MESSAGE Successfully.

 Creates three other processes, each running their own code.
 RECEIVE and SEND messages are winged back and forth at them.

 Z502_REG1              Pointer to data private to each process
 running this routine.
 Z502_REG2              OUR process ID
 Z502_REG3 - 5          Target process IDs
 Z502_REG9              Error returned

 The SEND and RECEIVE system calls as implemented by this test
 imply the following behavior:

 SENDER = PID A          RECEIVER = PID B,

 Designates source_pid =
 target_pid =          A            C             -1
 ----------------+------------+------------+--------------+
 |            |            |              |               |
 | B          |  Message   |     X        |   Message     |
 |Transmitted |            | Transmitted  |               |
 ----------------+------------+------------+--------------+
 |            |            |              |               |
 | C          |     X      |     X        |       X       |
 |            |            |              |               |
 ----------------+------------+------------+--------------+
 |            |            |              |               |
 | -1         |   Message  |     X        |   Message     |
 | Transmitted|            | Transmitted  |               |
 ----------------+------------+------------+--------------+
 A broadcast ( target_pid = -1 ) means send to everyone BUT yourself.
 ANY of the receiving processes can handle a broadcast message.
 A receive ( source_pid = -1 ) means receive from anyone.
 **************************************************************************/

#define         LEGAL_MESSAGE_LENGTH            (INT16)64
#define         ILLEGAL_MESSAGE_LENGTH          (INT16)1000
#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct {
    long    target_pid;
    long    source_pid;
    long    actual_source_pid;
    long    send_length;
    long    receive_length;
    long    actual_send_length;
    long    send_loop_count;
    long    receive_loop_count;
    char    msg_buffer[LEGAL_MESSAGE_LENGTH ];
    char    msg_sent[LEGAL_MESSAGE_LENGTH ];
} TEST1J_DATA;

void test1j(void) {
    int Iteration;
    TEST1J_DATA *td;           // Use as pointer to data 

    // Here we maintain the data to be used by this process when running
    // on this routine.  This code should be re-entrant.                */

    td = (TEST1J_DATA *) calloc(1, sizeof(TEST1J_DATA));
    if (td == 0) {
        printf("Something screwed up allocating space in test1j\n");
    }
    td->send_loop_count = 0;
    td->receive_loop_count = 0;

    // Get OUR PID         
    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);

    // Make our prior high 
    printf("Release %s:Test 1j: Pid %ld\n", CURRENT_REL, Z502_REG2);
    CHANGE_PRIORITY(-1, MOST_FAVORABLE_PRIORITY, &Z502_REG9);
    SuccessExpected(Z502_REG9, "CHANGE_PRIORITY");

    // Make 3 legal targets  
    CREATE_PROCESS("test1j_1", test1j_echo, NORMAL_PRIORITY, &Z502_REG3,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");
    CREATE_PROCESS("test1j_2", test1j_echo, NORMAL_PRIORITY, &Z502_REG4,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");
    CREATE_PROCESS("test1j_3", test1j_echo, NORMAL_PRIORITY, &Z502_REG5,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    //      Send/receive a legal message to each child    
    for (Iteration = 1; Iteration <= 3; Iteration++) {
        if (Iteration == 1) {
            td->target_pid = Z502_REG3;
            strcpy(td->msg_sent, "message to #1");
        }
        if (Iteration == 2) {
            td->target_pid = Z502_REG4;
            strcpy(td->msg_sent, "message to #2");
        }
        if (Iteration == 3) {
            td->target_pid = Z502_REG5;
            strcpy(td->msg_sent, "message to #3");
        }
        td->send_length = 20;
        SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);
        SuccessExpected(Z502_REG9, "SEND_MESSAGE");

        td->source_pid = -1;
        td->receive_length = LEGAL_MESSAGE_LENGTH;
        RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
                &(td->actual_send_length), &(td->actual_source_pid),
                &Z502_REG9);
        SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");

        if (strcmp(td->msg_buffer, td->msg_sent) != 0)
            printf("ERROR - msg sent != msg received.\n");

        if (td->actual_source_pid != td->target_pid )
            printf("ERROR - source PID not correct.\n");

        if (td->actual_send_length != td->send_length)
            printf("ERROR - send length not sent correctly.\n");
    }    // End of for loop

    //      Keep sending legal messages until the architectural (OS)
    //      limit for buffer space is exhausted.     

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        td->target_pid = -1;
        sprintf(td->msg_sent, "This is message %ld", td->send_loop_count);
        td->send_length = 20;
        SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);

        td->send_loop_count++;
    }
    td->send_loop_count--;
    printf("A total of %ld messages were enqueued.\n", td->send_loop_count);

    //  Now receive back from the other processes the same number of messages we've just sent.
    SLEEP(100);
    while (td->receive_loop_count < td->send_loop_count) {
        td->source_pid = -1;
        td->receive_length = LEGAL_MESSAGE_LENGTH;
        RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
                &(td->actual_send_length), &(td->actual_source_pid),
                &Z502_REG9);
        SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");
        printf("Receive from PID = %ld: length = %ld: msg = %s:\n",
                td->actual_source_pid, td->actual_send_length, td->msg_buffer);
        td->receive_loop_count++;
    }

    printf("A total of %ld messages were received.\n",
            td->receive_loop_count);
    CALL(TERMINATE_PROCESS(-2, &Z502_REG9));

}                                                 // End of test1j     

/**************************************************************************

 Test1k  Test other oddities in your system.


 There are many other strange effects, not taken into account
 by the previous tests.  One of these is:

 1. Executing a privileged instruction from a user program
 This should cause the program to be terminated.  The hardware will
 cause a fault to occur.  Your fault handler in the OS will catch this
 and terminate the program.

 Registers Used:
 Z502_REG2              OUR process ID
 Z502_REG9              Error returned

 **************************************************************************/

void test1k(void) {
    INT32 Result;

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);

    printf("Release %s:Test 1k: Pid %ld\n", CURRENT_REL, Z502_REG2);

    /*      Do an illegal hardware instruction - we will
     not return from this.                                   */

    MEM_READ(Z502TimerStatus, &Result);
}                       // End of test1k

/**************************************************************************
 Test1l   SEND_MESSAGE and RECEIVE_MESSAGE with SUSPEND/RESUME

 Explores how message handling is done in the midst of SUSPEND/RESUME
 system calls,

 The SEND and RECEIVE system calls as implemented by this test
 imply the following behavior:

 Case 1:
 - a process waiting to recieve a message can be suspended
 - a process waiting to recieve a message can be resumed
 - after being resumed, it can receive a message

 Case 2:
 - when a process waiting for a message is suspended, it is out of
 circulation and cannot recieve any message
 - once it is unsuspended, it may recieve a message and go on the ready
 queue

 Case 3:
 - a process that waited for and found a message is now the ready queue
 - this process can be suspended before handling the message
 - the message and process remain paired up, no other process can have
 that message
 - when resumed, the process will handle the message

 **************************************************************************/

#define         LEGAL_MESSAGE_LENGTH            (INT16)64
#define         MOST_FAVORABLE_PRIORITY         1
#define         NORMAL_PRIORITY                20

typedef struct {
    long    target_pid;
    long    source_pid;
    long    actual_source_pid;
    long    send_length;
    long    receive_length;
    long    actual_send_length;
    long    send_loop_count;
    long    receive_loop_count;
    char    msg_buffer[LEGAL_MESSAGE_LENGTH ];
    char    msg_sent[LEGAL_MESSAGE_LENGTH ];
} TEST1L_DATA;

void test1l(void) {
    TEST1L_DATA *td; // Use as ptr to data */

    // Here we maintain the data to be used by this process when running
    // on this routine.  This code should be re-entrant.                

    td = (TEST1L_DATA *) calloc(1, sizeof(TEST1L_DATA));
    if (td == 0) {
        printf("Something screwed up allocating space in test1j\n");
    }

    // Get OUR PID
    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);

    // Make our priority high
    printf("Release %s:Test 1l: Pid %ld\n", CURRENT_REL, Z502_REG2);
    CHANGE_PRIORITY(-1, MOST_FAVORABLE_PRIORITY, &Z502_REG9);
    SuccessExpected(Z502_REG9, "CHANGE_PRIORITY");

    // Make process to test with
    CREATE_PROCESS("test1l_1", test1j_echo, NORMAL_PRIORITY, &Z502_REG3,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    // BEGIN CASE 1 ------------------------------------------
    printf("\n\nBegin Case 1:\n\n");

    //      Sleep so that first process will wake and receive
    SLEEP(200);

    GET_PROCESS_ID("test1l_1", &Z502_REG4, &Z502_REG9);
    SuccessExpected(Z502_REG9, "Get Receiving Process ID");

    if (Z502_REG3 != Z502_REG4)
        printf("ERROR!  The process ids should match! New process ID is: %ld",
                Z502_REG4);

    //      Suspend the receiving process
    SUSPEND_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SUSPEND");

    // Resume the recieving process
    RESUME_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "RESUME");

    // Send it a message
    td->target_pid = Z502_REG3;
    td->send_length = 30;
    strcpy(td->msg_sent, "Resume first echo");
    SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SEND_MESSAGE");

    // Receive it's response (process is now back in recieving mode)
    td->source_pid = -1;
    td->receive_length = LEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");
    if (strcmp(td->msg_buffer, td->msg_sent) != 0)
        printf("ERROR - msg sent != msg received.\n");
    if (td->actual_source_pid != Z502_REG3)
        printf("ERROR - source PID not correct.\n");
    if (td->actual_send_length != td->send_length)
        printf("ERROR - send length not sent correctly.\n");

    // BEGIN CASE 2 ------------------------------------------
    printf("\n\nBegin Case 2:\n\n");

    // create a competitor to show suspend works with incoming messages
    CREATE_PROCESS("test1l_2", test1j_echo, NORMAL_PRIORITY, &Z502_REG5,
            &Z502_REG9);
    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    //      Sleep so that the new process will wake and receive
    SLEEP(200);

    GET_PROCESS_ID("test1l_2", &Z502_REG6, &Z502_REG9);
    SuccessExpected(Z502_REG9, "Get Receiving Process ID");
    if (Z502_REG5 != Z502_REG6)
        printf("ERROR!  The process ids should match! New process ID is: %ld",
                Z502_REG4);

    //      Suspend the first process
    SUSPEND_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SUSPEND");

    // Send anyone a message
    td->target_pid = -1;
    td->send_length = 30;
    strcpy(td->msg_sent, "Going to second process");
    SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SEND_MESSAGE");

    // Resume the first process
    RESUME_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "RESUME");

    // Receive the second process' response
    td->source_pid = Z502_REG5;
    td->receive_length = LEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");
    if (strcmp(td->msg_buffer, td->msg_sent) != 0)
        printf("ERROR - msg sent != msg received.\n");
    if (td->actual_source_pid != Z502_REG5)
        printf("ERROR - source PID not correct.\n");
    if (td->actual_send_length != td->send_length)
        printf("ERROR - send length not sent correctly.\n");

    //
    // BEGIN CASE 3 ------------------------------------------
    printf("\n\nBegin Case 3:\n\n");

    //      Suspend the first process
    SUSPEND_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SUSPEND");

    // Send it, specifically, a message
    td->target_pid = Z502_REG3;
    td->send_length = 30;
    strcpy(td->msg_sent, "Going to suspended");
    SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);
    SuccessExpected(Z502_REG9, "SEND_MESSAGE");

    // Resume the first process
    RESUME_PROCESS(Z502_REG3, &Z502_REG9);
    SuccessExpected(Z502_REG9, "RESUME");

    // Receive the process' response
    td->source_pid = Z502_REG3;
    td->receive_length = LEGAL_MESSAGE_LENGTH;
    RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
            &(td->actual_send_length), &(td->actual_source_pid), &Z502_REG9);
    SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");

    if (strcmp(td->msg_buffer, td->msg_sent) != 0)
        printf("ERROR - msg sent != msg received.\n");
    if (td->actual_source_pid != Z502_REG3)
        printf("ERROR - source PID not correct.\n");
    if (td->actual_send_length != td->send_length)
        printf("ERROR - send length not sent correctly.\n");

    TERMINATE_PROCESS(-2, &Z502_REG9);

}                                                 // End of test1l

/**************************************************************************
 Test 1m

 Write an interesting test of your own to exhibit some feature of
 your Operating System.

 **************************************************************************/
void test1m(void) {
}                                               // End test1m

/**************************************************************************
 Test1x

 is used as a target by the process creation programs.
 It has the virtue of causing lots of rescheduling activity in
 a relatively random way.

 Z502_REG1              Loop counter
 Z502_REG2              OUR process ID
 Z502_REG3              Starting time
 Z502_REG4              Ending time
 Z502_REG9              Error returned

 **************************************************************************/

#define         NUMBER_OF_TEST1X_ITERATIONS     10

void test1x(void) {
    long   RandomSleep = 17;
    int    Iterations;

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);
    printf("Release %s:Test 1x: Pid %ld\n", CURRENT_REL, Z502_REG2);

    for (Iterations = 0; Iterations < NUMBER_OF_TEST1X_ITERATIONS;
            Iterations++) {
        GET_TIME_OF_DAY(&Z502_REG3);
        RandomSleep = (RandomSleep * Z502_REG3) % 143;
        SLEEP(RandomSleep);
        GET_TIME_OF_DAY(&Z502_REG4);
        printf("Test1X: Pid = %d, Sleep Time = %ld, Latency Time = %d\n",
                (int) Z502_REG2, RandomSleep, (int) (Z502_REG4 - Z502_REG3));
    }
    printf("Test1x, PID %ld, Ends at Time %ld\n", Z502_REG2, Z502_REG4);

    TERMINATE_PROCESS(-1, &Z502_REG9);
    printf("ERROR: Test1x should be terminated but isn't.\n");

} /* End of test1x    */

/**************************************************************************

 Test1j_echo

 is used as a target by the message send/receive programs.
 All it does is send back the same message it received to the
 same sender.

 Z502_REG1              Pointer to data private to each process
 running this routine.
 Z502_REG2              OUR process ID
 Z502_REG3              Starting time
 Z502_REG4              Ending time
 Z502_REG9              Error returned
 **************************************************************************/

typedef struct {
    long     target_pid;
    long     source_pid;
    long     actual_source_pid;
    long     send_length;
    long     receive_length;
    long     actual_senders_length;
    char     msg_buffer[LEGAL_MESSAGE_LENGTH ];
    char     msg_sent[LEGAL_MESSAGE_LENGTH ];
} TEST1J_ECHO_DATA;

void test1j_echo(void) {
    TEST1J_ECHO_DATA *td;                          // Use as ptr to data 

    // Here we maintain the data to be used by this process when running
    // on this routine.  This code should be re-entrant.     

    td = (TEST1J_ECHO_DATA *) calloc(1, sizeof(TEST1J_ECHO_DATA));
    if (td == 0) {
        printf("Something screwed up allocating space in test1j_echo\n");
    }

    GET_PROCESS_ID("", &Z502_REG2, &Z502_REG9);
    SuccessExpected(Z502_REG9, "GET_PROCESS_ID");
    printf("Release %s:Test 1j_echo: Pid %ld\n", CURRENT_REL, Z502_REG2);

    while (1) {       // Loop forever.
        td->source_pid = -1;
        td->receive_length = LEGAL_MESSAGE_LENGTH;
        RECEIVE_MESSAGE(td->source_pid, td->msg_buffer, td->receive_length,
                &(td->actual_senders_length), &(td->actual_source_pid),
                &Z502_REG9);
        SuccessExpected(Z502_REG9, "RECEIVE_MESSAGE");

        printf("Receive from PID = %ld: length = %ld: msg = %s:\n",
                td->actual_source_pid, td->actual_senders_length,
                td->msg_buffer);

        td->target_pid = td->actual_source_pid;
        strcpy(td->msg_sent, td->msg_buffer);
        td->send_length = td->actual_senders_length;
        SEND_MESSAGE(td->target_pid, td->msg_sent, td->send_length, &Z502_REG9);
        SuccessExpected(Z502_REG9, "SEND_MESSAGE");
    }     // End of while

}                                               // End of test1j_echo

/**************************************************************************
 ErrorExpected    and    SuccessExpected

 These routines simply handle the display of success/error data.

 **************************************************************************/

void ErrorExpected(INT32 ErrorCode, char sys_call[]) {
    if (ErrorCode == ERR_SUCCESS) {
        printf("An Error SHOULD have occurred.\n");
        printf("????: Error( %d ) occurred in case %d (%s)\n", ErrorCode,
                Z502_PROGRAM_COUNTER - 2, sys_call);
    } else
        printf("Program correctly returned an error: %d\n", ErrorCode);

}                      // End of ErrorExpected

void SuccessExpected(INT32 ErrorCode, char sys_call[]) {
    if (ErrorCode != ERR_SUCCESS) {
        printf("An Error should NOT have occurred.\n");
        printf("????: Error( %d ) occurred in case %d (%s)\n", ErrorCode,
                Z502_PROGRAM_COUNTER - 2, sys_call);
    } else
        printf("Program correctly returned success.\n");

}                      // End of SuccessExpected

/**************************************************************************
 Test2a exercises a simple memory write and read

 Use:  Z502_REG1                data_written
 Z502_REG2                data_read
 Z502_REG3                address
 Z502_REG4                process_id
 Z502_REG9                error

 In global.h, there's a variable  DO_MEMORY_DEBUG.   Switching it to
 TRUE will allow you to see what the memory system thinks is happening.
 WARNING - it's verbose -- and I don't want to see such output - it's
 strictly for your debugging pleasure.
 **************************************************************************/
void test2a(void) {

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);

    printf("Release %s:Test 2a: Pid %ld\n", CURRENT_REL, Z502_REG4);
    Z502_REG3 = 412;
    Z502_REG1 = Z502_REG3 + Z502_REG4;
    MEM_WRITE(Z502_REG3, &Z502_REG1);

    MEM_READ(Z502_REG3, &Z502_REG2);

    printf("PID= %ld  address= %ld   written= %ld   read= %ld\n", Z502_REG4,
            Z502_REG3, Z502_REG1, Z502_REG2);
    if (Z502_REG2 != Z502_REG1)
        printf("AN ERROR HAS OCCURRED.\n");
    TERMINATE_PROCESS(-1, &Z502_REG9);

}                   // End of test2a   

/**************************************************************************
 Test2b

 Exercises simple memory writes and reads.  Watch out, the addresses 
 used are diabolical and are designed to show unusual features of your 
 memory management system.

 Use:  
 Z502_REG1                data_written
 Z502_REG2                data_read
 Z502_REG3                address
 Z502_REG4                process_id
 Z502_REG5                test_data_index
 Z502_REG9                error

 The following registers are used for sanity checks - after each
 read/write pair, we will read back the first set of data to make
 sure it's still there.

 Z502_REG6                First data written
 Z502_REG7                First data read
 Z502_REG8                First address

 **************************************************************************/

#define         TEST_DATA_SIZE          (INT16)7

void test2b(void) {
    static INT32 test_data[TEST_DATA_SIZE ] = { 0, 4, PGSIZE - 2, PGSIZE, 3
            * PGSIZE - 2, (VIRTUAL_MEM_PAGES - 1) * PGSIZE, VIRTUAL_MEM_PAGES
            * PGSIZE - 2 };

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);
    printf("\n\nRelease %s:Test 2b: Pid %ld\n", CURRENT_REL, Z502_REG4);

    Z502_REG8 = 5 * PGSIZE;
    Z502_REG6 = Z502_REG8 + Z502_REG4 + 7;
    MEM_WRITE(Z502_REG8, &Z502_REG6);

    // Loop through all the memory addresses defined
    while (TRUE ) {
        Z502_REG3 = test_data[(INT16) Z502_REG5];
        //printf("z502_reg3 is %d, z502_reg5 is %d\n", Z502_REG3, Z502_REG5);
        Z502_REG1 = Z502_REG3 + Z502_REG4 + 27;
        MEM_WRITE(Z502_REG3, &Z502_REG1);

        MEM_READ(Z502_REG3, &Z502_REG2);

        printf("PID= %ld  address= %ld  written= %ld   read= %ld\n", Z502_REG4,
                Z502_REG3, Z502_REG1, Z502_REG2);
        if (Z502_REG2 != Z502_REG1)
            printf("AN ERROR HAS OCCURRED.\n");

        //      Go back and check earlier write
        MEM_READ(Z502_REG8, &Z502_REG7);

        printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                Z502_REG4, Z502_REG8, Z502_REG6, Z502_REG7);
        if (Z502_REG6 != Z502_REG7)
            printf("AN ERROR HAS OCCURRED.\n");
        Z502_REG5++;
    }
}                            // End of test2b    

/**************************************************************************

 Test2c causes usage of disks.  The test is designed to give
 you a chance to develop a mechanism for handling disk requests.

 Z502_REG3  - address where data was written/read.
 Z502_REG4  - process id of this process.
 Z502_REG6  - number of iterations/loops through the code.
 Z502_REG7  - which page will the write/read be on. start at 0
 Z502_REG9  - returned error code.

 You will need a way to get the data read back from the disk into the
 buffer defined by the user process.  This can most easily be done after
 the process is rescheduled and about to return to user code.
 **************************************************************************/

#define         DISPLAY_GRANULARITY2c           10
#define         TEST2C_LOOPS                    50

typedef union {
    char char_data[PGSIZE ];
    UINT32 int_data[PGSIZE / sizeof(int)];
} DISK_DATA;

void test2c(void) {
    DISK_DATA *data_written;
    DISK_DATA *data_read;
    long       disk_id;
    INT32      sanity = 1234;
    long       sector;
    int        Iterations;

    data_written = (DISK_DATA *) calloc(1, sizeof(DISK_DATA));
    data_read = (DISK_DATA *) calloc(1, sizeof(DISK_DATA));
    if (data_read == 0)
        printf("Something screwed up allocating space in test2c\n");

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);

    sector = Z502_REG4;
    printf("\n\nRelease %s:Test 2c: Pid %ld\n", CURRENT_REL, Z502_REG4);

    for (Iterations = 0; Iterations < TEST2C_LOOPS; Iterations++) {
        // Pick some location on the disk to write to
        disk_id = (Z502_REG4 / 2) % MAX_NUMBER_OF_DISKS + 1;
        //sector = ( Iterations + (sector * 177)) % NUM_LOGICAL_SECTORS;
        sector = Z502_REG4 + (Iterations * 17) % NUM_LOGICAL_SECTORS;  // Bugfix 4.11
        data_written->int_data[0] = disk_id;
        data_written->int_data[1] = sanity;
        data_written->int_data[2] = sector;
        data_written->int_data[3] = (int) Z502_REG4;
        //printf( "Test2c - Pid = %d, Writing from Addr = %X\n", (int)Z502_REG4, (unsigned int )(data_written->char_data));
        DISK_WRITE(disk_id, sector, (char* )(data_written->char_data));

        // Now read back the same data.  Note that we assume the
        // disk_id and sector have not been modified by the previous
        // call.
        DISK_READ(disk_id, sector, (char* )(data_read->char_data));

        if ((data_read->int_data[0] != data_written->int_data[0])  
                || (data_read->int_data[1] != data_written->int_data[1])
                || (data_read->int_data[2] != data_written->int_data[2])
                || (data_read->int_data[3] != data_written->int_data[3])) {
            printf("ERROR in Test 2c, Pid = %ld. Disk = %ld\n", 
                    Z502_REG4, disk_id );
            printf("Written:  %d,  %d,  %d,  %d\n",
                data_written->int_data[0], data_written->int_data[1],
                data_written->int_data[2], data_written->int_data[3] );
            printf("Read:     %d,  %d,  %d,  %d\n",
                data_read->int_data[0], data_read->int_data[1],
                data_read->int_data[2], data_read->int_data[3] );
        }  else if (( Iterations % DISPLAY_GRANULARITY2c ) == 0) {
            printf("TEST2C: PID = %ld: SUCCESS READING  disk_id =%ld, sector = %ld\n",
                    Z502_REG4, disk_id, sector);
        }
    }   // End of for loop

    // Now read back the data we've previously written and read

    printf("TEST2C: Reading back data: PID %ld.\n", Z502_REG4);
    sector = Z502_REG4;

    for (Iterations = 0; Iterations < TEST2C_LOOPS; Iterations++) {
        disk_id = (Z502_REG4 / 2) % MAX_NUMBER_OF_DISKS + 1;
        //sector = ( Iterations + (sector * 177)) % NUM_LOGICAL_SECTORS;
        sector = Z502_REG4 + (Iterations * 17) % NUM_LOGICAL_SECTORS;  // Bugfix 4.11
        data_written->int_data[0] = disk_id;
        data_written->int_data[1] = sanity;
        data_written->int_data[2] = sector;
        data_written->int_data[3] = Z502_REG4;

        DISK_READ(disk_id, sector, (char* )(data_read->char_data));

        if ((data_read->int_data[0] != data_written->int_data[0])
                || (data_read->int_data[1] != data_written->int_data[1])
                || (data_read->int_data[2] != data_written->int_data[2])
                || (data_read->int_data[3] != data_written->int_data[3])) {
            printf("AN ERROR HAS OCCURRED in Test 2c, Pid = %ld.\n", Z502_REG4);
            printf("Written:  %d,  %d,  %d,  %d\n",
                data_written->int_data[0], data_written->int_data[1],
                data_written->int_data[2], data_written->int_data[3] );
            printf("Read:     %d,  %d,  %d,  %d\n",
                data_read->int_data[0], data_read->int_data[1],
                data_read->int_data[2], data_read->int_data[3] );
        } else if ( ( Iterations % DISPLAY_GRANULARITY2c ) == 0) {
            printf("TEST2C: PID = %ld: SUCCESS READING  disk_id =%ld, sector = %ld\n",
                    Z502_REG4, disk_id, sector);
        }

    }   // End of for loop

    GET_TIME_OF_DAY(&Z502_REG8);
    printf("TEST2C:    PID %ld, Ends at Time %ld\n", Z502_REG4, Z502_REG8);
    TERMINATE_PROCESS(-1, &Z502_REG9);

}                                       // End of test2c    

/**************************************************************************

 Test2d runs several disk programs at a time.  The purpose here
 is to watch the scheduling that goes on for these
 various disk processes.  The behavior that should be seen
 is that the processes alternately run and do disk
 activity - there should always be someone running unless
 ALL processes happen to be waiting on the disk at some
 point.
 This program will terminate when all the test2c routines
 have finished.

 Z502_REG4  - process id of this process.
 Z502_REG5  - returned error code.
 Z502_REG6  - pid of target process.
 Z502_REG8  - returned error code from the GET_PROCESS_ID call.

 **************************************************************************/
#define           MOST_FAVORABLE_PRIORITY                       1

void test2d(void) {
    static INT32 trash;
    static long    sleep_time = 10000;

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG5);
    printf("\n\nRelease %s:Test 2d: Pid %ld\n", CURRENT_REL, Z502_REG4);
    CHANGE_PRIORITY(-1, MOST_FAVORABLE_PRIORITY, &Z502_REG9);

    CREATE_PROCESS("first", test2c, 5, &trash, &Z502_REG5);
    CREATE_PROCESS("second", test2c, 5, &trash, &Z502_REG5);
    CREATE_PROCESS("third", test2c, 7, &trash, &Z502_REG5);
    CREATE_PROCESS("fourth", test2c, 7, &trash, &Z502_REG5);
    CREATE_PROCESS("fifth", test2c, 10, &trash, &Z502_REG5);

    SLEEP(500000);

    // In these next cases, we will loop until EACH of the child processes
    // has terminated.  We know it terminated because for a while we get 
    // success on the call GET_PROCESS_ID, and then we get failure when the 
    // process no longer exists.
    // We do this for each process so we can get decent statistics on completion

    Z502_REG9 = ERR_SUCCESS;      // Modified 07/2014 Rev 4.10
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("first", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("second", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("third", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("fourth", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("fifth", &Z502_REG6, &Z502_REG9);
    }
    GET_TIME_OF_DAY(&Z502_REG7);
    printf("Test2d, PID %ld, Ends at Time %ld\n\n", Z502_REG4, Z502_REG7);
    TERMINATE_PROCESS(-2, &Z502_REG5);

}                                   // End of test2d 

/**************************************************************************

 Test2e touches a number of logical pages - but the number of pages touched
 will fit into the physical memory available.
 The addresses accessed are pseudo-random distributed in a non-uniform manner.

 Z502_REG1  - data that was written.
 Z502_REG2  - data that was read from memory.
 Z502_REG3  - address where data was written/read.
 Z502_REG4  - process id of this process.
 Z502_REG6  - number of iterations/loops through the code.
 Z502_REG9  - returned error code.

 **************************************************************************/

#define         STEP_SIZE               VIRTUAL_MEM_PAGES/(4 * PHYS_MEM_PGS )
#define         DISPLAY_GRANULARITY2e     16 * STEP_SIZE
#define         TOTAL_ITERATIONS        256
void test2e(void) {
    int Iteration, MixItUp;
    int AddressesWritten[VIRTUAL_MEM_PAGES];

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);
    printf("\n\nRelease %s:Test 2e: Pid %ld\n", CURRENT_REL, Z502_REG4);

    for (Iteration = 0; Iteration < TOTAL_ITERATIONS; Iteration++ )
            AddressesWritten[Iteration] = 0;
    for (Iteration = 0; Iteration < TOTAL_ITERATIONS; Iteration++ ) {
        GetSkewedRandomNumber(&Z502_REG3, 1024);  // Generate Address    Bugfix 4.12
	Z502_REG3 = 4 * (Z502_REG3 / 4);          // Put address on mod 4 boundary
        AddressesWritten[Iteration] = Z502_REG3;  // Keep record of location written
        Z502_REG1 = PGSIZE * Z502_REG3;           // Generate Data    Bugfix 4.12
        MEM_WRITE(Z502_REG3, &Z502_REG1);         // Write the data

        MEM_READ(Z502_REG3, &Z502_REG2); // Read back data

        if (Iteration % DISPLAY_GRANULARITY2e == 0)
            printf("PID= %ld  address= %6ld   written= %6ld   read= %6ld\n",
                    Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
        if (Z502_REG2 != Z502_REG1) {  // Written = Read?
            printf("AN ERROR HAS OCCURRED ON 1ST READBACK.\n");
            printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                    Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
        }

        // It makes life more fun!! to write the data again
        MEM_WRITE(Z502_REG3, &Z502_REG1); // Write the data

    }    // End of for loop

    // Now read back the data we've written and paged
    // We try to jump around a bit in choosing addresses we read back
    printf("Reading back data: test 2e, PID %ld.\n", Z502_REG4);

    for (Iteration = 0; Iteration < TOTAL_ITERATIONS; Iteration++ ) {
        if (!( Iteration % 2 ))      // Bugfix 4.11
            MixItUp = Iteration;
        else
            MixItUp = TOTAL_ITERATIONS - Iteration;
        Z502_REG3 = AddressesWritten[MixItUp];     // Get location we wrote
        Z502_REG1 = PGSIZE * Z502_REG3;           // Generate Data    Bugfix 4.12
        MEM_READ(Z502_REG3, &Z502_REG2);      // Read back data

        if (Iteration % DISPLAY_GRANULARITY2e == 0)
            printf("PID= %ld  address= %6ld   written= %6ld   read= %6ld\n",
                    Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
        if (Z502_REG2 != Z502_REG1) {  // Written = Read?
            printf("AN ERROR HAS OCCURRED ON 2ND READBACK.\n");
            printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                    Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
        }
    }    // End of for loop
    TERMINATE_PROCESS(-2, &Z502_REG5);      // Added 12/1/2013
}                                           // End of test2e    

/**************************************************************************

 Test2f causes extensive page replacement, but reuses pages.
 This program will terminate, but it might take a while.

 Z502_REG1  - data that was written.
 Z502_REG2  - data that was read from memory.
 Z502_REG3  - address where data was written/read.
 Z502_REG4  - process id of this process.
 Z502_REG9  - returned error code.

 **************************************************************************/

#define                 NUMBER_OF_ITERATIONS           3
#define                 LOOP_COUNT                    400
#define                 DISPLAY_GRANULARITY2          100
#define                 LOGICAL_PAGES_TO_TOUCH       2 * PHYS_MEM_PGS

typedef struct {
    INT16 page_touched[LOOP_COUNT];   // Bugfix Rel 4.03  12/1/2013
} MEMORY_TOUCHED_RECORD;

void test2f(void) {
    MEMORY_TOUCHED_RECORD *mtr;
    short Iterations, Index, Loops;

    mtr = (MEMORY_TOUCHED_RECORD *) calloc(1, sizeof(MEMORY_TOUCHED_RECORD));

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);
    printf("\n\nRelease %s:Test 2f: Pid %ld\n", CURRENT_REL, Z502_REG4);

    for (Iterations = 0; Iterations < NUMBER_OF_ITERATIONS; Iterations++) {
        for (Index = 0; Index < LOOP_COUNT; Index++)   // Bugfix Rel 4.03  12/1/2013
            mtr->page_touched[Index] = 0;
        for (Loops = 0; Loops < LOOP_COUNT; Loops++) {
            // Get a random page number
            GetSkewedRandomNumber(&Z502_REG7, LOGICAL_PAGES_TO_TOUCH);
            Z502_REG3 = PGSIZE * Z502_REG7; // Convert page to addr.
            Z502_REG1 = Z502_REG3 + Z502_REG4;   // Generate data for page
            MEM_WRITE(Z502_REG3, &Z502_REG1);
            // Write it again, just as a test
            MEM_WRITE(Z502_REG3, &Z502_REG1);

            // Read it back and make sure it's the same
            MEM_READ(Z502_REG3, &Z502_REG2);
            if (Loops % DISPLAY_GRANULARITY2 == 0)
                printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
            if (Z502_REG2 != Z502_REG1)
                printf("AN ERROR HAS OCCURRED: READ NOT EQUAL WRITE.\n");


            // Record in our data-base that we've accessed this page
            mtr->page_touched[(short) Loops] = Z502_REG7;
            Test2f_Statistics( Z502_REG4 );

        }   // End of for Loops

        for (Loops = 0; Loops < LOOP_COUNT; Loops++) {

            // We can only read back from pages we've previously
            // written to, so find out which pages those are.
            Z502_REG6 = mtr->page_touched[(short) Loops];
            Z502_REG3 = PGSIZE * Z502_REG6; // Convert page to addr.
            Z502_REG1 = Z502_REG3 + Z502_REG4; // Expected read
            MEM_READ(Z502_REG3, &Z502_REG2);
            Test2f_Statistics( Z502_REG4 );

            if (Loops % DISPLAY_GRANULARITY2 == 0)
                printf("PID= %ld  address= %ld   written= %ld   read= %ld\n",
                        Z502_REG4, Z502_REG3, Z502_REG1, Z502_REG2);
            if (Z502_REG2 != Z502_REG1)
                printf("ERROR HAS OCCURRED: READ NOT SAME AS WRITE.\n");

        }   // End of for Loops

        // We've completed reading back everything
        printf("TEST 2f, PID %ld, HAS COMPLETED %d ITERATIONS\n", Z502_REG4,
                Iterations);
    }   // End of for Iterations

    TERMINATE_PROCESS(-1, &Z502_REG9);

}                                 // End of test2f

/**************************************************************************
 Test2g

 Tests multiple copies of test2f running simultaneously.
 Test2f runs these with the same priority in order to show
 equal preference for each child process.  This means all the
 child processes will be stealing memory from each other.

 WARNING:  This test assumes tests 2e - 2f run successfully

 Z502_REG1, 2, 3, 4, 5  Used as return of process id's.
 Z502_REG6              Return of PID on GET_PROCESS_ID
 Z502_REG9              Used as return of error code.

 **************************************************************************/

#define         PRIORITY2G              10

void test2g(void) {
    static long    sleep_time = 100;//00;

    printf("This is Release %s:  Test 2g\n", CURRENT_REL);
    CREATE_PROCESS("test2g_a", test2f, PRIORITY2G, &Z502_REG1, &Z502_REG9);

    CREATE_PROCESS("test2g_b", test2f, PRIORITY2G, &Z502_REG2, &Z502_REG9);

    CREATE_PROCESS("test2g_c", test2f, PRIORITY2G, &Z502_REG3, &Z502_REG9);
    CREATE_PROCESS("test2g_d", test2f, PRIORITY2G, &Z502_REG4, &Z502_REG9);
    CREATE_PROCESS("test2g_e", test2f, PRIORITY2G, &Z502_REG5, &Z502_REG9);

    SuccessExpected(Z502_REG9, "CREATE_PROCESS");

    // In these next cases, we will loop until EACH of the child processes
    // has terminated.  We know it terminated because for a while we get 
    // success on the call GET_PROCESS_ID, and then we get failure when the 
    // process no longer exists.
    // We do this for each process so we can get decent statistics on completion

    Z502_REG9 = ERR_SUCCESS;      // Modified 12/2013 Rev 4.10
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test2g_a", &Z502_REG6, &Z502_REG9);
    }

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test2g_b", &Z502_REG6, &Z502_REG9);
    }


    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test2g_c", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test2g_d", &Z502_REG6, &Z502_REG9);
    }
    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(sleep_time);
        GET_PROCESS_ID("test2g_e", &Z502_REG6, &Z502_REG9);
    }

    TERMINATE_PROCESS(-2, &Z502_REG9); // Terminate all

}            // End test2g

/**************************************************************************

 Test2h starts up a number of processes who do tests of shared area.

 Z502_REG4  - process id of this process.
 Z502_REG5  - returned error code.
 Z502_REG6  - pid of target process.
 Z502_REG9  - returned error code from the GET_PROCESS_ID call.

 **************************************************************************/

#define           MOST_FAVORABLE_PRIORITY                       1
#define           NUMBER_2HX_PROCESSES                          5
#define           SLEEP_TIME_2H                             10000

void test2h(void) {
    INT32 trash;

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG9);
    printf("\n\nRelease %s:Test 2h: Pid %ld\n", CURRENT_REL, Z502_REG4);
    CHANGE_PRIORITY(-1, MOST_FAVORABLE_PRIORITY, &Z502_REG5);

    CREATE_PROCESS("first",  test2hx, 5, &trash, &Z502_REG5);
    CREATE_PROCESS("second", test2hx, 6, &trash, &Z502_REG5);
    CREATE_PROCESS("third",  test2hx, 7, &trash, &Z502_REG5);
    CREATE_PROCESS("fourth", test2hx, 8, &trash, &Z502_REG5);
    CREATE_PROCESS("fifth",  test2hx, 9, &trash, &Z502_REG5);

    // Loop here until the "2hx" final process terminate. 

    Z502_REG9 = ERR_SUCCESS;
    while (Z502_REG9 == ERR_SUCCESS) {
        SLEEP(SLEEP_TIME_2H);
        GET_PROCESS_ID("first", &Z502_REG6, &Z502_REG9);
    }
    TERMINATE_PROCESS(-2, &Z502_REG5);

}                                  // End of test2h

/**************************************************************************

 Test2hx - test shared memory usage.

 This test runs as multiple instances of processes; there are several
 processes who in turn manipulate shared memory.

 The algorithm used here flows as follows:

 o Get our PID and print it out.
 o Use our PID to determine the address at which to start shared
 area - every process will have a different starting address.
 o Define the shared area.
 o Fill in initial portions of the shared area by:
   + No locking required because we're modifying only our portion of
     the shared area.
   + Determine which location within shared area is ours by using the
     Shared ID returned by the DEFINE_SHARED_AREA.  The OS will return
     a unique ID for each caller.
   + Fill in portions of the shared area.
 o Sleep to let all 2hx PIDs start up.

 o If (shared_index == 0)   This is the MASTER Process

 o LOOP many times doing the following steps:
   + We DON'T need to lock the shared area since the SEND/RECEIVE
     act as synchronization.
   + Select a target process
   + Communicate back and forth with the target process.
   + SEND_MESSAGE( "next", ..... );
 END OF LOOP
 o Loop through each Slave process telling them to terminate

 o If (shared_index > 0)   This is a SLAVE Process
 o LOOP Many times until we get terminate message
   + RECEIVE_MESSAGE( "-1", ..... )
   + Read my mailbox and communicate with master
   + Print out lots of stuff
   + Do lots of sanity checks
   + If MASTER tells us to terminate, do so.
 END OF LOOP

 o If (shared_index == 0)   This is the MASTER Process
 o sleep                      Until everyone is done
 o print the whole shared structure

 o Terminate the process.

 **************************************************************************/

#define           NUMBER_MASTER_ITERATIONS    28
#define           PROC_INFO_STRUCT_TAG        1234
#define           SHARED_MEM_NAME             "almost_done!!\0"

// This is the per-process area for each of the processes participating
// in the shared memory.
typedef struct {
    INT32 structure_tag;      // Unique Tag so we know everything is right
    INT32 Pid;                // The PID of the slave owning this area
    INT32 TerminationRequest; // If non-zero, process should terminate.
    INT32 MailboxToMaster;    // Data sent from Slave to Master
    INT32 MailboxToSlave;     // Data sent from Master To Slave
    INT32 WriterOfMailbox;    // PID of process who last wrote in this area
} PROCESS_INFO;

// The following structure will be laid on shared memory by using
// the MEM_ADJUST   macro                                          

typedef struct {
    PROCESS_INFO proc_info[NUMBER_2HX_PROCESSES + 1];
} SHARED_DATA;

// We use this local structure to gather together the information we
// have for this process.
typedef struct {
    INT32    StartingAddressOfSharedArea;     // Local Virtual Address
    INT32    PagesInSharedArea;               // Size of shared area
            // How OS knows to put all in same shared area
    char    AreaTag[32];        
            // Unique number supplied by OS. First must be 0 and the
            // return values must be monotonically increasing for each
            // process that's doing the Shared Memory request.
    INT32    OurSharedID;   
    INT32    TargetShared;                   // Shared ID of slave we're sending to
    long     TargetPid;                      // Pid of slave we're sending to
    INT32    ErrorReturned;

    long     SourcePid;
    char     ReceiveBuffer[20];
    long     MessageReceiveLength;
    INT32    MessageSendLength;
    INT32    MessageSenderPid;
} LOCAL_DATA;

// This MEM_ADJUST macro allows us to overlay the SHARED_DATA structure
// onto the shared memory we've defined.  It generates an address
// appropriate for use by READ and MEM_WRITE.

#define         MEM_ADJUST( arg )                                       \
  (long)&(shared_ptr->arg) - (long)(shared_ptr)                             \
                      + (long)ld->StartingAddressOfSharedArea

#define         MEM_ADJUST2( shared, local, arg )                                       \
  (long)&(shared->arg) - (long)(shared)                             \
                      + (long)local->StartingAddressOfSharedArea

// This allows us to print out the shared memory for debugging purposes
void   PrintTest2hMemory( SHARED_DATA *sp, LOCAL_DATA *ld )   {
    int  Index;
    INT32  Data1, Data2, Data3, Data4, Data5;

    printf("\nNumber of Masters + Slaves = %d\n", (int)NUMBER_2HX_PROCESSES );

    for (Index = 0; Index < NUMBER_2HX_PROCESSES; Index++) {
        MEM_READ(MEM_ADJUST2( sp, ld, proc_info[ Index ].structure_tag), &Data1);
        MEM_READ(MEM_ADJUST2( sp, ld, proc_info[ Index ].Pid ), &Data2);
        MEM_READ(MEM_ADJUST2( sp, ld, proc_info[ Index ].MailboxToMaster ), &Data3);
        MEM_READ(MEM_ADJUST2( sp, ld, proc_info[ Index ].MailboxToSlave ), &Data4);
        MEM_READ(MEM_ADJUST2( sp, ld, proc_info[ Index ].WriterOfMailbox), &Data5);

        printf("Mailbox info for index %d:\n", Index);
        printf("\tIndex = %d, Struct Tag = %d,  ", Index, Data1 );
        printf(" Pid =  %d,  Mail To Master = %d, ", Data2, Data3 );
        printf(" Mail To Slave =  %d,  Writer Of Mailbox = %d\n", Data4, Data5 );
    }          // END of for Index
}      // End of PrintTest2hMemory

void test2hx(void) {
    // The declaration of shared_ptr is only for use by MEM_ADJUST macro.
    // It points to a bogus location - but that's ok because we never
    // actually use the result of the pointer, only its offset.

    LOCAL_DATA *ld;
    SHARED_DATA *shared_ptr = 0;
    int        Index;
    INT32      ReadWriteData;    // Used to move to and from shared memory

    ld = (LOCAL_DATA *) calloc(1, sizeof(LOCAL_DATA));
    if ( ld == 0) {
        printf("Unable to allocate memory in test2hx\n");
    }
    strcpy(ld->AreaTag, SHARED_MEM_NAME);

    GET_PROCESS_ID("", &Z502_REG4, &Z502_REG5);
    printf("\n\nRelease %s:Test 2hx: Pid %ld\n", CURRENT_REL, Z502_REG4);

    // As an interesting wrinkle, each process should start
    // its shared region at a somewhat different virtual address;
    // determine that here.
    ld->StartingAddressOfSharedArea = (Z502_REG4 % 17) * PGSIZE;

    // This is the number of pages required in the shared area.
    ld->PagesInSharedArea = sizeof(SHARED_DATA) / PGSIZE + 1;

    // Now ask the OS to map us into the shared area
    DEFINE_SHARED_AREA(
       (long)ld->StartingAddressOfSharedArea,   // Input - our virtual address
       (long)ld->PagesInSharedArea,             // Input - pages to map
       (long)ld->AreaTag,                       // Input - ID for this shared area
       &ld->OurSharedID,                  // Output - Unique shared ID
       &ld->ErrorReturned);              // Output - any error
    SuccessExpected(ld->ErrorReturned, "DEFINE_SHARED_AREA");

    ReadWriteData = PROC_INFO_STRUCT_TAG; // Sanity data 
    MEM_WRITE(MEM_ADJUST(proc_info[ld->OurSharedID].structure_tag),
            &ReadWriteData);

    ReadWriteData = Z502_REG4; // Store PID in our slot 
    MEM_WRITE(MEM_ADJUST(proc_info[ld->OurSharedID].Pid), &ReadWriteData );
    ReadWriteData = 0;         // Initialize this counter
    MEM_WRITE(MEM_ADJUST(proc_info[ld->OurSharedID].MailboxToMaster), &ReadWriteData );
    ReadWriteData = 0;         // Initialize this counter
    MEM_WRITE(MEM_ADJUST(proc_info[ld->OurSharedID].MailboxToSlave), &ReadWriteData );

    //  This is the code used ONLY by the MASTER Process
    if ( ld->OurSharedID == 0 )  {  //   We are the MASTER Process
        // Loop here the required number of times
        for ( Index = 0; Index < NUMBER_MASTER_ITERATIONS; Index++ )  {

            // Wait for all slaves to start up - we assume after the sleep
            // that the slaves are no longer modifying their shared areas.
            SLEEP(1000); // Wait for slaves to start

            // Get slave ID we're going to work with - be careful here - the
            // code further on depends on THIS algorithm
            ld->TargetShared = ( Index % ( NUMBER_2HX_PROCESSES  - 1 ) ) + 1;

            // Read the memory of that slave to make sure it's OK
            MEM_READ(MEM_ADJUST(proc_info[ld->TargetShared].structure_tag),
                    &ReadWriteData);
            if ( ReadWriteData != PROC_INFO_STRUCT_TAG) {
                printf("We should see a structure tag, but did not\n");
                printf("This means that this memory is not mapped \n");
                printf("consistent with the memory used by the writer\n");
                printf("of this structure.  It's a page table problem.\n");
            }
            // Get the pid of the process we're working with
            MEM_READ(MEM_ADJUST(proc_info[ld->TargetShared].Pid), &ld->TargetPid);

            // We're sending data to the Slave
            MEM_WRITE(MEM_ADJUST(proc_info[ld->TargetShared].MailboxToSlave), &Index );
            MEM_WRITE(MEM_ADJUST(proc_info[ld->TargetShared].WriterOfMailbox), &Z502_REG4 );
            ReadWriteData = 0;   // Do NOT terminate
            MEM_WRITE(MEM_ADJUST(proc_info[ld->TargetShared].TerminationRequest), &ReadWriteData );
            printf("Sender %ld to Receiver %d passing data %d\n", Z502_REG4,
                    (int)ld->TargetPid, Index);

            // Check the iteration count of the slave.  If it tells us it has done a
            // certain number of iterations, then tell it to terminate itself.
            MEM_READ(MEM_ADJUST(proc_info[ld->TargetShared].MailboxToMaster),
                    &ReadWriteData );
            if ( ReadWriteData >= ( NUMBER_MASTER_ITERATIONS / (NUMBER_2HX_PROCESSES - 1) ) -1 ) {
                ReadWriteData = 1;   // Do terminate
                MEM_WRITE(MEM_ADJUST(proc_info[ld->TargetShared].TerminationRequest), &ReadWriteData );
                printf("Master is sending termination message to PID %d\n", (int)ld->TargetPid );
            }

            // Now we are done with this slave - send it a message which will start it working.
            // The iterations may not be quite right - we may be sending a message to a
            // process that's already terminated, but that's OK
            SEND_MESSAGE(ld->TargetPid, " ", 0, &ld->ErrorReturned);
        }     // End of For Index
    }     // End of MASTER PROCESS


    // This is the start of the slave process work
    if ( ld->OurSharedID != 0 )  {  //   We are a SLAVE Process
        // The slaves keep going forever until the master tells them to quit
        while ( TRUE )  {

            ld->SourcePid = -1; // From anyone
            ld->MessageReceiveLength = 20;
            RECEIVE_MESSAGE(ld->SourcePid, 
                            ld->ReceiveBuffer,
                            ld->MessageReceiveLength, 
                            &ld->MessageSendLength,
                            &ld->MessageSenderPid, 
                            &ld->ErrorReturned);
            SuccessExpected(ld->ErrorReturned, "RECEIVE_MESSAGE");
    
            // Make sure we have our memory mapped correctly
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].structure_tag),
                    &ReadWriteData );
            if ( ReadWriteData != PROC_INFO_STRUCT_TAG) {
                printf("We should see a structure tag, but did not.\n");
                printf("This means that this memory is not mapped \n");
                printf("consistent with the memory used when WE wrote\n");
                printf("this structure.  It's a page table problem.\n");
            }
    
            // Get the value placed in shared memory and compare it with the PID provided
            // by the messaging system.
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].WriterOfMailbox), &ReadWriteData );
            if ( ReadWriteData != ld->MessageSenderPid ) {
                printf("ERROR: ERROR: The sender PID, given by the \n");
                printf("RECEIVE_MESSAGE and by the mailbox, don't match\n");
            }
    
            // We're receiving data from the Master
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].MailboxToSlave), &ReadWriteData );
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].WriterOfMailbox), &ld->MessageSenderPid );
            printf("Receiver %ld got message from %d passing data %d\n", Z502_REG4,
                        ld->MessageSenderPid, ReadWriteData);

            // See if we've been asked to terminate
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].TerminationRequest), &ReadWriteData );
            if (ReadWriteData > 0 )  {
                printf( "Process %ld received termination message\n", Z502_REG4 );
                TERMINATE_PROCESS(-1, &Z502_REG9);
            }

            // Increment the number of iterations we've done.  This will ultimately lead
            // to the master telling us to terminate.
            MEM_READ(MEM_ADJUST(proc_info[ld->OurSharedID].MailboxToMaster), &ReadWriteData );
            ReadWriteData++;
            MEM_WRITE(MEM_ADJUST(proc_info[ld->OurSharedID].MailboxToMaster), &ReadWriteData );

        }  //End of while TRUE
    }      // End of SLAVE

    // The Master comes here and prints out the entire shared area    

    if (ld->OurSharedID == 0) {             // The slaves should terminate before this.
        SLEEP(5000);                        // Wait for msgs to finish
        printf("Overview of shared area at completion of Test2h\n");
        PrintTest2hMemory( shared_ptr, ld );
        TERMINATE_PROCESS(-1, &Z502_REG9);
    }              // END of if
    TERMINATE_PROCESS(-2, &Z502_REG9);

}                                // End of test2hx   
/**************************************************************************

 test2f_Statistics   This is designed to give an overview of how the
 paging algorithms are working.  It is especially useful when running
 test2g because it will enable us to better understand the global policy
 being used for allocating pages.

 **************************************************************************/

#define  MAX2F_PID    20
#define  REPORT_GRANULARITY_2F     100
void Test2f_Statistics(int Pid) {
    static short NotInitialized = TRUE;
    static int   PagesTouched[MAX2F_PID];
    int i;
    int LowestReportingPid = -1;
    if ( NotInitialized )  {
        for ( i = 0; i < MAX2F_PID; i++ )
            PagesTouched[i] = 0;
        NotInitialized = FALSE;
    }
    if ( Pid >= MAX2F_PID )  {
        printf( "In Test2f_Statistics - the pid entered, ");
        printf( "%d, is larger than the maximum allowed", Pid );
        exit(0);
    }
    PagesTouched[Pid]++;
    // It's time to print out a report
    if ( PagesTouched[Pid] % REPORT_GRANULARITY_2F == 0 )  {
        i = 0;
        while( PagesTouched[i] == 0 ) i++;
        LowestReportingPid = i;
        if ( Pid == LowestReportingPid ) {
            printf("----- Report by test2f - Pid %d -----\n", Pid );
            for ( i = 0; i < MAX2F_PID; i++ )  {
                if ( PagesTouched[i] > 0 )  
                    printf( "Pid = %d, Pages Touched = %d\n", i, PagesTouched[i] );
            }
        }
    }
}                 // End of Test2f_Statistics
/**************************************************************************

 GetSkewedRandomNumber   Is a homegrown deterministic random
 number generator.  It produces  numbers that are NOT uniform across
 the allowed range.
 This is useful in picking page locations so that pages
 get reused and makes a LRU algorithm meaningful.
 This algorithm is VERY good for developing page replacement tests.

 **************************************************************************/

#define                 SKEWING_FACTOR          0.60
void GetSkewedRandomNumber(long *random_number, long range) {
    double temp, temp2, temp3;
    long extended_range = (long) pow(range, (double) (1 / SKEWING_FACTOR));
    double multiplier = (double)extended_range / (double)RAND_MAX;

    temp = multiplier * (double) rand();
    if (temp < 0)
        temp = -temp;
    temp2 = (double) ((long) temp % extended_range);
    temp3 = pow(temp2, (double) SKEWING_FACTOR);
    *random_number = (long) temp3;
    //printf( "GSRN RAND_MAX Extended, temp, temp2, temp3 %6d %6ld  %6d  %6d  %6d\n", 
    //	   RAND_MAX, extended_range, (int)temp, (int)temp2, (int)temp3 );
} // End GetSkewedRandomNumber 

void GetRandomNumber(long *random_number, long range) {

    *random_number = (long) rand() % range;
} // End GetRandomNumber 

/*****************************************************************
 testStartCode()
 A new thread (other than the initial thread) comes here the
 first time it's scheduled.
 *****************************************************************/
void testStartCode() {
    void (*routine)(void);
    routine = (void (*)(void)) Z502PrepareProcessForExecution();
    (*routine)();
    // If we ever get here, it's because the thread ran to the end
    // of a test program and wasn't terminated properly.
    printf("ERROR:  Simulation did not end correctly\n");
    exit(0);
}

/*****************************************************************
 main()
 This is the routine that will start running when the
 simulator is invoked.
 *****************************************************************/
int main(int argc, char *argv[]) {
    int i;
    for (i = 0; i < MAX_NUMBER_OF_USER_THREADS; i++) {
        Z502CreateUserThread(testStartCode);
    }

    osInit(argc, argv);
    // We should NEVER return from this routine.  The result of
    // osInit is to select a program to run, to start a process
    // to execute that program, and NEVER RETURN!
    return (-1);
}    // End of main

