/*----------------------------------------------------------------------
   extracted from imap.c

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
    mailbox.c
    managing pine's concept of mailboxes
 ====*/

#include "headers.h"

/*----------------------------------------------------------------------
      Receive list of folders from c-client/imap.
 
 Puts an entry into the specified list of mail folders associated with
 the global context (aka collection). 
 ----*/
extern void       *find_folder_list;
extern MAILSTREAM *find_folder_stream;
   
void context_mailbox(name)
    char *name;
{
    FOLDER_S          *new_f;

    if(*name == '.')
      return;

    new_f                    = new_folder(name);
    new_f->prefix[0]         = '\0';
    new_f->msg_count         = 0;
    new_f->unread_count      = 0;
    folder_insert(-1, new_f, find_folder_list);

    dprint(4, (debugfile, "====== context_mailbox: (%s)\n", name));
}

/*----------------------------------------------------------------------
     Receive list of bulliten boards from c-client/imap

 Puts an entry into the specified list of bulletin boards associated with
 the global context (aka collection). 
 ----*/

void context_bboard(name)
char *name;
{
    FOLDER_S          *new_f = new_folder(name);


#ifdef	LATER
/*
 * Right now it's felt that it's too costly to 
 * display the new folder count by default for newsgoups...
 * (930507)
 */
    find_folder_stream = context_open(find_folder_context->context, 
				      find_folder_stream, name, 0);

    if(find_folder_stream != NULL) {
	new_f->msg_count    = find_folder_stream->nmsgs;
	new_f->unread_count = find_folder_stream->recent;
	sprintf(new_f->prefix, "%4.4s ",
		new_f->unread_count ? int2string(new_f->msg_count)  : "");
    } else {
	new_f->msg_count    = 0; /* zero message count different... */
	new_f->unread_count = 0; /* than inaccessible group!        */
	sprintf(new_f->prefix, "* ");
    }

    new_f->name_len += strlen(new_f->prefix);

    dprint(4, (debugfile, "====== context_bboard: (%s) %s, %d of %d unread\n",
	       find_folder_context->context, new_f->name, 
	       new_f->unread_count, new_f->msg_count));
#endif

    folder_insert(folder_total(find_folder_list), new_f, find_folder_list);

    dprint(4, (debugfile, "====== context_bboard: (%s)\n", name));
}

/*---------------------------------------------------------------------- 
        receive notification that search found something           

 Input:  mail stream and message number of located item

 Result: nothing, not used by pine
  ----*/
void mm_searched(stream, msg_no_l)
     MAILSTREAM *stream;
     long        msg_no_l;
{
    char tmp[20];
    int  msg_no = (int)msg_no_l;

    sprintf(tmp,"%ld",msg_no);
    q_status_message2(0, 2,4, "\007 %s searched %s",
		      stream ? stream->mailbox : "<no folder>", tmp);
}

/*----------------------------------------------------------------------
       receive notification of new mail from imap daemon

   Args: stream -- The stream the message count report is for.
         number -- The number of messages now in folder.
 
  Result: Sets value in pine state indicating new mailbox size

     Called when the number of messages in the mailbox goes up.  This
 may also be called as a result of an expunge. It increments the
 new_mail_count based on a the difference between the current idea of
 the maximum number of messages and what mm_exists claims. The new mail
 notification and actual incorporation is done in newmail.c

 ----*/

void mm_exists(stream, number_l)
     MAILSTREAM *stream;
     long number_l;
{
    int number = (int)number_l;
    dprint(3, (debugfile, "=== mm_exists(%ld,%s) called ===\n", number,
	       stream == NULL ? "(no stream)" 
	       : stream->mailbox == NULL ? "(null)" 
	       : stream->mailbox));
    if(stream == ps_global->inbox_stream  &&
                            ps_global->inbox_stream != ps_global->mail_stream){
        ps_global->inbox_changed |= (ps_global->inbox_max_msgno != number);
        if(ps_global->inbox_max_msgno < number)
          ps_global->inbox_new_mail_count += number-ps_global->inbox_max_msgno;
        ps_global->inbox_max_msgno = number;
    } else if(stream == ps_global->mail_stream) {
        ps_global->mail_box_changed |= (ps_global->max_msgno != number);
        if(ps_global->max_msgno < number)
          ps_global->new_mail_count +=  number - ps_global->max_msgno;
        ps_global->max_msgno = number;
    } else {
        /*--- ignore mm_exist for other. These are quick opens --*/
    }
}

/*----------------------------------------------------------------------
    Receive notification from IMAP that a message has been expunged

   Args: stream -- The stream/folder the message is expunged from
         number -- The message number that was expunged

mm_expunged is always called on an expunge. mm_exists() which reports
the current number of messages is sometimes called, depending on the
kind of mail file (Bezerk, imap...). For this reason there are two
expunge counts. tot_expunge_count counts the total messages expunged
since the last new mail check. The new mail check will only see a
change in the number of messages and not know how the change was made
up as a result of adds and deletes unless it has the
total_expunge_count. The secound counter, expunge_count, counts
messages expunged between calls to mm_exists handling the case where
mm_expunged is called without mm_exists() being called. See new_mail()
for more details.

  ----*/
void mm_expunged(stream, number_l)
     MAILSTREAM *stream;
     long        number_l;
{
     dprint(3, (debugfile, "mm_expunged called %s %ld\n",
		stream == NULL ? "(no stream)" 
		: stream->mailbox == NULL ? "(null)" 
		: stream->mailbox, 
		number_l));
     if(stream == ps_global->inbox_stream &&
            ps_global->inbox_stream != ps_global->mail_stream) {
         ps_global->inbox_expunge_count++;
         ps_global->inbox_changed = 1;
         ps_global->inbox_max_msgno--;
    } else {
         ps_global->expunge_count++;
         ps_global->mail_box_changed = 1;
         ps_global->max_msgno--;
    }
}

/*----------------------------------------------------------------------
       Receive notification of an error writing to disk
      
  Args: stream  -- The stream the error occured on
        errcode -- The system error code (errno)
        serious -- Flag indicating error is serious (mail may be lost)

Result: If error is non serious, the stream is marked as having an error
        and deletes are disallowed until error clears
        If error is serious this goes modal, allowing the user to retry
        of get a shell escape to fix the condition. When the condition is
        serious it means that mail existing in the mailbox will be lost
        if Pine exists without writing so we try to induce the user to 
        fix the error, go get some one that can fix the error or whatever
        and don't provide an easy way out.
  ----*/

long
mm_diskerror (stream, errcode, serious)
     MAILSTREAM *stream;
     long        errcode;
     long        serious;
{
    int line, ch;
    char q[200];

    dprint(0, (debugfile,
       "\n***** DISK ERROR on stream %s. Error code %ld. Error is %sserious\n",
               stream ? stream->mailbox : "<no mail folder>", errcode,
               serious ? "" : "not "));
    dprint(0, (debugfile, "***** message: \"%s\"\n\n", ps_global->last_error));

    if(!serious) {
/*        q_status_message2(1, 3, 6,"\007Error saving mail folder \"%s\" : %s",
                    pretty_fn(stream ? stream->mailbox : ""),
                          error_description((int)errcode)); */
        if(stream == ps_global->mail_stream) {
            ps_global->io_error_on_stream = 1;
        }
        return (1) ;
    }

    line = ps_global->ttyo->screen_rows - 3;
    MoveCursor(line, 0);
    CleartoEOLN();
    StartInverse();
    Writechar('\007', 0);
    sprintf(q,  "Serious error saving mail folder \"%s\":",
                    pretty_fn(stream ? stream->mailbox : ""));
    PutLine0(line, 0, q);
    ch = strlen(q); /* Use ch temporarily */
    MoveCursor(line, ch);
    while(ch++ < ps_global->ttyo->screen_cols - 1)
      Writechar(' ', 0);
/*    sprintf(q, "%-*.*s", ps_global->ttyo->screen_cols - 1,
            ps_global->ttyo->screen_cols - 1,
            ps_global->last_error);
    d_q_status_message();
    PutLine0(line+1, 0, q); */
    if(F_ON(F_CAN_SUSPEND,ps_global)) {
        sprintf(q,  "Press return to retry saving or press ^Z to suspend Pine: ");
    } else {
        sprintf(q,  "Press return to retry saving or press ! to get to system prompt: ");
    }
    PutLine0(line+2, 0, q);
    Writechar('\007', 0);
    ch = strlen(q); /* Use ch temporarily */
    MoveCursor(line+2, ch);
    while(ch++ < ps_global->ttyo->screen_cols - 1)
      Writechar(' ', 0);
    MoveCursor(line+2, strlen(q));

    ch = read_char(0);
    while(!(ch == F_ON(F_CAN_SUSPEND,ps_global) ? ctrl('Z') : '!' || ch == '\r')) {
        Writechar('\007', 0);
        ch = read_char(0);
    }
    switch(ch) {
      case '\r':
        Write_to_screen("Retry");
        EndInverse();
        MoveCursor(line -2, 0);
        fflush(stdout);
        return (0L);
       
      case '!':
        EndInverse();
        end_keyboard(ps_global != NULL ? F_ON(F_USE_FK,ps_global) : 0);
        end_tty_driver(ps_global);
        printf("\n\nPlease attempt to correct the error preventing the saving of the mail folder.\n");
        printf("For example if the disk is out of space try removing uneeded files.\n");
        printf("You might also contact your system administrator.\n");
        printf("Give the command \"exit\" to get back to Pine and try saving the folder again.\n");

        system("csh");

        init_tty_driver(ps_global);
        init_keyboard(F_ON(F_USE_FK,ps_global));
        MoveCursor(line - 2, 0);
        return (0L); /* go retry it again */
       
      case ctrl('Z'):
        EndInverse();
        do_suspend(ps_global);
        break;
    }
    return(0);
}

/*----------------------------------------------------------------------
         recieve notification from IMAP

  Args: stream  --  Mail stream message is relavant to 
        string  --  The message text
        errflag --  Set if it is a serious error

 Result: message displayed in status line

 The facility is for general notices, such as connection to server;
 server shutting down etc... It is used infrequently.
  ----------------------------------------------------------------------*/
void mm_notify(stream, string, errflag)
     MAILSTREAM *stream;
     char       *string;
     long        errflag;
{
    dprint(1, (debugfile, "IMAP mm_notify %s : %s : %s\n",
               errflag == ERROR ? "error" : "warning" ,
               stream != NULL && stream->mailbox != NULL ? stream->mailbox :
               "-no folder-" ,
               string));

    if((strncmp(string, "[TRYCREATE]", 11) == 0)){
	ps_global->try_to_create = 1;
    }
    else{
	q_status_message2(1, errflag == ERROR ? 2 : 1, 6,
			  errflag == ERROR ?"\007%s : %s" : "%s : %s",
			  stream != NULL && stream->mailbox != NULL ?
			  stream->mailbox : "-no folder-", string);

	sprintf(ps_global->last_error,
		"%s : %s", stream != NULL&& stream->mailbox != NULL?
		stream->mailbox : "-no folder-",
		string);
    }
}

void
mm_fatal(message)
     char *message;
{
    panic(message);
}

