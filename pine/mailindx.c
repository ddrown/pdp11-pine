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

/* not static -- also used from folder.c, see the externs there */
char *already_first_page = "\007Already on first page";
char *already_last_page  = "\007Already on last page";

/* Command-key/label strings shared across every screen's key-menu
 * table (index_keys/folder_keys/help_keys/composer_help_keys/
 * view_keys/text_att_view_keys/main_keys/oe_keys) -- these are the
 * common navigation commands (Help, OTHER CMDS, WhereIs, PrevPage/
 * NextPage, Quit, ...) that show up on nearly every screen, so the
 * same literal text was previously duplicated up to a dozen times
 * across these files. Not static -- also used from folder.c/
 * mailview.c/pine.c/ttyin.c, see the externs there. This only
 * dedupes the *string content*; it doesn't touch the struct key
 * array-of-values format itself (see TODO-pdp11.md's KEYMENU_S
 * entry for that separate, larger, deliberately-deferred refactor).
 */
char km_qmark[] = "?";
char km_help[] = "Help";
char km_othercmds[] = "OTHER CMDS";
char km_mainmenu[] = "Main Menu";
char km_gotofldr[] = "GotoFldr";
char km_compose[] = "Compose";
char km_prevpage[] = "PrevPage";
char km_nextpage[] = "NextPage";
char km_spc[] = "Spc";
char km_delete[] = "Delete";
char km_undelete[] = "Undelete";
char km_forward[] = "Forward";
char km_prevmsg[] = "PrevMsg";
char km_nextmsg[] = "NextMsg";
char km_nextnew[] = "NextNew";
char km_hdrmode[] = "HdrMode";
char km_save[] = "Save";
char km_whereis[] = "WhereIs";
char km_listfldrs[] = "ListFldrs";
char km_quit[] = "Quit";
char km_exithelp[] = "Exit Help";
char km_o[] = "O";
char km_w[] = "W";
char km_e[] = "E";
char km_dash[] = "-";


static struct key index_keys[] =
     {{km_qmark,km_help,0},         {km_o,km_othercmds,0},  {"M",km_mainmenu,0},
      {"V","[ViewMsg]",0},    {"P",km_prevmsg,0},     {"N",km_nextmsg,0},
      {km_dash,km_prevpage,0},     {km_spc,km_nextpage,0},  {"D",km_delete,0},
      {"U",km_undelete,0},     {"R","Reply",0},       {"F",km_forward,0}, 

      {km_qmark,km_help,0},         {km_o,km_othercmds,0},  {"Q",km_quit,0},
      {"C",km_compose,0},      {"L",km_listfldrs,0},   {"G",km_gotofldr,0},
      {"Z","Zoom",0},         {km_w,km_whereis,0},     {NULL,NULL,0},
      {NULL,NULL,0},          {"S",km_save,0},        {km_e,"Export",0},

      {km_qmark,km_help,0},         {km_o,km_othercmds,0},  {"X",NULL,0},
      {"&","unXclude",0},     {"A","Apply",0},       {"$","SortIndex",0},
      {"J","Jump",0},         {"Tab",km_nextnew,0},   {"H",km_hdrmode,0},
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

static OtherMenu what_keymenu = FirstMenu;

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
int   i_cache_width(void);
void  sort_blip(void);


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
	{
          km = &index_keymenu;
          clrbitn(VIEW_PIPE_KEY, bitmap);
          clrbitn(VIEW_FULL_HEADERS_KEY, bitmap);
          clrbitn(BOUNCE_KEY, bitmap);
          clrbitn(FLAG_KEY, bitmap);
          clrbitn(ZOOM_KEY, bitmap);
          clrbitn(APPLY_KEY, bitmap);
          clrbitn(UNEXCLUDE_KEY, bitmap);
	  index_keys[EXCLUDE_KEY].label = "eXpunge";
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


    
    ch                          = 'x'; /* For displaying msg 1st time through */
    force                       = 0;
    state->mangled_screen       = 1;
    old_max_msgno               = 0L;
    old_top_ent                 = 0L;
    old_current_sorted_msgno    = 0L;
    index_body_state.paint_from = 1;
    what_keymenu                = FirstMenu;
	

    if(state->mail_stream == NULL) {
	q_status_message(0, 1, 3, "No folder open");
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
                      panic("Bad index position");
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
	ch = read_command();

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
	        q_status_message(0, 0, 1, already_first_page);
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
	        q_status_message(0, 0, 1, already_last_page);
	    break;


            /*---------- Suspend Pine ----------*/
          case ctrl('Z'):
            if(!have_job_control())
              goto df;
            do_suspend(state);
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
    char          status[20], from[MAX_SCREEN_COLS/2+1], size[32],
                  subject[MAX_SCREEN_COLS+1], to_us;
    char         *s, *s_tmp, *buffer;
    struct date   d;



    if(*(buffer = get_index_cache(message_number)) != '\0') {
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
		"[No Text]");
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
    from_length = 18 + max(0, ps_global->ttyo->screen_cols - 80 + 1)/2;
    if(envelope->from==NULL || address_is_us(envelope->from, ps_global)) {
        ADDRESS *to_addr;
        char    *a_string;
        to_addr = envelope->to != NULL ? envelope->to :
                 envelope->cc != NULL ? envelope->cc : NULL;
        if(to_addr == NULL) {
            sprintf(from, "%*.*s", from_length, from_length,
                  "                                        ");
        } else {
            a_string =  addr_list_string(to_addr, 0);
            sprintf(from, "To: %-*.*s", from_length - 4, from_length - 4,
                a_string);
            fs_give((void **)&a_string);
        }
    } else {
        mail_fetchfrom(from, ps_global->mail_stream,
                       ps_global->sort[message_number], (long)from_length);
    }
    from_length = strlen(from);

    /*--- size ---*/
    if(ps_global->mail_stream->mailbox[0] != '*'){
        if(cache->rfc822_size < 100000) {
            s_tmp = comatose(cache->rfc822_size);
            sprintf(size, " %-*.*s(%s) ", 6 - (int) strlen(s_tmp),
                6 - (int) strlen(s_tmp), "       ", s_tmp);
        } else if(cache->rfc822_size < 10000000) {
            s_tmp = comatose(cache->rfc822_size/1000);
            sprintf(size, " %-*.*s(%sK) ", 5 - (int) strlen(s_tmp),
                5 - (int) strlen(s_tmp), "       ", s_tmp);
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
    mail_fetchsubject(subject, ps_global->mail_stream,
                      ps_global->sort[message_number], (long)subject_length);
    for(s = subject + strlen(subject); s < subject + subject_length; s++)
      *s = ' ';
    *s = '\0';

    /*--- Put them all together ---*/
    sprintf(buffer, "%s%s%s%s", status, from, size, subject);
    /* Truncate it to proper size */
    buffer[min(ps_global->ttyo->screen_cols, i_cache_width())] = '\0';
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


    convert_to_gmt(&d_a);


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
    size_a = mc != NULL ? (long)mc->rfc822_size : -1;

    mail_fetchstructure(ps_global->mail_stream, *mess_b, NULL);
    mc = mail_elt(ps_global->mail_stream, *mess_b);
    size_b = mc != NULL ? (long)mc->rfc822_size : -1;

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


    if(so == SortArrival){
	for(mess_no = 1; mess_no <= ps_global->max_msgno; mess_no++)
            sort[mess_no] = ps_global->current_reverse_sort ? 
			       1+ ps_global->max_msgno - mess_no: mess_no;

    }else{
	for(mess_no = 1; mess_no <= ps_global->max_msgno; mess_no++)
            sort[mess_no] = mess_no;
        if(ps_global->max_msgno > 500)
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
   long      num;			/* number of last index in cache */
   unsigned  size;			/* size of each index line       */
} icache = { NULL, 0L, 0 };
  
/*
 * cache size growth increment. Was 100 -- an unconditional first
 * allocation of (100+1)*(screen_cols+1) bytes (8181 at 80 columns) as
 * soon as the FIRST index line is drawn, regardless of how many
 * messages the folder actually has (2, for a brand new PDP-11 user's
 * INBOX). fs_resize() below already grows this on demand in IC_SIZE
 * steps, so a smaller increment only changes resize frequency for a
 * large folder, not correctness -- same tradeoff as MSIZE_INIT
 * (filter.c) and other "start small, grow on demand" fixes in this
 * tree. See TODO-pdp11-done.md.
 */
#define	IC_SIZE		20

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

    if(icache.size != (unsigned)(max(ps_global->ttyo->screen_cols, 80) + 1)){
        clear_index_cache();		/* clear cache and start over! */
	icache.size = max(ps_global->ttyo->screen_cols, 80) + 1;
    }

    if(indx > (j = icache.num - 1L)){		/* make room for entry! */
	int     tmplen = (size_t)icache.size * sizeof(char);
	char   *tmpline;

	while(indx >= icache.num)
	  icache.num += IC_SIZE;

	if(icache.cache == NULL){
	    icache.cache = (void *)fs_get((icache.num+1)*tmplen);
	    memset(icache.cache, 0, (unsigned)((icache.num+1)*tmplen));
	}
	else{
            fs_resize((void **)&(icache.cache), (size_t)(icache.num+1)*tmplen);
	    tmpline = (char *)icache.cache + ((j+1) * tmplen);
	    memset(tmpline, 0, (unsigned)((icache.num - j) * tmplen));
	}
    }

    return(1);
}


/*
 * return the index line associated with the given message number
 */
char *
get_index_cache(msgno)
    long         msgno;
{
    if(!i_cache_size(--msgno)){
	q_status_message(0, 1, 3, "\007Index cache error");
	return(NULL);
    }

    return((char *)(icache.cache) + (msgno * (long)icache.size * sizeof(char)));
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
    if(icache.cache)
      fs_give((void **)&(icache.cache));
    icache.num  = 0L;
    icache.size = 0;
    bc_this_stream = NULL;
}
