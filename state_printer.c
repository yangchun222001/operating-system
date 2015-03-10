
/****************************************************************************

    state_printer.c

    Read Appendix D - output generation - in order to understand how
    to use these routines.

    The first portion of this file is SCHEDULER_PRINTER.

    Revision History:
    1.1    December 1990    Initial release
    2.1    April 2001       Added memory_printer
    2.2    July  2002       Make code appropriate for undergrads.
    3.0    August  2004:    Modified to support memory mapped IO
    3.60   August  2012:    Used student supplied code to add support
                            for Mac machines
    4.10   December 2013:   Remove SP_setup_file and SP_setup_action
                            Roll SP_ACTION_MODE into the routine SP_setup
			    Define some new states.
****************************************************************************/

#include                 "global.h"
#include                 "syscalls.h"
#include                 "z502.h"
#include                 "protos.h"
#include                 "stdio.h"
#include                 "string.h"
#if defined LINUX || defined MAC
#include                 <unistd.h>
#endif

INT16           SP_target_pid = -1;
INT16           SP_pid_states[SP_NUMBER_OF_STATES][SP_MAX_NUMBER_OF_PIDS];
INT16           SP_number_of_pids[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
INT32           SP_time = -1;
char            SP_action[ SP_LENGTH_OF_ACTION + 1 ] = { '\0' };

FILE            *SP_file_ptr;

char            *mode_name[] = {"NEW:", "RUNNING:", "READY  :",
                         "SUS-PRC:",  "SUS-TMR:", "SUS-DSK:", "TERM'D: " };
//
//      This string is printed out when requested as the header 

#define         SP_HEADER_STRING        \
" Time Target Action  Run New Done       State Populations \n"


short    SP_do_output( char *text );      // Prototype
/****************************************************************************
        SP_setup_action

        This routine allows the caller to set up a string indicating
        the action that will be performed.

****************************************************************************/

short SP_setup_action( INT16 mode, char *data ) {
    if ( mode != SP_ACTION_MODE ) {
        printf( "Illegal mode specified in SP_setup_action.\n" );
        return 1;
    }
    if ( strlen( data) > SP_LENGTH_OF_ACTION ) {
        printf( "Too many characters in string entered in SP_setup.\n" );
        return 1;
    }
    (void)strncpy( SP_action, data, SP_LENGTH_OF_ACTION);
    return 0;
}      // End of SP_setup_action

/****************************************************************************
        SP_setup

        This routine allows the caller to set up all the information
        that will eventually be printed.
        NOTE: The meaning of data passed in here depends on the mode specified.

****************************************************************************/

short SP_setup( INT16 mode, INT32 data ) {
    INT16       index;

    switch( mode ) {
	case SP_ACTION_MODE:
	    printf("Illegal use of SP_ACTION_MODE for SP_setup\n");
	    break;
        case SP_TIME_MODE:
            if ( data < 0 ) {
                 printf( "Negative TIME entered in SP_setup.\n" );
                 return 1;
            }
            SP_time            = (INT32)data % 1000000L;
            break;
        case SP_TARGET_MODE:
             if ( data < 0 || data > 99 ) {
                 printf( "Expected PID not in range 0 - 99 in SP_setup.\n" );
                 return 1;
             }

             SP_target_pid      = (INT16)data;
             break;
        case SP_NEW_MODE:
        case SP_RUNNING_MODE:
        case SP_READY_MODE:
        case SP_PROCESS_SUSPENDED_MODE:
        case SP_TIMER_SUSPENDED_MODE:
        case SP_DISK_SUSPENDED_MODE:
        case SP_TERMINATED_MODE:
             if ( data < 0 || data > 99 ) {
                  printf( "Expected PID not in range 0 - 99 in SP_setup.\n" );
                  return 1;
             }
             index = SP_number_of_pids[mode - SP_STATE_MODE_START];
             if ( index >= SP_MAX_NUMBER_OF_PIDS ) {
                   printf( "Too many (more than %d) PIDs ", SP_MAX_NUMBER_OF_PIDS );
                   printf( "for this mode entered in SP_setup.\n" );
                   return 1;
              }
              SP_pid_states[mode - SP_STATE_MODE_START][index]  = (INT16)data;
              SP_number_of_pids[ mode - SP_STATE_MODE_START ]++;
              break;
        default:
              printf( "ERROR - illegal mode used in SP_setup\n" );
    }

    return 0;
}                                               /* End of SP_setup      */


/****************************************************************************

        SP_print_line

        SP_print_line prints out all the data that has been sent to the
        routine via various SP_setup commands. After the print is complete,
        the setup data  is deleted.

****************************************************************************/

short    SP_print_line( void ) {
    char        output_line[400];
    char        temp[12];
    INT16       index;
    INT16       ind;
    INT16       pos;
    BOOL        mode_print_done;
    INT32       current_time;

    // print out the header
    SP_do_output( SP_HEADER_STRING );

    sprintf( output_line, "%5d", SP_time );             /* Time         */
    if ( SP_time == -1 ) {
        MEM_READ( Z502ClockStatus, &current_time );
        sprintf( output_line, "%5d", current_time % 100000 );
    }

    sprintf( temp, " %3d ", SP_target_pid );            /* Target Pid   */
    if ( SP_target_pid < 0 )
        sprintf( temp, "%s", "     " );
    (void)strcat( output_line, temp );

    sprintf( temp, " %8s", SP_action );                 /* Action       */
    (void)strcat( output_line, temp );

    index = SP_RUNNING_MODE - SP_STATE_MODE_START;      /* Running proc.*/
    if ( SP_number_of_pids[ index ] > 0 )
        sprintf( temp, " %3d", SP_pid_states[index][0] );
    else
        sprintf( temp, "    " );
    (void)strcat( output_line, temp );

    index = SP_NEW_MODE - SP_STATE_MODE_START;          /* New proc.*/
    if ( SP_number_of_pids[ index ] > 0 )
        sprintf( temp, " %3d ", SP_pid_states[index][0] );
    else
        sprintf( temp, "    " );
    (void)strcat( output_line, temp );

    index = SP_TERMINATED_MODE - SP_STATE_MODE_START;   /* Done proc.*/
    if ( SP_number_of_pids[ index ] > 0 )
        sprintf( temp, "%3d ", SP_pid_states[index][0] );
    else
        sprintf( temp, "     " );
    (void)strcat( output_line, temp );
    SP_do_output( output_line );
    strcpy( output_line, "" );

    mode_print_done = FALSE;
    for ( ind = SP_READY_MODE; ind <= SP_TERMINATED_MODE; ind++ ) {
        index = ind - SP_STATE_MODE_START;
        if ( SP_number_of_pids[ index ] > 0 ) {
            (void)strcat( output_line, mode_name[ index ] );
            for (pos = 0; pos < SP_number_of_pids[index]; pos++ ) {
                sprintf( temp, " %d", SP_pid_states[index][pos] );
                (void)strcat( output_line, temp );
            }
            mode_print_done = TRUE;
            (void)strcat( output_line, "\n" );
            SP_do_output( output_line );
            strcpy( output_line, "                                 " );
        }                                               /* End of if */
    }                                                   /* End of for*/
    if ( mode_print_done == FALSE )
        SP_do_output( "\n" );

    /*                  Initialize everything                           */

    SP_time = -1;
    strcpy( SP_action, "" );
    SP_target_pid = -1;
    for ( index = 0 ; index <= SP_NUMBER_OF_STATES; index++ )
        SP_number_of_pids[ index ] = 0;
    return 0;
}                                               /* End of SP_print_line */

/****************************************************************************
        SP_do_output

        This little routine simply directs output Cto the screen.  

****************************************************************************/

short    SP_do_output( char *text ) {
    printf( "%s", text );
    return 0;
}                                               /* End of SP_do_output */

/****************************************************************************

        Read Appendix D - output generation - in order to understand how
        to use these routines.

        The second portion of this file is MEMORY_PRINTER.

        This is the definition of the table we will produce here.

A  Frame  0000000000111111111122222222223333333333444444444455555555556666
B  Frame  0123456789012345678901234567890123456789012345678901234567890123
C  PID    0000000001111
D  VPN    0000000110000
E  VPN    0000111000000
F  VPN    0000000220000
G  VPN    0123567230123
H  VMR    7775555447777

     The rows mean the following:
     A - B:     The frame number.  Note how the first column is "00" and the
                last column is "63".
     C:         The Process ID of the process having it's virtual page in the
                frame table.
     D - G:     The virtual page number of the process that's mapped to that
                frame.  Again the number (from 0 to 1023 possible) is written
                vertically.
     H:         The state of the page.   Valid = 4, Modified = 2,
                Referenced = 1.  These are OR'd (or added) together.

     Example: The page in frame 6 is virtual page 107 in  process 0.  That
                page has been made valid and has been referenced.
****************************************************************************/

/****************************************************************************
    Here's the structure we're going to fill in containing all the info to
        be printed.
****************************************************************************/

typedef struct {
    INT32           contains_data;
    INT32           pid;
    INT32           logical_page;
    INT32           state;
}MP_FRAME_ENTRY;

typedef struct {
    MP_FRAME_ENTRY  entry[PHYS_MEM_PGS];
}MP_FRAME_TABLE;

MP_FRAME_TABLE  MP_ft;

void    MP_initialize( void );

/****************************************************************************

        MP_setup

        This routine allows the caller to set up all the information
        that will eventually be printed.
        The type of data passed in here depends on the mode specified.

****************************************************************************/

short MP_setup( INT32 frame, INT32 pid, INT32 logical_page, INT32 state ) {
    short static            first_time = TRUE;

    if ( first_time == TRUE ) {
        MP_initialize( );
        first_time = FALSE;
    }

    if ( frame < 0 || frame >= PHYS_MEM_PGS ) {
        printf( "Frame value %d is not in the range 0 - %d in MP_setup\n",
                    frame, PHYS_MEM_PGS - 1 );
        return 1;
    }
    if ( pid < 0 || pid > 9 ) {
        printf( "Input PID %d not in range 0 - 9 in MP_setup.\n", pid );
        return 1;
    }
    if ( logical_page < 0 || logical_page >= VIRTUAL_MEM_PAGES ) {
        printf( "Input logical page (%d) not in range 0 - %d in MP_setup.\n",
                        logical_page, VIRTUAL_MEM_PAGES - 1 );
        return 1;
    }
    if ( state < 0 || state > 7 ) {
        printf( "Input state %d not in range 0 - 7 in MP_setup\n", state );
        return 1;
    }
    if ( logical_page > 0 || pid > 0 || state > 0 ) {   // Check there's data 2014
        MP_ft.entry[frame].contains_data = TRUE;
        MP_ft.entry[frame].logical_page  = logical_page;
        MP_ft.entry[frame].pid           = pid;
        MP_ft.entry[frame].state         = state;
    }
    return 0;
}


/****************************************************************************

        MP_print_line

        Outputs everything we know about the state of the physical memory.

****************************************************************************/

short    MP_print_line( void ) {
    INT32   index;
    INT32   temp;
    char    output_line3[PHYS_MEM_PGS+5];
    char    output_line4[PHYS_MEM_PGS+5];
    char    output_line5[PHYS_MEM_PGS+5];
    char    output_line6[PHYS_MEM_PGS+5];
    char    output_line7[PHYS_MEM_PGS+5];
    char    output_line8[PHYS_MEM_PGS+5];

//  Header Line 
    SP_do_output("\n                       PHYSICAL MEMORY STATE\n");

//  First Line 
    SP_do_output("Frame 0000000000111111111122222222223333333333444444444455555555556666\n");

//  Second Line 
    SP_do_output("Frame 0123456789012345678901234567890123456789012345678901234567890123\n");

//  Third - Eighth Line
    strcpy( output_line3, "                                                                 \n" );
    strcpy( output_line4, "                                                                 \n" );
    strcpy( output_line5, "                                                                 \n" );
    strcpy( output_line6, "                                                                 \n" );
    strcpy( output_line7, "                                                                 \n" );
    strcpy( output_line8, "                                                                 \n" );

    for ( index = 0; index < PHYS_MEM_PGS; index++ ) {
        if ( MP_ft.entry[index].contains_data == TRUE ) {
            output_line3[index] = (char)(MP_ft.entry[index].pid + 48);
            temp                = MP_ft.entry[index].logical_page;
            output_line4[index] = (char)(temp / 1000 ) +48;
            output_line5[index] = (char)((temp / 100 ) % 10) +48;
            output_line6[index] = (char)((temp / 10  ) % 10) +48;
            output_line7[index] = (char)((temp       ) % 10) +48;
            output_line8[index] = (char)MP_ft.entry[index].state +48;
        }
    }
    SP_do_output( "PID   " );  SP_do_output( output_line3 );
    SP_do_output( "VPN   " );  SP_do_output( output_line4 );
    SP_do_output( "VPN   " );  SP_do_output( output_line5 );
    SP_do_output( "VPN   " );  SP_do_output( output_line6 );
    SP_do_output( "VPN   " );  SP_do_output( output_line7 );
    SP_do_output( "VMR   " );  SP_do_output( output_line8 );
    MP_initialize( );
    return 0;
}

/****************************************************************************

        MP_initialize

        Simply zero out the structure used for holding data.

****************************************************************************/

void    MP_initialize( void ) {
    short   index;
    for ( index = 0; index < PHYS_MEM_PGS; index++ )
        MP_ft.entry[index].contains_data = FALSE;
}
