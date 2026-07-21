#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: newmail.c,v 4.22 1993/11/30 23:46:36 mikes Exp $";
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
     newmail.c

   Check for new mail and queue up notification

 ====*/

#include "headers.h"


void new_mail_mess(MAILSTREAM *, char *, long, long);


static long mailbox_mail_since_command = 0L,
	    inbox_mail_since_command   = 0L;

/*----------------------------------------------------------------------
     new_mail() - check for new mail in the inbox
 
  Input:  cursor_moved -- address of flag that is set if the cursor was
                          moved due to checkpointing.
          force       -- flag indicating we should check for new mail no matter
          time_for_check_point -- 0: GoodTime, 1: BadTime, 2: VeryBadTime

  Result: returns -1 if there was no new mail. Otherwise returns the
          sorted message number of the smallest message number that
          was changed. That is the screen needs to be repainted from
          that message down.

  Limit frequency of checks because checks use some resources. That is
  they generate an IMAP packet or require locking the local mailbox.
  (Acutally the lock isn't required, a stat will do, but the current
   c-client mail code locks before it stats.)

  Returns >= 0 only if there is a change in the given mail stream. Otherwise
  this returns -1.  On return the message counts in the pine
  structure are updated to reflect the current number of messages including
  any new mail and any expunging.
 
 --- */
long
new_mail(cursor_moved, force, time_for_check_point)
     int *cursor_moved, force, time_for_check_point;
{
    static time_t last_check = 0;
    static time_t last_check_point_call = 0;
    time_t        now;
    long          x, min_changed_sorted_msgno, old_current_raw_msgno, rv;
    MAILSTREAM   *stream;
    register struct pine *pine_state;
    int           checknow = 0;
#ifdef	DOS
    FILE	  *old_sort;
    long           old_sort_val;
#else
    long          *old_sort;
#endif

    dprint(9, (debugfile, "new mail called (%x %d %d)\n",
               cursor_moved, force, time_for_check_point));
    pine_state = ps_global;  /*  this gets called out of the composer which
                              *  doesn't know about pine_state
                              */
    now = time(0);

    /*
     * only check every 15 seconds, unless we're compelled to
     */
    if(!(stream = pine_state->mail_stream) || (now-last_check_point_call <= 15
	&& time_for_check_point != 0 && !pine_state->mail_box_changed 
	&& !force))
      return(-1);
    else if(force || now-last_check >= timeout-2){ /* 2 to check each timeout */
	checknow++;
        last_check = now;
    }

    last_check_point_call = now;

    if(!check_point(time_for_check_point == 0 ? GoodTime:
	      time_for_check_point == 1 ? BadTime : VeryBadTime) && checknow){
	StartInverse();
	PutLine0(0, 1, "*"); 		/* Show something indicate delay*/
	MoveCursor(ps_global->ttyo->screen_rows -3, 0);
	fflush(stdout);
#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile, "Mail_Ping(mail_stream): %s\n", ctime(&now)));
#endif
        /*-- Ping the mailstream to check for new mail --*/
        dprint(6, (debugfile, "New mail checked \n"));
	if((char *)mail_ping(stream) == NULL)
	  pine_state->dead_stream = 1;

#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile, "Ping complete: %s\n", ctime(&now)));
#endif
	PutLine0(0, 1, " ");
	EndInverse();
	fflush(stdout);
    }

    if(cursor_moved)		/* if we got here, the cursor moved. */
      *cursor_moved = 1;

    if(checknow && pine_state->inbox_stream 
       && stream != pine_state->inbox_stream){
	StartInverse();
	PutLine0(0, 1, "*"); 		/* Show something indicate delay*/
	MoveCursor(ps_global->ttyo->screen_rows -3, 0);
	fflush(stdout);
#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile, "Mail_Ping(inbox_stream): %s\n", ctime(&now)));
#endif
	if((char *)mail_ping(pine_state->inbox_stream) == NULL)
	  pine_state->dead_inbox = 1;

#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile, "Ping complete: %s\n", ctime(&now)));
#endif
	PutLine0(0, 1, " ");
	EndInverse();
	fflush(stdout);
    }


    /*-------------------------------------------------------------
       Mail box state changed, could be additions or deletions.
      -------------------------------------------------------------*/
    if(pine_state->mail_box_changed) {
        dprint(7, (debugfile,
        "New mail, %s,  new_mail_count:%d  expunge count:%d,  max_msgno:%d\n",
                   pine_state->mail_stream == pine_state->inbox_stream ?
                      "inbox" : "other",
                   pine_state->new_mail_count,
                   pine_state->expunge_count,
                   pine_state->max_msgno));

        if(ps_global->new_mail_count > 0) {
            mailbox_mail_since_command += ps_global->new_mail_count;

            new_mail_mess(pine_state->mail_stream,
                      pine_state->mail_stream == pine_state->inbox_stream ?
                      NULL :  pine_state->cur_folder,
                      mailbox_mail_since_command,
                      pine_state->max_msgno);
        }

        old_current_raw_msgno = (ps_global->current_sorted_msgno <= 0L) ? 0L :
                         ps_global->sort[ps_global->current_sorted_msgno];
        dprint(9, (debugfile,
                  "old_current_raw_msgno: %ld  sort_allocated; %ld\n",
                   old_current_raw_msgno, ps_global->sort_allocated));

        if(ps_global->sort_allocated <= ps_global->max_msgno + 1){
            /*--- Pad out the sort array if needed ----*/
#ifdef	DOS
	    /*
	     * Blindly doubling the array size is too costly under
	     * dos.
	     */
	    while(ps_global->sort_allocated < ps_global->max_msgno + 1)
		ps_global->sort_allocated += 64;
#else
            ps_global->sort_allocated = max(ps_global->max_msgno + 1,
                                            ps_global->sort_allocated * 2);
#endif
            fs_resize((void **)&(ps_global->sort),
                      (size_t)ps_global->sort_allocated * sizeof(long));
        }

        for(x  = ps_global->max_msgno - ps_global->new_mail_count + 1;
            x <= ps_global->max_msgno; x++) {
            ps_global->sort[x] = x;
        }

	if(ps_global->sort_allocated) {
#ifdef	DOS
	    if(old_sort = create_tmpfile()){
		for(x = 1; x <= ps_global->max_msgno; x++){
		    if(fwrite((void *)&(ps_global->sort[x]), sizeof(long), 
			      (size_t)1, old_sort) != 1){
			fclose(old_sort);
			old_sort = NULL;
			break;
		    }
		}
	    }
#else
           old_sort = (long *)fs_get(sizeof(long)*(size_t)ps_global->sort_allocated);
           for(x = 1; x <= ps_global->max_msgno; x++)
             old_sort[x] = ps_global->sort[x];
#endif
        }  else {
            old_sort = NULL;
        }


        sort_current_folder();

        /* Figure out what the current message number is now */
	if(old_current_raw_msgno && ps_global->max_msgno > 0){
	    /*
	     * Previously had messages AND still have messages
	     * to pick from...
	     */
	    for(x = 1; x <= ps_global->max_msgno; x++) {
		if(ps_global->sort[x] == old_current_raw_msgno) {
		    ps_global->new_current_sorted_msgno = x;
		    break;
		}
	    }
	}
	else if(ps_global->max_msgno > 0)
	  /*
	   * No previous messages, BUT there are new ones, so
	   * pick one to highlight...
	   */
	  ps_global->new_current_sorted_msgno = 1L;
	else
	  /*
	   * No old messages and no new messages so make sure 
	   * functions down the line know that...
	   */
	  ps_global->new_current_sorted_msgno = -1L;


        /* Figure out the lowest position message sorted too */
        if(old_sort != NULL){
#ifdef	DOS
	    fseek(old_sort, 0L, 0);
#endif
	    for(x = 1; x <= ps_global->max_msgno; x++) {
#ifdef	DOS
		if(fread(&old_sort_val,sizeof(long),(size_t)1,old_sort) != 1){
		    fclose(old_sort);
		    old_sort = NULL;
		    break;
		}

		if(ps_global->sort[x] != old_sort_val)
		  break;
#else
		if(ps_global->sort[x] != old_sort[x])
		  break;
#endif
	    }
	}

        /* Figure out the lowest position that changed */
        if(pine_state->expunge_count > 0 || old_sort == NULL) {
            min_changed_sorted_msgno = 0;
        } else if(x > ps_global->max_msgno) {
            min_changed_sorted_msgno = ps_global->max_msgno -
              ps_global->new_mail_count;
        } else {
            min_changed_sorted_msgno = x - 1;
        }
        dprint(9, (debugfile, "new sorted_msgno: %ld  min_changed: %ld\n",
                   ps_global->new_current_sorted_msgno,
                   min_changed_sorted_msgno));

        if(old_sort != NULL)
#ifdef	DOS
	  fclose(old_sort);
#else
          fs_give((void **)&old_sort);
#endif

	if((ps_global->current_sort_order != SortArrival) ||
	                                      ps_global->current_reverse_sort)
          clear_index_cache();
    }

    if(pine_state->inbox_changed &&
                       pine_state->inbox_stream != pine_state->mail_stream) {
        /*--  New mail for the inbox, queue up the notification           --*/
        /*-- If this happens then inbox is not current stream that's open --*/
        dprint(7, (debugfile,
         "New mail, inbox, new_mail_count:%ld expunge: %ld,  max_msgno %ld\n",
                   pine_state->inbox_new_mail_count,
                   pine_state->inbox_expunge_count,
                   pine_state->inbox_max_msgno));

        if(pine_state->inbox_new_mail_count > 0) {
            inbox_mail_since_command       += ps_global->inbox_new_mail_count;
            ps_global->inbox_new_mail_count = 0;
            new_mail_mess(pine_state->inbox_stream,NULL,
                          inbox_mail_since_command,
                          pine_state->inbox_max_msgno);
        }
    }

    rv = pine_state->mail_box_changed ? min_changed_sorted_msgno : -1;
    pine_state->inbox_changed    = 0;
    pine_state->mail_box_changed = 0;
    ps_global->new_mail_count    = 0;

    dprint(6, (debugfile, "******** new mail returning %ld  ********\n", rv));
    return(rv);
}


/*----------------------------------------------------------------------
     Format and queue a "new mail" message

  Args: stream     -- mailstream on which a mail has arrived
        folder     -- Name of folder, NULL if inbox
        number     -- number of new messages since last command
        max_num    -- The number of messages now on stream

 Not too much worry here about the length of the message because the
status_message code will fit what it can on the screen and truncation on
the right is about what we want which is what will happen.
  ----*/
void
new_mail_mess(stream, folder, number, max_num)
     MAILSTREAM *stream;
     long        number, max_num;
     char       *folder;

{
    ENVELOPE *e;
    char      subject[200], from[MAX_ADDRESS+20], intro[50 + MAXFOLDER];
    static char   *carray[] = { "regarding",
				"concerning",
				"about",
				"as to",
				"as regards",
				"as respects",
				"in re",
				"re",
				"respecting",
				"in point of",
				"with regard to",
				"subject:"
    };

    e = mail_fetchstructure(stream, max_num, NULL);

    if(folder == NULL) {
        if(number > 1)
          sprintf(intro, "%ld new messages!", number);
        else
          strcpy(intro, "New mail!");
    } else {
        if(number > 1)
          sprintf(intro,"%ld messages saved to folder \"%s\"", number, folder);
        else
          sprintf(intro, "Mail saved to folder \"%s\"", folder);
    }
     
    if(number > 1) {
        if(e != NULL && e->from != NULL) {
            if(e->from->personal != NULL)
              sprintf(from, " Most recent from %s", e->from->personal);
            else
              sprintf(from, " Most recent from %s@%s", e->from->mailbox,
                      e->from->host);
        } else {
            from[0] = '\0';
        }
        q_status_message2(0, 2, 4, "\007%s%s", intro, from);
    } else {
        if(e != NULL && e->from != NULL) {
            if(e->from->personal != NULL)
              sprintf(from, " From %s", e->from->personal);
            else
              sprintf(from, " From %s@%s", e->from->mailbox, e->from->host);
        } else {
            from[0] = '\0';
        }
        if(e != NULL && e->subject == NULL)
          strcpy(subject, " with no subject");
        else
          sprintf(subject, " %s %.183s", carray[(unsigned)random()%12],
		                         e->subject);
        if(from[0] == '\0')
          subject[1] = toupper(subject[1]);

        q_status_message3(0, 2,4, "\007%s%s%s", intro, from, subject);
    }
}



/*----------------------------------------------------------------------
    Force write of the main file so user doesn't lose too much when
 something bad happens. The only thing that can get lost is flags, such 
 as when new mail arrives, is read, deleted or answered.

 Args: timing      -- indicates if it's a good time for to do a checkpoint

  Result: returns 1 if checkpoint was written, 
                  0 if not.

NOTE: mail_check will also notice new mail arrival, so it's impetive that
code exist after this function is called that can deal with updating the 
various pieces of pines state associated with the message count and such.

Only need to checkpoint current stream because there are no changes going
on with other streams when we're not manipulating them.
  ----*/
static int check_count = 0;           /* number of changes since last chk_pt  */
static long first_status_change = 0;  /* time of 1st change since last chk_pt */
static long last_status_change = 0;   /* time of last change                  */
static long check_count_ave = 10 * 10;

check_point(timing)
     CheckPointTime timing;
{
    int     freq, tm;
    long    adj_cca;
    long    tmp;
#ifdef	DEBUG
    time_t  now;
#endif

    dprint(9, (debugfile, "check_point(%s)\n", 
               timing == GoodTime ? "GoodTime" :
               timing == BadTime  ? "BadTime" :
               timing == VeryBadTime  ? "VeryBadTime" : "DoItNow"));

    if(!ps_global->mail_stream || ps_global->mail_stream->readonly ||
							     check_count == 0)
	 return(0);

    freq = CHECK_POINT_FREQ * (timing==GoodTime ? 1 : timing==BadTime ? 3 : 4);
    tm   = CHECK_POINT_TIME * (timing==GoodTime ? 1 : timing==BadTime ? 2 : 3);

    dprint(9, (debugfile, "freq: %d  count: %d\n", freq, check_count));
    dprint(9, (debugfile, "tm: %d  elapsed: %d\n", tm,
               time(0) - first_status_change));

    if(!last_status_change)
        last_status_change = time(0);

    tmp = 10*(time(0)-last_status_change);
    adj_cca = (tmp > check_count_ave) ?
	  min((check_count_ave + tmp)/2, 2*check_count_ave) : check_count_ave;

    dprint(9, (debugfile, "freq %d tm %d changes %d since_1st_change %d\n",
	       freq, tm, check_count, time(0)-first_status_change));
    dprint(9, (debugfile, "since_status_chg %d chk_cnt_ave %d (tenths)\n",
	       time(0)-last_status_change, check_count_ave));
    dprint(9, (debugfile, "adj_chk_cnt_ave %d (tenths)\n", adj_cca));
    dprint(9, (debugfile, "Check:if changes(%d)xadj_cca(%d) >= freq(%d)x200\n",
	       check_count, adj_cca, freq));
    dprint(9, (debugfile, "      is %d >= %d ?\n",
	       check_count*adj_cca, 200*freq));

    /* the 200 comes from 20 seconds for an average status change time
       multiplied by 10 tenths per second */
    if((timing == DoItNow || (check_count * adj_cca >= freq * 200) ||
       (time(0) - first_status_change >= tm))){
#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile,
                     "Doing checkpoint: %s  Since 1st status change: %d\n",
                     ctime(&now), now - first_status_change));
#endif
        StartInverse();
        PutLine0(0, 0, "**"); 		/* Show something indicate delay*/
	MoveCursor(ps_global->ttyo->screen_rows -3, 0);
        fflush(stdout);
        mail_check(ps_global->mail_stream);
					/* Causes mail file to be written */
#ifdef	DEBUG
	now = time(0);
	dprint(7, (debugfile, "Checkpoint complete: %s\n", ctime(&now)));
#endif
        check_count = 0;
        first_status_change = time(0);
        PutLine0(0, 0, "  ");
        EndInverse();
        fflush(stdout);
        return(1);
    } else {
        return(0);
    }
}



/*----------------------------------------------------------------------
    Call this when we need to tell the check pointing mechanism about
  mailbox state changes.
  ----------------------------------------------------------------------*/
void
check_point_change()
{
    if(!last_status_change)
        last_status_change = time(0) - 10;  /* first change 10 seconds */

    if(!check_count++)
      first_status_change = time(0);
    /*
     * check_count_ave is the exponentially decaying average time between
     * status changes, in tenths of seconds, except never grow by more
     * than double, but always at least one (unless there's a fulll moon).
     */
    check_count_ave = min((check_count_ave +
                max(10*(time(0)-last_status_change),2))/2, 2*check_count_ave);

    last_status_change = time(0);
}



/*----------------------------------------------------------------------
    Call this when a mail file is written to reset timer and counter
  for next check_point.
  ----------------------------------------------------------------------*/
void
reset_check_point()
{
    check_count = 0;
    first_status_change = time(0);
}



/*----------------------------------------------------------------------
    Zero the counters that keep track of mail accumulated between
   commands.
 ----*/
void
zero_new_mail_count()
{
    dprint(9, (debugfile, "New_mail_count zeroed\n"));
    mailbox_mail_since_command = 0L;
    inbox_mail_since_command   = 0L;
}


/*----------------------------------------------------------------------
     Check and see if all the stream are alive

Returns:  0 if there was no change
          1 if streams have died since last call

Also outputs a message that the streams have died
 ----*/
streams_died()
{
    int rv = 0, inbox = 0;

    if(ps_global->dead_stream && !ps_global->noticed_dead_stream) {
        rv = 1;
        ps_global->noticed_dead_stream = 1;
        if(ps_global->mail_stream == ps_global->inbox_stream)
          ps_global->noticed_dead_inbox = 1;
    }

    if(ps_global->dead_inbox && !ps_global->noticed_dead_inbox) {
        rv = 1;
        ps_global->noticed_dead_inbox = 1;
        inbox = 1;
    }
    if(rv == 1) 
      q_status_message1(1, 3, 6,
                        "\007MAIL FOLDER \"%s\" CLOSED DUE TO ACCESS ERROR",
                        pretty_fn(inbox? ps_global->inbox_name :
                                         ps_global->cur_folder));
    return(rv);
}
        
