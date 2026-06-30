#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: print.c,v 4.12 1993/11/09 00:51:19 mikes Exp $";
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
    print.c
   
    Functions having to do with printing on paper and forking of spoolers

    In general one calls open_printer() to start printing. One of
    the little print functions to send a line or string, and then
    call print_end() when complete. This takes care of forking offa spooler
    and piping the stuff down it. No handles or anything here because there's
    only one printer open at a time.

 ====*/

#include "headers.h"
#ifndef	DOS
FILE        *fdopen();
static FILE *command_pipe;
static char  outputsave[40];
static int   pid;
#else

#ifdef	ANSI
int	  send_printer(char);				/* Prototypes */
unsigned  short printer_ready();
char     *p_printer_error(unsigned short);
#endif
#endif

static int   write_error;


#define WTR 1
#define RDR 0


/*----------------------------------------------------------------------
       Open the printer

  Args: desc --Description of item to print. Should have on tailing blank.

This does most of the work of popen so we can save the standard output of the
command we execute and send it back to the user.
  ----*/
open_printer(desc)
     char *desc;
{
#ifdef	DOS
    char           prompt[100];
    unsigned short status;

    if(status = printer_ready()){		/* 0 means things are OK */
        q_status_message1(0, 2, 4, "\007Error opening printer: %s",
                              p_printer_error(status));
	write_error = 1;	/* just in case */
        return(-1);
    }

    sprintf(prompt, "Print %sto desktop printer", desc == NULL ? "" : desc);
    if(want_to(prompt, 'y', 'n', NO_HELP, 0) == 'n') {
	q_status_message(0, 0, 2, "Print cancelled");
	return(-1);
    }

    q_status_message(1, 2, 3,"Printing to desktop printer...");
    display_message('x');
#else
    char command[100], prompt[100];
    int  p[2];

    if(ps_global->VAR_PRINTER == NULL) {
        q_status_message(1,1,3,"\007No printer has been chosen.  Use SETUP on main menu to make choice.");
        return(-1);
    }
    sprintf(outputsave, "/tmp/pine-print-output-%d", getpid());


    /*========= Sort out the printer command ==========*/
    if(strucmp(ps_global->VAR_PRINTER, ANSI_PRINTER) == 0) {
        /*---------- Print on an attatched ascii printer --------*/
        command[0] = '\0';
        sprintf(prompt, "Print %susing \"%s\"",desc == NULL ? "" : desc,
                ps_global->VAR_PRINTER);
        if(want_to(prompt, 'y', 'n', (char **)NULL, 0) == 'n') {
            q_status_message(0, 0,2, "Print cancelled");
            return(-1);
        }
    } else {
        
        /*------- custom configuration ------*/
        sprintf(prompt, "Print %susing command \"%s\"", desc==NULL ? "" : desc,
                ps_global->VAR_PRINTER);
        if(want_to(prompt, 'y', 'n', (char **)NULL, 0) == 'n') {
            q_status_message(0, 0,2, "Print cancelled");
            return(-1);
        }
        strcpy(command, ps_global->VAR_PRINTER);
        q_status_message1(1, 1,3,"Printing with command \"%s\"",
                          ps_global->VAR_PRINTER);
    }

    display_message('x');

    if(command[0] == '\0') {
        /*----------- Printer attached to ansi device ---------*/
        q_status_message(1, 2, 3,"Printing to attached desktop printer...");
        display_message('x');
        sleep(1);
        Raw(0);
        printf("\033[5i");
        command_pipe = stdout;

    }  else {
        /*----------- Print by forking off a UNIX command ------------*/
        /*--- we do our own popen here, so all error output can be saved ---*/
        pipe(p);
        
        if((pid = fork()) == 0){
            /* connect output to our file */
            int output = creat(outputsave, 0777);
            dup2(output, 1); 
            dup2(output, 2);
            /*  connect process to pipe */
            close(p[WTR]);
            dup2(p[RDR], 0);
            close(p[RDR]);

            execl("/bin/sh", "sh", "-c", ps_global->VAR_PRINTER, 0);
            exit(-1);
        }
        if(pid == -1) {
            close(p[WTR]);
            close(p[RDR]);
        }
        close(p[RDR]);
        command_pipe = fdopen(p[WTR], "w");
       
        dprint(4, (debugfile, "Printing using command \"%s\"\n",
                                              ps_global->VAR_PRINTER));
    
        if(command_pipe == NULL) {
            q_status_message1(1, 2, 4, "\007Error opening printer: %s",
                              error_description(errno));
            dprint(2, (debugfile, "Error opening popening printer \"%s\"\n",
                      error_description(errno)));
            return(-1);
        }
    }
#endif
    write_error = 0;
    return(0);
}



/*----------------------------------------------------------------------
     Close printer
  
  If we're piping to a spooler close down the pipe and wait for the process
to finish. If we're sending to an attached printer send the escape sequence.
Also let the user know the result of the print
 ----*/
void close_printer()
{
#ifdef	DOS
    print_char(FORMFEED);
#else
    char  buf[512];
    int   messages_queued, status, r;
    FILE *f;
    SigType (*hsig)(), (*isig)(), (*qsig)();
  
    messages_queued = 0;
    if(command_pipe == stdout) {
        printf("\f\033[4i");
        fflush(stdout);
        Raw(1);
    } else {
        fclose(command_pipe);

        isig = signal(SIGINT, SIG_IGN);
        qsig = signal(SIGQUIT, SIG_IGN);
        hsig = signal(SIGHUP, SIG_IGN);
#ifdef HAVE_WAIT_UNION
        while((r=  wait((union wait *)&status)) && r != -1 && r != pid);
#else
        while((r=  wait(&status)) && r != -1 && r != pid);
#endif
        signal(SIGINT,  isig);
        signal(SIGHUP,  hsig);
        signal(SIGQUIT, qsig);
        
        f = fopen(outputsave, "r");
        if(f != NULL){
            unlink(outputsave);
            while(fgets(buf, sizeof(buf), f) != NULL && messages_queued < 5) {
                buf[strlen(buf) - 1] = '\0';
                q_status_message2(1, 1, 4, "Print %s: %s",
                                  write_error ? "error\007": "result",buf);
                messages_queued++;
            }
            fclose(f);
        }
        dprint(2, (debugfile, "Error reopening %s to get results: %s\n",
                   outputsave, error_description(errno)));
    }
    if(!messages_queued) 
#endif	/* !DOS */
      q_status_message(0, 1, 3, "Printing complete");

    display_message('x');
}



/*----------------------------------------------------------------------
     Print a single character

  Args: c -- char to print
  Returns: 1 on success, 0 on write_error
 ----*/
int
print_char(c)
     int c;
{
#ifdef	DOS
    if(!write_error)
	if(write_error = send_printer((char) c)){
            q_status_message1(0, 2, 4, "\007Print cancelled: %s",
                              p_printer_error((unsigned short)write_error));
	}
#else
    if(write_error)
      return(0);
    if(putc(c, command_pipe) == EOF)
      write_error = 1;
#endif	/* DOS */
    return(!write_error);
}



/*----------------------------------------------------------------------
     Send a line of text to the printer

  Args:  line -- Text to print

  ----*/
    
void print_text(line)
     char *line;
{
#ifdef	DOS
    register char *c = line;

    while(*c != '\0' && !write_error)
      print_char(*c++);
#else
    if(write_error)
      return;
    if(fputs(line, command_pipe) == EOF)
      write_error = 1;
#endif	/* DOS */
}


/*----------------------------------------------------------------------
      printf style formatting with one arg for printer

 Args: line -- The printf control string
       a1   -- The 1st argument for printf
 ----*/

void print_text1(line, a1)
     char *line, *a1;
{
#ifdef	DOS
    sprintf(tmp_20k_buf, line, a1);
    print_text(tmp_20k_buf);
#else
    if(write_error)
      return;
    if(fprintf(command_pipe, line, a1) < 0)
      write_error = 1;
#endif
}


/*----------------------------------------------------------------------
      printf style formatting with one arg for printer

 Args: line -- The printf control string
       a1   -- The 1st argument for printf
       a2   -- The 2nd argument for printf
 ----*/

void print_text2(line, a1, a2)
     char *line, *a1, *a2;
{
#ifdef	DOS
    sprintf(tmp_20k_buf, line, a1, a2);
    print_text(tmp_20k_buf);
#else
    if(write_error)
      return;
    if(fprintf(command_pipe, line, a1, a2) < 0)
      write_error = 1;
#endif
}


/*----------------------------------------------------------------------
      printf style formatting with one arg for printer

 Args: line -- The printf control string
       a1   -- The 1st argument for printf
       a2   -- The 2nd argument for printf
       a3   -- The 3rd argument for printf
 ----*/

void print_text3(line, a1, a2, a3)
     char *line, *a1, *a2, *a3;
{
#ifdef	DOS
    sprintf(tmp_20k_buf, line, a1, a2, a3);
    print_text(tmp_20k_buf);
#else
    if(write_error)
      return;
    if(fprintf(command_pipe, line, a1, a2, a3)< 0)
      write_error = 1;
#endif
}
