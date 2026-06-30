#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: addrbook.c,v 4.33 1993/11/23 00:03:20 mikes Exp $";
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
    addrbook.c
    display, browse and edit the address book.

    The actual file access to the address book file is provided by
    the library adrbklib.

 The policy for changing the address book is to write it immediately 
after the change is made so there is no idea of having to save the 
address book. It was determined that this will be fast even for 
large address books and is implemented in the adrbklib. Each call to
these functions can then expect write errors. 
        
 ====*/


#include "headers.h"
#include "adrbklib.h"


/*----------------------------------------------------------------------
   Data structures for the display of the address book. There's one
entry in this array of structures per line on the screen. The list 
here is resorted so the distribution lists are at the end of the
display. The strings returned by the adrbklib routines stay around
so only pointers to them are maintained, and not the actual strings.
The storage used here shouldn't be that much.
 ----*/

typedef struct addrscrn_disp {
    AdrBk_Entry *abe;
    char        *addr_in_list;
    enum {End, Single, ListEnt, ListHead, Blank} type;
} AddrScrn_Disp;

static struct AddrScreen {
    AdrBk         *address_book;
    AddrScrn_Disp *disp_list;
    int            dl_used,
                   dl_allocated,
                   cur_col,
                   cur_row,
                   old_cur_row,
                   top_ent,
                   l_p_page;
} addr_screen =  {NULL, NULL, 0, 0, 0, 0, 0, 0, 0};




#ifdef ANSI
int   addr_book_delete(int, int, AdrBk *);
int   add_add(AdrBk *, int, int *);
int   addr_to_list(AdrBk *, int, int *, int);
int   change_address_entry(AdrBk *, int, int, int);
int   create_list(AdrBk *, int, int *);
int   find_in_book(AddrScrn_Disp *, int, int, char *, int *, int *);
int   search_book(AddrScrn_Disp *, int, int, int, int *,int *);
int   next_line(AddrScrn_Disp *, int, int, int *, int *);
int   next_field(AddrScrn_Disp *, int, int, int *, int *);
int   prev_line(AddrScrn_Disp *, int, int, int *, int *);
int   prev_field(AddrScrn_Disp *, int, int, int *, int *);
void  display_book(int, int, int, int, int, int, int, int);
int   init_addr_disp(AdrBk_Entry  *, char *);
void  paint_line(int, AddrScrn_Disp *, int, int);
int   illegal_chars(char *);
int   warn_bad_addr(char *);

#define  ADD_FUN_NULL  ( (int(*)(AdrBk_Entry *)) NULL)

#else  /* ANSI */

int   addr_book_delete();
int   add_add();
int   addr_to_list();
int   create_list();
int   change_address_entry();
int   find_in_book();
int   search_book();
int   next_line();
int   next_field();
int   prev_line();
int   prev_field();
void  display_book();
int   init_addr_disp();
void  paint_line();
int   illegal_chars();
int   warn_bad_addr();

#define  ADD_FUN_NULL  (NULL)

#endif /* ANSI */


/*
  HEADER_LINES is the number of lines in the header
  FOOTER_LINES is the number of lines in the footer
 */
#define ADDR_BOOK_HEADER_LINES   2
#define ADDR_BOOK_FOOTER_LINES   3





/*----------------------------------------------------------------------
    Return the current open address book so functions outside this
file can reference it
 ----*/

AdrBk *
get_adrbk()
{
    if(addr_screen.address_book == NULL) {
#ifdef	DOS
	char book_path[MAXPATH];

	if(ps_global->VAR_ADDRESSBOOK[0] == '\\' 
	   || (isalpha(ps_global->VAR_ADDRESSBOOK[0])
	       && ps_global->VAR_ADDRESSBOOK[1] == ':')){
	    strcpy(book_path, ps_global->VAR_ADDRESSBOOK);
	}
	else{
	    int l = last_cmpnt(ps_global->pinerc) - ps_global->pinerc;

	    strncpy(book_path, ps_global->pinerc, l);
	    book_path[l] = '\0';
	    strcat(book_path, ps_global->VAR_ADDRESSBOOK);
	}

	addr_screen.address_book = adrbk_open(book_path, NULL);
#else
        addr_screen.address_book = 
	  adrbk_open(ps_global->VAR_ADDRESSBOOK, ps_global->home_dir);
#endif
        if(addr_screen.address_book == NULL) {
            q_status_message1(1, 2, 4,
                              "\007Error opening/creating address book: %s",
                              error_description(errno));
            dprint(1, (debugfile, "Error opening address book: %s\n",
                       error_description(errno)));
            return(NULL);
        }
        dprint(1, (debugfile, "Address book opened with %d items\n",
                   adrbk_count(addr_screen.address_book)));

        if(ps_global->VAR_BUGS_NICKNAME != NULL &&
           strlen(ps_global->VAR_BUGS_NICKNAME) > 0 &&
           adrbk_lookup(addr_screen.address_book,
                        ps_global->VAR_BUGS_NICKNAME) == NULL &&
           ps_global->VAR_BUGS_ADDRESS != NULL &&
           strlen(ps_global->VAR_BUGS_ADDRESS) > 0) {
            adrbk_add(addr_screen.address_book, NULL,
                      ps_global->VAR_BUGS_NICKNAME,
                      ps_global->VAR_BUGS_FULLNAME,
                      ps_global->VAR_BUGS_ADDRESS, Atom, ADD_FUN_NULL);
        }
        (void)init_addr_disp((AdrBk_Entry *)NULL, NULL);
    }
    return(addr_screen.address_book);
}

end_adrbk()
{
    if(addr_screen.address_book != NULL) {
        adrbk_close(addr_screen.address_book);
    }
    if(addr_screen.disp_list != NULL)
      fs_give((void **)&addr_screen.disp_list);
}


/*----------------------------------------------------------------------
   Initialize the address book display structure

  Args: match_ent --  address of entry to return line number of
        match_addr -- address of address to return line number of

Result: The disp_list structure is initialized or re-initialized from
        the current address book. The matched line in disp_list is returned.

A number of the address book editing functions need to know what line
in the disp_list the just edited item is on so they can paint the screen
(somewhat) efficiently, so they pass in pointers to the entry just edited
and this returns the line number in disp_list where they were found. 
 This sorts the list with the distribution lists at the end. See comments 
on data structures above.
 ----*/

int
init_addr_disp(match_ent, match_addr)
     AdrBk_Entry *match_ent;
     char        *match_addr;
{
    int             n, n_max, match_n = 0;
    int             are_lists;
    AdrBk_Entry    *ae;
    register char **p;

    dprint(6, (debugfile, "=== init_addr_disp called ===\n"));

    /*--- Now fill in the structure we use for display ---*/
    n_max = adrbk_count(addr_screen.address_book);

    addr_screen.dl_used = 0;

    if(addr_screen.disp_list == NULL) {
        addr_screen.dl_allocated = n_max * 2 + 1; /* +1 so it's never 0 */
        addr_screen.disp_list    = (AddrScrn_Disp *)
                      fs_get(addr_screen.dl_allocated * sizeof(AddrScrn_Disp));
    } 
        

    are_lists = 0;
    /*--- first pass for non-lists ----*/
    for(n = 0; n < n_max; n++) {

        ae = adrbk_get(addr_screen.address_book, n);

        if(ae->tag == List) {
            are_lists = 1;
            continue;
        }

        if(ae == match_ent)
          match_n = addr_screen.dl_used;


        addr_screen.disp_list[addr_screen.dl_used].abe          = ae;
        addr_screen.disp_list[addr_screen.dl_used].addr_in_list = NULL;
        addr_screen.disp_list[addr_screen.dl_used].type         = Single;

        addr_screen.dl_used++;
        if(addr_screen.dl_used >= addr_screen.dl_allocated - 1) {
            addr_screen.dl_allocated *= 2;
            fs_resize((void **)&addr_screen.disp_list,
                      addr_screen.dl_allocated * sizeof(AddrScrn_Disp));
        }
    }

    if(are_lists) {

        for(n = 0; n < n_max; n++) {
            /*------- Second pass for lists -------*/

            ae = adrbk_get(addr_screen.address_book, n);

            if(ae->tag == Atom) {
                continue;
            }

            /*----- First a blank line ------*/
            if(addr_screen.dl_used != 0) {
                addr_screen.disp_list[addr_screen.dl_used].type = Blank;
                addr_screen.dl_used++;
                if(addr_screen.dl_used >= addr_screen.dl_allocated - 1) {
                    addr_screen.dl_allocated *= 2;
                    fs_resize((void **)&addr_screen.disp_list,
                        addr_screen.dl_allocated * sizeof(AddrScrn_Disp));
                }
            }

            if(ae == match_ent && match_addr == NULL)
              match_n = addr_screen.dl_used;

            /*----- The head of the list -----*/
            addr_screen.disp_list[addr_screen.dl_used].abe          = ae;
            addr_screen.disp_list[addr_screen.dl_used].addr_in_list = NULL;
            addr_screen.disp_list[addr_screen.dl_used].type         = ListHead;
    
            addr_screen.dl_used++;
            if(addr_screen.dl_used >= addr_screen.dl_allocated - 1) {
                addr_screen.dl_allocated *= 2;
                fs_resize((void **)&addr_screen.disp_list,
                           addr_screen.dl_allocated * sizeof(AddrScrn_Disp));
            }

            if(ae->addr.list != NULL)
              for(p = ae->addr.list; *p != NULL; p++) {
                  /*----- each entry in the list ------*/
                  addr_screen.disp_list[addr_screen.dl_used].abe          = ae;
                  addr_screen.disp_list[addr_screen.dl_used].addr_in_list = *p;
                  addr_screen.disp_list[addr_screen.dl_used].type    = ListEnt;
  
                  if(ae == match_ent && match_addr == *p)
                    match_n = addr_screen.dl_used;
          
                  addr_screen.dl_used++;
                  if(addr_screen.dl_used >= addr_screen.dl_allocated - 1) {
                      addr_screen.dl_allocated *= 2;
                      fs_resize((void **)&addr_screen.disp_list,
                             addr_screen.dl_allocated * sizeof(AddrScrn_Disp));
                  }
              }

        }
    }
    addr_screen.disp_list[addr_screen.dl_used].type = End;

    return(match_n);
}



/*----------------------------------------------------------------------
    Display entries in the address book on the screen

  Args: top_of_screen  -- entry number in display list of current top of screen
        lines_per_page --  lines on the screen for addresses
        start_disp     --  line to start displaying on when redrawing
        cur_line       --  current line number
        cur_col        --  current column number
        old_line       --  old line number
        old_col        --  old column number
        redraw         --  flag requesting redraw

   Result: lines painted on the screen
 
All the work of painting the lines on the screen by this routine and its
subroutines. It either redraws the screen from a specified line down or
moves the cursor from one field to another.
 ----*/

void
display_book(top_of_screen, lines_per_page, start_disp, cur_line,
             cur_col, old_line, old_col, redraw)
int     lines_per_page;     /* Number of addresses to display */
int     top_of_screen;      /* Index into the address_book->entries
		               of the first thing on the screen */
int     start_disp;         /* Line number to start painting at.
                               The first entry on the screen is 0 */
int     cur_line, cur_col;  /* currently selected line and column */
int     old_line, old_col;  /* currently selected line and column */
                            /* Origin is 0,0 for line, column and
                               the 0th line is the first one displayed
                               which is about the 3rd line on the
                               screen.
                             */
int     redraw;
{
    int  i, j, cursor;

    if(lines_per_page <= 0)
      return;

    if(redraw){
        /*------ Repaint all of the screen or bottom part of screen ---*/
        j = top_of_screen + start_disp;
        for(i = start_disp; i < lines_per_page; i++) {
            if(i == cur_line)
              cursor = cur_col + 1;
            else
              cursor = 0;
            MoveCursor(i + ADDR_BOOK_HEADER_LINES, 0);
            CleartoEOLN();
            paint_line(i + ADDR_BOOK_HEADER_LINES, &addr_screen.disp_list[j],
                       0, cursor);
            if(addr_screen.disp_list[j].type != End)
              j++;
        }
    } else {

        /*----- Only update current, or move the cursor -----*/
        if(cur_line != old_line || cur_col != old_col) {

            /*---- Repaint old position to erase "cursor" ----*/
            paint_line(old_line + ADDR_BOOK_HEADER_LINES,
                       &addr_screen.disp_list[top_of_screen + old_line],
                       old_col + 1, 0);
        }

        /*---- paint the position with the cursor ---*/
        paint_line(cur_line + ADDR_BOOK_HEADER_LINES,
                   &addr_screen.disp_list[top_of_screen +cur_line],
                   cur_col + 1, cur_col + 1);
    }
    fflush(stdout);
}



/*----------------------------------------------------------------------
   Paint a line or part of an address book display line on the screen

  Args: line    --  Line on screen to paint, origin top of screen
        ab_disp --  The display list,
        field   --  The field in the line to paint (see below)
        cursor  --  The field the cursor is on (see below)

Result: Line is painted

The field and cursor arguments have special meaning:
    0 means the whole line, n means colum n. There are 3 columns 1,2 and 3.
    It doesn't make sense for the cursor to be 0.

The column spacing and formatting is done here.
 ----*/

void
paint_line(line, ab_disp, field, cursor)
     AddrScrn_Disp *ab_disp;
     int            line;
     int            cursor,field;
{
    int   col1_width, col2_width, col3_width, col3_pos, screen_width;
    char  col1_control[10],col2_control[10],col3_control[10];

    screen_width = ps_global->ttyo->screen_cols;
    /*--- these control the formatting of the three columns ---*/
    col1_width = min(MAX_NICKNAME-1, screen_width);
    col2_width = min(35, max(0, screen_width - MAX_NICKNAME));
    col3_width = max(screen_width - 2 - col1_width - col2_width, 0);
                                          /* 2 is 2 blank columns */
    col3_pos = col1_width + col2_width + 2;

    /*---- so we don't have to pass funky args to Putlinex() ---*/
    sprintf(col1_control, "%%-%d.%ds", col1_width, col1_width);
    sprintf(col2_control, "%%-%d.%ds", col2_width, col2_width);
    sprintf(col3_control, "%%-%d.%ds", col3_width, col3_width);


    switch(ab_disp->type) {

        case Blank:
        case End:
            MoveCursor(line, 0);
            CleartoEOLN();
            break;
        
    
        case Single:
            if(field == 0 || field == 1) {
                if(cursor == 1)
                  StartInverse();
                PutLine1(line, 0, col1_control, ab_disp->abe->nickname);
                if(cursor == 1)
                  EndInverse();
            }
            if((field == 0 || field == 2) && col2_width != 0) {
                if(cursor == 2)
                  StartInverse();
                PutLine1(line, col1_width + 1, col2_control,
                                                     ab_disp->abe->fullname);
                if(cursor == 2)
                  EndInverse();
            }
            if((field == 0 || field == 3) && col3_width != 0) {
                if(cursor == 3)
                  StartInverse();
                PutLine1(line, col3_pos, col3_control,ab_disp->abe->addr.addr);
                if(cursor == 3)
                  EndInverse();
            }
            break;
    
        case ListHead:
            if(field == 0 || field == 1) {
                if(cursor == 1)
                  StartInverse();
                PutLine1(line, 0, col1_control, ab_disp->abe->nickname);
                if(cursor == 1)
                  EndInverse();
            }
            if((field == 0 || field == 2) && col2_width != 0) {
                if(cursor == 2)
                  StartInverse();
                PutLine1(line, col1_width+ 1, col2_control,
                                                     ab_disp->abe->fullname);
                if(cursor == 2)
                  EndInverse();
            }
            if(field == 0 && col3_width != 0) {
                PutLine1(line, col3_pos, col3_control, "DISTRIBUTION LIST:");
            }
            break;
        
        case ListEnt:
            if(field == 0) {
                if(line == ADDR_BOOK_HEADER_LINES) {
                    char temp[50];
                    sprintf(temp,"%.18s (list continued)",
                            ab_disp->abe->fullname);
                    PutLine1(line, col1_width + 1, col2_control, temp);
                  }
            }
            if(col3_width != 0) {
                if(cursor != 0)
                  StartInverse();
                PutLine1(line, col3_pos, col3_control, ab_disp->addr_in_list);
                if(cursor != 0)
                  EndInverse();
            }
            break;
    }
}



/*----------------------------------------------------------------------
  ---*/
void
redraw_addr_screen()
{
    int  i;

    addr_screen.l_p_page     = ps_global->ttyo->screen_rows -
                               ADDR_BOOK_FOOTER_LINES -
                               ADDR_BOOK_HEADER_LINES;
    if(addr_screen.l_p_page <=0)
      return;
    i                        = addr_screen.cur_row + addr_screen.top_ent;
    addr_screen.top_ent      = (i/addr_screen.l_p_page) * addr_screen.l_p_page;
    addr_screen.cur_row      =  i - addr_screen.top_ent;
    addr_screen.old_cur_row  =  addr_screen.cur_row;

    display_book(addr_screen.top_ent, addr_screen.l_p_page, 0,
                 addr_screen.cur_row, addr_screen.cur_col, -1, -1 , 1);
}



/*----------------------------------------------------------------------
  Little front end for address book screen so it can be called out
 of the main command loop in pine.c
 ----*/

void
addr_book_screen(pine_state)
     struct pine *pine_state;
{
    dprint(1, (debugfile, "=== addr_book_screen  called ====\n"));
    addr_book(pine_state->prev_screen == compose_screen);
    pine_state->prev_screen = addr_book_screen;
}


static struct key ab_keys[] =
     {{"?","Help",0},       {"O","OTHER CMDS",0},   {NULL,NULL,0},
      {NULL,NULL,0},        {"P","PrevField",0},    {"N","NextField",0},
      {"-","PrevPage",0},   {"Spc","NextPage",0},   {"D","Delete",0},
      {"A","Add",0},        {"S","CreateList",0},   {NULL,NULL,0},
      {"?","Help",0},       {"O","OTHER CMDS",0},   {"Q","Quit",0},
      {"C","ComposeTo",0},  {"L","ListFldrs",0},    {"G","GotoFldr",0},
      {"I","Index",0},      {"W","WhereIs",0},      {"Y","Print",0},
      {NULL,NULL,0},        {NULL,NULL,0},          {NULL,NULL,0}};
static struct key_menu ab_keymenu =
	{sizeof(ab_keys)/(sizeof(ab_keys[0])*12), 0, 0,0,0,0, ab_keys};
#define OTHER_KEY  1
#define MAIN_KEY   2
#define EDIT_KEY   3
#define DELETE_KEY 8
#define ADD_KEY    9
#define CREATE_KEY 10
#define ADDTO_KEY  11


/*----------------------------------------------------------------------
    Main address book screen 

  Loop executing all address book commands, executing the commands and
painting the screen. The are_sending flag controls whether or not it is
called to return an address to the composer, or just for address book
maintenance.

 Args: are_sending -- set if selecting an address to return to composer
 ----*/

char *
addr_book(are_sending)
     int are_sending;
{
    int do_not_quit, i, c, orig_c,mangled_footer_flag, page_changed_flag,
        all_changed_flag, current_changed_flag = 0, mangled_header = 0,
        old_cur_col, new_line, new_col, r, display_scr, screen_lines, good_addr,
        command_line, new_ent, did_delete_flag, new_top_ent, screen_changed;
    char            *title,
		    *new_folder,
                    *addr;
    BITMAP           bitmap;
    struct key_menu *km;
    OtherMenu        what;
    CONTEXT_S       *tc;

    km = &ab_keymenu;

    dprint(1, (debugfile, "\n\n   ---- ADDR BOOK ----\n", are_sending));

    /* ---- A few variables to describe the size of the sceen
        screen_lines:   number of total lines on screen including header
	                and footer,
        lines_per_page: Number of lines acutally used for address book 
                        entries excluding footer, and header.
       ----- */

    if(are_sending) {
        /* Coming in from the composer, need to reset this stuff */
        get_windsize(ps_global->ttyo);
        init_signals();
        clear_cursor_pos();
        mark_status_dirty();
    }

    screen_lines         = ps_global->ttyo->screen_rows;
    command_line         = - 3; /* 3 lines from the bottom */
    addr_screen.l_p_page = screen_lines - ADDR_BOOK_FOOTER_LINES -
                              ADDR_BOOK_HEADER_LINES;
    title                = are_sending ? "COMPOSER: SELECT ADDRESS" :
							    "ADDRESS BOOK";
    what                 = FirstMenu;

    get_adrbk();
    if(addr_screen.address_book == NULL) {
      ps_global->next_screen = ps_global->prev_screen ;
      return(NULL);
    }

    old_cur_col              = 0;
    addr_screen.old_cur_row  = 0;
    addr_screen.cur_row      = 0;
    addr_screen.cur_col      = 0;
    do_not_quit              = 1;
    addr_screen.top_ent      = 0;
    all_changed_flag         = 1;
    mangled_header           = 1;
    page_changed_flag        = 1;
    mangled_footer_flag      = 1;
    display_scr              = 0;
    all_changed_flag         = 0;


    ClearScreen();
    c = 'x'; /* For display_message, the first time through */

    ps_global->redrawer = redraw_addr_screen;

    do {

        /*----------- check for new mail!!! ------------- */
        if(new_mail(NULL, 0, c == NO_OP_IDLE ? 0 : 2) >= 0){
            if(ps_global->new_current_sorted_msgno > 0) {
                ps_global->current_sorted_msgno =
                  ps_global->new_current_sorted_msgno;
                ps_global->new_current_sorted_msgno = -1L;
            }
            mangled_header = 1;
        }
        if(streams_died())
          mangled_header = 1;


	if(all_changed_flag) {
	    /*------ Paint the screen_entries on the screen -------*/
	    ClearScreen();
	    mangled_header      = 1;
	    page_changed_flag   = 1;
	    mangled_footer_flag = 1;
	    display_scr         = 0;
	    all_changed_flag    = 0;
	}


	if(mangled_header){
            set_titlebar(title, 1, are_sending ? FolderName : MessageNumber,
		       ps_global->current_sorted_msgno, 0, 0);
	    mangled_header = 0;
	}

	if(page_changed_flag) {
	    /*---------- Paint the addresses in the book -------------*/
	    if(addr_screen.dl_used != 0) {
                display_book(addr_screen.top_ent,
                             addr_screen.l_p_page,
                             display_scr,
                             addr_screen.cur_row,  
                             addr_screen.cur_col,  
                             addr_screen.old_cur_row,  
                             old_cur_col,
                             1);

	    } else {
		for(i = 2; i < ps_global->ttyo->screen_rows - 2; i++){
		    MoveCursor(i,0);
		    CleartoEOLN(); 
		}
                PutLine0(3,0, "            No Addresses in Address Book.");
                fflush(stdout);
                addr_screen.cur_row = 0;
                addr_screen.cur_col = 0;
	    }
	    addr_screen.old_cur_row       = addr_screen.cur_row;
	    old_cur_col       = addr_screen.cur_col;
	    page_changed_flag = 0;
            display_scr       = 0;

	} else if(current_changed_flag) {
	    /*---------- Update the current entry, (move or change) -------*/
            display_book(addr_screen.top_ent,
                         addr_screen.l_p_page,
                         addr_screen.cur_row,
                         addr_screen.cur_row,  
                         addr_screen.cur_col,  
                         addr_screen.old_cur_row,  
                         old_cur_col,  
                         0);
	    addr_screen.old_cur_row = addr_screen.cur_row;
	    old_cur_col             = addr_screen.cur_col;
	    current_changed_flag    = 0;
        }


	/*----------- Repaint the footer -----------------*/
        if(mangled_footer_flag) {

	    setbitmap(bitmap);
	    if(are_sending) {
	      km->how_many = 1;
	      ab_keys[MAIN_KEY].name =  "E";
	      ab_keys[MAIN_KEY].label = "ExitSelect";
	      ab_keys[EDIT_KEY].name =  "S";
	      ab_keys[EDIT_KEY].label = "[Select]";
	      ab_keys[ADDTO_KEY].name =  "W";
	      ab_keys[ADDTO_KEY].label = "WhereIs";
	      clrbitn(OTHER_KEY, bitmap);
	      clrbitn(DELETE_KEY, bitmap);
	      clrbitn(ADD_KEY, bitmap);
	      clrbitn(CREATE_KEY, bitmap);
	    }else {
	      km->how_many = 2;
	      ab_keys[MAIN_KEY].name =  "M";
	      ab_keys[MAIN_KEY].label = "MainMenu";
	      ab_keys[EDIT_KEY].name =  "E";
	      ab_keys[EDIT_KEY].label = "[Edit]";
	      ab_keys[ADDTO_KEY].name =  "Z";
	      ab_keys[ADDTO_KEY].label = "AddToList";
	    }

	    draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
						    -2, 0, what, 0);
	    mangled_footer_flag = 0;
	    what = SameTwelve;
	}


	/*------------ display any status messages ------------------*/
	display_message(c);
	MoveCursor(max(0, ps_global->ttyo->screen_rows - 3), 0);
	fflush(stdout);


	/*---------------- Get command and validate -------------------*/
	c = read_command();
        orig_c = c;

	if(c == ctrl('M') || c == ctrl('J')) /* set up default */
	  if(F_ON(F_USE_FK,ps_global))
	    c = PF4;
	  else
	    c = are_sending ? 's' : 'e';

	if(c < 'z' && isupper(c))
	  c = tolower(c);

	if(km->which == 1)
          if(c >= PF1 && c <= PF12)
            c = PF2OPF(c);

        c = validatekeys(c); 


        dprint(5, (debugfile, "Addrbook command :'%c' (%d)\n", c,c));

	/*------------- execute command ----------------*/
	switch (c) {
          /*
           * Ask for new address, then put it into the book in it's proper
           * sorted position, and move the cursor to that entry so the user
           * gets feed back that it really went in.
           */

            /*------------ Noop   (new mail check) --------------*/
          case NO_OP_IDLE:
	  case NO_OP_COMMAND: 
	    break;


            /*----------- Help -------------------*/
	  case PF1:
	  case OPF1:
	  case '?':
	  case ctrl('G'):
            ps_global->next_screen = SCREEN_FUN_NULL;
	    if(are_sending)
              helper(h_use_address_book, "HELP ON ADDRESS BOOK", 0);
            else
              helper(h_address_book, "HELP ON ADDRESS BOOK", 0);
            if(ps_global->next_screen != SCREEN_FUN_NULL)
              do_not_quit = 0;
	    all_changed_flag++;
	    break;

             
            /*---------- display other key bindings ------*/
          case PF2:
          case OPF2:
          case 'o' :
            if(are_sending)
              goto bleep;
            if (c == 'o')
	      warn_other_cmds();
            what = NextTwelve;
            mangled_footer_flag++;
            break;


            /*------------- Back to main menu or exit to composer -------*/
	  case PF3:
	  case 'm':
	  case 'e':
	    if(c == 'm' && are_sending)
	      goto bleep;
	    if(c == 'e' && !are_sending)
	      goto edit;
	    ps_global->next_screen = main_menu_screen;
	    do_not_quit = 0;
	    break;


            /*------------ Edit field or select -------------*/
	  case PF4:
	    if(are_sending) {
select:
              /*----------- Select an entry to mail too ---------*/
	      if(addr_screen.dl_used == 0) {
	        q_status_message(0, 1, 3,
        "\007No entries in address book. Use ExitSelect to leave address book");
	        break;
	      }
              build_address(addr_screen.disp_list[
                                           addr_screen.top_ent +
                                           addr_screen.cur_row].abe->nickname,
                          tmp_20k_buf, NULL);
              return(tmp_20k_buf);
	    }
edit:
            if(addr_screen.dl_used == 0) {
                q_status_message(1, 2, 4, "No entries to edit");
               break;
            }
                
            new_ent = change_address_entry(addr_screen.address_book,
                                           command_line,
                                           addr_screen.cur_row + addr_screen.top_ent,
                                           addr_screen.cur_col);
            mangled_footer_flag++;
            if(new_ent == -1) {
	        break;
	    }
            if(new_ent == -2) {
              /* Just changed current entry */
                current_changed_flag++;
                break;
            }
            if(new_ent == -3) {
                break;
            }
	    /*----- Edit succeeded, now update the screen -----*/
            new_top_ent = addr_screen.l_p_page > 0 ? 
                       (new_ent/addr_screen.l_p_page)*addr_screen.l_p_page :
                       new_ent;
	    addr_screen.cur_row  = new_ent - new_top_ent;
	    if(new_top_ent != addr_screen.top_ent) {
	        addr_screen.top_ent  = new_top_ent;
		display_scr = 0;
	    } else {
	        display_scr = min(addr_screen.old_cur_row,addr_screen.cur_row);
	    }
	    addr_screen.old_cur_row = addr_screen.cur_row;
	    page_changed_flag++;
	    break;


            /*-------------- Move Right, or Down --------------*/
          case PF5:
          case ctrl('N'):
          case KEY_DOWN:
	  case ctrl('F'): 
	  case KEY_RIGHT:
          case '\t':
          case 'n':
            if(addr_screen.dl_used != 0){
                if(c == ctrl('N') || c == KEY_DOWN)
                  r = next_line(addr_screen.disp_list,
                                addr_screen.cur_row + addr_screen.top_ent,
                                addr_screen.cur_col,
                                &new_line, &new_col);
                else
                  r = next_field(addr_screen.disp_list,
                                 addr_screen.cur_row + addr_screen.top_ent,
                                 addr_screen.cur_col,&new_line, &new_col);
                if(r == 0) {
                    putchar('\007'); /* End of address book */
                    break;
                }
                addr_screen.cur_row = new_line - addr_screen.top_ent;
                addr_screen.cur_col  = new_col;
                if(addr_screen.cur_row >=  addr_screen.l_p_page) {
                    /*-- Changed pages --*/
                    addr_screen.top_ent += addr_screen.l_p_page;
                    addr_screen.cur_row -= addr_screen.l_p_page;
                    page_changed_flag++;
                } else {
                    current_changed_flag++;
                }
	    } else {
		q_status_message(0, 1, 3, "\007Address book is empty");
	    }
	    break;


            /*------------------- Move Left, or Up ----------------*/
          case PF6:
          case ctrl('P'):
          case KEY_UP:
	  case ctrl('B'):
	  case KEY_LEFT:
	  case 'p':
            if(addr_screen.dl_used != 0){
                if(c == ctrl('P') || c == KEY_UP)
                  r = prev_line(addr_screen.disp_list,
                                addr_screen.cur_row + addr_screen.top_ent,
                                addr_screen.cur_col, &new_line, &new_col);
                else
                  r = prev_field(addr_screen.disp_list,
                                 addr_screen.cur_row + addr_screen.top_ent,
                                 addr_screen.cur_col, &new_line, &new_col);
                if(r == 0) {
                    putchar('\007');
                    break;
                }
                addr_screen.cur_row = new_line - addr_screen.top_ent;
                addr_screen.cur_col = new_col;
                if(addr_screen.cur_row < 0) {
                    addr_screen.top_ent -= addr_screen.l_p_page;
                    addr_screen.cur_row += addr_screen.l_p_page;
                    page_changed_flag++;
                } else {
                    current_changed_flag++;
                }
	    } else {
	        q_status_message(0, 1, 3, "\007Address book is empty");
	    }
	    break;


            /*------------ Page up ----------------*/
	  case '-':
          case ctrl('Y'): 
	  case PF7:
	  case KEY_PGUP:
            if(addr_screen.top_ent <= 0) {
		q_status_message(0, 1,1, "Already at top of address book");
		break;
	    }
	    display_scr = 0;
	    addr_screen.top_ent -= max(addr_screen.l_p_page, 1);
            addr_screen.cur_row  = max(0, addr_screen.l_p_page - 1);
            while(addr_screen.disp_list[addr_screen.top_ent+addr_screen.cur_row].type
                                                                      == Blank)
              addr_screen.cur_row--;
            switch(addr_screen.disp_list[addr_screen.top_ent+addr_screen.cur_row].type){
              case Single:
                addr_screen.cur_col = 2;
                break;
              case ListHead:
                addr_screen.cur_col = 1;
                break;
              case ListEnt:
                addr_screen.cur_col = 0;
                break;
            }
	    addr_screen.old_cur_row = addr_screen.cur_row;
	    page_changed_flag++;
	    break;


            /*------------- Page Down --------------*/
          case ' ':
          case ctrl('V'): 
          case '+':		    
	  case PF8:
	  case KEY_PGDN:
	    if(addr_screen.top_ent + addr_screen.l_p_page >=
                                                 addr_screen.dl_used) {
		q_status_message(0, 1, 1, "At end of list");
		break;
	    }
	    display_scr = 0;
	    addr_screen.top_ent += max(addr_screen.l_p_page, 1);
            addr_screen.cur_row = 0;
            addr_screen.cur_col = 0;
            while(addr_screen.disp_list
                  [addr_screen.cur_row+addr_screen.top_ent].type ==Blank)
              addr_screen.cur_row++;
	    addr_screen.old_cur_row = addr_screen.cur_row;
	    page_changed_flag++;
	    break;


	    /*------------- Delete item from list ---------*/
	  case PF9:
	  case 'd': 
	    if(are_sending)
	      goto bleep;
            did_delete_flag = addr_book_delete(command_line,
                                      addr_screen.cur_row+addr_screen.top_ent,
                                      addr_screen.address_book);
	    mangled_footer_flag = 1;
	    if(did_delete_flag || addr_screen.dl_used == 0) {
	        if(addr_screen.top_ent+addr_screen.cur_row >=
                   addr_screen.dl_used) {
	            addr_screen.cur_row--;
                    if(addr_screen.disp_list[addr_screen.top_ent+addr_screen.cur_row].
                                                                  type == End)
                      addr_screen.cur_row--;
                }
	        if(addr_screen.dl_used == 0)
	          addr_screen.cur_row = 0;
                if(addr_screen.disp_list[addr_screen.cur_row +
                                         addr_screen.top_ent].type == Blank) {
                    int new_line, new_col;
                    r = prev_line(addr_screen.disp_list,
                                  addr_screen.cur_row + addr_screen.top_ent,
                                  addr_screen.cur_col, &new_line, &new_col);

                    addr_screen.cur_row = new_line - addr_screen.top_ent;
                    addr_screen.cur_col = new_col;
                    if(addr_screen.cur_row < 0) {
                        addr_screen.top_ent -= addr_screen.l_p_page;
                        addr_screen.cur_row += addr_screen.l_p_page;
                    }
                }
	        addr_screen.old_cur_row = display_scr = addr_screen.cur_row;
	        page_changed_flag++;
	    } 
            break;


            /*-------- Add a single (Atom) forwarding to the list --------*/
          case PF10:
 	  case 'a':
            /*--------------- Create a new list -----------------*/
          case PF11:
          case 's':
            /*-------------- Add entry to list ----------*/
          case PF12:
          case 'z':
	    if(c == 's' && are_sending)
	      goto select;
	    if(c == PF12 && are_sending)
	      goto whereis;
	    if(are_sending)
	      goto bleep;
            screen_changed = 0;
	    new_ent = (c == 's' || c == PF11) ?
                           create_list(addr_screen.address_book, command_line,
                                       &screen_changed)
                    : (c == 'z' || c == PF12) ?
                           addr_to_list(addr_screen.address_book,
                                     command_line, &screen_changed,
                                     addr_screen.top_ent + addr_screen.cur_row)
                    :
                           add_add(addr_screen.address_book, command_line,
                                   &screen_changed);
            if(new_ent < 0) {
                /*---- didn't succeed ----*/
		if(screen_changed) {
		    all_changed_flag++;
		    display_scr = 0;
		}
		mangled_footer_flag++;
		break;
	    }
	    /*----- Addition succeeded, now update the screen -----*/
            
            new_top_ent = addr_screen.l_p_page > 0 ? 
                        (new_ent/addr_screen.l_p_page) * addr_screen.l_p_page :
                        new_ent;
	    addr_screen.cur_row  = new_ent - new_top_ent;
	    if(new_top_ent != addr_screen.top_ent) {
	        addr_screen.top_ent  = new_top_ent;
		display_scr = 0;
	    } else {
	        display_scr = min(addr_screen.old_cur_row,addr_screen.cur_row);
	    }
            addr_screen.cur_col      = 0;
	    addr_screen.old_cur_row  = addr_screen.cur_row;
	    if(screen_changed) {
		all_changed_flag++;
	        display_scr = 0;
	    }
	    page_changed_flag++;
	    mangled_footer_flag++;
	    break;


            /*--------- QUIT pine -----------*/
          case OPF3:
	  case 'q':
            if(are_sending)
              goto bleep;
            ps_global->next_screen = quit_screen;
	    do_not_quit = 0;
            break;


            /*--------- Compose -----------*/
          case OPF4:
	  case 'c':
            if(are_sending)
              goto bleep;
            good_addr = (build_address(addr_screen.disp_list[
		      addr_screen.top_ent + addr_screen.cur_row].abe->nickname,
		             tmp_20k_buf, NULL) >= 0);
            ps_global->next_screen = ps_global->prev_screen; /* necessary? */
	    addr = good_addr ? cpystr(tmp_20k_buf) : NULL;
            compose_mail(addr);
            if(addr)
              fs_give((void **)&addr);
            all_changed_flag++;
            break;


            /*--------- Folders -----------*/
          case OPF5:
	  case 'l':
            if(are_sending)
              goto bleep;
            ps_global->next_screen = folder_screen;
	    do_not_quit = 0;
            break;


            /*---------- Open specific new folder ----------*/
          case OPF6:
	  case 'g':
            if(are_sending || ps_global->nr_mode)
              goto bleep;

	    tc = (ps_global->context_last
		      && !(ps_global->context_current->type & FTYPE_BBOARD)) 
                       ? ps_global->context_last : ps_global->context_current;

            new_folder = broach_folder(-3, 1, &tc);
#ifdef	DOS
	    if(new_folder && *new_folder == '{' && coreleft() < 20000){
	      q_status_message(0, 0, 2,
			       "\007Not enough memory to open IMAP folder");
	      new_folder = NULL;
	    }
#endif
            if(new_folder != NULL) {
              dprint(9, (debugfile, "do_broach_folder (%s, %s)\n",
                         new_folder, tc->context));
              if(do_broach_folder(new_folder, tc) > 0)
                  /* so we go out and come back into the index */
                  ps_global->next_screen = mail_index_screen;
            }
	    do_not_quit = 0;
            break;


            /*--------- Index -----------*/
          case OPF7:
	  case 'i':
            if(are_sending)
              goto bleep;
            ps_global->next_screen = mail_index_screen;
	    do_not_quit = 0;
            break;


            /*----------- Where is (search) ----------------*/
	  case OPF8:
	  case 'w':
	    if(c == OPF8 && are_sending)
	      goto bleep;
whereis:
	    r = search_book(addr_screen.disp_list,
                            addr_screen.top_ent + addr_screen.cur_row,
                            addr_screen.cur_col, command_line,
                            &new_line, &new_col);
	    if(r == -2) {
	        mangled_footer_flag++;
	        break;
	    }
	    if(r == -1) {
	        q_status_message(0, 1, 2,"\007Word not found");
	        mangled_footer_flag++;
	        break;
	    }

	    /*-- place cursor on new address ----*/
            q_status_message1(0, 1, 2, "Word found%s",
                 new_line < addr_screen.cur_row + addr_screen.top_ent ||
                  (new_line == addr_screen.cur_row + addr_screen.top_ent &&
                   new_col < addr_screen.cur_col)
                              ?
                        ". Search wrapped to beginning"
                               :
                       "");
            new_top_ent = addr_screen.l_p_page > 0 ? 
                     (new_line / addr_screen.l_p_page) * addr_screen.l_p_page :
                      new_line;
	    addr_screen.cur_row     = new_line - new_top_ent;
            addr_screen.cur_col     = new_col;
	    if(new_top_ent != addr_screen.top_ent) {
	        addr_screen.top_ent     = new_top_ent;
	        display_scr             = 0;
	        addr_screen.old_cur_row = addr_screen.cur_row;
	        page_changed_flag++;
	    } else {
	        current_changed_flag++;
	    }
	    mangled_footer_flag++;
	    break;


            /*----------------- Print --------------------*/
	  case OPF9: 
	  case 'y':
            if(open_printer("address book ") == 0){
                AddrScrn_Disp *l; 

                for(l = addr_screen.disp_list; l->type != End; l++) {
                    switch(l->type) {
                      case Single:
                        print_text3("%-10.10s %-35.35s %s\n",
                                    l->abe->nickname, l->abe->fullname,
                                    l->abe->addr.addr);
                        break;

                      case ListHead:
                        print_text2("%-10.10s %-35.35s DISTRIBUTION LIST:\n",
                                    l->abe->nickname, l->abe->fullname);
                        break;

                      case ListEnt:
                        print_text1("                                               %s\n", l->addr_in_list);
                        break;

                      case Blank:
                        print_text("\n");
                    }
                }
                close_printer();
            }
            break;


            /*------------ Suspend pine  (^Z) -------------*/
          case ctrl('Z'):
            if(!have_job_control())
              goto bleep;
            if(F_OFF(F_CAN_SUSPEND,ps_global)) {
                q_status_message(1, 2, 4, 
                          "\007Pine suspension not enabled - see help text");
                break;
            } else {
                do_suspend(ps_global);
            }
            /*-- Fall through to redraw --*/

          case KEY_RESIZE:
          case ctrl('L'):
	    screen_lines         = ps_global->ttyo->screen_rows;
	    command_line         = - 3; /* 3 lines from the bottom */
	    addr_screen.l_p_page = screen_lines - ADDR_BOOK_FOOTER_LINES
						- ADDR_BOOK_HEADER_LINES;
            all_changed_flag++;
	    if(c == KEY_RESIZE)
	      clear_index_cache();

	    break;


	  default:
          bleep:
	    q_status_message2(0, 0, 2,
              "\007Command \"%s\" not defined for this screen. Use %s for help",
                          (void *)pretty_command(orig_c),
                          F_ON(F_USE_FK,ps_global) ? "F1" : "?");
	    break;
	}
    } while(do_not_quit);
    
    return(NULL);
}


/*----------------------------------------------------------------------
   Find the line and column number of the next field on

   Args: display_list -- The display data structure
         cur_line     -- The current line position of cursor
         cur_col      -- The current column position of cursor
         new_line     -- Pointer to return new line position in
         new_col      -- Pointer to return new column position in

 Result: The new line and column number, and the value 1 is returned if OK
         or 0 if there is no next field

This negotiates blanks lines, empty fields and such
 ----*/

int
next_field(disp_list, cur_line, cur_col, new_line, new_col)
     AddrScrn_Disp *disp_list;
     int cur_line, cur_col;
     int *new_line, *new_col;
{
    int nl, nc;

    nl = cur_line;
    nc = cur_col;

    switch(disp_list[cur_line].type) {

      case Single:
        if(cur_col < 2) {
            nc = cur_col + 1;
        } else {
            nc = 0;
            nl = cur_line + 1;
        }
        break;

      case ListHead:
        if(cur_col == 0) {
            nc = cur_col + 1;
        } else {
            nl = cur_line + 1;
            nc = 0;
        }
        break;

      case ListEnt:
        nl = cur_line + 1;
        break;
    }
    while(disp_list[nl].type == Blank)
      nl++;

    if(disp_list[nl].type != End) {
        *new_line = nl;
        *new_col = nc;
        return(1);
    } else {
        return(0);
    }
}



/*----------------------------------------------------------------------
   Find the line and column number of the next line, keeping the column
about the same (depends on what's next).

   Args: display_list -- The display data structure
         cur_line     -- The current line position of cursor
         cur_col      -- The current column position of cursor
         new_line     -- Pointer to return new line position in
         new_col      -- Pointer to return new column position in

 Result: The new line and column number, and returns the value 1 if OK
         or 0 if there is no next line

This negotiates blank lines, and fields.
 ---*/

int
next_line(disp_list, cur_line, cur_col, new_line, new_col)
     AddrScrn_Disp *disp_list;
     int cur_line, cur_col;
     int *new_line, *new_col;
{
    int nl, nc;

    nl = cur_line;
    nc = cur_col; /* So compilers don't think is used before set */

    if(disp_list[nl].type == End)
      return(0);
    nl++;
    while(disp_list[nl].type == Blank)
      nl++;

    switch(disp_list[nl].type) {

      case Single:
        nc = cur_col;
        break;

      case ListHead:
        nc = cur_col;
        if(nc > 1)
          nc = 1;
        break;

      case ListEnt:
        if(disp_list[cur_line].type == ListEnt)
          nc = 1;
        else
          nc = 0;
        break; 

      case End:
        return(0);

    }

    *new_line = nl;
    *new_col = nc;
    return(1);
}

 


/*----------------------------------------------------------------------
   Find the line and column number of the previous line back

   Args: display_list -- The display data structure
         cur_line     -- The current line position of cursor
         cur_col      -- The current column position of cursor
         new_line     -- Pointer to return new line position in
         new_col      -- Pointer to return new column position in

 Result: The new line and column number, and a value of 1 if OK
         or 0 if there is no previous line.

This negotiates blank lines and fields.
 ----*/

int
prev_line(disp_list, cur_line, cur_col, new_line, new_col)
     AddrScrn_Disp *disp_list;
     int            cur_line, cur_col;
     int           *new_line, *new_col;
{
    int nl, nc;

    nl = cur_line;
    nc = cur_col; /* So compilers don't think is used before set */

    if(nl == 0)
      return(0);
    nl--;
    while(disp_list[nl].type == Blank && nl >= 0)
      nl--;
    if(nl < 0)
      return(0);

    switch(disp_list[nl].type) {

      case Single:
        nc = cur_col;
        break;

      case ListHead:
        if(disp_list[cur_line].type == ListHead)
          nc = cur_col;
        else
          nc = 1; 
        break;

      case ListEnt:
        nc = 0;
        break; 
    }

    *new_line = nl;
    *new_col = nc;
    return(1);
}
 



/*----------------------------------------------------------------------
  Find the line and column number of the previous field, keep the
column number about the same depending on the type of fields there
are.

   Args: display_list -- The display data structure
         cur_line     -- The current line position of cursor
         cur_col      -- The current column position of cursor
         new_line     -- Pointer to return new line position in
         new_col      -- Pointer to return new column position in
                            
 Result: The new line and column number, and returns the value 1 if OK
         or 0 if there is no previous field

This negotiates blanke lines and fields
 ---*/

int
prev_field(disp_list, cur_line, cur_col, new_line, new_col)
     AddrScrn_Disp *disp_list;
     int cur_line, cur_col;
     int *new_line, *new_col;
{
    int nl, nc;

    nl = cur_line;
    nc = cur_col;

    switch(disp_list[cur_line].type) {

      case Single:
        /* never back up into list because they are always last,
           no blank lines either */
        if(cur_col != 0 ){
            nc = cur_col - 1;
        } else {
            if(cur_line != 0){
                nc = 2;
                nl = cur_line - 1;
            } else {
                return(0);
            }
        }
        break;



      case ListHead:
        if(cur_col == 0) {
            if(cur_line == 0)
              return(0);
            cur_line--;
            while(cur_line != 0 && disp_list[cur_line].type == Blank)
              cur_line--;
            if(cur_line != 0) {
                switch(disp_list[cur_line].type) {
                  case ListEnt:
                    nc = 0;
                    break;
                  case ListHead:
                    nc = 1;
                    break;
                  case Single:
                    nc = 2;
                    break;
                }
                nl = cur_line;
            } else {
                return(0);
            }
        } else {
            nl = cur_line;
            nc = 0;
        }
        break;


      case ListEnt:
        nl = cur_line - 1;
        if(disp_list[nl].type == ListHead)
          nc = 1;
        else
          nc = 0;
        break;

    }

    *new_line = nl;
    *new_col = nc;
    return(1);
}
        
        


/*----------------------------------------------------------------------
   Delete an entry from the address book

   Args: command_line -- The screen line on which to prompt
         cur_ab_ent   -- The entry number in the display list
         ab           -- The addrbook handle into access library

  Result: returns 1 if an entry was delete, 0 if not.

The main routine above knows what to repaint because it's alwyas the
current entry that's always deleted. Here confirmation is asked of the user
and the appropriate adrbklib functions are called.
 ----*/

int
addr_book_delete(command_line, cur_ab_ent, ab)
     int    command_line, cur_ab_ent;
     AdrBk *ab;
{
    char   ch, *cmd, *dname;
    char   prompt[40 +MAX_ADDRESS +1];/* 40 is len of string constants below */
    int    rc = command_line; /* nuke warning about command_line unused*/
    register AddrScrn_Disp *ae = &addr_screen.disp_list[cur_ab_ent];

    dname = ""; /* So compilers won't complain this used before it's set */
    cmd   = ""; /* So compilers won't complain this used before it's set */

    dprint(4, (debugfile, "\n - addr_book_delete -\n"));

    if(addr_screen.dl_used  <= 0) {
        q_status_message(0, 1,2, "\007Address book is empty. Nothing to delete");
	return(0);
    }

    switch(ae->type) {
      case Single:
        dname = ae->abe->fullname;
        cmd   = "Really delete \"%.50s\"";
        break;

      case ListHead:
        dname = ae->abe->fullname;
	cmd   = "Really delete ENTIRE list \"%.50s\"";
        break;

      case ListEnt:
        dname = ae->addr_in_list;
	cmd   = "Really delete \"%.100s\" from list";
        break;
    } 

    sprintf(prompt, cmd, dname);
    ch = want_to(prompt, 'n', 'n', NO_HELP, 0);
    if(ch == 'y') {
	if(ae->type == Single || ae->type == ListHead){
	    /*--- Kill a single entry or an entry in a list ---*/
            rc = adrbk_delete(ab, ae->abe);
	} else {
            /*---- Kill an entry out of a list ----*/
            rc = adrbk_listdel(ab, ae->abe, ae->addr_in_list);
            (void)init_addr_disp((AdrBk_Entry *)NULL, NULL);
        }
	if(rc == 0) {
	    q_status_message(0, 1, 3, "Entry deleted, address book updated");
            dprint(2, (debugfile, "\"%s\" deleted from addr book\n", dname));
            (void)init_addr_disp((AdrBk_Entry *)NULL, NULL);
            return(1);
        } else {
            q_status_message1(0, 2, 5, "\007Error writing address book: %s",
                                                   error_description(errno));
            dprint(1, (debugfile, "Error deleting \"%s\" : %s\n", dname,
                       error_description(errno)));
        }
	return(0);
    } else {
	q_status_message(0, 0, 2, "\007Entry not deleted");
	return(0);
    }
}



/*----------------------------------------------------------------------
     Prompt for name, address, etc for a simple addition

   Args: abook          -- The address book handle for the addrbook access lib
         command_line   -- The screen line to prompt on
         screen_mangled -- pointer to flag to be set if screen was mangled

 Result: address book possibly updated.  If address selection screen
         is called up returns fact that screen needs repainting. Returns
         less than zero on failre or cancelation and line number of entry that
         was just added.

 This is only for adding a plain address book entry, and does nothing with
lists.
 ----*/

int
add_add(abook, command_line, screen_mangled)
     AdrBk *abook;
     int    command_line;
     int   *screen_mangled;
{
    char         new_name[MAX_FULLNAME + 1], new_address[MAX_ADDRESS + 1],
                 new_nick_name[MAX_NICKNAME + 1];
    char        *prompt;
    HelpType     help;
    int          where, rc, oe_append;
    AdrBk_Entry *new;


    dprint(4, (debugfile, "\n - add_add -\n"));;
    *screen_mangled = 0;
    new_address[0]  = '\0';
    new_name[0]     = '\0';

    /*------ full name ------*/
    help = NO_HELP;
    oe_append = 0;
    do {
	char *q;
        prompt = "New full name (last, first) : ";
        rc = optionally_enter(new_name, command_line, 0, MAX_FULLNAME,
                               oe_append, 0, prompt, NULL, help, 0);
        oe_append = 1;
        if(rc == 1) /* ^C cancel */
          goto add_cancel;

        if(rc == 3) { /* Help */
            help = (help == NO_HELP ? h_oe_add_addr : NO_HELP);
            continue;
        }
    
        if(rc == 4)  /* Redraw */
            continue;

	for(q=new_name; isspace(*q); ++q)
	  ;
        if(*q == '\0') 
          goto add_cancel;
        else
          break;

    } while(1);
    
            
    /*----- nickname ------*/
    help = NO_HELP;
    oe_append = 0;
    while(1) {
	char *q;
        prompt ="Enter new nickname (one word and easy to remember) : ";
        rc = optionally_enter(new_nick_name, command_line, 0, MAX_NICKNAME,
                              oe_append, 0, prompt, NULL, help, 0);

        oe_append = 1;
        if(rc == 3) { /* Help */
            help = (help == NO_HELP ? h_oe_add_nick : NO_HELP);
            continue;
        }

        if(rc == 4) /* redraw */
          continue;

        if(rc == 1)
          goto add_cancel;
	for(q=new_nick_name; isspace(*q); ++q)
	  ;
        if(*q == '\0') 
          goto add_cancel;

        if(strindex(new_nick_name, ' ') != NULL) {
            q_status_message(0, 1,2,"\007Blank spaces not allowed in nicknames");
            display_message(NO_OP_COMMAND);
            sleep(1);
            continue;
        }
            
        if(adrbk_lookup(abook, new_nick_name) == NULL) 
	  break;

        q_status_message1(0, 1, 2, "\007Already an entry with nickname \"%s\"",
			  new_nick_name);
        display_message(NO_OP_COMMAND);
	sleep(1);
    }


    /*---- address ------*/
    help = NO_HELP;
    while(1) {
	char *q;
        char *prompt = "Enter new e-mail address :";
        rc = optionally_enter(new_address, command_line, 0, MAX_ADDRESS, 1, 0,
                                  prompt , NULL, help, 0);

        if(rc == 1)
          goto add_cancel;
        

        if(rc == 3) { /* help */
            help = (help == NO_HELP ? h_oe_add_addr : NO_HELP );
            continue;
        }

        if(rc == 4)
          continue;
          

	for(q=new_address; isspace(*q); ++q)
	  ;
        if(*q == '\0') 
          goto add_cancel;

	if(!warn_bad_addr(new_address))
	  break;
    }
            

    /*---- write it into the file ----*/
    rc = adrbk_add(addr_screen.address_book, &new, new_nick_name, new_name,
                   new_address, Atom , ADD_FUN_NULL);
    if(rc == -2 || rc == -3) {
        q_status_message1(1, 2, 4, "\007Error updating address book: %s",
              rc == -2 ?  error_description(errno) : "Pine bug");
        dprint(1, (debugfile, "Error adding \"%s\": %s\n", new_nick_name,
              rc == -2 ?  error_description(errno) : "Pine bug"));
        return(-2);
    }
    if(rc == -4) {
        q_status_message(0, 2, 3,  "\007Tabs not allowed in address book");
        return(-4);
    }


    where = init_addr_disp(new, NULL);
    q_status_message(0, 0, 2, "Addition complete. Address book updated.");
    dprint(2, (debugfile, "\"%s\" added to address book\n", new_nick_name));
    return(where);
 
 add_cancel:
    q_status_message(0, 0, 2,"\007Address book addition cancelled");
    return(-1);
  
}


/*----------------------------------------------------------------------
     Create a distribution list

   Args: abook          -- Handle into address book access library
         command_line   -- screen line to prompt on
         screen_mangled -- Pointer to flag to set if whole screen is mangled.

 Result: Distribution list possibly created, 
         returns flag if screen was painted by the address grabber
         return value: 0 - n: new entry added at line n
                         -1 : creation cancelled
                         -2 : error writing address book
                         -4 : Tabs in fields (tabs are not allowed).
                                  (should never happen)

  Prompt for the description, then nickname and then up to 100 address
entries. (100 is the usual configuration).    
  ----*/

int
create_list(abook, command_line, screen_mangled)
     AdrBk *abook;
     int       command_line, *screen_mangled;
{
    char         list_name[MAX_FULLNAME+1], new_nick_name[MAX_NICKNAME+1],
                 new_address[MAX_ADDRESS+1], prompt2[80];
    char        *temp_list[MAX_NEW_LIST+1];
    char        **p, *prompt, *q;
    HelpType	 help;
    AdrBk_Entry  *new;
    int           rc, where, oe_append;

    screen_mangled = screen_mangled; /* get rid of unused param message */


    dprint(4, (debugfile, "\n - create_list -\n"));
    /*------ name for list ------*/
    prompt = "Long name/description of new list: ";
    help = NO_HELP;
    oe_append = 0;
    do{
        rc = optionally_enter(list_name, command_line, 0, MAX_FULLNAME,
                              oe_append, 0, prompt, NULL, help, 0);
        oe_append = 1;

        if(rc == 3) 
            help = (help == NO_HELP ? h_oe_crlst_full : NO_HELP);
    } while(rc == 3 || rc == 4); /* redraw and help */

    if(rc == 1)
          goto create_cancel;
    for(q=list_name; isspace(*q); ++q)
      ;
    if(*q == '\0') 
          goto create_cancel;

            
    /*----- nickname ------*/
    prompt = "Enter list nickname (one word and easy to remember) : ";
    help   = NO_HELP;
    oe_append = 0;
    while(1) {
        rc = optionally_enter(new_nick_name, command_line, 0, MAX_NICKNAME,
                              oe_append, 0, prompt,  NULL, help, 0);

        oe_append = 1;
        if(rc == 3) {
            help = (help == NO_HELP ? h_oe_crlst_nick : NO_HELP);
            continue;
        }

        if(rc == 4) /* Redraw */
          continue; 
        
        if(rc == 1)
          goto create_cancel;
        for(q=new_nick_name; isspace(*q); ++q)
          ;
        if(*q == '\0') 
          goto create_cancel;

        if(strindex(new_nick_name, ' ') != NULL) {
            q_status_message(0, 1,1,"\007Blank spaces not allowed in nicknames");
            display_message(NO_OP_COMMAND);
            sleep(1);
            continue;
        }
            
        if(adrbk_lookup(abook, new_nick_name) == NULL) 
	  break;

        q_status_message1(0, 1,1,"\007Already an entry with nickname \"%s\"",
			  new_nick_name);
        display_message(NO_OP_COMMAND);
	sleep(1);
    }


    /*---- addresses ------*/
    for(p = temp_list; p < &temp_list[MAX_NEW_LIST]; p++){
        sprintf(prompt2, "Enter %s address or blank when done :",
                enth_string(p - temp_list + 1));

        new_address[0] = '\0';
        help = NO_HELP;
        while(1) {
	    char *q;
            rc = optionally_enter(new_address, command_line, 0, MAX_ADDRESS,
                                  1, 0,
                                  prompt2, NULL, help, 0);
            if(rc == 1)
              goto create_cancel;

            if(rc == 3) { /* help */
                help = (help == NO_HELP ? h_oe_crlst_addr : NO_HELP);
                continue;
            }
            if(rc == 4) /* redraw */
              continue ;
	    for(q=new_address; isspace(*q); ++q)
	      ;
            if(*q == '\0') 
                 goto done_with_addrs; /* we're done! */

	    if(!warn_bad_addr(new_address))
	      break;
        }
        *p = cpystr(new_address);

    }
  done_with_addrs:
    *p = NULL;

    rc = adrbk_add(addr_screen.address_book, &new, new_nick_name,
                      list_name, NULL, List ,
#ifdef ANSI
                      (int (*)(AdrBk_Entry *))
#endif                   
                   NULL);

    if(rc == 0)
      for(p = temp_list; *p != NULL; p++) 
        if((rc = adrbk_listadd(addr_screen.address_book, new, *p, NULL)) != 0)
          break;

    if(rc == -2 || rc == -3) {
        q_status_message1(1, 2, 4,"\007Error updating address book: %s",
                   rc == -2 ?  error_description(errno) : "Pine bug");
        dprint(1, (debugfile, "Error creating list \"%s\" in addrbook: %s\n",
               new_nick_name, rc == -2 ? error_description(errno) :
               "pine bug"));
        return(-2);
    }

    if(rc == -4) {
        q_status_message(0, 2,2, "\007Tabs not allowed in address book");
        return(-4);
    }



    where = init_addr_disp(new, NULL);
    q_status_message1(0, 1,2,
        "Addition of list %s complete. Address book updated.", new_nick_name);
    dprint(2, (debugfile, "List addition to address book \"%s\"\n",
                                                          new_nick_name));
    return(where);
 
 create_cancel:
    q_status_message(0, 0,1, "\007Address book list creation cancelled");
    return(-1);
}



/*----------------------------------------------------------------------
      Add an entry to a distribution list

   Args: abook          -- Handle to address book for adrbklib access library
         command-line   -- Screen line number to prompt on 
         screen_mangled -- Pointer to flag to set if sceen is mangled
         cur            -- Entry number in display list of list to add to

 Result: flag set if screen was mangled, possible addition to list
         Returns: 0 - n: line in disp list of new entry
                     -1: addition cancelled
                     -2: error writing address book
 ----*/

int
addr_to_list(abook, command_line, screen_mangled, cur)
     AdrBk *abook;
     int       command_line, *screen_mangled, cur;
{
    char           new_address[MAX_ADDRESS+1], prompt[80];
    char          *new_addr, *edited_nick;
    HelpType	  help;
    int            rc, where;
    AddrScrn_Disp *a = &addr_screen.disp_list[cur];

    screen_mangled = screen_mangled; /* rid of unused param message */

    dprint(4, (debugfile, "\n - addr_to_list -\n"));

    if (abook->book_used == 0) {
        q_status_message(0, 1,3, "\007No distribution lists. To create, use \"S\" for list or \"A\" single entry");
        return(-1);
    }

    if (a->type == Single || a->type == Blank) {
        q_status_message(1, 1,3, "\007Move cursor to list you wish to add to. Use \"A\" to create plain entry");
        return(-1);
    }

    new_address[0] = '\0';

    edited_nick = a->abe->nickname;

    sprintf(prompt, "Address to add to \"%s\" list :", edited_nick);


    help = NO_HELP;
    while(1) {
	char *q;
        rc = optionally_enter(new_address, command_line, 0, MAX_ADDRESS, 1, 0,
                                  prompt, NULL, help, 0);

        if(rc == 1)
          goto alist_cancel;

        if(rc == 3) {
            help = (help == NO_HELP ? h_oe_adlst_addr : NO_HELP);
            continue;
        }
        if(rc == 4) /* Redraw */
            continue;
	for(q=new_address; isspace(*q); ++q)
	  ;
        if(*q == '\0') 
          goto alist_cancel;

	if(!warn_bad_addr(new_address))
	  break;
    }

    rc = adrbk_listadd(abook, a->abe, new_address, &new_addr);

    if(rc == -2) {
        q_status_message1(1, 1,3, "\007Error updating address book: %s",
                                                    error_description(errno));
        dprint(1, (debugfile, "Error adding to list \"%s\": %s\n",
                       edited_nick, error_description(errno)));
        return(-2);
    }

    where = init_addr_disp(a->abe, new_addr);
    q_status_message1(0, 0, 2,
        "Addition to \"%s\" list complete. Address book updated.",
                                edited_nick);
    dprint(3, (debugfile, "Addition to list \"%s\"\n", edited_nick));
    return(where);

alist_cancel:
    q_status_message(0, 0,1, "\007Address book addition to list cancelled");
    return(-1);
}



/*----------------------------------------------------------------------
    Edit some individual field in the address book

    Args: abook          -- Handle into access library for open address book
          command_line   -- Screen line number to prompt on
          cur_line       -- Current line in display list fo cursor
          cur_col        -- Current column cursor is on (to be edited)

  Result: Returns -1 - addition was cencelled
                  -2 - Simple change to current, not resorting required
                  -3 - No change made
                >= 0 - Change made and address book resorted

This can edit any field in the address book. Some times resorting of
the address book may be required because of an edit. The adrbklib routines 
do their part, and the display is reinitialized and the display line
number of the position of the new entry is returned so the cursor can
be positioned on it.
 ----*/

int
change_address_entry(abook, command_line, cur_line, cur_col)
     AdrBk *abook;
     int    command_line, cur_line, cur_col;
{
    AddrScrn_Disp *a;
    char           edit_buf[max(MAX_ADDRESS, MAX_FULLNAME) + 1];
    char          *addr_match, **p, *prompt;
    HelpType	   help;
    int            rc, where;
    AdrBk_Entry   *check, *new;

    a          = &addr_screen.disp_list[cur_line];
    addr_match = NULL;
    new        = NULL;

    dprint(4, (debugfile, "\n - change_address_entry: %d %d-\n", a->type,
               cur_col));
    if((a->type == ListHead || a->type == Single) && cur_col == 0) {
        /*================ Nickname ===================================*/
        strcpy(edit_buf, a->abe->nickname);
        help = NO_HELP;
        while(1) {
            prompt =  "Edit Nickname :";
            rc = optionally_enter(edit_buf, command_line, 0, MAX_NICKNAME, 1,
                                  0, prompt, NULL, help, 0);
            if(rc == 1)
              goto change_cancel;
            if(rc == 3) {
                help = (help == NO_HELP ? h_oe_editab_nick : NO_HELP);
                continue;
            }
            if(rc == 4)
              continue;

            if(rc == 0 && strcmp(edit_buf, a->abe->nickname) == 0)
              return(-3);

            if(strindex(edit_buf, ' ') != NULL) {
                q_status_message(0, 1,1,
                                 "\007Blank space not allowed in nicknames");
                display_message(NO_OP_COMMAND);
                sleep(1);
                continue;
            }

            check = adrbk_lookup(abook, edit_buf);
            if(check == NULL || check == a->abe)
              break;

            q_status_message1(0, 1, 1,"\007Already an entry with nickname \"%s\"",
                             edit_buf);
            display_message(NO_OP_COMMAND);
            sleep(1);
        }
        if(rc != 0) 
          goto change_cancel;

        rc = adrbk_add(abook, &new, edit_buf, a->abe->fullname,
              a->abe->tag==List? (char *)a->abe->addr.list: a->abe->addr.addr,
                       a->abe->tag, ADD_FUN_NULL);

        if(a->abe->tag == List && a->abe->addr.list != NULL)
          /* Add list back in if it's a list */
          for(p = a->abe->addr.list; rc == 0 && *p != NULL; p++)
            rc = adrbk_listadd(abook, new, *p, NULL);
            
        
        if(rc == 0 && strucmp(a->abe->nickname, edit_buf) != 0) {
            /* if only change was case in nickname, then above was an edit
               and no delete is required */
            rc = adrbk_delete(abook, a->abe);
            (void)init_addr_disp((AdrBk_Entry *)NULL, NULL);
        } 

    } else if(a->type == Single && cur_col == 1) {
        /*======================== Full name for a single forwarding =======*/
        strcpy(edit_buf, a->abe->fullname);
        prompt =  "Edit Full name :";
        help = NO_HELP;
        do {
            rc = optionally_enter(edit_buf, command_line, 0, MAX_FULLNAME, 1,
                                  0, prompt, NULL, help, 0);
            
            if(rc == 3)
                help = (help == NO_HELP ? h_oe_editab_full: NO_HELP);

        } while(rc == 3 || rc == 4); /* loop on help and redraw */

        if(rc == 0)
          rc = adrbk_add(abook, &new, a->abe->nickname, edit_buf, 
                         a->abe->addr.addr, a->abe->tag, ADD_FUN_NULL);
        else
          goto change_cancel;

        
    } else if(a->type == Single && cur_col == 2) {
        /*================ Address in a single forwarding ===============*/
        strcpy(edit_buf, a->abe->addr.addr);
        prompt =    "Edit address :";
        help = NO_HELP;
	while(1) {
            rc = optionally_enter(edit_buf, command_line, 0, MAX_ADDRESS, 1, 0,
                                      prompt, NULL, help, 0);
            if(rc == 1)
              goto change_cancel;

            if(rc == 3) {/* Help */
                help = help == NO_HELP ? h_oe_editab_addr : NO_HELP;
                continue;
            }

            if(rc == 4) /* redraw */
              continue;

	    if(!warn_bad_addr(edit_buf))
	      break;
	}

        if(rc == 0)
          rc = adrbk_add(abook, &new, a->abe->nickname, a->abe->fullname,
                         edit_buf, a->abe->tag, ADD_FUN_NULL);
        else
          goto change_cancel;
        
    } else if(a->type == ListHead && cur_col == 1){
        /*=============== Full name  of a list ====================*/
        strcpy(edit_buf, a->abe->fullname);
        prompt =  "Edit Full name of list:";
        help = NO_HELP;
        do {
            rc = optionally_enter(edit_buf, command_line, 0, MAX_FULLNAME, 1,
                                  0, prompt, NULL, help, 0);
              if(rc == 3)
                help = help == NO_HELP ? h_oe_editab_addr : NO_HELP;

        } while(rc == 3 || rc == 4); /* Loop for help and redraw */

        if(rc == 0) 
          rc = adrbk_add(abook, &new, a->abe->nickname, edit_buf, 
                        (char *)a->abe->addr.list, a->abe->tag, ADD_FUN_NULL);
        else
          goto change_cancel;

    } else {
        /*============ Must be address in list ==============*/ 
        strcpy(edit_buf, a->addr_in_list);
        prompt =  "Edit address in list:";
        help = NO_HELP;
        while(1) {
            rc = optionally_enter(edit_buf, command_line, 0, MAX_ADDRESS, 1, 0,
                                      prompt, NULL, help, 0);
            if(rc == 1)
              goto change_cancel;

            if(rc == 3) {/* Help */
                help = help == NO_HELP ? h_oe_editab_al : NO_HELP;
                continue;
            }

            if(rc == 4) /* redraw */
              continue;

	    if(!warn_bad_addr(edit_buf))
	      break;
        }
        rc = adrbk_listdel(abook, a->abe, a->addr_in_list);
        if( rc == 0 )
          rc = adrbk_listadd(abook, a->abe, edit_buf, &addr_match);
        new = a->abe;
    }
        
        
    if(rc == -2 || rc == -3) {
        q_status_message1(1, 2,3, "\007Error updating address book: %s",
                          rc == -2 ? error_description(errno) : "Pine bug");
        dprint(1, (debugfile, "Error editing address book %s\n",
                   rc == -2 ? error_description(errno) : "Pine bug"));
        return(-2);
    }
    if(new != NULL) {
        where = init_addr_disp(new, addr_match);
    } else {
        where = -2; /* -2 means no resorting happened */
    }
    q_status_message(0, 1,3, "Address book edited and updated");
    dprint(2, (debugfile, "Address book edited\n"));
    return(where);

  change_cancel:
    q_status_message(0, 0,2, "\007Address book change cancelled");
    return(-1);
}



/*----------------------------------------------------------------------
    prompt user for search string and call search

    Arg: disp_list   -- The display data structure
         cur_line    -- The current line the cursor is on (search start)
         cur_col     -- The current columne the cursor is on ( "   "  )
         command_line -- The screen line to prompt on
         new_line    -- Pointer to return found entry line number in
         new_col     -- Pointer to return found entry column number in

  Result: returns 0 if found, -1 if not, -2 if cancelled
          If found the line and col are returned
 ----*/

int
search_book(disp_list, cur_line, cur_col, command_line, new_line, new_col)
     AddrScrn_Disp *disp_list;
     int            cur_line, cur_col, command_line, *new_line, *new_col;
{
    int         x, rc, oe_append;
    static char search_string[MAX_SEARCH + 1] = { '\0' };
    char        prompt[MAX_SEARCH + 50], nsearch_string[MAX_SEARCH+1];
    HelpType	help;

    dprint(4, (debugfile, "\n - search_book -\n"));

    sprintf(prompt, "Word to search for [%s] : ", search_string);
    help              = NO_HELP;
    nsearch_string[0] = '\0';
    oe_append         = 1;
    while(1) {
        rc = optionally_enter(nsearch_string, command_line, 0, MAX_SEARCH,
                              oe_append, 0, prompt, NULL, help, 0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_searchab : NO_HELP;
            continue;
        }
        if(rc != 4) /* redraw */
          break; /* no redraw */
    }

        
    if(rc == 1 || (search_string[0] == '\0' && nsearch_string[0] == '\0'))
      return(-2);

    if(nsearch_string[0] != '\0' )
      strcpy(search_string, nsearch_string);

    x = find_in_book(disp_list, cur_line, cur_col, search_string, new_line,
                     new_col);

    return(x);
}



/*----------------------------------------------------------------------
  search the incore address book for a given string

  Input: disp_list  --  The Addressbook display data structure to search 
         start_line --  Line number in display to start on
         start_col  --  Column number in display structure to start on
         string     --  String to search for
         new_line   --  Pointer to line number string was found on
         new_col    --  Pointer to column number string was found on

  Result: returns 0 if found, -1 if not
          If found the line and column are returned

Bug: Would be nice if the search wrapped around to the top.
 ---*/

int
find_in_book(disp_list, start_line, start_col, string, new_line, new_col)
  AddrScrn_Disp *disp_list;  
  char          *string;
  int            start_line, start_col, *new_line, *new_col;
{
    register AddrScrn_Disp *l;
    register int            c;
    int                     nl, nc;

    if(next_field(disp_list, start_line, start_col, &nl, &nc) == 0)
      return(-1);

    for(l = &disp_list[nl]; l->type != End; l++) {
        if(l->type == Blank)
          continue;
        for(c = nc; c < 3; c++) {
            if(c == 0 && l->type != ListEnt &&
                                srchstr(l->abe->nickname, string) != NULL)
              goto found;

            if(c == 0 && l->type == ListEnt &&
                                srchstr(l->addr_in_list, string) != NULL)
              goto found;

            if(c == 1 && l->type != ListEnt &&
                                srchstr(l->abe->fullname, string) != NULL)
              goto found;

            if(c == 2 && l->type == Single &&
                                srchstr(l->abe->addr.addr, string) != NULL)
              goto found;
        }
        nc = 0;
    }


    /*---- Now wrap the search -----*/
    for(l = disp_list; l != &disp_list[nl]; l++) {
        if(l->type == Blank)
          continue;
        for(c = nc; c < 3; c++) {
            if(c == 0 && l->type != ListEnt &&
                                srchstr(l->abe->nickname, string) != NULL)
              goto found;

            if(c == 0 && l->type == ListEnt &&
                                srchstr(l->addr_in_list, string) != NULL)
              goto found;

            if(c == 1 && l->type != ListEnt &&
                                srchstr(l->abe->fullname, string) != NULL)
              goto found;

            if(c == 2 && l->type == Single &&
                                srchstr(l->abe->addr.addr, string) != NULL)
              goto found;
        }
        nc = 0;
    }

    /* the last few entries if any in the line we're in */
    l = &disp_list[start_line];
    if(l->type == Blank)
      return(-1);
    for(c = 0; c < start_col; c++) {
        if(c == 0 && l->type != ListEnt &&
                            srchstr(l->abe->nickname, string) != NULL)
          goto found;

        if(c == 0 && l->type == ListEnt &&
                            srchstr(l->addr_in_list, string) != NULL)
          goto found;

        if(c == 1 && l->type != ListEnt &&
                            srchstr(l->abe->fullname, string) != NULL)
          goto found;

        if(c == 2 && l->type == Single &&
                            srchstr(l->abe->addr.addr, string) != NULL)
          goto found;
    }
    return(-1);

  found:
    *new_line = l - disp_list;
    *new_col  = c;
    return(0);
}
    


/*----------------------------------------------------------------------
      Add an entry to address book with no prompting. This is used by
the Take command in mailcmd.c that does the promping. It is for capturing
addresses off incoming mail.

   Args: nick  --  The nickname of entry to add (ought to be unique)
         full  --  The full name of the entry to add
         addr  --  The address for item to be added

 Result: item is added to address book
         error message queued if appropriate
         returns 0 if successful, -1 if not
 ----*/

addr_add(nick, full, addr)
     char *nick, *full, *addr;
{
    int rc;

    get_adrbk();
    if(addr_screen.address_book == NULL) {
        return(-1);
    }

    rc = adrbk_add(addr_screen.address_book, (AdrBk_Entry **)NULL, nick, full,
                   addr, Atom, ADD_FUN_NULL);

    switch(rc) {
      case 0:
        q_status_message1(0, 1, 3, "%s added. Address book updated", nick);
        dprint(3, (debugfile, "Added \"%s\",\"%s\",\"%s\": %s\n",
                         nick,full, addr, error_description(errno)));
        init_addr_disp(NULL, NULL);
        return(0);

      case -3:
      case -2:
        q_status_message1(1, 2,4, "\007Error updating address book: %s",
               rc == -2 ? error_description(errno) : "Pine bug");
        dprint(1, (debugfile, "Error adding \"%s\",\"%s\",\"%s\": %s\n",
                   nick,full, addr, rc == -2 ? error_description(errno) :
                   "Pine bug"));
        break;

      case -4:
        q_status_message(0, 2,2, "\007Tabs not allowed in address book");
        break;
    }
    return(0);
}



/*----------------------------------------------------------------------
    Simple interface to lookup for callers outside this file

   Args: nickname  -- The nickname to look up

 Result: returns NULL or the corresponding full name

This opens the address book if it hasn't been opened
 ----*/

char *
addr_lookup(nickname)
     char *nickname;
{
    AdrBk_Entry *l;

    get_adrbk();
    if(addr_screen.address_book == NULL) {
        return(NULL);
    }

    l = adrbk_lookup(addr_screen.address_book, nickname);

    return( l == NULL ? NULL : l->fullname);
}

/*
 * Warn about bad characters in address.
 *
 * Args:  string -- the address to check
 *
 * Returns: 0 - ok
 *          1 - bad chars, try again
 */
int
warn_bad_addr(string)
    char *string;
{
    int ic;

    if(!(ic = illegal_chars(string)))
      return 0;

    if(ic&0x8)
          q_status_message(0, 1, 1,
	                  "\007unbalanced quotes in addresses not allowed");
    else
          q_status_message1(0, 1, 1,
                          "\007unquoted %ss not allowed in addresses",
			  ic&0x1 ? "space" : ic&0x2 ? "comma" : "paren");

    display_message(NO_OP_COMMAND);
    sleep(1);
    return 1;
}


/*
 * Looks for illegal characters in addresses.  Don't have to worry about
 * \ quoting since addr-specs can't use that kind of quoting.
 * 
 * Args:  string -- the address to check
 *
 * Returns: 0 -- ok
 *       else, bitwise or of
 *          0x1 -- found a space
 *          0x2 -- found a comma
 *          0x4 -- found a (
 *          0x8 -- unbalanced "'s 
 *
 * (Should probably generalize this a little to take an argument telling
 *  what to look for.  I'm going to put that off for now.)
 */
int
illegal_chars(string)
  char *string;
{
  register char *p;
  register int   in_quotes = 0,
                 ret = 0;

  for(p=string; p && *p; p++) {
    if(*p == '"') {
      in_quotes = !in_quotes;
    }else if(!in_quotes) {
      switch(*p) {
	case SPACE:
	  ret |= 0x1;
	  break;
	case ',':
	  ret |= 0x2;
	  break;
	case '(':
	  ret |= 0x4;
	  break;
      }
    }
  }

  if(in_quotes)
    ret |= 0x8;

  return ret;
}
