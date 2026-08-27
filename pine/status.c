#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c,v 4.13 1993/11/19 04:10:59 mikes Exp $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"

   Copyright 1989-1993  University of Washington

    Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee to the University of
   Washington is hereby granted, provided that the above copyright notice
   appears in all copies and that both the above copyright notice and this
   permission notice appear in supporting documentation, and that the name
   of the University of Washington not be used in advertising or publicity
   pertaining to distribution of the software without specific, written
   prior permission.  This software is made available "as is", and
   THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
   WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
   NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
   INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
   (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
   WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  
   Pine and Pico are trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior
   written permission of the University of Washington.

   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
     status.c
     Functions that manage the status line (third from the bottom)
       - put messages on the queue to be displayed
       - display messages on the queue with timers 
       - check queue to figure out next timeout
       - prompt for yes/no type of questions
  ====*/

#include "headers.h"

int  output_message(char *);
void radio_help(int, int, HelpType);
void draw_radio_prompt(int, int, char *, int, HelpType);



/*----------------------------------------------------------------------
     Manage the second line from the bottom where status and error messages
re displayed. A small queue is set up and messages are put on the queue
by calling on of the q_status_message routines. Even though this is a queue
most of the time message will go right on through. The messages are 
displayed just before the read for the next command, or when a read time
out. Read timeouts occur every minute or so for new mail checking and every
few seconds when the are still messages on the queue. Hopefully this scheme 
will not let messages fly past the usr can't see.
  ----------------------------------------------------------------------*/
struct message{
    char text[MAX_SCREEN_COLS+1];
    long min_display_time, max_display_time;
};

#define QUEUE_LENGTH 6


static struct message  message_queue[QUEUE_LENGTH];
static struct message  currently_displayed;
static int             message_is_displayed = 0,
                       queue_length = 0,
                       needs_clearing = 0; /* Flag set by want_to()
                                              and optionally_enter() */
static long            displayed_time;


/*----------------------------------------------------------------------
        Put a message for the status line on the queue

  Args: time    -- the min time in seconds to display the message
        message -- message string

  Result: queues message on queue represented by static variables

    This puts a single message on the queue to be shown.
  ----------*/
void
q_status_message(modal, min_time, max_time, message)
     char *message;
     int   min_time,max_time, modal;
{
    strncpy(message_queue[queue_length].text, message, MAX_SCREEN_COLS);
    (message_queue[queue_length].text)[MAX_SCREEN_COLS] = '\0';

    /* save the display time length */
    message_queue[queue_length].min_display_time = min_time;
    message_queue[queue_length].max_display_time = max_time;


    /* Bump up the queue length if it's not full. (If there's more than
         6 messages we just over write last message */
    if(queue_length < QUEUE_LENGTH - 1 )
      queue_length++;
}


/*----------------------------------------------------------------------
        Put a message with 1 printf argument on queue for status line
 
    Args: min_t -- minimum time to display message for
          max_t -- minimum time to display message for
          s -- printf style control string
          a -- argument for printf
 
   Result: message queued
  ----*/

/*VARARGS1*/
void
q_status_message1(modal, min_t, max_t, s, a)
     int   min_t, max_t, modal;
     char *s;
     void *a;
{
    /* every q_status_messageN() call site passes a compile-time
     * literal for s -- this whole family is a fixed-arity printf
     * wrapper (see the VARARGS1 comments above each). */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    sprintf(tmp_20k_buf,s,a);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    q_status_message(modal, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 2 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message2(modal, min_t, max_t, s, a1, a2)
     int   modal;
     int   min_t, max_t;
     char *s;
     void *a1, *a2;
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    sprintf(tmp_20k_buf,s, a1, a2);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    q_status_message(modal, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 3 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message3(modal, min_t, max_t, s, a1, a2, a3)
     int   min_t, max_t, modal;
     char *s;
     void *a1, *a2, *a3;
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    sprintf(tmp_20k_buf,s, a1, a2, a3);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    q_status_message(modal, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 4 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf

  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message4(modal, min_t, max_t, s, a1, a2, a3, a4)
     int   min_t, max_t, modal;
     char *s;
     void *a1, *a2, *a3, *a4;
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	sprintf(tmp_20k_buf,s, a1, a2, a3, a4);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	q_status_message(modal, min_t, max_t, tmp_20k_buf);
}


/*----------------------------------------------------------------------
        Put a message with 7 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf
          a5 -- argument for printf
          a6 -- argument for printf
          a7 -- argument for printf


  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message7(modal, min_t, max_t, s, a1, a2, a3, a4, a5, a6, a7)
     int   modal;
     int   min_t, max_t;
     char *s;
     void *a1, *a2, *a3, *a4, *a5, *a6, *a7;
{
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
	sprintf(tmp_20k_buf, s, a1, a2, a3, a4, a5, a6, a7);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
	q_status_message(modal, min_t, max_t, tmp_20k_buf);
}


/*----------------------------------------------------------------------
     Mark the status line as dirty so it gets cleared next chance
 ----*/
void
mark_status_dirty()
{
    needs_clearing++;
}


     
/*----------------------------------------------------------------------
        Find out how many messages are on queue to be displayed

  Args:   none

  Result: time to display currently displayed message if there
          are more messages waiting to be displayed. 1 even if time
          for current status message is 0.

  This is used to know how long to set the timer for the next message
  to be displayed. If the message time is 0 return 1 instead so the alarm
  will work and all. Could go to more precise sub second timers, but try it
  this way first.
  ---------*/
int
messages_queued()
{
    return(queue_length <= 0 ? 0 : 
           currently_displayed.min_display_time == 0 ?
                                  1: currently_displayed.min_display_time);
}



/*----------------------------------------------------------------------
       Update status line, clearing or displaying a message

   Arg: command -- The command that is about to be executed

  Result:  status line cleared or
             next message queued is displayed or
             current message is redisplayed.

   This is called when ready to display the next message, usually just
before reading the next command from the user. We pass in the nature
of the command because it affects what we do here. If the command just
executed by the user is a redraw screen, we don't want to reset or go to 
next message because it might not have been seen.  Also if the  command
is just a noop, which are usually executed when checking for new mail 
and happen every few minutes, we don't clear the message. If it was
really a command and there's nothing more to show, then we clear, because 
know the user has seen the message. In this case the user might be typing 
commands very quickly and miss a message, so there is a time stamp and
time check that each message has been on the screen for a few seconds. 
If it hasn't we just return and let if be taken care of next time.

At slow terminal output speeds all of this can be for not, the amount
of time it takes to paint the screen when the whole screen is being painted
is greater than the second or two delay so the time stamps set here have
nothing to do with when the user actually sees the message.
----------------------------------------------------------------------*/
void
display_message(command)
     int command;
{
    long now, diff;

    if(ps_global == NULL  || ps_global->ttyo == NULL ||
       ps_global->ttyo->screen_rows <= 1 || ps_global->in_init_seq)
      return;

    now = time(0);

    if(message_is_displayed) {
	/* message is showing */
	if(command == ctrl('L')) {
	    output_message(currently_displayed.text);
	} else {
	    now = time(0);
	    /* must be displayed for at least 2 seconds */
	    diff = (displayed_time +
                    (command == NO_OP_COMMAND || command == NO_OP_IDLE ?
                       currently_displayed.max_display_time : 
                         currently_displayed.min_display_time)
                    ) - now;
            if(diff > 0)
              return;
	    if(queue_length > 0){
                do {
                     /* Dequeue message and display next */
                    currently_displayed = message_queue[0];
                    d_q_status_message();
                    output_message(currently_displayed.text);
                    message_is_displayed = 1;
                    displayed_time       = time(0);
                } while(queue_length>0 &&
                          currently_displayed.min_display_time==0);
    	    } else {
    	        if(command != NO_OP_COMMAND && command != NO_OP_IDLE) {
    		    /* Clear the current message and dequeue it */
		    MoveCursor(ps_global->ttyo->screen_rows - 3, 0);
		    CleartoEOLN();
		    queue_length         = 0;
		    message_is_displayed = 0;
		}
	    }
	}
    } else {
	/* No message is currently displayed */
	if(queue_length > 0) {
            do{
                currently_displayed = message_queue[0];
                d_q_status_message();
	        message_is_displayed = 1;
    	        output_message(currently_displayed.text);
	        displayed_time       = time(0);
            } while(queue_length > 0 &&
                    currently_displayed.min_display_time == 0);
	} else {
	    if(command == ctrl('L') || needs_clearing) {
	        MoveCursor(ps_global->ttyo->screen_rows - 3, 0);
	        CleartoEOLN();
	    }
	}
    }
    needs_clearing = 0; /* always cleared or overwritten */
    fflush(stdout);
}


/*----------------------------------------------------------------------
     Display all the messages on the queue as quickly as possible

     This is only called from pine_send()'s composer loop, right before
     the screen is handed over to the composer subprocess. It used to
     sleep() for each message's min_display_time to guarantee the user
     saw it (e.g. "Folder \"INBOX\" opened with N messages") before the
     composer wiped it out -- but composing a new message doesn't need
     to wait on a folder-status notice the user can check later, so
     queued messages are drained and shown once here without blocking.
  ----------------------------------------------------------------------*/
void
flush_status_messages()
{
    while(ps_global->ttyo->screen_rows > 1 && queue_length > 0) {
        currently_displayed = message_queue[0];
        d_q_status_message();
        output_message(currently_displayed.text);
	displayed_time = time(0);
    }
    message_is_displayed = 1;
}


/*----------------------------------------------------------------------
      Remove a message from the message queue.
  ----*/
void
d_q_status_message()
{
    int i;
    
    for(i = 0; i < queue_length; i++) {
        message_queue[i] = message_queue[i + 1];
    }
    queue_length--;
}



/*----------------------------------------------------------------------
    Actually output the message to the screen

  Args: message -- The message to output
 
 ----*/
int 
output_message(message)
     char        *message;
{
    int  col, row, max_length;
    char obuff[MAX_SCREEN_COLS + 5];

    row = ps_global->ttyo->screen_rows -
                     (ps_global->ttyo->screen_rows < 4 ? 0 : 3);

    MoveCursor(row, 0);
    CleartoEOLN();
    StartInverse();

    /*Put [] around message and truncate to screen width */
    max_length = ps_global->ttyo != NULL ? ps_global->ttyo->screen_cols : 80;
    strcpy(obuff, "[");
    strncat(obuff, message, MAX_SCREEN_COLS);
    obuff[max_length -1] = '\0';
    strcat(obuff, "]");

    col = Centerline(row, obuff);
    EndInverse();
    fflush(stdout);
    return(col);
}


/*----------------------------------------------------------------------
     Ask a yes/no question in the status line

   Args: question     -- string to prompt user with
         dflt         -- The default answer to the question (should probably
			 be y or n)
         on_ctrl_C    -- Answer returned on ^C
	 help         -- Two line help text
	 display_help -- If true, display help without being asked

 Result: Messes up the status line,
         returns y, n, dflt, or dflt_C
  ---*/
int
want_to(question, dflt, on_ctrl_C, help, display_help)
     char      *question;
     HelpType   help;
     int    dflt, on_ctrl_C, display_help;
{
    char *yorn[3], *q2;
    int   rv;

    yorn[0] = "Yes";
    yorn[1] = "No";
    yorn[2] = NULL;

    /*----
       One problem with adding the (y/n) here is that shrinking the 
       screen while in radio_buttons() will cause it to get chopped
       off. It would be better to truncate the question passed in
       hear and leave the full "(y/n) [x] : " on.
      ----*/
    q2 = fs_get(strlen(question) + 17);
    sprintf(q2, "%.*s? (y/n%s) [%c]: ", ps_global->ttyo->screen_cols - 17,
	    question, (on_ctrl_C) ? "/^C" : "", dflt);
    
    rv = radio_buttons(q2, ps_global->ttyo->screen_rows > 4 ? -3 : -1, 0,
                       "yn", yorn, dflt, on_ctrl_C, help, display_help);

    fs_give((void **)&q2);

    return(rv);
}


/*----------------------------------------------------------------------
    Prompt user for a choice among alternatives

Args --  prompt: The prompt for the question/selection
         line:   The line to prompt on, if negative then relative to bottom
         column:  Column number to prompt on
         buttons: The letter the user types to push the buttons
         labels:  String labels on the buttons. Displayed when pushed
         deefault: The selection when the <CR> is pressed (should probably
		   be one of the chars in buttons)
         on_ctrl_C: The selection when ^C is pressed
         help_text: Text to be displayed on bottom two lines
         help_on:   If set the help will displayed without the user asking 

Result -- Returns the letter pressed. Will be one of the characters in the
          buttons argument or one of the two deefaults.

This will pause for any new status message to be seen and then prompt the user.
The prompt will be truncated to fit on the screen. Redraw and resize are
handled along with ^Z suspension. Typing ^G will toggle the help text on and
off. Character types that are not buttons wil result in a beep.
  ----*/
int
radio_buttons(prompt, line, column, buttons, labels, deefault, on_ctrl_C,
							help_text, help_on)
     char *prompt, *buttons, **labels;
     HelpType help_text;
     int  line, column, help_on, deefault, on_ctrl_C;
{
    register int  ch, real_line;
    char         *q, *p, **ll;
    int           cursor_moved, max_label;

    /*----
       Give the user time to see the status message that was
      just put up if there was one.
      ----*/
    if(messages_queued())
      sleep(3);

    needs_clearing = 1;

    mark_status_dirty();
    real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
    MoveCursor(real_line, column);
    CleartoEOLN();

    /*---- Find longest label ----*/
    max_label = 0;
    for(ll = labels; *ll != NULL; ll++)
      max_label = max(max_label, (int)strlen(*ll));
    
    q = cpystr(prompt); /* So we can truncate string below if need be */
    if(strlen(q) + max_label > (size_t)ps_global->ttyo->screen_cols)
        q[ps_global->ttyo->screen_cols - max_label] = '\0';

    draw_radio_prompt(real_line, column, q, help_on, help_text);

    do {
        fflush(stdout);
        fflush(stdin);

	/* Timeout 5 min to keep imap mail stream alive */
        ch = read_char(600);
        ch = isupper(ch) ? tolower(ch) : ch;

        switch(ch) {

          default:
            for(p = buttons; ch!=(int)*p && *p!='\0'; p++);/* Which button */
            if(*p == '\0') {
                Writechar('\007');
                break;
            }
            Write_to_screen(labels[p - buttons]);
            goto out_of_loop;

          case ctrl('M'):
          case ctrl('J'):
            ch = deefault;
            goto out_of_loop;

          case ctrl('C'):
	    if(on_ctrl_C){
		ch = on_ctrl_C;
		goto out_of_loop;
	    }

	    Writechar('\007');
	    break;


          case '?':
          case ctrl('G'):
            if(help_text == NO_HELP)
              break;
            if(help_on) {
                MoveCursor(real_line + 1, column);
                CleartoEOLN();
                MoveCursor(real_line + 2, column);
                CleartoEOLN();
            } else {
                radio_help(real_line, column, help_text);
            }
            help_on = ! help_on;
            MoveCursor(real_line, column + strlen(q));
            break;
            

          case NO_OP_COMMAND:
          case NO_OP_IDLE:
            new_mail(&cursor_moved, 0, 2); /* Keep mail stream alive */
            if(!cursor_moved) /* In case checkpoint happened */
              break;
            /* Else fall into redraw */


          case KEY_RESIZE:
          case ctrl('L'):
            real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
            EndInverse();
            ClearScreen();
            redraw_titlebar();
            if(ps_global->redrawer != NULL)
              (*ps_global->redrawer)();
            redraw_keymenu();
            draw_radio_prompt(real_line, column, q, help_on, help_text);
            break;

            
          case ctrl('Z'):
            if(!have_job_control()) {
                Writechar('\007');
            } else {
                EndInverse();
                do_suspend(ps_global);
                clear_cursor_pos();
                draw_radio_prompt(real_line, column, q, help_on, help_text);
            }
            break;

        } /* switch */
    } while(1);

  out_of_loop:
    fs_give((void *)&q);
    EndInverse();
    fflush(stdout);
    return(ch);
}


/*----------------------------------------------------------------------

  ----*/
void
radio_help(line, column, help)
     int line, column;
     HelpType help;
{
    char **text;

    text = help;
    if(text == NULL)
      return;
    
    EndInverse();
    MoveCursor(line + 1, column);
    CleartoEOLN();
    PutLine0(line + 1, column, text[0]);
    MoveCursor(line + 2, column);
    CleartoEOLN();
    PutLine0(line + 2, column, text[1]);
    StartInverse();
    fflush(stdout);
}


/*----------------------------------------------------------------------
   Paint the screen with the radio buttons prompt
  ----*/
void
draw_radio_prompt(l, c, q, help_on, help_text)
     int l, c, help_on;
     char     *q;
     HelpType  help_text;
{
    int x;

    StartInverse();
    PutLine0(l, c, q);
    x = c + strlen(q);
    MoveCursor(l, x);
    while(x++ < ps_global->ttyo->screen_cols)
      Writechar(' ');
    if(help_on) 
      radio_help(l, c, help_text);
    MoveCursor(l, c + strlen(q));
    fflush(stdout);
}
