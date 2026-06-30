#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailindx.c,v 4.52 1993/12/03 21:54:31 mikes Exp $";
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
    mailindx.c
    Implements the mail index screen
     - most code here builds the header list and displays it

 ====*/
 
#include "headers.h"


static struct key index_keys[] = 
     {{"?","Help",0},         {"O","OTHER CMDS",0},  {"M","Main Menu",0},
      {"V","[ViewMsg]",0},    {"P","PrevMsg",0},     {"N","NextMsg",0},
      {"-","PrevPage",0},     {"Spc","NextPage",0},  {"D","Delete",0},
      {"U","Undelete",0},     {"R","Reply",0},       {"F","Forward",0}, 

      {"?","Help",0},         {"O","OTHER CMDS",0},  {"Q","Quit",0},
      {"C","Compose",0},      {"L","ListFldrs",0},   {"G","GotoFldr",0},
      {"Z","Zoom",0},         {"W","WhereIs",0},     {"Y","Print",0},
      {"T","TakeAddr",0},     {"S","Save",0},        {"E","Export",0},

      {"?","Help",0},         {"O","OTHER CMDS",0},  {"X",NULL,0},
      {"&","unXclude",0},     {"A","Apply",0},       {"$","SortIndex",0},
      {"J","Jump",0},         {"Tab","NextNew",0},   {"H","HdrMode",0},
      {"B","Bounce",0},       {"*","Flag",0},        {"|","Pipe",0}};
static struct key_menu index_keymenu =
      {sizeof(index_keys)/(sizeof(index_keys[0])*12), 0, 0,0,0,0, index_keys};
#define ZOOM_KEY 18
#define EXCLUDE_KEY 26
#define UNEXCLUDE_KEY 27
#define APPLY_KEY 28
#define VIEW_FULL_HEADERS_KEY 32
#define BOUNCE_KEY 33
#define FLAG_KEY 34
#define VIEW_PIPE_KEY 35

static struct key nr_anon_index_keys[] = 
       {{"?","Help",0},      {"W","WhereIs", 0},      {"Q","Quit",0},
        {"V","[ViewMsg]",0}, {"P","PrevMsg",0},       {"N","NextMsg",0},
        {"-","PrevPage",0},  {"Spc","NextPage",0},    {"F","Fwd Email",0},
        {"J","Jump",0},      {"$","SortIndex",0},     {NULL,NULL,0}}; 
static struct key_menu nr_anon_index_keymenu = {sizeof(nr_anon_index_keys)/(sizeof(nr_anon_index_keys[0])*12), 0, 0,0,0,0, nr_anon_index_keys};

static struct key nr_index_keys[] = 
       {{"?","Help",0},      {"O","OTHER CMDS",0},    {"Q","Quit",0},
        {"V","[ViewMsg]",0}, {"P","PrevMsg",0},       {"N","NextMsg",0},
        {"-","PrevPage",0},  {"Spc","NextPage",0},    {"F","Fwd Email",0},
        {"J","Jump",0},      {"Y","Print",0},         {"S","Save",0}, 

        {"?","Help",0},      {"O","OTHER CMDS",0},    {"E","Export",0},
        {"C","Compose",0},   {"$","SortIndex",0},     {NULL,NULL,0},
        {NULL,NULL,0},       {"W","WhereIs",0},       {NULL,NULL,0},
        {NULL,NULL,0},       {NULL,NULL,0},           {NULL,NULL,0}};
static struct key_menu nr_index_keymenu = {sizeof(nr_index_keys)/(sizeof(nr_index_keys[0])*12), 0, 0,0,0,0, nr_index_keys};
  

static OtherMenu what_keymenu = FirstMenu;

#ifdef ANSI
long  top_ent_calc(long, long);
void  update_mess_status(MAILSTREAM *, long, long);
char *get_sub(long);
int   compare_subjects(const QSType *, const QSType *);
int   compare_from(const QSType *, const QSType *);
int   compare_to(const QSType *, const QSType *);
int   compare_cc(const QSType *, const QSType *);
int   compare_message_dates(const QSType *, const QSType *);
int   compare_size(const QSType *, const QSType *);
char *get_index_cache(long);
int   i_cache_size(long);
int   i_cache_width();
#ifdef	DOS
void  i_cache_hit(long);
void  icread(void);
void  icwrite(void);
#endif
#else
long  top_ent_calc();
void  update_mess_status();
char *get_sub();
char *get_index_cache();
int   i_cache_size();
int   i_cache_width();
#endif
void  sort_blip();


/*-----------
  Saved state to redraw message index body 
  ----*/
static struct index_state {
    long   current_sorted_msgno,
           max_msgno, paint_from;
} index_body_state;




/*----------------------------------------------------------------------


  ----*/
void
do_index_border(do_header, do_footer, do_clear, which_keys)
     int do_header, do_footer, do_clear;
     int *which_keys;
{
    if(do_clear)
      ClearScreen();

    if(do_header)
      set_titlebar( "FOLDER INDEX", 1, MessageNumber,
                   ps_global->current_sorted_msgno, 0, 0);

    if(do_footer) {
	struct key_menu *km;
	BITMAP bitmap;

	setbitmap(bitmap);
	if(ps_global->anonymous)
	  km = &nr_anon_index_keymenu;
	else if(ps_global->nr_mode)
          km = &nr_index_keymenu;
	else{
          km = &index_keymenu;
#ifndef DOS
          if(F_OFF(F_ENABLE_PIPE,ps_global))
#endif
            clrbitn(VIEW_PIPE_KEY, bitmap);  /* always clear for DOS */
          if(F_OFF(F_ENABLE_FULL_HDR,ps_global))
            clrbitn(VIEW_FULL_HEADERS_KEY, bitmap);
          if(F_OFF(F_ENABLE_BOUNCE,ps_global))
            clrbitn(BOUNCE_KEY, bitmap);
          if(F_OFF(F_ENABLE_FLAG,ps_global))
            clrbitn(FLAG_KEY, bitmap);
          if(F_OFF(F_ENABLE_ZOOM,ps_global))
            clrbitn(ZOOM_KEY, bitmap);
          if(F_OFF(F_ENABLE_APPLY,ps_global))
	    clrbitn(APPLY_KEY, bitmap);
	  if(IS_NEWS)
	    index_keys[EXCLUDE_KEY].label = "eXclude";
	  else {
            clrbitn(UNEXCLUDE_KEY, bitmap);
	    index_keys[EXCLUDE_KEY].label = "eXpunge";
	  }
	}
        draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
						-2, 0, what_keymenu, 0);
	what_keymenu = SameTwelve;
	if(which_keys)
	  *which_keys = km->which;  /* pass back to caller */
    }
}

      
    

/*----------------------------------------------------------------------
        Main loop executing commands for the mail index screen

   Args: The pine_state structure

  Result: Manifold
 ----*/

void
mail_index_screen(state)
     struct pine *state;
{
    int   ch, orig_ch, lines_per_page, which_keys, force;
    long  old_top_ent, old_current_sorted_msgno, old_max_msgno,
          changed_sorted_msgno, tmp;
#ifdef	DOS
    extern void (*while_waiting)();
#endif


    dprint(1, (debugfile, "\n\n ---- MAIL INDEX ----\n"));
    
    ch                          = 'x'; /* For displaying msg 1st time through */
    force                       = 0;
    state->mangled_screen       = 1;
    old_max_msgno               = 0L;
    old_top_ent                 = 0L;
    old_current_sorted_msgno    = 0L;
    index_body_state.paint_from = 1;
    what_keymenu                = FirstMenu;
	

    if(state->mail_stream == NULL) {
	q_status_message(0, 1, 3, "No folder is currently open");
        state->prev_screen = mail_index_screen;
	state->next_screen = main_menu_screen;
	return;
    }


    while (1) {
        lines_per_page  = max(0,state->ttyo->screen_rows - 5);
        index_body_state.paint_from = -1;

        if(state->mangled_screen) {
            state->mangled_header = 1;
            state->mangled_body   = 1;
            state->mangled_footer = 1;
            state->mangled_screen = 0;
        }
        if(state->mangled_body) {
            index_body_state.paint_from = 0;
            state->mangled_body         = 0;
        }


	/*--- Check for new mail, calculate current msgno, and redrawing ---*/
        changed_sorted_msgno = new_mail(NULL, force, 
					ch == NO_OP_IDLE ? 0 :
					ch == NO_OP_COMMAND ? 1 : 2);
	force = 0;			/* may not need to next time around */
        dprint(9, (debugfile, "@@@@@ changed_sorted_msgno: %ld current:%ld\n",
                   changed_sorted_msgno,
                   state->current_sorted_msgno));
        if(changed_sorted_msgno >= 0 || state->max_msgno != old_max_msgno) {
            /* Here we have gotten some new mail, expunged or something. 
               In any case we have to figure if some painting needs doing */
            if(state->current_sorted_msgno < 0L && state->max_msgno > 0L) {
                /* Used to be no mail in folder, now there is */
                 index_body_state.paint_from = 0;                
                state->current_sorted_msgno  = 1L;
            } else if(changed_sorted_msgno >= 0L &&
                      changed_sorted_msgno < old_top_ent+(long)lines_per_page){
                /* Here we know some painting needs doing because the
                   change in the index was above the bottom of the screen.
                   (We can ignore changes in the index off the bottom of 
                    the screen -- no repainting will be required)
                 */
                dprint(9, (debugfile, "@@@@@ new: %d,  current: %d\n",
                           state->new_current_sorted_msgno,
                           state->current_sorted_msgno));
                if(state->current_sorted_msgno !=
                   state->new_current_sorted_msgno){
                    tmp = 32000; /* This code breaks for more that 32K msgs*/
                    if(state->current_sorted_msgno >= 0)
                      tmp = min(tmp, state->current_sorted_msgno);
                    if(state->new_current_sorted_msgno >= 0)
                      tmp = min(tmp, state->new_current_sorted_msgno);
                    if(changed_sorted_msgno >= 0)
                      tmp = min(tmp, changed_sorted_msgno);
                    if(index_body_state.paint_from >= 0)
                      tmp = min(tmp, index_body_state.paint_from);
                    if(tmp < 0L)
                      panic("Error calculating index position");
                    index_body_state.paint_from = tmp;
                } else  {
                    index_body_state.paint_from = 
                      index_body_state.paint_from < 0L ? changed_sorted_msgno: 
                       min(index_body_state.paint_from, changed_sorted_msgno);
                }
                if(state->new_current_sorted_msgno > 0L) {
                    if(changed_sorted_msgno >= 0)
                      state->current_sorted_msgno =
                        state->new_current_sorted_msgno;
                    state->new_current_sorted_msgno = -1L;
                }
            } else {
                state->new_current_sorted_msgno = -1L;
            }
            state->mangled_header = 1;
        }
        if(state->current_sorted_msgno > 0L) {
            if(old_top_ent !=
               top_ent_calc(state->current_sorted_msgno,(long)lines_per_page)){
                 index_body_state.paint_from = 0;

            }
        }

        if(streams_died())
          state->mangled_header = 1;

        /*------------ Update the title bar -----------*/
	if(state->mangled_header) {
            do_index_border(1, 0, 0, NULL);
	    state->mangled_header = 0;
	} else if(old_current_sorted_msgno != state->current_sorted_msgno) {
            MESSAGECACHE *mc;
            mc = mail_elt(state->mail_stream,state->current_sorted_msgno);
	    update_titlebar_message((int)state->current_sorted_msgno);
            update_titlebar_status(mc);
	}


        /*------------- Paint/update the index screen body ---------------*/
        /* The positioning of things on the screen is controlled *only* 
           by the current message number and the number of lines on the
           screen. The total number of messages has some affect,
           as does the arrival of new mail. The mangled_body flag 
           controls when the redrawing happens. These variables along
           with the saved state in the old_.... variables are all that
           control the screen painting. Some of the calculations are done 
           in the new mail section above. */
        if(index_body_state.paint_from >= 0) {
            index_body_state.current_sorted_msgno =state->current_sorted_msgno;
            index_body_state.max_msgno            = state->max_msgno;
            if(state->current_sorted_msgno != old_current_sorted_msgno)
              index_body_state.paint_from =
                min(index_body_state.paint_from,
                   min(state->current_sorted_msgno, old_current_sorted_msgno));
            redraw_index_body();
            state->mangled_body = 0;
        } else if(state->current_sorted_msgno != old_current_sorted_msgno) {
            if(ps_global->low_speed) {

                PutLine1(2 + (int)(old_current_sorted_msgno - old_top_ent),
                         0, "%c ",
			(*build_header_line(old_current_sorted_msgno)=='+')
			   ? (void *) '+': (void *) ' ');

                PutLine1(2 + (int)(state->current_sorted_msgno - old_top_ent),
                         0, "%c>", 
			(*build_header_line(state->current_sorted_msgno)=='+')
			   ? (void *) '+': (void *) '-');
            } else {
                PutLine0(2 + (int)(old_current_sorted_msgno - old_top_ent),
                         0, build_header_line(old_current_sorted_msgno));
                StartInverse();

                PutLine0(2 + (int)(state->current_sorted_msgno - old_top_ent),
                         0,build_header_line(state->current_sorted_msgno));
                EndInverse();
            }
            index_body_state.current_sorted_msgno =state->current_sorted_msgno;
        }
        /* Save the state to know what was drawn last */
        old_top_ent                = top_ent_calc(state->current_sorted_msgno,
                                          (long)lines_per_page);
        old_current_sorted_msgno   = state->current_sorted_msgno;
        old_max_msgno              = state->max_msgno;
        ps_global->redrawer        = redraw_index_body;


        /*------------ draw the footer/key menus ---------------*/
	if(state->mangled_footer) {
            if(!state->painted_footer_on_startup)
              do_index_border(0, 1, 0, &which_keys);
	    state->mangled_footer = 0;
	}

        state->painted_body_on_startup   = 0;
        state->painted_footer_on_startup = 0;

	/*------- Display any message queued up, such as new mail ----*/
        display_message(ch);
	MoveCursor(state->ttyo->screen_rows - 3, 0);

        /* Let read_command do the fflush(stdout) */

        /*---------- Read command and validate it ----------------*/
#ifdef	DOS
#ifdef	MOUSE
	{extern int register_mfunc();
	 extern unsigned long mouse_in_index();

	 ps_global->top_of_screen_sorted_msgno = old_top_ent;
	 register_mfunc(mouse_in_index, 2, 0, state->ttyo->screen_rows-4,
			state->ttyo->screen_cols);
	}
#endif
	/*
	 * AND pre-build header lines.  This works just fine under
	 * DOS since we wait for characters in a loop. Something will
         * will have to change under UNIX if we want to do the same.
	 */
	while_waiting = build_header_cache;
#endif
	ch = read_command();
#ifdef	DOS
	while_waiting = NULL;
#ifdef	MOUSE
	{extern void clear_mfunc();
	clear_mfunc();
	}
#endif
#endif

        orig_ch = ch;

	if(ch < 0x0100)
	  if(isupper(ch))
	    ch = tolower(ch);

        if(which_keys == 1)
          if(ch >= PF1 && ch <= PF12)
            ch = PF2OPF(ch);
        if(which_keys == 2)
          if(ch >= PF1 && ch <= PF12)
            ch = PF2OOPF(ch);

	ch = validatekeys(ch);


	/*----------- Execute the command ------------------*/
	switch(ch) {

            /*---------- Roll keymenu ----------*/
          case PF2:
          case OPF2:
          case OOPF2:
	  case 'o':
            if(ps_global->anonymous) {
	      if(ch == PF2)
		ch = 'w';
              goto df;
	    }
            if(ch == 'o')
	      warn_other_cmds();
	    what_keymenu = NextTwelve;
	    state->mangled_footer = 1;
	    break;


            /*---------- Scroll back up ----------*/
	  case PF7:
	  case '-' :
          case ctrl('Y'): 
	  case KEY_PGUP:
	    if(old_top_ent > lines_per_page)
                state->current_sorted_msgno = old_top_ent - lines_per_page;
	    else
	        q_status_message(0, 0, 1, "\007Already on first page.");
	    break;


            /*---------- Scroll forward, next page ----------*/
	  case PF8:  /* NEXT PAGE */
	  case '+':
          case ctrl('V'): 
	  case KEY_PGDN:
	  case ' ':
	    if(lines_per_page + old_top_ent <= state->max_msgno)
                state->current_sorted_msgno = old_top_ent + lines_per_page;
	    else
	        q_status_message(0, 0, 1, "\007Already on last page");
	    break;


            /*---------- Suspend Pine ----------*/
          case ctrl('Z'):
            if(!have_job_control())
              goto df;
            if(F_OFF(F_CAN_SUSPEND,state)) {
                q_status_message(1, 1, 3,
                            "\007Pine suspension not enabled - see help text");
                break;
            } else {
                do_suspend(state);
            }
            /*-- Fall through to redraw --*/

            /*---------- Redraw/resize ----------*/
          case KEY_RESIZE:
	    clear_index_cache();
	  case ctrl('L'):
	    ClearScreen();
            state->mangled_screen = 1;		/* force repaint and... */
	    force                 = 1;		/* check for new mail   */
            break;
                     

            /*---------- No op command ----------*/
          case NO_OP_IDLE:
	  case NO_OP_COMMAND:
            break;	/* no op check for new mail */


            /*---------- Default -- all other command ----------*/
          default:
          df:
            process_cmd(ch, 1, orig_ch, &force);
            if(state->next_screen != SCREEN_FUN_NULL)
            {
                state->prev_screen = mail_index_screen;
      	        return;
            }
      	    if(state->status_changed)
      	      update_mess_status(state->mail_stream,
                                 ps_global->sort[old_current_sorted_msgno],
                                 old_current_sorted_msgno - old_top_ent);
	  }/* The big switch */
    } /* the BIG while loop! */
} 



/*----------------------------------------------------------------------
     Calculate the message number that should be at the top of the display

  Args: current - the current message number
        lines_per_page - the number of lines for the body of the index only

  Returns: -1 if the current message is -1 
           the message entry for the first message at the top of the screen.

When paging in the index it is always on even page boundies, and the
current message is always on the page thus the top of the page is
completely determined by the current message and the number of lines
on the page. 
 ----*/
long
top_ent_calc(current, lines_per_page)
  long current, lines_per_page;
{
    if(current < 0L)
      return(-1);
    if(lines_per_page == 0L)
      return(current);
    return(lines_per_page * ((current - 1L)/ lines_per_page) + 1L);
}


/*----------------------------------------------------------------------
    This redraws the body of the index screen possibly taking into
account and changes in the size of the screen. All the state needed to
repaint is in the static variables so this can be called from
anywhere.
 ----*/
void
redraw_index_body()
{
    long ent, scr, lines, top_ent;

    lines   = (long)(ps_global->ttyo->screen_rows - 5);
    top_ent = top_ent_calc(index_body_state.current_sorted_msgno, lines);
    ent     = max(top_ent, index_body_state.paint_from);

    dprint(9, (debugfile, "Repaint starting with %ld at %ld\n",
               ent, ent- top_ent));
    
    for(scr = ent - top_ent; scr < lines; scr++) {
        MoveCursor(2 + (int)scr, 0);
        CleartoEOLN();
        if(ent > 0 && ent <= index_body_state.max_msgno){
            if(ent == index_body_state.current_sorted_msgno 
		&& !ps_global->low_speed)
                  StartInverse();

            PutLine0(2 + (int)scr, 0, build_header_line(ent));
            if(ent == index_body_state.current_sorted_msgno){
		if(ps_global->low_speed)
		  PutLine1(2 + (int)scr, 0, "%c>", 
			   (*build_header_line(ent) == '+') 
				? (void *) '+' : (void *) '-');
		else
                  EndInverse();
	    }
            ent++;
        }
    }
    fflush(stdout);
}



/*----------------------------------------------------------------------
      Update the status char on the left, usually D or N

   Args:  stream      -- MAILSTREAM of current folder
          msgno       -- message number of update
          screen_line -- line on the screen the message is at

  Result: The status char is updated on the screen

   only works with current message which is high lit 
 ----*/

void
update_mess_status(stream, msgno, screen_line)
     long msgno, screen_line;
     MAILSTREAM *stream;
{
    MESSAGECACHE *m;
    char         *string, sequence[10];

    sprintf(sequence, "%ld", msgno);
    mail_fetchflags(stream, sequence);
    m = mail_elt(stream, (long)msgno);
    string = status_string(m);
    if(!ps_global->low_speed)
      StartInverse();
    MoveCursor(2 + (int)screen_line, 2);
    Write_to_screen(string);
    if(!ps_global->low_speed)
      EndInverse();
    fflush(stdout);
}
    


/*----------------------------------------------------------------------
      Create a string summarizing the message header for index on screen

   Args:  message_number  -- Message number to create line for

  Result: returns a malloced string
          saves string in a cache for next call for same header
 ----*/

char *
build_header_line(message_number)
     long message_number;
{
    ENVELOPE     *envelope;
    MESSAGECACHE *cache;
    ADDRESS      *addr;
    int           status_length, size_length, subject_length, from_length;
    char          status[20], from[MAX_SCREEN_COLS/2+1], size[12],
                  subject[MAX_SCREEN_COLS+1], to_us;
    char         *s, *s_tmp, *buffer;
    struct date   d;


    dprint(8, (debugfile, "=== build_header_line (%ld) called ===\n",
               message_number));

    if(*(buffer = get_index_cache(message_number)) != '\0') {
        dprint(9, (debugfile, "Returning %x<%s> (%d)\n", buffer, 
		   buffer, strlen(buffer)));
	return(buffer);
    }

    envelope = mail_fetchstructure(ps_global->mail_stream,
                                  ps_global->sort[message_number],
				  NULL);
    cache    = mail_elt(ps_global->mail_stream,
                        ps_global->sort[message_number]);

    if(envelope == NULL || cache == NULL) {
        sprintf(buffer,"    %-3ld", message_number);
        return(buffer);
    }

    /*
     * check that the envelope returned has something to display.
     * if empty, indicate that no message info found...
     */
    if(!envelope->remail && !envelope->return_path && !envelope->date &&
       !envelope->from && !envelope->sender && !envelope->reply_to &&
       !envelope->subject && !envelope->to && !envelope->cc &&
       !envelope->bcc && !envelope->in_reply_to && !envelope->message_id &&
       !envelope->newsgroups){
	sprintf(buffer,"    %-3ld        %-*.*s", message_number,
		max(i_cache_width()-15, 0), max(i_cache_width()-15, 0),
		"[ No Message Text Available ]");
	return(buffer);
    }

    /*------------------------------------------------------
       Four parts:
          status:  arrow cursor, date, msgno, status, 15 chars unless more
		     than 3 digits in message_number
          from:    may show To:, always at least 18 chars, half of space over
		     80 is used for this field
          size:    currently in bytes up to 99K, always 10 chars
                     when line counts become available, always 8 chars
          subject: uses up all the leftover space
 over all minimum: 80 chars 
       ----------------------------------------------------------------------*/

    /*-- status --*/
    parse_date(envelope->date, &d);
    to_us = ' ';
    for(addr = envelope->to; addr && to_us == ' '; addr = addr->next)
      if(address_is_us(addr, ps_global))
        to_us = '+';

    sprintf(status, "%c %s %-3ld %s %2d ", to_us, status_string(cache),
	    message_number, month_abbrev(d.month), d.day);
    status_length = strlen(status);

    /*--- from ---*/
    if(!ps_global->nr_mode) {
        from_length = 18 + max(0, ps_global->ttyo->screen_cols - 80 + 1)/2;
        if(envelope->from==NULL || address_is_us(envelope->from, ps_global)) {
            ADDRESS *addr;
            char    *a_string;
            addr = envelope->to != NULL ? envelope->to :
                     envelope->cc != NULL ? envelope->cc : NULL;
            if(addr == NULL) {
                sprintf(from, "%*.*s", from_length, from_length,
                      "                                        ");
            } else {
                a_string =  addr_list_string(addr, 0);
                sprintf(from, "To: %-*.*s", from_length - 4, from_length - 4,
                    a_string);
                fs_give((void **)&a_string);
            }
        } else {
            mail_fetchfrom(from, ps_global->mail_stream,
                           ps_global->sort[message_number], (long)from_length);
        }
    } else {
        from[0] = '\0';
    }
    from_length = strlen(from);

    /*--- size ---*/
    if(!ps_global->nr_mode && ps_global->mail_stream->mailbox[0] != '*'){
        if(cache->rfc822_size < 100000) {
            s_tmp = comatose(cache->rfc822_size);
            sprintf(size, " %-*.*s(%s) ", 6 - strlen(s_tmp),
                6 - strlen(s_tmp), "       ", s_tmp);
        } else if(cache->rfc822_size < 10000000) {
            s_tmp = comatose(cache->rfc822_size/1000);
            sprintf(size, " %-*.*s(%sK) ", 5 - strlen(s_tmp),
                5 - strlen(s_tmp), "       ", s_tmp);
        } else {
            strcpy(size, "  (****)");
        }
    } else {
        size[0] = ' ';
        size[1] = '\0';
    }
    size_length = strlen(size);

        
    /*-- subject --*/
    subject_length = max(ps_global->ttyo->screen_cols, 80) -
			          status_length - from_length - size_length;
    dprint(9, (debugfile, "subject_length : %d\n", subject_length));
    mail_fetchsubject(subject, ps_global->mail_stream,
                      ps_global->sort[message_number], (long)subject_length);
    for(s = subject + strlen(subject); s < subject + subject_length; s++)
      *s = ' ';
    *s = '\0';
    dprint(9, (debugfile, "strlen(subject : %d\n", strlen(subject)));

    /*--- Put them all together ---*/
    sprintf(buffer, "%s%s%s%s", status, from, size, subject);
    /* Truncate it to proper size */
    buffer[min(ps_global->ttyo->screen_cols, i_cache_width())] = '\0';
    dprint(9, (debugfile, "Returning %x<%s> (%d)\n", buffer, buffer, strlen(buffer)));
    return(buffer);
}



/*-----
   Every DISPLAY_COUNT compares, call display_message to keep
 the user informed that we're still grinding away on the sort. 
 The cost of the sort is fetching all the headers which happens in the
 first itterations of the sort
  ----*/
#define    DISPLAY_COUNT  200
static long compare_count = 0;
static long last_mess;
static int  mess_count;


/*----------------------------------------------------------------------
  Compare function for sorting on subjects. Ignores case, space and "re:"
  ----*/
int
compare_subjects(a, b)
    const QSType *a, *b;
{
    char *suba, *subb;
    long *mess_a = (long *)a, *mess_b = (long *)b;
    int   diff, res;
    long  mdiff;

    sort_blip();

    suba = get_sub(*mess_a);
    subb = get_sub(*mess_b);

    diff = strucmp(suba, subb);

    fs_give((void **)&suba);
    fs_give((void **)&subb);

    if(diff == 0)mdiff = *mess_a - *mess_b;
    /* convert to int */
    res = diff != 0 ? diff :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
    return(ps_global->current_reverse_sort ? -res : res);
}

char *
get_sub(mess)
     long mess;
{
    ENVELOPE *e;
    char *s, *s2;    

    e = mail_fetchstructure(ps_global->mail_stream, mess, NULL);
    if(e == NULL || e->subject == NULL) {
        s2 = cpystr("");
    } else {
        for(s = e->subject; *s && (isspace(*s) || *s == '['); s++);
        if(struncmp(s, "re:", 3) == 0)
          s += 3;
        while(*s && (isspace(*s) || *s == '['))
          s++;
        s2 = cpystr(s);
    }
    return(s2);
}



/*----------------------------------------------------------------------
   Compare the To: fields for sorting. Ignore case
   ----*/
int 
compare_to(a, b)
    const QSType *a, *b;
{
    long     *mess_a = (long *)a, *mess_b = (long *)b;
    ENVELOPE *e;
    char     *b_addr, *a_addr;
    int       res, diff;
    long      mdiff;

    sort_blip();

    e = mail_fetchstructure(ps_global->mail_stream, *mess_a, NULL);
    if(e == NULL || e->to == NULL)
      a_addr = cpystr("");
    else
      a_addr = cpystr(addr_string(e->to));

    e = mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
    if(e == NULL || e->to == NULL)
      b_addr = cpystr("");
    else
      b_addr = cpystr(addr_string(e->to));

    diff = strcmp(a_addr, b_addr);

    fs_give((void **)&a_addr);
    fs_give((void **)&b_addr);

    if(diff == 0)mdiff = *mess_a - *mess_b;
    /* convert to int */
    res = diff != 0 ? diff :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
    return(ps_global->current_reverse_sort ? -res : res);
}


/*----------------------------------------------------------------------
   Compares the From: field for sorting. Ignores case.
  BUG should do something when the sender is us
  ----*/
int 
compare_from(a, b)
    const QSType *a, *b;
{
    long     *mess_a = (long *)a, *mess_b = (long *)b;
    ENVELOPE *e;
    char  froma[200];
    int   diff, res;
    long  mdiff;

    sort_blip();

    e = mail_fetchstructure(ps_global->mail_stream, *mess_a, NULL);
    if(e == NULL || e->from == NULL || e->from->mailbox == NULL)  
      froma[0] = '\0';
    else
      strncpy(froma, e->from->mailbox, sizeof(froma) - 1);
    froma[sizeof(froma) - 1] = '\0';

    e = mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
    if(e != NULL && e->from != NULL && e->from->mailbox != NULL) {
        diff = strucmp(froma, e->from->mailbox);
        if(diff == 0)mdiff = *mess_a - *mess_b;
        /* convert to int */
        res = diff != 0 ? diff :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
        return(ps_global->current_reverse_sort ? -res : res);
    }
    mdiff = *mess_a - *mess_b;
    return(mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0);
}


/*----------------------------------------------------------------------
   Compare the Cc: fields for sorting. Ignore case.
   ----*/
int 
compare_cc(a, b)
    const QSType *a, *b;
{
    long     *mess_a = (long *)a, *mess_b = (long *)b;
    ENVELOPE *e;
    char     *b_addr, *a_addr;
    int       diff, res;
    long      mdiff;

    sort_blip();

    e = mail_fetchstructure(ps_global->mail_stream, *mess_a, NULL);
    if(e == NULL || e->cc == NULL)
      a_addr = cpystr("");
    else
      a_addr = cpystr(addr_string(e->to));

    e = mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
    if(e == NULL || e->cc == NULL)
      b_addr = cpystr("");
    else
      b_addr = cpystr(addr_string(e->to));

    diff = strcmp(a_addr, b_addr);
    fs_give((void **)&a_addr);
    fs_give((void **)&b_addr);
    if(diff == 0)mdiff = *mess_a - *mess_b;
    /* convert to int */
    res = diff != 0 ? diff :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
    return(ps_global->current_reverse_sort ? -res : res);
}


/*----------------------------------------------------------------------
   Compare dates
  ----*/
int
compare_message_dates(a, b)
    const QSType *a, *b;
{
    long       *mess_a = (long *)a, *mess_b = (long *)b;
    int         diff, res;
    long        mdiff;
    ENVELOPE   *e;
    struct date d_a, d_b;

    sort_blip();

    mdiff = *mess_a - *mess_b;
    res = mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;

    e = mail_fetchstructure(ps_global->mail_stream, *mess_a, NULL);

    if(e != NULL && e->date != NULL) {
      parse_date(e->date, &d_a);
      e = mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
      if(e == NULL || e->date == NULL)  /* B is NULL, A is not */
        return(1);
      parse_date(e->date, &d_b);
    }else {
      e = mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
      if(e == NULL || e->date == NULL)  /* both A and B are NULL */
        return(res);
      return(-1);  /* A is NULL, B is not */
    }

    dprint(9,(debugfile,"before GMT: wkday:%d  month:%d  year:%d  day:%d  hour:%d  min:%d  sec:%d  off_gmt:%d\n",
            d_a.wkday, d_a.month, d_a.year, d_a.day, d_a.hour, d_a.minute,
              d_a.sec,d_a.hours_off_gmt));

    convert_to_gmt(&d_a);

    dprint(9,(debugfile,"after GMT: wkday:%d  month:%d  year:%d  day:%d  hour:%d  min:%d  sec:%d\n",
            d_a.wkday, d_a.month, d_a.year, d_a.day, d_a.hour, d_a.minute,
              d_a.sec));

    convert_to_gmt(&d_b);
    
    diff = compare_dates(&d_a, &d_b);
    
    /* convert to int */
    res = diff != 0 ? diff :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
    return(ps_global->current_reverse_sort ? -res : res);
}

    

/*----------------------------------------------------------------------
  Compare size of messages for sorting
 ----*/
int
compare_size(a, b)
    const QSType *a, *b;
{
    long *mess_a = (long *)a, *mess_b = (long *)b;
    long size_a, size_b, sdiff, mdiff;
    MESSAGECACHE *mc;
    int  res;

    sort_blip();

    mail_fetchstructure(ps_global->mail_stream, *mess_a, NULL);
    mc = mail_elt(ps_global->mail_stream, *mess_a);
    size_a = mc != NULL ? mc->rfc822_size : -1;

    mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
    mc = mail_elt(ps_global->mail_stream, *mess_b);
    size_b = mc != NULL ? mc->rfc822_size : -1;

    sdiff = size_a - size_b;
    if(sdiff == 0L)mdiff = *mess_a - *mess_b;
    /* convert to int */
    res = sdiff != 0L ? (sdiff > 0 ? 1 : -1) :
           mdiff != 0L ? (mdiff > 0L ? 1 : -1) : 0;
    return(ps_global->current_reverse_sort ? -res : res);
}



void
sort_blip()
{
    compare_count++;
    if((compare_count % DISPLAY_COUNT) == 0) {
        display_message(NO_OP_COMMAND);
        if(time(0) - last_mess > 15) {
            q_status_message1(0, 2, 5, "Still sorting..%s",
                              repeat_char(mess_count, '.'));
            mess_count++;
            last_mess = time(0);
        }
    }
}

    



/*----------------------------------------------------------------------
    Sort the current folder into the order set in the sort_order
 variable
  ----*/
void
sort_current_folder()
{
    long       mess_no, *sort, tmp;
    SortOrder  so;

    sort = ps_global->sort;
    so   = ps_global->current_sort_order;

    dprint(2, (debugfile, "Sorting by %s%s\n", sort_name(so),
               ps_global->current_reverse_sort ? "/reverse" : ""));

    if(so == SortArrival){
	for(mess_no = 1; mess_no <= ps_global->max_msgno; mess_no++)
            sort[mess_no] = ps_global->current_reverse_sort ? 
			       1+ ps_global->max_msgno - mess_no: mess_no;

    }else{
	for(mess_no = 1; mess_no <= ps_global->max_msgno; mess_no++)
            sort[mess_no] = mess_no;
#ifndef	DOS
        if(ps_global->max_msgno > 500) 
#endif
            q_status_message1(0, 2, 4, "Sorting folder by %s", sort_name(so));
                              
        compare_count = 0;
        mess_count    = 0;
        last_mess     = time(0);
        qsort(sort+1, (size_t)ps_global->max_msgno, sizeof(long),
              so == SortSubject ? compare_subjects :
               so == SortFrom    ? compare_from :
                so == SortTo      ? compare_to :
                 so == SortCc      ? compare_cc :
                  so == SortDate    ? compare_message_dates :
                                       compare_size);
    }
}


char *    
sort_name(so)
  SortOrder so;
{
    static char *sort_names[] = {"arrival", "date", "subject", "cc",
                                 "from",    "to",   "size",    NULL};
    static SortOrder sort_types[] = { SortArrival, SortDate, SortSubject,
                                  SortCc,      SortFrom, SortTo, SortSize};
    int x;
    for(x = 0; so != sort_types[x]; x++);
    return(sort_names[x]);
}


/*
 *           * * *  Index entry cache manager  * * *
 */

/*
 * at some point, this could be made part of the pine_state struct.
 * the only changes here would be to pass the ps pointer around
 */
static struct index_cache {
   void	    *cache;			/* pointer to cache */
   char	    *name;			/* pointer to cache name */
   long      num;			/* number of last index in cache */
   unsigned  size;			/* size of each index line       */
} icache = { NULL, 0L, 0 };
  
/*
 * cache size growth increment
 */

#ifdef	DOS
/*
 * the idea is to have the cache increment be a multiple of the block
 * size (4K), for efficient swapping of blocks.  we can pretty much
 * assume 81 character lines.
 *
 * REMEMBER: number of lines in the incore cache has to be a mutliple 
 *           of the cache growth increment!
 */
#define	IC_SIZE		(50L)			/* cache growth increment  */
#define	ICC_SIZE	(50L)			/* enties in incore cache  */
#define FUDGE           (46L)			/* extra chars to make 4096*/

static char	*incore_cache = NULL;		/* pointer to incore cache */
static long      cache_block_s = 0L;		/* save recomputing time   */
static long      cache_base = 0L;		/* index of line 0 in block*/
#else
#define	IC_SIZE		100
#endif

/*
 * important values for cache building
 */
static MAILSTREAM *bc_this_stream = NULL;
static long  bc_start, bc_current;
static short bc_done = 0;


/*
 * way to return the current cache entry size
 */
int
i_cache_width()
{
    return(icache.size-1);
}


/* 
 * i_cache_size - make sure the cache is big enough to contain
 * requested entry
 */
int
i_cache_size(indx)
    long         indx;
{
    long j;

    if(icache.size != max(ps_global->ttyo->screen_cols, 80) + 1){
        clear_index_cache();		/* clear cache and start over! */
	icache.size = max(ps_global->ttyo->screen_cols, 80) + 1;
    }

    if(indx > (j = icache.num - 1L)){		/* make room for entry! */
	int     tmplen = (size_t)icache.size * sizeof(char);
	char   *tmpline;

	while(indx >= icache.num)
	  icache.num += IC_SIZE;

#ifdef	DOS
	tmpline = fs_get((size_t)tmplen);
	memset(tmpline, 0, (size_t)tmplen);
	if(icache.cache == NULL){
	    if(!icache.name)
	      icache.name = (char *)temp_nam(NULL, "pi");

	    if((icache.cache = (void *)fopen(icache.name,"w+b")) == NULL){
		sprintf(tmp_20k_buf, "Can't open index cache: %s",icache.name);
		fatal(tmp_20k_buf);
	    }

	    for(j = 0; j < icache.num; j++){
	        if(fwrite(tmpline,(size_t)tmplen,
			  (size_t)1,(FILE *)icache.cache) != 1)
		  fatal("Can't write index cache in resize");

		if(j%ICC_SIZE == 0){
		  if(fwrite(tmpline,(size_t)FUDGE,
				(size_t)1,(FILE *)icache.cache) != 1)
		    fatal("Can't write FUDGE factor in resize");
	        }
	    }
	}
	else{
	    /* init new entries */
	    fseek((FILE *)icache.cache, 0L, 2);		/* seek to end */

	    for(;j < icache.num; j++){
	        if(fwrite(tmpline,(size_t)tmplen,
			(size_t)1,(FILE *)icache.cache) != 1)
		  fatal("Can't write index cache in resize");

		if(j%ICC_SIZE == 0){
		  if(fwrite(tmpline,(size_t)FUDGE,
				(size_t)1,(FILE *)icache.cache) != 1)
		    fatal("Can't write FUDGE factor in resize");
	        }
	    }
	}

	fs_give((void **)&tmpline);
#else
	if(icache.cache == NULL){
	    icache.cache = (void *)fs_get((icache.num+1)*tmplen);
	    memset(icache.cache, 0, (icache.num+1)*tmplen);
	}
	else{
            fs_resize((void **)&(icache.cache), (size_t)(icache.num+1)*tmplen);
	    tmpline = (char *)icache.cache + ((j+1) * tmplen);
	    memset(tmpline, 0, (icache.num - j) * tmplen);
	}
#endif
    }

    return(1);
}

#ifdef	DOS
/*
 * read a block into the incore cache
 */
void
icread()
{
    size_t n;

    if(fseek((FILE *)icache.cache, (cache_base/ICC_SIZE) * cache_block_s, 0))
      fatal("ran off end of index cache file in icread");

    n = fread((void *)incore_cache, (size_t)cache_block_s, 
		(size_t)1, (FILE *)icache.cache);

    if(n != 1L)
      fatal("Can't read index cache block in from disk");
}


/*
 * write the incore cache out to disk
 */
void
icwrite()
{
    size_t n;

    if(fseek((FILE *)icache.cache, (cache_base/ICC_SIZE) * cache_block_s, 0))
      fatal("ran off end of index cache file in icwrite");

    n = fwrite((void *)incore_cache, (size_t)cache_block_s,
		(size_t)1, (FILE *)icache.cache);

    if(n != 1L)
      fatal("Can't write index cache block in from disk");
}


/*
 * make sure the neccessary block of index lines is in core
 */
void
i_cache_hit(indx)
    long         indx;
{
    dprint(9, (debugfile, "i_cache_hit: %d\n", indx));
    /* no incore cache, create it */
    if(!incore_cache){
	cache_block_s = (((long)icache.size * ICC_SIZE) + FUDGE)*sizeof(char);
	incore_cache  = (char *)fs_get((size_t)cache_block_s);
	cache_base = (indx/ICC_SIZE) * ICC_SIZE;
	icread();
	return;
    }

    if(indx >= cache_base && indx < (cache_base + ICC_SIZE))
	return;

    icwrite();

    cache_base = (indx/ICC_SIZE) * ICC_SIZE;
    icread();
}
#endif


/*
 * return the index line associated with the given message number
 */
char *
get_index_cache(msgno)
    long         msgno;
{
    if(!i_cache_size(--msgno)){
	q_status_message(0, 1, 3, "get_index_cache failed!");
	return(NULL);
    }

#ifdef	DOS
    i_cache_hit(msgno);			/* get entry into core */
    return(incore_cache + ((msgno%ICC_SIZE) * (long)max(icache.size,FUDGE)));
#else
    return((char *)(icache.cache) + (msgno * (long)icache.size * sizeof(char)));
#endif
}


/*
 * the idea is to pre-build and cache index lines while waiting
 * for command input.
 */
void
build_header_cache()
{
    long lines_per_page = max(0,ps_global->ttyo->screen_rows - 5);

    if(ps_global->max_msgno == 0 || ps_global->mail_stream == NULL
	|| (bc_this_stream == ps_global->mail_stream && bc_done >= 2))
      return;

    if(bc_this_stream != ps_global->mail_stream){ /* reset? */
	bc_this_stream = ps_global->mail_stream;
	bc_current = bc_start = top_ent_calc(ps_global->current_sorted_msgno,
				lines_per_page);
	bc_done  = 0;
    }

    if(bc_done == 0 && bc_current > ps_global->max_msgno){	/* wrap? */
	bc_current = bc_start - lines_per_page;
	bc_done++;
    }
    else if(bc_done == 1 && (bc_current % lines_per_page) == 1)
      bc_current -= (2L * lines_per_page);

    if(bc_current < 1){
	bc_done = 2;			/* really done! */
	return;
    }

    (void)build_header_line(bc_current++);
}


/*
 * erase a particular entry in the cache
 */
void
clear_index_cache_ent(indx)
    long indx;
{
    *get_index_cache(indx) = '\0';
}


/*
 * clear the index cache associated with the current mailbox
 */
void
clear_index_cache()
{
#ifdef	DOS
    cache_base = 0L;
    if(incore_cache)
      fs_give((void **)&incore_cache);

    if(icache.cache){
	fclose((FILE *)icache.cache);
	icache.cache = NULL;
    }

    if(icache.name){
	unlink(icache.name);
	fs_give((void **)&icache.name);
    }
#else
    if(icache.cache)
      fs_give((void **)&(icache.cache));
#endif
    icache.num  = 0L;
    icache.size = 0;
    bc_this_stream = NULL;
}


#ifdef	DOS
/*
 * flush the incore_cache, but not the whole enchilada
 */
void
flush_index_cache()
{
    if(incore_cache){
	if(ps_global->max_msgno)	/* write this block out to disk */
	  icwrite();

	fs_give((void **)&incore_cache);
	cache_base = 0L;
    }
}
#endif
