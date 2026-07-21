#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: screen.c,v 4.28 1993/10/02 07:29:22 hubert Exp $";
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
       screen.c
       Some functions for screen painting
          - Painting and formatting of the key menu at bottom of screen
          - Convert message status bits to string for display
          - Painting and formatting of the titlebar line on top of the screen
          - Updating of the titlebar line for changes in message number...
  ====*/


#include "headers.h"

#ifdef	ANSI
char *format_titlebar(void);
void  savebits(BITMAP, BITMAP, int);
int   equalbits(BITMAP, BITMAP, int);
#else
char *format_titlebar();
void  savebits();
int   equalbits();
#endif


/*------ 
   Saved key menu drawing state 
  ------*/
static struct {
    struct key_menu *km;
    int              row, column;
    int              blanked;
    BITMAP           bitmap;
} km_state;


/*----------------------------------------------------------------------
      Splat an already formatted key_menu to the screen

 Args:  km      - key_menu structure
        bm      - bitmap, 0's mean don't draw this key
        row     - the row on the screen to begin on, negative values
                  are counted from the bottom of the screen up
        column  - column on the screen to begin on

 Just write out the name and the label at the specified column.

 ----*/
void
output_keymenu(km, bm, row, column)
    struct key_menu *km;
    BITMAP bm;
    int row, column;
{
    register struct key *k;
    int                  i, c, ufk, real_row, fkey, max_column;
    char                 temp[20];
    int			 off;
#if	defined(DOS) && defined(MOUSE)
    extern void          register_key();
    int    j = -1;
#endif

    off    	  = km->which * 12;
    fkey          = 0;         /* Avoid ref before set warning */
    max_column    = ps_global->ttyo->screen_cols;

    if(ps_global->ttyo->screen_rows < 4 || max_column <= 0)
      return;

    for(i = 0; i < 12; i++){
	int e;
	e = off + i;
        dprint(9, (debugfile, "%d %-7.7s %-10.10s %d\n", i,
		   km == NULL ? "(no km)" 
		   : km->keys[e].name == NULL ? "(null)" 
		   : km->keys[e].name, 		   
		   km == NULL ? "(no km)" 
		   : km->keys[e].label == NULL ? "(null)" 
		   : km->keys[e].label, km ? km->keys[e].column : 0));
#if	defined(DOS) && defined(MOUSE)
	register_key(i, NO_OP_COMMAND, "", 0, 0, 0);
#endif
    }
    ufk = F_ON(F_USE_FK,ps_global);
    real_row = row > 0 ? row : ps_global->ttyo->screen_rows + row;
    dprint(9, (debugfile, "row: %d, real_row: %d, column: %d\n", row, 
               real_row, column)) ;
    for(i = 0; i < 2; i++) {
	int el;
        MoveCursor(real_row + i, column);
        CleartoEOLN();
        for(c = 0, el = off+i, k = &km->keys[el]; k < &km->keys[off+12];
							k += 2, el += 2) {
#if	defined(DOS) && defined(MOUSE)
	    j++;
#endif
            if(ufk)
              fkey =  1 + k - &km->keys[off];
            if(!bitnset(el,bm) || k->name == NULL || k->column > max_column)
              continue;
     
            if(k->column < c) {
                /*--- over writing previous entry, a bit weird */
                MoveCursor(row + i, k->column + column);
            } else { 
                Write_to_screen(repeat_char(k->column - c, ' '));
            }

            StartInverse();
            if(ufk) {
                sprintf(temp, "F%d", fkey);
            } else {
                strcpy(temp, k->name);
            }
            if(strlen(temp) + k->column > max_column)
              temp[max_column - k->column] = '\0';
            Write_to_screen(temp);
            EndInverse();
            c = k->column + strlen(temp);

            if(++c < max_column)
              Writechar(' ');

            strcpy(temp, k->label);
            if(c + strlen(temp) >= max_column)
              temp[max(0,max_column - c)] = '\0';
            Write_to_screen(temp);
#if	defined(DOS) && defined(MOUSE)
	    register_key(j, (ufk) ? PF1 + fkey - 1 : 
				(k->name[0] == '^') ? ctrl(k->name[1]) : 
				(!strucmp(k->name, "return")) ? ctrl('M') : 
				(!strucmp(k->name, "tab")) ? '\t' : 
				(!strucmp(k->name, "spc")) ? ' ' :
				 k->name[0],
			 k->label, real_row+i, c, strlen(temp));
#endif
            
            c = strlen(k->label) + 1 + k->column +
                (ufk ? (fkey > 9 ? 3 : 2) :strlen(k->name));
        }
    }
    fflush(stdout);
}


/*----------------------------------------------------------------------
   Blank out the key menu and clear that part of the screen 
  ----*/
void
blank_keymenu(row, column)
     int row, column;
{
    km_state.blanked    = 1;
    km_state.row        = row;
    km_state.column     = column;
    MoveCursor(row, column);
    CleartoEOLN();
    MoveCursor(row+1, column);
    CleartoEOLN();
    fflush(stdout);
}
        


/*----------------------------------------------------------------------
    Calculate formatting for key menu at bottom of screen

   Args:  km -- The key_menu structure to format
          bm -- Bitmap indicating which menu items should be displayed.  If
		an item is NULL, that also means it shouldn't be displayed.
		Sometimes the bitmap will be turned on in that case and just
		rely on the NULL entry.
          width -- the screen width to format it at
	  what  -- Used to indicate which of the possible sets of 12 keys
		   to show next
	  which -- Only used when what == AParticularTwelve, in which case
		   it indicates which particular twelve.

 If already formatted for this particular screen width, this set of twelve
 keys, and this part of the bitmap set this way, then return.

 The formatting results in the column field in the key_menu being filled in.
 The column field is the column to start the label at, the name of the key;
 after that is the label for or command that key will do. This would be simple
 except for a few long key names such as SPACE. All the calculations are to
 make this look nice. The basic idea is to line up the end of the names and 
 beginning of the labels. If the name is too long and shifting it left
 would run into previous label, then shift the whole menu right, or at least
 that entry of things following are short enough to fit back into the
 regular spacing. There's a special case to make 80 column screens look nice.
 This has to be calculated and not fixed so it can cope with screen resize.
   ----*/
void
format_keymenu(km, bm, width, what, which)
    struct key_menu *km;
    BITMAP bm;
    int width;
    OtherMenu what;
    int which;  /* only used if what == AParticularTwelve */
{
    short   spacing[6];
    int     i, first, prev_end_top, prev_end_bot, ufk, top_column, bot_column;
    int     off;  /* offset into keymenu */
    struct key *keytop, *keybot, *prev_keytop, *prev_keybot;

    if(what == FirstMenu) {
      km->which = 0;
    }else if(what == NextTwelve) {
      km->which = (km->which + 1) % (unsigned int)km->how_many;
    }else if(what == AParticularTwelve) {
      km->which = which;
    } /* else SameTwelve */

    /* it's already formatted. */
    if(width == km->width[km->which] && equalbits(bm,km->bitmap,km->which))
      return;

    /*
     * If we're in the initial command sequence we may be using function
     * keys instead of alphas, or vice versa, so we want to recalculate
     * the formatting next time through.
     */
    if ((F_ON(F_USE_FK,ps_global) && ps_global->orig_use_fkeys) ||
        (F_OFF(F_USE_FK,ps_global) && !ps_global->orig_use_fkeys)) {
      km->width[km->which] = width;
      savebits(km->bitmap, bm, km->which);
    }

    off = km->which * 12;

    ufk = F_ON(F_USE_FK,ps_global);

    /* this spacing is column numbers for column key name ends in */
    /*       F1 Menu	   */
    /*        ^            */
    spacing[0] = 0;
    for(i = 1; i < 6; i++) 
      spacing[i] = spacing[i-1] + (width == 80 ? (i > 2 ? 14 : 13) : width/6);


    km->keys[off].column = km->keys[off+6].column = 0;
    if(ufk)
      first = 2;
    else
      first = max((bitnset(off,bm) && km->keys[off].name != NULL) ?
	        strlen(km->keys[off].name) : 0,
	        (bitnset(off+1,bm) && km->keys[off+1].name != NULL) ?
	        strlen(km->keys[off+1].name) : 0);

    prev_keytop = &km->keys[off];
    prev_keybot = &km->keys[off+1];
    for(i = 1 ; i < 6; i++) {
	int ktop, kbot;

	ktop = off + i*2;
	kbot = ktop + 1;
        keytop = &km->keys[ktop];
        keybot = &km->keys[kbot];

        if(!bitnset(ktop,bm) || keytop->name == NULL) {
            top_column = first + spacing[i];
        } else {
            prev_end_top = 
                 prev_keytop->column +
                 ((!bitnset(ktop-2,bm) || prev_keytop->name == NULL) ? 0 : 
                   (ufk ? (i-1 >= 5 ? 3 : 2) : strlen(prev_keytop->name))) +
                 1 + ((!bitnset(ktop-2,bm) || prev_keytop->label==NULL) ? 0 :
		 strlen(prev_keytop->label)) +
                 1 + ((!bitnset(ktop,bm) || keytop->name == NULL) ? 0 : 
                   (ufk ? (i >= 5 ? 3 : 2) : strlen(keytop->name)));
            top_column = max(prev_end_top, first + spacing[i]);
            dprint(9, (debugfile,
                       "prev_col: %d, prev_end:%d, top_column:%d spacing:%d\n",
                       prev_keytop->column, prev_end_top, top_column,
                       spacing[i]));
        }

        if(!bitnset(kbot,bm) || keybot->name == NULL) {
            bot_column = first + spacing[i];
        } else {
            prev_end_bot = 
                 prev_keybot->column +
                 ((!bitnset(kbot-2,bm) || prev_keybot->name  == NULL) ? 0 :
                  (ufk ? (i-1 >= 4 ? 3 : 2) :strlen(prev_keybot->name))) +
                 1 + ((!bitnset(kbot-2,bm) || prev_keybot->label==NULL) ? 0 :
		 strlen(prev_keybot->label)) +
                 1 + ((!bitnset(kbot,bm) || keybot->name  == NULL) ? 0 :
                  (ufk ? (i >= 4 ? 3 : 2) :strlen(keybot->name)));
            bot_column = max(prev_end_bot, first + spacing[i]);
        }
        keytop->column = max(bot_column, top_column) -
                          ((!bitnset(ktop,bm) || keytop->name == NULL) ? 0 : 
                            (ufk ? (i >= 5 ? 3 : 2) : strlen(keytop->name)));
        keybot->column = max(bot_column, top_column) -
                          ((!bitnset(kbot,bm) || keybot->name  == NULL) ? 0 :
                            (ufk ? (i >= 4 ? 3 : 2) : strlen(keybot->name)));

        prev_keytop = keytop;
        prev_keybot = keybot;
    }
}



/*----------------------------------------------------------------------
    Draw the key menu at bottom of screen

   Args:  km -- key_menu structure
          bitmap -- which fields are active
          width -- the screen width to format it at
	  row,column -- where to put it
          what -- this is an enum telling us whether to display the
		  first menu (first set of 12 keys) or to display the same
		  one we displayed last time, or to display a particular
		  one (which), or to display the next one.
	  which -- the keys to display if (what == AParticularTwelve)
 
   Fields are inactive if either the corresponding bitmap entry is 0 or
   the actual entry in the key_menu is NULL.  Therefore, it is sometimes
   useful to just turn on all the bits in a bitmap and let the NULLs take
   care of it.  On the other hand, the bitmap gives a convenient method
   for turning some keys on or off dynamically or due to options.

   Also saves the state for a possible redraw later.
   ----*/
void
draw_keymenu(km, bitmap, width, row, column, what, which)
     struct key_menu *km;
     int width;
     BITMAP bitmap;
     int row, column;
     OtherMenu what;
     int which;
{
    format_keymenu(km, bitmap, width, what, which);
    output_keymenu(km, bitmap, row, column);

    /*--- save state for a possible redraw ---*/
    km_state.km         = km;
    km_state.row        = row;
    km_state.column     = column;
    memcpy(km_state.bitmap, bitmap, BM_SIZE);
    km_state.blanked    = 0;
}



/*----------------------------------------------------------------------
    
 */
void
redraw_keymenu()
{
    if(km_state.blanked)
        blank_keymenu(km_state.row, km_state.column);
    else
        draw_keymenu(km_state.km, km_state.bitmap, ps_global->ttyo->screen_cols,
		km_state.row, km_state.column, SameTwelve, 0);
}
    


/*
 * some useful macros...
 */
#define	MS_DEL			(0x01)
#define	MS_NEW			(0x02)
#define	MS_ANS			(0x04)

#define	BAR_STATUS(X)	(((X) & MS_DEL) ? "DEL"   \
			 : ((X) & MS_ANS) ? "ANS"   \
		           : ((X) & MS_NEW) ? "NEW" : "   ")



static struct titlebar_state {
    char *title,
         *folder_name,
         *context_name;
    int   msg_state,
          total_pages,
          current_page,
          total_lines,
          current_line,
          total_messages,
          current_message,
          cur_mess_col,
          del_column, 
          percent_column,
          page_column,
          screen_cols;
    enum  {Normal, ReadOnly, Closed} stream_status;
    TitleBarType style;
} as, titlebar_stack;

    

/*----------------------------------------------------------------------
       Create little string for displaying message status

  Args: message_cache  -- pointer to MESSAGECACHE 

    Create a string with letters that indicate the status of the message.
  This is a function despite it's current simplicity so we can easily 
  add a few more flags
  ----------------------------------------------------------------------*/
char *
status_string(message_cache)
     MESSAGECACHE *message_cache;
{
     static char string[3];

     string[1] = '\0';

     string[0] = 'N';

     if(message_cache == NULL || message_cache->seen)
       string[0] = ' ';

     if(message_cache->answered)
       string[0] = 'A';

     if(message_cache->deleted)
       string[0] = 'D';

     return(string);
}



/*--------
------*/
void
push_titlebar_state()
{
    titlebar_stack  = as;
    as.folder_name  = NULL;	/* erase knowledge of malloc'd data */
    as.context_name = NULL;
}



/*--------
------*/
void
pop_titlebar_state()
{
    fs_give((void **)&(as.folder_name)); /* free malloc'd values */
    fs_give((void **)&(as.context_name));
    as = titlebar_stack;
}



/*----------------------------------------------------------------------
      Sets up style and contents of current titlebar line

    Args: title -- The title that appears in the center of the line
          display_on_screen -- flag whether to display on screen or generate
                                string
          style  -- The format/style of the titlebar line
          current_message -- The current message number
          current_pl -- The current page or line number
          total_pl   -- The total line or page count

  Set the contents of the acnhor line. It's called an acnhor line
because it's always present and titlebars the user. This accesses a
number of global variables, but doesn't change any. There are 4
different styles of the titlebar line. First three parts are put
together and then the three parts are put together adjusting the
spacing to make it look nice. Finally column numbers and lengths of
various fields are saved to make updating parts of it more efficient.

It's OK to call this withy a bogus current message - it is only used
to look up status of current message 
 
Formats only change the right section (part3).
  FolderName:    "<folder>"  xx Messages
  MessageNumber: "<folder>" message x,xxx of x,xxx XXX
  TextPercent:   "<folder>" message x,xxx of x,xxx  xx% XXX
  PageNumber:    "page xx of xx

Several strings and column numbers are saved so later updates to the status 
line for changes in message number or percentage can be done efficiently.
This code is some what complex, and might benefit from some improvements.
 ----*/

char *
set_titlebar(title, display_on_screen, style, current_message, current_pl,
                                                                  total_pl)
     char        *title;
     TitleBarType style;
     int          display_on_screen, current_pl, total_pl;
     long         current_message;
{
    char          *tb;
    MESSAGECACHE *mc;
    extern char   version_buff[];


    dprint(9, (debugfile,
"set_titlebar - style: %d  current_message:%ld  current_pl: %d  total_pl: %d\n",
               style, current_message, current_pl, total_pl));

    as.style = style;
    as.title = title;
    as.stream_status = (ps_global->mail_stream == NULL ||
                                  ps_global->dead_stream) ? Closed :
                       ps_global->mail_stream->readonly ? ReadOnly : Normal;

    if(as.folder_name)
      fs_give((void **)&as.folder_name);

    as.folder_name = cpystr(pretty_fn(ps_global->cur_folder));

    if(as.context_name)
      fs_give((void **)&as.context_name);

    as.context_name = cpystr("");

    if(current_message < 0)
      current_message = 0;		/* BUG Don't like this */

    if(style != FolderName && current_message > 0) {
        (void)mail_fetchstructure(ps_global->mail_stream,
				 (long)ps_global->sort[current_message],
				 NULL);
        mc  = mail_elt(ps_global->mail_stream,
                       (long)ps_global->sort[current_message]);
    } else {
	mc  = (MESSAGECACHE *)NULL;
    }

    
    
    switch(style) {
      case FolderName:
        as.total_messages = ps_global->max_msgno;
	break;

      case MessageNumber:
        as.total_lines = total_pl;
        as.current_line = current_pl;
        as.current_message = current_message;
        as.total_messages = ps_global->max_msgno;
        as.msg_state = !mc ? 0
			   : mc->deleted ? MS_DEL
			     : mc->answered ? MS_ANS
			       : !mc->seen ? MS_NEW : 0;
	break;

      case TextPercent:
        as.total_lines = total_pl;
        as.current_line = current_pl;
        as.current_message = current_message;
        as.total_messages = ps_global->max_msgno;
        as.msg_state = !mc ? 0
			   : mc->deleted ? MS_DEL
			     : mc->answered ? MS_ANS
			       : !mc->seen ? MS_NEW : 0;
	break; 

      case PageNumber:
        as.total_pages  = total_pl;
        as.current_page = current_pl;
	break;
    }
    tb = format_titlebar();
    if(display_on_screen) {
        StartInverse();
        PutLine0(0, 0, tb);
        EndInverse();
        fflush(stdout);
    }
    return(tb);
}

void 
redraw_titlebar()
{
    StartInverse();
    PutLine0(0, 0, format_titlebar());
    EndInverse();
    fflush(stdout);
}
     


#define NUM_LEN(x) ((x) < 10 ? 1 : (x) < 100 ? 2 : (x) < 1000 ? 3 : 4)
#define CALC_PERCENT(part, tot) ((tot) == 0 ? 0 : \
				  (int)((100L * (long)(part))/(long)(tot)))


/*----------------------------------------------------------------------
      Redraw or draw the top line, the title bar 

 The titlebar has Four fields:
     1) "Version" of fixed length and is always positioned two spaces 
        in from left display edge.
     2) "Location" which is fixed for each style of titlebar and
        is positioned two spaces from the right display edge
     3) "Title" which is of fixed length, and is centered if
        there's space
     4) "Folder" whose existance depends on style and which can
        have it's length adjusted (within limits) so it will
        equally share the space between 1) and 2) with the 
        "Title".  The rule for existance is that in the
        space between 1) and 2) there must be two spaces between
        3) and 4) AND at least 50% of 4) must be displayed.


 The rules for dislay are:
     a) Show at least some portion of 3)
     b) If no room for 1) and 3), 3)
     c) If no room for 1), 2) and 3), show 1) and 2)
     d) If no room for all and > 50% of 4), show 1), 2), and 3)
     e) show 1), 2) 3), and some portion of 4)

   Returns - Formatted title bar 
 ----*/
char *
format_titlebar()
{
    static  char titlebar_line[MAX_SCREEN_COLS+1];
    char    version[50], comatmp[20], fold_tmp[MAXPATH],
           *loc_label, *ss_string;
    int     sc, tit_len, ver_len, loc_len, fold_len, num_len, ss_len, 
            is_context;
    extern  char version_buff[];

    /* blank the line */
    memset((void *)titlebar_line, ' ', MAX_SCREEN_COLS*sizeof(char));
    sc = min(ps_global->ttyo->screen_cols, MAX_SCREEN_COLS);
    titlebar_line[sc] = '\0';

    /* initialize */
    as.del_column     = -1;
    as.cur_mess_col   = -1;
    as.percent_column = -1;
    as.page_column    = -1;
    is_context        = strlen(as.context_name);
    sprintf(version, "PINE %s", version_buff); 
    ss_string         = as.stream_status == Closed ? "(CLOSED)" :
                        (as.stream_status == ReadOnly)
                           ? "(READONLY)" : "";
    ss_len            = strlen(ss_string);

    tit_len = strlen(as.title);		/* fixed title field width   */
    ver_len = strlen(version);		/* fixed version field width */

    /* if only room for title we can get out early... */
    if(tit_len >= sc || (tit_len + ver_len + 6) > sc){
	int i = max(0, sc - tit_len)/2;
	strncpy(titlebar_line + i, as.title, min(sc, tit_len));
	return(titlebar_line);
    }

    /* 
     * set location field's length and value based on requested style 
     */
    loc_label = (is_context) ? "Msg" : "Message";
    loc_len   = strlen(loc_label);
    switch(as.style){
      case PageNumber :			/* "page xxx of xxx" */
	loc_len = 15;
	as.page_column = sc - 12;
	sprintf(tmp_20k_buf, "Page %d of %d  ",
		as.current_page, as.total_pages);
	break;
      case FolderName :			/* "x,xxx <loc_label>s" */
	loc_len += NUM_LEN(as.total_messages) + 3;
	sprintf(tmp_20k_buf, "%s %s%s", comatose((long)as.total_messages),
		loc_label, plural(as.total_messages));
	break;
      case MessageNumber :	       	/* "<loc_label> xxx of xxx DEL"  */
	num_len  = NUM_LEN(as.current_message) + NUM_LEN(as.total_messages);
	if(as.current_message >= 1000)
	  num_len++;
	if(as.total_messages >= 1000)
	  num_len++;
	loc_len += num_len + 9;		/* add spaces and "DEL" */
	as.cur_mess_col = sc - 10 - num_len;
	as.del_column   = sc - 5;
	strcpy(comatmp, comatose((long)as.current_message));
	sprintf(tmp_20k_buf, "%s %s of %s %s", loc_label, 
		comatmp, comatose((long)as.total_messages),
		BAR_STATUS(as.msg_state));
	break;
      case TextPercent :		/* "<loc_label> xxx of xxx xxx% DEL" */
	num_len  = NUM_LEN(as.current_message) + NUM_LEN(as.total_messages);
	if(as.current_message >= 1000)
	  num_len++;
	if(as.total_messages >= 1000)
	  num_len++;
	loc_len += num_len + 14;	/* add spaces, percent, and "DEL" */
	as.del_column     = sc - 5;
	as.percent_column = sc - 10;
	as.cur_mess_col   = sc - 16 - num_len;
	strcpy(comatmp, comatose((long)as.current_message));
	sprintf(tmp_20k_buf, "%s %s of %s %3d%% %s", loc_label, 
		comatmp, comatose((long)as.total_messages),
		CALC_PERCENT(as.current_line, as.total_lines),
		BAR_STATUS(as.msg_state));
	break;
    }

    /* at least the version will fit */
    strncpy(titlebar_line + 2, version, ver_len);

    /* if no room for location string, bail early? */
    if(ver_len + tit_len + loc_len + 10 > sc){
	strncpy((titlebar_line + sc) - (tit_len + 2), as.title, tit_len);
        as.del_column = as.cur_mess_col = as.percent_column
	  = as.page_column = -1;
	return(titlebar_line);		/* put title and leave */
    }

    /* figure folder_length and what's to be displayed */
    fold_tmp[0] = '\0';
    if(as.style != PageNumber){
	int ct_len,
	    avail  = sc - (ver_len + tit_len + loc_len + 10);
	fold_len   = strlen(as.folder_name);
	
	/*
	 * fmt below is always one of the compile-time literals assigned
	 * to it a few lines down, never external input.
	 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	if(is_context && (ct_len = strlen(as.context_name))){
	    char *fmt;
	    int  extra;

	    fmt = "<%*.*s> %s%s"; extra = 3;

	    /*
	     * below are other formats we'd considered
	     *
	     * fmt = "%s - %s%s"; extra = 3;
	     * fmt = "%s[%s%s]"; extra = 2;
	     * fmt = "<%s>%s%s"; extra = 2;
	     * fmt = "%s: %s%s"; extra = 2;
	     */
	    if(ct_len + fold_len + ss_len + extra < avail)
	      sprintf(fold_tmp, fmt, ct_len, ct_len, as.context_name,
		      as.folder_name, ss_string);
	    else if((ct_len/2) + fold_len + ss_len + extra < avail)
	      sprintf(fold_tmp, fmt,
		      ct_len - (ct_len-(avail-(fold_len+ss_len+extra))),
		      ct_len - (ct_len-(avail-(fold_len+ss_len+extra))),
		      as.context_name,
		      as.folder_name, ss_string);
	    else if((ct_len/2) + (fold_len/2) + ss_len + extra < avail)
	      sprintf(fold_tmp, fmt, (ct_len/2), (ct_len/2), as.context_name,
		   as.folder_name+(fold_len-(avail-((ct_len/2)+ss_len+extra))),
		      ss_string);
	}
	else{
	    char *fmt = "Folder: %s%s";
	    if(fold_len + ss_len + 8 < avail) 	/* all of folder fit? */
	      sprintf(fold_tmp, fmt, as.folder_name, ss_string);
	    else if((fold_len/2) + ss_len + 8 < avail)
	      sprintf(fold_tmp, fmt,
		      as.folder_name + fold_len - (avail - (8 + ss_len)),
		      ss_string);
	}
#pragma GCC diagnostic pop
    }
    
    /* write title, location and, optionally, the folder name */
    fold_len = strlen(fold_tmp);
    strncpy(titlebar_line + ver_len + 5, as.title, tit_len);
    strncpy((titlebar_line + sc) - (loc_len + 2), tmp_20k_buf, 
	    strlen(tmp_20k_buf));
    if(fold_len)
      strncpy((titlebar_line + sc) - (loc_len + fold_len + 4), fold_tmp,
	      fold_len);

    return(titlebar_line);
}


/*----------------------------------------------------------------------
    Update the titlebar line if the message number changed

   Args: new_message_number -- message number to display

This is a bit messy because the length of the number displayed might 
change which repositions everything after it, so we adjust all the saved 
columns and shorten tail, the string holding the rest of the line.
  ----*/

void
update_titlebar_message(new_message_number)
     int new_message_number;
{
    char  s_new[20], s_old[20];
    int   l_change;         

    if(as.cur_mess_col < 0) {
      return;
    }

    strcpy(s_new, comatose((long)new_message_number));
    strcpy(s_old, comatose((long)as.current_message));
    l_change = strlen(s_new) - strlen(s_old);
    

    StartInverse();
    if(l_change == 0) {
        PutLine0(0, as.cur_mess_col, s_new);
    } else {
        if(as.style == TextPercent) {
            PutLine5(0, as.cur_mess_col, "%s of %s %3d%% %s%s",
                     s_new, comatose((long)as.total_messages),
                     (void *)(long)CALC_PERCENT(as.current_line, as.total_lines),
                     BAR_STATUS(as.msg_state),
                     repeat_char(max(0, -l_change),' '));
            if(as.del_column >= 0) {
                as.del_column += l_change;
            }
            if(as.percent_column >= 0) {
                as.percent_column += l_change;
            }
        } else {
            PutLine4(0, as.cur_mess_col, "%s of %s %s%s",
                     s_new, comatose((long)as.total_messages),
                     BAR_STATUS(as.msg_state),
                     repeat_char(max(0, -l_change),' '));
            if(as.del_column >= 0) {
                as.del_column += l_change;
            }
        }
    }
    EndInverse();
    fflush(stdout);
    as.current_message = new_message_number;
}



/*---------------------------------------------------------------------- 
    Update the percentage shown in the titlebar line

  Args: new_line_number -- line number to calculate new percentage
   
  ----*/

void
update_titlebar_percent(new_line_number)
     int new_line_number;
{
    int new_percent, old_percent;
    
    if(as.percent_column < 0) {
        return;
    }

    new_percent     = CALC_PERCENT(new_line_number, as.total_lines);
    old_percent     = CALC_PERCENT(as.current_line, as.total_lines);
    as.current_line = new_line_number;


   if(old_percent == new_percent)
     return;

    StartInverse();
    PutLine1(0, as.percent_column, "%3d", (void *)(long)new_percent);
    EndInverse();
    fflush(stdout);
}



/*----------------------------------------------------------------------
    Update the page number. Changes here change only the tail length

  Args:  new_page -- new page number to display

  ---*/
void
update_titlebar_page(new_page)
     int new_page;
{
    if(as.style != PageNumber) 
      panic("Update_titlebar_page with wrong style");

    if(new_page == as.current_page)
      return;

    as.current_page = new_page;

    StartInverse();
    if(NUM_LEN(new_page) != NUM_LEN(as.current_page))  {
        PutLine1(0, as.page_column, "%s", int2string(new_page));
    } else {
      /* Blanks at end for when it gets shorter */
        char tmp[30];
        strcpy(tmp, int2string(new_page));
        PutLine2(0,as.page_column,
                 "%s of %s   ",tmp,int2string(as.total_pages));
    }
    EndInverse();
    fflush(stdout);
}


update_titlebar_status(mc)
    MESSAGECACHE *mc;
{
    if(!mc || as.del_column < 0)
      return(0);

    if(mc->deleted){			/* deleted takes precedence */
	if(as.msg_state & MS_DEL)
	  return(0);
    }
    else if(mc->answered){		/* then answered */
	if(as.msg_state & MS_ANS)
	  return(0);
    }
    else if(!mc->seen){			/* then seen */
	if(as.msg_state & MS_NEW)
	  return(0);
    }
    else if(as.msg_state == 0)		/* nothing to change... */
      return(0);

    as.msg_state = (mc->deleted) ? MS_DEL 
				 : mc->answered ? MS_ANS
				   : (!mc->seen) ? MS_NEW : 0;

    StartInverse();
    PutLine0(0, as.del_column, BAR_STATUS(as.msg_state));
    EndInverse();
    fflush(stdout);
    return(1);
}


/*
 * Save the bits from the which'th set of twelve bits in from into to.
 * An assumption is that BITMAPs are 48 bits longs.  (There must be a
 * clever macro way to do this.)
 */
void
savebits(to, from, which)
    BITMAP to, from;
    int which;
{
    char c1, c2;
    int x;

    switch(which){
      case 0:
      case 2:
	x       =  which == 0 ? 0 : 3;
	to[x]   =  from[x];
	c1      =  from[x+1]  &  0xf0;
	c2      =  to[x+1]    &  0xf;
	to[x+1] =  c1         |  c2;
	break;

      case 1:
      case 3:
	x       =  which == 1 ? 1 : 4;
	c1      =  to[x]    &  0xf0;
	c2      =  from[x]  &  0xf;
	to[x]   =  c1       |  c2;
	to[x+1] =  from[x+1];
	break;

      default:
	panic("Can't happen in savebits()\n");
	break;
    }
}

/*
 * Returns 1 if the which'th set of twelve bits in bm1 is the same as the
 * which'th set of twelve bits in bm2, else 0.
 */
int
equalbits(bm1, bm2, which)
    BITMAP bm1, bm2;
    int which;
{
    int x;

    switch(which){
      case 0:
      case 2:
	x = which == 0 ? 0 : 3;
	return ((bm1[x] == bm2[x]) && ((bm1[x+1] & 0xf0) == (bm2[x+1] & 0xf0)));

      case 1:
      case 3:
	x = which == 1 ? 1 : 4;
	return (((bm1[x] & 0xf) == (bm2[x] & 0xf)) && (bm1[x+1] == bm2[x+1]));

      default:
	panic("Can't happen in equalbits()\n");
    }
}
