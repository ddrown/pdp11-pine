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
     signals.c
     Different signal handlers for different signals
       - Catches all the abort signals, cleans up tty modes and then coredumps
       - Not much to do for SIGHUP
       - Not much to do for SIGTERM
       - turn SIGWINCH into a KEY_RESIZE command
       - No signals for ^Z/suspend, but do it here anyway
       - Also set up the signal handlers, and hold signals for
         critical imap sections of code.

 ====*/

#include "headers.h"


              /* SigType is defined in os.h and is either int or void */
static SigType auger_in_signal(int), winch_signal(int), quit_signal(int),
               usr2_signal(int);



/*----------------------------------------------------------------------
    Install handlers for all the signals we care to catch

A couple of complications here. Most systems/debuggers can trace back 
the calling stack past something like a SIGBUS so we can trap the signal,
reset the tty modes and do other clean up and then call abort to get a useful
core dump. Ultrix can't do this so we don't try to catch the signals.
  ----------------------------------------------------------------------*/
void
init_signals()
{
    signal(SIGILL,  auger_in_signal);
    signal(SIGTRAP, auger_in_signal);
    signal(SIGIOT,  auger_in_signal);
#ifdef SIGEMT
    signal(SIGEMT,  auger_in_signal);
#endif
    signal(SIGBUS,  auger_in_signal);
    signal(SIGSEGV, auger_in_signal);
    signal(SIGSYS,  auger_in_signal);
    signal(SIGQUIT, quit_signal);
    /* Don't catch SIGFPE cause it's rare and we use it in a hack below*/

#ifdef RESIZING
    signal(SIGWINCH, winch_signal);
#endif

    signal(SIGHUP, hup_signal);
    signal(SIGTERM, term_signal);

    /*
     * Set up SIGUSR2 to catch signal from other software using the 
     * c-client to tell us that other access to the folder is being 
     * attempted.  THIS IS A TEST: if it turns out that simply
     * going R/O when another pine is started or the same folder is opened,
     * then we may want to install a smarter handler that uses idle time
     * or even prompts the user to see if it's ok to give up R/O access...
     */
    signal(SIGUSR2, usr2_signal);

    signal(SIGPIPE, SIG_IGN);

#ifdef SIGTSTP
    /* Some unexplained behaviour on Ultrix 4.2 (Hardy) seems to be
       resulting in Pine getting sent a SIGTSTP. Ignore it here.
       probably better to ignore it than let it happen in any case
     */
    signal(SIGTSTP, SIG_IGN);
#endif /* SIGTSTP */
}



/*----------------------------------------------------------------------
    Return all signal handling back to normal
  ----------------------------------------------------------------------*/
void
end_signals()
{
    if(signal(SIGILL,  SIG_DFL) == SIG_ERR) {
        fprintf(stderr, "Signal reset error: %s\n",
                error_description(errno));
        exit(-1);
    }
    signal(SIGTRAP, SIG_DFL);
    signal(SIGIOT,  SIG_DFL);
#ifdef SIGEMT
    signal(SIGEMT,  SIG_DFL);
#endif
    signal(SIGFPE,  SIG_DFL);
    signal(SIGBUS,  SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGSYS,  SIG_DFL);
#ifdef RESIZING
    signal(SIGWINCH, SIG_DFL);
#endif
    signal(SIGQUIT, SIG_DFL);
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif /* SIGTSTP */
}




/*----------------------------------------------------------------------
     Handle signals caused by aborts -- SIGSEGV, SIGILL
Call panic which cleans up tty modes and then core dumps
  ----------------------------------------------------------------------*/
static SigType
auger_in_signal(int signo)
{
    panic("Received abort signal");
    /* No return from panic */
}




/*----------------------------------------------------------------------
  Catch the quit signal and log in .debug file so we can know if a core
file is the result of the user typing ^\. Otherwise do minimum
processing here.
  ----------------------------------------------------------------------*/

static SigType
quit_signal(int signo)
{
    if(want_to("\007Pine emergency exit. REALLY ABORT PINE RIGHT NOW", 'n',
               'n', h_wt_auger_in, 0) == 'n') {
        q_status_message(0, 2,3,
          "Type ^L (control-L) to repaint the screen and continue with Pine");
        do {
            display_message('x');
        } while(messages_queued() > 0);
        return;
    }
    fprintf(stderr, "\n\nPine aborting, core dumped\n\n");
    coredump();
}



/*----------------------------------------------------------------------
      handle cleaning up mail streams and tty modes...

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static int
fast_clean_up(void)
{
    if(ps_global->inbox_stream != NULL && !ps_global->inbox_stream->lock){
        if(ps_global->inbox_stream == ps_global->mail_stream)
          ps_global->mail_stream = NULL; 
        mail_close(ps_global->inbox_stream);
    }

    if(ps_global->mail_stream != NULL &&
       ps_global->mail_stream != ps_global->inbox_stream &&
       !ps_global->mail_stream->lock)
      mail_close(ps_global->mail_stream);

    end_screen();
    end_tty_driver(ps_global);

    return(0);
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGHUP

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
SigType
hup_signal(int signo)
{
    fast_clean_up();
    printf("\n\nPine finished. Received hang up signal\n\n");
    exit(0);
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGTERM

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
SigType
term_signal(int signo)
{
    fast_clean_up();
    printf("\n\nPine finished. Received terminate signal\n\n");
    exit(0);
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGUSR2

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static SigType
usr2_signal(int signo)
{
    char c;

    check_point(DoItNow);
    if(ps_global->inbox_stream != NULL && !ps_global->inbox_stream->lock
       && (c = *ps_global->inbox_stream->mailbox) != '{' && c != '*'){
	ps_global->inbox_stream->readonly = 1; 
	mail_ping(ps_global->inbox_stream);
	q_status_message(0,2,2,"Inbox open elsewhere.  Session now Read-Only.");
    }

    if(ps_global->mail_stream != NULL &&
       ps_global->mail_stream != ps_global->inbox_stream &&
       !ps_global->mail_stream->lock &&
       (c = *ps_global->mail_stream->mailbox) != '{' && c != '*'){
	ps_global->mail_stream->readonly = 1; 
	mail_ping(ps_global->mail_stream);
	q_status_message(0,2,2,"Folder open elsewhere.  Session now Read-Only.");
    }
}


#ifdef RESIZING
/*----------------------------------------------------------------------
   Handle window resize signal -- SIGWINCH
  
   The planned strategy is just force a redraw command. This is similar
  to new mail handling which forces a noop command. The signals are
  help until pine reads input. Then a KEY_RESIZE is forced into the command
  stream .
  ----------------------------------------------------------------------*/
extern jmp_buf  winch_state;
extern int      ready_for_winch, winch_occured;

SigType
static winch_signal(int signo)
{
    get_windsize(ps_global->ttyo);
    if(ready_for_winch)
      longjmp(winch_state, 1);
    else
      winch_occured = 1;
}
#endif



/*----------------------------------------------------------------------
     Suspend Pine. Reset tty and suspend. Suspend is done when this returns

   Args:  The pine structure

 Result:  Execution suspended for a while. Screen will need redrawing 
          after this is done.

 Instead of the usual handling of ^Z by catching a signal, we actually read
the ^Z and then clean up the tty driver, then kill ourself to stop, and 
pick up where we left off when execution resumes.
  ----------------------------------------------------------------------*/
void
do_suspend(pine) 
    struct pine *pine;
{
    long now, then;
    char curf[MAXPATH+1];

    if(!have_job_control())
      return;

    strcpy(curf, ps_global->cur_folder);
    expand_foldername(curf);
    then = time(0);
    end_tty_driver(pine);
    end_screen();
    printf("\nPine suspended; \"fg\" to resume.\n");
    if(curf[0] == '{')
      printf("Warning: IMAP connection closes after 30 min suspended\n\n");

    stop_process();
    now = time(0);
    init_screen();
    init_tty_driver(pine);

    if(curf[0] == '{' &&
       (char *)mail_ping(ps_global->mail_stream) == NULL) {
        q_status_message(1, 3, 6,
                         "\007Suspend timeout, IMAP connection broken");
    }
}


SigType (*hold_hup)(int), (*hold_int)(int), (*hold_term)(int), (*hold_usr2)(int);

/*----------------------------------------------------------------------
     Ignore signals when imap is running through critical code

 Args: stream -- The stream on which critical operation is proceeding
 ----*/

void 
mm_critical(stream)
     MAILSTREAM *stream;
{
    stream = stream; /* For compiler complaints that this isn't used */
    hold_hup  = signal(SIGHUP, SIG_IGN);
    hold_usr2 = signal(SIGUSR2, SIG_IGN);
    hold_int  = signal(SIGINT, SIG_IGN);
    hold_term = signal(SIGTERM, SIG_IGN);
}



/*----------------------------------------------------------------------
   Reset signals after critical imap code
 ----*/
void
mm_nocritical(stream)
     MAILSTREAM *stream;
{ 
    stream = stream; /* For compiler complaints that this isn't used */

    (void)signal(SIGHUP, hold_hup);
    (void)signal(SIGUSR2, hold_usr2);
    (void)signal(SIGINT, hold_int);
    (void)signal(SIGTERM, hold_term);
}
