/*----------------------------------------------------------------------
   note: originally from imap.c

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
    log.c
    log message functions

 ====*/

#include <time.h>
#include "headers.h"

/*----------------------------------------------------------------------
      Write imap debugging information into log file

   Args: strings -- the string for the debug file

 Result: message written to the debug log file
  ----*/
void mm_dlog(string)
     char *string;
{
    dprint(0, (debugfile, "IMAP DEBUG: %s\n", string));
}



/*----------------------------------------------------------------------
      Queue imap log message for display in the message line

   Args: string -- The message 
         errflg -- flag set to 1 if pertains to an error

 Result: Message queued for display

 The c-client/imap reports most of it's status and errors here
  ---*/
void mm_log(string, errflg)
     char *string;
     long  errflg;
{
    char        message[300];
    char       *occurance;
    int         was_capitalized;
    long        now;
    struct tm  *tm_now;

    now = time(0);
    tm_now = localtime(&now);

    dprint((errflg == ERROR ? 1 : 2), (debugfile,
                                     "IMAP %d:%d %d/%d mm_log %s: %s\n",
                                     tm_now->tm_hour, tm_now->tm_min,
                                     tm_now->tm_mon+1, tm_now->tm_mday,
                                     errflg == ERROR ? "ERROR" :
                                     errflg == WARN  ? "warn"  :
                                     errflg == PARSE ? "parse" : "babble",
                                     string));

    if(strncmp(string , "APPEND", 6) == 0){
	if(ps_global->try_to_create) /* hide error if creating new folder  */
	  return;

	strncpy(string += 2, "Save", 4); /* replace "APPEND" with "Save" */
    }

    /*---- replace all "mailbox" with "folder" ------*/
    strncpy(message, string, sizeof(message));
    message[sizeof(message) - 1] = '\0';
    occurance = srchstr(message, "mailbox");
    while(occurance != NULL) {
        was_capitalized = isupper(*occurance);
        rplstr(occurance, 7, (errflg == PARSE ? "address" : "folder"));
        if(was_capitalized)
          *occurance = (errflg == PARSE ? 'A' : 'F');
        occurance = srchstr(occurance, "mailbox");
    }

    if(errflg == PARSE || ps_global->noshow_error) 
      strcpy(ps_global->c_client_error, message);

    if(ps_global->noshow_error ||
		       (ps_global->noshow_warn && errflg == WARN) ||
                       (errflg != ERROR && errflg != WARN))
      return; /* Only care about errors; don't print when asked not to */

    /*---- Display the message ------*/
    q_status_message1(1, errflg == ERROR ? 3 : 2, 5, "\007%s", message);
    strcpy(ps_global->last_error, message);
    display_message('x');
}

