#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: folder.c,v 4.74 1993/11/23 00:03:51 mikes Exp $";
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
     folder.c

  Screen to display and manage all the users folders

This puts up a list of all the folders in the users mail directory on
the screen spacing it nicely. The arrow keys move from one to another
and the user can delete the folder or select it to change to or copy a
message to. The dispay lets the user scroll up or down a screen full,
or search for a folder name.
 ====*/


#include "headers.h"

/*
  HEADER LINES is the number of lines in the header/title
  BOTTOM is the number in the menu and such at the bottom.
  These include all blank lines and such. 
 */
#define FOLDER_LIST_HEADER_LINES 2
#define FOLDER_LIST_BOTTOM_LINES 2
#define	FIRST_DLINE	2		/* first screen line for display */


#define	CLICKHERE	"[ Select Here to See Expanded List ]"
#define	CLICKHERETOO	"[ ** Empty List **  Select Here to Try Re-Expanding ]"
#define	ALL_FOUND(X)	(((X)->use&CNTXT_NOFIND) == 0 && \
			  ((X)->use&CNTXT_PARTFIND) == 0)



/*----------------------------------------------------------------------
   The data needed to redraw the folders screen, including the case where the 
screen changes size in which case it may recalculate the folder_display.
  ----*/

/* BUG: this strategy is doomed to fail when resize happens during 
 *      printing that refigures line numbers and such as they're
 *      stored with actual folder data (ie only one postition at a time).
 */
typedef struct folder_screen_state
{
    CONTEXT_S *context_list;		/* the single context to display */
    CONTEXT_S *context;			/* current context              */
    int        folder_index;		/* current index in context     */
    int        display_cols;		/* number of columns on display   */
    int        display_rows;		/* number of rows on display      */
    int	       top_row;			/* folder list row at top left    */
    int        last_row;		/* last row of folder list        */
} FSTATE_S;

static FSTATE_S fs;


/*
 * Global's used by context_mailbox and context_bboard to tie foldernames
 * returned by c-client into the proper folder list
 */
void       *find_folder_list;
MAILSTREAM *find_folder_stream;



void       redraw_folder_screen(void);
void       display_folder(FSTATE_S *, CONTEXT_S *, int, CONTEXT_S *, int);
void       paint_folder_name(int, FSTATE_S *, int, CONTEXT_S *);
char      *folder_list_entry(FSTATE_S *, int, int *, CONTEXT_S **);
void       folder_insert_index(FOLDER_S *, int, void *);
int        folder_total(void *);
int        off_folder_display(FSTATE_S *, int, CONTEXT_S *);
char      *add_new_folder(int, CONTEXT_S *);
char      *rename_folder(int, int, CONTEXT_S *);
int        delete_folder(int, CONTEXT_S *, int *);
int        search_folders(FSTATE_S *, int);
int        compare_names(QSType *, QSType *);
void       create_folder_display(FSTATE_S *, int);
CONTEXT_S *new_context(char *);
void      *new_folder_list(void);
void       free_folder_list(void **);
void      *find_folder_names(char *, char *);
void       free_folders_in_context(CONTEXT_S *);
FOLDER_S  *new_folder(char *);
void       folder_delete(int, void *);


static int sort_folder_entry = 0;

static struct key folder_keys[] =
    {{"?","Help",0},      {"O","OTHER CMDS",0},    {NULL,NULL, 0},
     {NULL,NULL,0},       {"P","PrevFldr",0},      {"N","NextFldr",0},
     {"-","PrevPage",0},  {"Spc","NextPage",0},    {"D","Delete",0},
     {"A","Add",0},       {"R","Rename",0},        {"W","WhereIs",0},

     {"?","Help",0},      {"O","OTHER CMDS",0},    {"Q","Quit",0},
     {"C","Compose",0},   {NULL,NULL,0},           {"G","GotoFldr",0},
     {"I","CurIndex",0},  {"W","WhereIs",0},       {NULL,NULL,0},
     {NULL,NULL,0},       {NULL,NULL,0},           {NULL,NULL,0}};
static struct key_menu folder_keymenu =
  {sizeof(folder_keys)/(sizeof(folder_keys[0])*12), 0, 0,0,0,0,folder_keys};
#define	MAIN_KEY  	2  /* Sometimes Main, sometimes Exit */
#define	SELECT_KEY	3  /* Sometimes View, sometimes Select */
#define	OTHER_KEY	1
#define	DELETE_KEY	8
#define	ADD_KEY	        9
#define	RENAME_KEY	10
#define	WHEREIS_KEY	11


/*----------------------------------------------------------------------
      Front end to folder lister when it's called from the main menu

 Args: pine_state -- The general pine_state data structure

 Result: runs folder_lister

  ----*/
void
folder_screen(pine_state)
    struct pine *pine_state;
{
    pine_state->next_screen = main_menu_screen;
    folder_lister(pine_state, FolderMaint, NULL, NULL, NULL);
    pine_state->prev_screen = folder_screen;
}






/*----------------------------------------------------------------------
      Main routine for the list of folders screen, displays and execute cmds.

  Args:  ps            -- The pine_state data structure
         do_what       -- What function we're called as -- select, maint...
         return_string -- Buffer to return selected folder name in
	 return_context -- Context that the return_string makes is applied to
	 start_context -- Context to first display

  Result: return 0 for abort, 1 for open folder, and 2 for save folder
          The name selected is copied into the given buffer

This code assume that the folder list will have one folder in it and
is likey to crash if there isn't one.

  ----*/
int 
folder_lister(ps, do_what, start_context, return_context, return_string)
     struct pine *ps;
     FolderFun    do_what;
     CONTEXT_S   *start_context;
     CONTEXT_S  **return_context;
     char        *return_string;
{
    int              ch, orig_ch, mangled_footer, mangled_header,
                     rv, quest_line, rc, old_folder_index;
    unsigned short   new_col;
    CONTEXT_S       *old_folder_context;
    char            *new_file, *cur_name, *new_fold;
    FOLDER_S        *f;
    struct key_menu *km;
    BITMAP           bitmap;
    OtherMenu        what;


    quest_line         = -3;
    old_folder_index   = -1;
    old_folder_context = NULL;
    mangled_footer     = 1;
    mangled_header     = 1;
    what               = FirstMenu;
    fs.context_list    = ps_global->context_list;
    fs.context         = start_context ? start_context 
                                       : (ps_global->context_current) 
					   ? ps_global->context_current
					   : ps_global->context_list;

    find_folders_in_context(fs.context, NULL);

    if((fs.folder_index = folder_index(ps->cur_folder,
				       fs.context->folders)) < 0)
      fs.folder_index = 0;

    ClearScreen();

    create_folder_display(&fs, ps->ttyo->screen_cols);

    ps_global->redrawer = redraw_folder_screen;

    for(ch = 'x' /* For display_message first time through */;;) {

        /*------------ New mail check ----------*/
        if(ps_global->ttyo->screen_rows > 1 &&
           new_mail(NULL, 0, ch==NO_OP_IDLE ?0 :ch==NO_OP_COMMAND ?1 :2) >=0 ){
            if(ps_global->new_current_sorted_msgno > 0) {
                ps_global->current_sorted_msgno =
                  ps_global->new_current_sorted_msgno;
                ps_global->new_current_sorted_msgno = -1L;
            }
            mangled_header = 1;
        }

        if(streams_died())
          ps_global->mangled_header = 1;

        /*----------  screen painting -------------*/
	if(mangled_header && ps_global->ttyo->screen_rows > 0) {
	    mangled_header = 0;
            switch(do_what) {
              case FolderMaint:
	        set_titlebar("FOLDER LIST", 1, FolderName, 0, 0, 0);
                break;

              case OpenFolder:
	        set_titlebar("GOTO: SELECT FOLDER", 1, FolderName, 0, 0, 0);
                break;

              case SaveMessage:
	        set_titlebar("SAVE: SELECT FOLDER", 1, MessageNumber,
			     ps->current_sorted_msgno, 0, 0);
                break;
            }
	}

	/*
	 * display_folders handles all display painting.
	 * only the first time thru (or if screen parms change)
	 * do we need to explicitly tell it to redraw.  Otherwise
	 * it handles framing the page and highlighting the current
	 * folder...
	 */
	if(old_folder_index!=fs.folder_index || old_folder_context!=fs.context)
	  display_folder(&fs, fs.context, fs.folder_index,
			 old_folder_context, old_folder_index);

	if(mangled_footer && ps_global->ttyo->screen_rows > 3) {
	    setbitmap(bitmap);
	    km = &folder_keymenu;
	    if(do_what == FolderMaint){
	      km->how_many = 2;
	      folder_keys[MAIN_KEY].name = "M";
	      folder_keys[MAIN_KEY].label = "Main Menu";
	      folder_keys[SELECT_KEY].name = "V";
	      folder_keys[SELECT_KEY].label = 
		   (fs.context->use & CNTXT_PSEUDO) ? "[Select]":"[ViewFldr]";
	      clrbitn(WHEREIS_KEY, bitmap); /* the one in the 1st menu */
	    }
	    else {
	      km->how_many = 1;
	      folder_keys[MAIN_KEY].name = "E";
	      folder_keys[MAIN_KEY].label = "ExitSelect";
	      folder_keys[SELECT_KEY].name = "S";
	      folder_keys[SELECT_KEY].label = "[Select]";
	      clrbitn(OTHER_KEY, bitmap);
	      clrbitn(DELETE_KEY, bitmap);
	      clrbitn(ADD_KEY, bitmap);
	      clrbitn(RENAME_KEY, bitmap);
	    }

	    draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
						    -2, 0, what, 0);
	    mangled_footer = 0;
	    what           = SameTwelve;
	}

        old_folder_index   = fs.folder_index;
        old_folder_context = fs.context;
	if(folder_total(fs.context->folders)){
	    f = folder_entry(fs.folder_index, fs.context->folders);
	    cur_name = (f->nickname) ? f->nickname : f->name;
	}

        /*------- display any status messages -----*/
	display_message(ch);

        /*----- Read and validate the next command ------*/
	MoveCursor(min(0, ps_global->ttyo->screen_rows - 3), 0);

	ch = read_command();
        orig_ch = ch;
        
	if(ch < 'z' && isupper(ch))
	  ch = tolower(ch);

	if(km->which == 1)
          if(ch >= PF1 && ch <= PF12)
            ch = PF2OPF(ch);

	ch = validatekeys(ch);


        /*----------- Execute command --------------*/
	switch(ch) {

            /*---------- display other key bindings ------*/
          case PF2:
          case OPF2:
          case 'o' :
            if(do_what != FolderMaint)
              goto bleep;

            if (ch == 'o')
	      warn_other_cmds();
            what = NextTwelve;
            mangled_footer++;
            break;


            /*---------------------- Key left --------------*/
	  case ctrl('B'):
	  case KEY_LEFT:
	  case PF5:
	  case 'p':
	    if(fs.folder_index > 0 && ALL_FOUND(fs.context)){
		fs.folder_index--;
	    }
	    else
	      q_status_message(0,0,1,"\007Already on first folder.");

	    break;
 

            /*--------------------- Key right -------------------*/
          case ctrl('F'): 
          case KEY_RIGHT:
	  case PF6:
	  case 'n':
          case '\t':
	    if(fs.folder_index + 1 < folder_total(fs.context->folders)
	       && ALL_FOUND(fs.context)){
		fs.folder_index++;
	    }
	    else
	      q_status_message(0,0,1,"\007Already on last folder.");

            break;


            /*--------------- Key up ---------------------*/
	  case ctrl('P'):
          case KEY_UP:
	    if(!folder_total(fs.context->folders))
	      break;

	    new_col = folder_entry(fs.folder_index, 
				   fs.context->folders)->d_col;
	    rc      = folder_entry(fs.folder_index, 
				   fs.context->folders)->d_line - 1;

	    /* find next line */
	    while(rc >= 0 && folder_list_entry(&fs, rc, &(fs.folder_index),
					      &(fs.context)))
	      rc--;

	    if(rc < 0){
		q_status_message(0,0,1,"\007Already on first line.");
		break;
	    }

	    /* find the right column on it */
	    while(folder_entry(fs.folder_index, 
			       fs.context->folders)->d_col < new_col){
		if(fs.folder_index+1 >= folder_total(fs.context->folders)
		   || folder_entry(fs.folder_index + 1,
				   fs.context->folders)->d_col == 0)
		  break;
		else
		  fs.folder_index++;
	    }

            break;


            /*----------------- Key Down --------------------*/
	  case ctrl('N'):
          case KEY_DOWN:
	    if(!folder_total(fs.context->folders))
	      break;

	    new_col = folder_entry(fs.folder_index, 
				   fs.context->folders)->d_col;
	    rc      = folder_entry(fs.folder_index, 
				   fs.context->folders)->d_line + 1;

	    if(rc > fs.last_row){
		q_status_message(0,0,1,"\007Already on last line.");
		break;
	    }

	    /* find next line */
	    while(rc <= fs.last_row && folder_list_entry(&fs, rc, 
							 &(fs.folder_index),
							 &(fs.context)))
	      rc++;

	    /* find the right column on it */
	    while(folder_entry(fs.folder_index, 
			       fs.context->folders)->d_col < new_col){
		if(fs.folder_index+1 >= folder_total(fs.context->folders)
		   || folder_entry(fs.folder_index + 1, 
				   fs.context->folders)->d_col == 0)
		  break;
		else
		  fs.folder_index++;
	    }

	    break;


            /*--------------Scroll Up ----------------------*/
          case PF7: 
	  case KEY_PGUP:
          case ctrl('Y'): 
	  case '-':
	    if(fs.top_row){
		rc = max(0, fs.top_row - fs.display_rows);

		while(folder_list_entry(&fs, rc, &(fs.folder_index), 
					&(fs.context)))
		      rc++;
	    } else {
                q_status_message(0,0,1,"\007Already on first page.");
	    }

            break;


            /*---------- Scroll screenful ------------*/
	  case PF8:
	  case KEY_PGDN:
	  case ' ': 
          case ctrl('V'): 
	  case '+':
	    if((rc = fs.top_row + fs.display_rows) > fs.last_row){
                q_status_message(0,0,1,"\007Already on last page.");
	    }
	    else{
		while(rc <= fs.last_row
		      && folder_list_entry(&fs, rc, &(fs.folder_index),
					   &(fs.context)))
		  rc++;
	    }

	    break;


            /*------------------ Help ----------------------*/
	  case PF1:
	  case OPF1:
	  case '?':
	  case ctrl('G'):
            ps_global->next_screen = SCREEN_FUN_NULL;
            ps_global->redrawer = (void (*)())NULL;
            switch(do_what) {
              case FolderMaint:
                helper(h_folder_maint, "HELP FOR FOLDERS", 0);
                break;
              case OpenFolder:
                helper(h_folder_open, "HELP FOR OPENING FOLDERS", 0);
                break;
              case SaveMessage:
                helper(h_folder_save,"HELP FOR SAVING MESSAGES TO FOLDERS", 0);
                break;
            }
            if(ps_global->next_screen != SCREEN_FUN_NULL) {
                /* So "m" to go back to main menu works */
	        return(0);
            }
            ps_global->redrawer = redraw_folder_screen;
	    mangled_header++;
	    mangled_footer++;
	    old_folder_index = -1;
	    break;


            /*---------- Select or View ----------*/
          case ctrl('M') :
          case ctrl('J') :
          case PF4:
          case 'v':
          case 's':
	    if((do_what != FolderMaint && ch == 'v') 
	       || (do_what == FolderMaint && ch == 's'))
	      goto bleep;
	    if(!folder_total(fs.context->folders)){
		q_status_message(0,2,3, 
		      	 "\007Empty folder collection.  Nothing to select!");
	    } else if(!ALL_FOUND(fs.context) 
		      || (fs.context->use & CNTXT_PSEUDO)){
		if(fs.context->use & CNTXT_PSEUDO){
		    folder_delete(0, fs.context->folders);
		    fs.context->use &= ~CNTXT_PSEUDO;
		}

		if(!folder_total(fs.context->folders))
		  fs.context->use |= CNTXT_NOFIND; /* ok to try find */

		find_folders_in_context(fs.context, NULL);

		if(fs.context == ps->context_current
		   && (fs.folder_index = folder_index(ps->cur_folder, 
						     fs.context->folders)) < 0)
		  fs.folder_index = 0;

		create_folder_display(&fs, ps_global->ttyo->screen_cols);

		old_folder_index = -1;		  /* redraw display */

		if(do_what == FolderMaint)
		  mangled_footer++;
	    } else if(do_what == FolderMaint) {
		
                /*--- Open folder ---*/
		if(cur_name == NULL)
		  break;

                if(do_broach_folder(cur_name, fs.context) == 1) {
		    free_folders_in_context(fs.context_list);

                    ps_global->redrawer = (void(*)())NULL;
                    ps_global->next_screen = mail_index_screen;
                    return(1); 
                }

                /* Open Failed. Message will be issued by do_broach_folder. */
                /* Stay here in folder lister and let the user try again. */
		mangled_footer++;
                break;
            } else {
                /*-- save message --- */
	        if(cur_name == NULL){
		    rv = 0;
	        } else {
		    if(return_context)
		      *return_context = fs.context;

		    if(return_string)
		      strcpy(return_string, cur_name);

		    rv = 1;
	        }

		free_folders_in_context(fs.context_list);

                ps_global->redrawer = (void (*)())NULL;
	        return(rv);
            }

	    break;


            /*--------- Hidden "To Fldrs" command -----------*/
	  case 'l':
	    if(do_what == FolderMaint)
	      q_status_message(0, 1,3, "\007Already in Folder List");
	    else
	      goto bleep;

	    break;

    
            /*--------- EXIT menu -----------*/
	  case PF3:
	  case 'm':
	  case 'e':
	    if((do_what != FolderMaint && ch == 'm') 
	       || (do_what == FolderMaint && ch == 'e'))
	      goto bleep;

	    ps_global->redrawer = (void (*)())NULL;
	    free_folders_in_context(fs.context_list);

	    return(0);


            /*--------- QUIT pine -----------*/
          case OPF3:
	  case 'q':
            if(do_what != FolderMaint)
                goto bleep;

	    free_folders_in_context(fs.context_list);

            ps_global->redrawer = (void (*)())NULL;
            ps_global->next_screen = quit_screen;
	    return(0);
	    

            /*--------- Compose -----------*/
          case OPF4:
	  case 'c':
            if(do_what != FolderMaint)
                goto bleep;

	    ps_global->redrawer = (void (*)())NULL;
	    free_folders_in_context(fs.context_list);

            ps_global->next_screen = compose_screen;
	    return(0);
	    

            /*--------- Message Index -----------*/
	  case OPF7:
	  case 'i':
	    if(do_what != FolderMaint)
	      goto bleep;

	    ps_global->redrawer = (void (*)())NULL;
	    free_folders_in_context(fs.context_list);

	    ps_global->next_screen = mail_index_screen;
            q_status_message(0, 1, 2, "Returning to current index");
	    return(0);


            /*----------------- Add a new folder name -----------*/
	  case PF10:
	  case 'a':
            /*--------------- Rename folder ----------------*/
	  case PF11:
	  case 'r':
	    if(do_what != FolderMaint)
	      goto bleep;

            if(ch == 'r' || ch == PF11)
	      new_file = rename_folder(quest_line,fs.folder_index,fs.context);
	    else
	      new_file = add_new_folder(quest_line, fs.context);

            if(new_file && ALL_FOUND(fs.context)) {
                /* place cursor on new folder! */
		create_folder_display(&fs, ps->ttyo->screen_cols);
		old_folder_index = -1;
		fs.folder_index = folder_index(new_file, fs.context->folders);
            }

	    mangled_footer++;
            break;
		     

            /*-------------- Delete --------------------*/
          case PF9:
	  case 'd':
	    if(do_what != FolderMaint)
	      goto bleep;

	    if(!ALL_FOUND(fs.context) || (fs.context->use & CNTXT_PSEUDO)){
		q_status_message1(0,1,3,
			    "\007No folder selected to delete.  %s list.",
			    (ALL_FOUND(fs.context)) ? "Empty" : "Expand");
		break;
	    }

            if(delete_folder(fs.folder_index, fs.context, &mangled_header)){
		/* remove from file list */
                folder_delete(fs.folder_index, fs.context->folders);

		if(fs.folder_index >= folder_total(fs.context->folders))
		  fs.folder_index = max(0, fs.folder_index - 1);

		create_folder_display(&fs, ps->ttyo->screen_cols);
                old_folder_index = -1; /* Force redraw */
            }

            mangled_footer++;
	    break;


            /*---------- Look (Search) ----------*/
	  case OPF8:
	  case PF12:
	  case 'w':
	    if((do_what != FolderMaint && ch == OPF8) 
	       || (do_what == FolderMaint && ch == PF12))
	      goto bleep;
            mangled_footer++;
            rc = search_folders(&fs, quest_line);
            if(rc == -1) {
                q_status_message(0, 0, 2,"\007Folder name search cancelled");
                break;
            }
	    else if(rc == 0) {
	        q_status_message(0, 0, 2, "\007Word not found");
                break;
            }
	    else if(rc == 2){
              q_status_message(0, 0, 2, "Search wrapped to beginning");
	    }
	    
	    break;


            /*---------- Go to Folder, ----------*/
          case OPF6:
          case 'g':
	    if(do_what != FolderMaint)
	      goto bleep;

            new_fold = broach_folder(-3, 0, &(fs.context));
            mangled_footer = 1;;
            if(new_fold){
                if(do_broach_folder(new_fold, fs.context) > 0) {
                    ps_global->redrawer = (void (*)())NULL;
                    ps_global->next_screen = mail_index_screen;
                    return(1); 
                }
            }
            break;


            /*----------no op to check for new mail -------*/
          case NO_OP_IDLE:
	  case NO_OP_COMMAND :
	    break;


            /*----------- Suspend  -- ^Z ---------------*/
          case ctrl('Z'):
            if(!have_job_control())
              goto bleep;
            do_suspend(ps);
            /* Fall through to redraw-*/


            /*------------ Redraw command -------------*/
          case KEY_RESIZE:
          case ctrl('L'):
            ClearScreen();
            mangled_footer++;
            mangled_header++;
            redraw_folder_screen();
	    if(ch == KEY_RESIZE)
	      clear_index_cache();

	    break;


            /*--------------- Invalid Command --------------*/
	  default: 
          bleep:
              q_status_message1(0, 0, 2,
	      "\007Command \"%s\" not defined for this screen. Use ? for help",
                                (void *)pretty_command(orig_ch));
	       break;
	} /* End of switch */
    }
}



/*----------------------------------------------------------------------
     Redraw the folders screen

Uses the static fs data structure so it can be called almost from
any context. It will recalculate the screen size if need be.
  ----*/
void
redraw_folder_screen(void)
{
    create_folder_display(&fs, ps_global->ttyo->screen_cols);
    display_folder(&fs, fs.context, fs.folder_index, NULL, -1);
}

                

/*----------------------------------------------------------------------
   Arrange and paint the lines of the folders directory on the screen

   Args: fd      -- The folder display structure
         lines   -- The number of folder lines to display on the screen
  
 Result: the lines are painted or repainted on the screen

    Paint folder list, or part of it.

Called to either paint or repaint the whole list, or just move the
cursor. If old_row is -1 then we are repainting. In this case have
to watch out for names that are blank as the data sometimes has blanks in
it. Go through the data row by row, column by column. Paint the item that's 
currently "it" in reverse.

When the changing the current one, just repaint the old one normal and the
new one reverse.

  ----*/
void
display_folder(fd, cntxt, index, old_cntxt, old_index)
     FSTATE_S  *fd;
     CONTEXT_S *cntxt, *old_cntxt;
     int        index, old_index;
{
    CONTEXT_S *c;
    char      *s;
    int        i, row;

    if(fd->display_rows <= 0)		/* room for display? */
      return;				/* nope. */

    /*
     * Check the framing of the current context/folder...
     */
    if(old_index < 0 || off_folder_display(fd, index, cntxt)){
	/*------------ check framing, then... -----------*/
	while(off_folder_display(fd, index, cntxt) < 0)
	  fd->top_row = max(0, fd->top_row - fd->display_rows);

	while(off_folder_display(fd, index, cntxt) > 0)
	  fd->top_row = min(fd->last_row, fd->top_row + fd->display_rows);

	/*------------ repaint entire screen _-----------*/
	for(row = 0; row < fd->display_rows; row++){
	    MoveCursor(FIRST_DLINE + row, 0);
	    CleartoEOLN();

	    if(s = folder_list_entry(fd, row + fd->top_row, &i, &c)){
		/*-------- paint text centered --------*/
		if(*s){
			PutLine0(FIRST_DLINE + row,
				 max(0, (fd->display_cols/2)-((int) strlen(s)/2)),
				 s);
		}
	    }
	    else{			/* paint folder names on row */
		do
		  paint_folder_name((i == index && c == cntxt), fd, i, c);
		while(++i < folder_total(c->folders)
		      && folder_entry(i,c->folders)->d_line==row+fd->top_row
		      && !(c->use & CNTXT_PSEUDO));
	    }
	}
    }
    else{				/* restore old name to normal */
	if(folder_total(old_cntxt->folders) 
	   && !off_folder_display(fd, old_index, old_cntxt))
	  paint_folder_name(0, fd, old_index, old_cntxt);

	if(folder_total(cntxt->folders))
	  paint_folder_name(1, fd, index, cntxt); /* and hilite the new name */
    }
    fflush(stdout);
}



/*
 * paint_folder_name - paint the folder at the given index in the given
 *                     collection.  
 */
void
paint_folder_name(hilite, fd, index, context)
    int        hilite, index;
    FSTATE_S  *fd;
    CONTEXT_S *context;
{
    FOLDER_S *f;

    if((f = folder_entry(index, context->folders)) == NULL)
      return;

    if(hilite)
      StartInverse();

    PutLine0(FIRST_DLINE + (f->d_line - fd->top_row),
	    f->d_col, f->nickname ? f->nickname : f->name);

    if(hilite)
      EndInverse();
}



/*
 * folder_list_entry - return either the string associated with the
 *                     current folder list line, or the first
 *                     folder index and context associated with it.
 *
 * NOTE: this is kind of dumb right now since it starts from the top 
 *       each time it's called.  Caching the last values would make it
 *       alot less costly.
 */
char *
folder_list_entry(fd, goal_row, index, context)
    FSTATE_S   *fd;
    int         goal_row;
    int        *index;
    CONTEXT_S **context;
{
    char     *s;
    int       row, ftotal;

    if(goal_row > fd->last_row) 	/* return blanks past last entry */
      return("");

    goal_row = max(0, goal_row);
    row      = 0;
    *index   = 0;
    *context = fs.context_list;

    while(1){
	if((ftotal = folder_total((*context)->folders))
		&& row < (int)folder_entry(0, (*context)->folders)->d_line){
	    s = "";
	}
	else{
	    s          = NULL;
	    if(*index >= ftotal		/* maybe continue where we left off? */
	       || (int)folder_entry(*index,
				    (*context)->folders)->d_line > goal_row)
	      *index = 0;

	    for(; *index < ftotal; (*index)++)
	      if(goal_row <= (int)folder_entry(*index,
					       (*context)->folders)->d_line)
		break;			/* bingo! */

	    if(*index >= ftotal){
		*index = 0;
		s = "";			/* dropped off end of list */
		break;
	    }
	}

	if(row == goal_row)
	  break;
	else
	  row++;
    }

    *index = max(0, *index);
    return(s);
}



/*
 * off_folder_display - returns: 0 if given folder is on display,
 *                               1 if given folder below display, and
 *                              -1 if given folder above display
 */
int
off_folder_display(fd, index, context)
    FSTATE_S  *fd;
    int        index;
    CONTEXT_S *context;
{
    int l;

    if(index >= folder_total(context->folders))
      return(0);			/* no folder to display */

    if((l = folder_entry(index, context->folders)->d_line) < fd->top_row)
      return(-1);
    else if(l >= (fd->top_row + fd->display_rows))
      return(1);
    else
      return(0);
}



/*----------------------------------------------------------------------
      Create a new folder

   Args: quest_line  -- Screen line to prompt on
         folder_list -- The current list of folders

 Result: returns the name of the folder created

  ----*/

char *
add_new_folder(quest_line, cntxt)
     int        quest_line;
     CONTEXT_S *cntxt;
{
    static char     add_folder[MAXFOLDER+1];
    char            prompt[100];
    HelpType        help;
    int             rc;
    MAILSTREAM     *create_stream;


    add_folder[0] = '\0';
    help          = NO_HELP;
    sprintf(prompt, "Name of folder to add : ");
    while(1){
        rc = optionally_enter(add_folder, quest_line, 0, MAXFOLDER, 1,
                               0, prompt, NULL, help, 0);

        if(rc == 0 && *add_folder) {
	    if(!cntxt->proto){		/* network folder, stream reuse ? */
		create_stream = context_same_stream(cntxt->context, add_folder,
						    ps_global->mail_stream);

		if(!create_stream 
		   && ps_global->mail_stream != ps_global->inbox_stream)
		  create_stream = context_same_stream(cntxt->context, 
						      add_folder,
						      ps_global->inbox_stream);
	    }
	    else
	      create_stream = cntxt->proto;

	    if(context_create(cntxt->context, create_stream, add_folder)){
		if(ALL_FOUND(cntxt)){
		    if(cntxt->use & CNTXT_PSEUDO){
			folder_delete(0, cntxt->folders);
			cntxt->use &= ~CNTXT_PSEUDO;
		    }

		    folder_insert(-1, new_folder(add_folder), cntxt->folders);
		}

		q_status_message1(0, 1,3,
				  "Folder \"%s\" created", add_folder);
		return(add_folder);
	    }
	    else
	      return(NULL);		/* c-client should've reported error */
        }

        if(rc == 3) {
            help = help == NO_HELP ? h_oe_foldadd : NO_HELP;
            continue;
        }

	if(rc == 1 || add_folder[0] == '\0') {
	    q_status_message(0,0,2, "\007Addition of new folder cancelled");
	    return(NULL);
	}
    }
}





/*----------------------------------------------------------------------
      Rename folder
  
   Args: q_line     -- Screen line to prompt on
         index      -- index of folder in folder list to rename
         cntxt      -- collection of folders making up folder list

 Result: returns the new name of the folder, or NULL if nothing happened.

 When either the sent-mail or saved-message folders are renamed, immediately 
create a new one in their place so they always exist. The main loop above also
detects this and makes the rename look like an add of the sent-mail or
saved-messages folder. (This behavior may not be optimal, but it keeps things
consistent.

  ----*/
char *
rename_folder(q_line, index, cntxt)
     int        q_line, index;
     CONTEXT_S *cntxt;
{
    static char  new_foldername[MAXFOLDER+1];
    char        *folder, *prompt;
    HelpType     help;
    int          rc, ren_cur;
    FOLDER_S	*new_f;
    MAILSTREAM  *ren_stream = NULL;


    if(!ALL_FOUND(cntxt) || (cntxt->use & CNTXT_PSEUDO)){
	q_status_message1(0,1,3, 
			 "\007No folder selected to rename.  %s list.",
			  (ALL_FOUND(cntxt)) ? "Empty" : "Expand");
	return(NULL);
    }
    else if((new_f = folder_entry(index, cntxt->folders))
	    && strucmp(new_f->nickname ? new_f->nickname : new_f->name,
		       ps_global->inbox_name) == 0) {
        q_status_message1(1,2,4,"\007Can't change special folder name \"%s\"",
			  ps_global->inbox_name);
        return(NULL);
    }
    else if(new_f->nickname){ 
	q_status_message(0,2,3, 
			 "\007Can't rename folder nicknames at this time!");
	return(NULL);
    }

    folder  = new_f->name;
    ren_cur = strcmp(folder, ps_global->cur_folder) == 0;

    prompt = "Change name of folder: ";
    help   = NO_HELP;
    strcpy(new_foldername, folder);
    while(1) {
        rc = optionally_enter(new_foldername, q_line, 0, MAXFOLDER, 1, 0,
                              prompt, NULL, help, 0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_foldrename : NO_HELP;
            continue;
        }

        if(rc == 0 && *new_foldername) {
	    if(folder_index(new_foldername, cntxt->folders) >= 0){
                q_status_message1(1, 1,3, "Folder \"%s\" already exists",
                                  pretty_fn(new_foldername));
                display_message(NO_OP_COMMAND);
                sleep(1);
                continue;
            }
        }

        if(rc != 4) /* redraw */
          break;  /* no redraw */

    }

    removing_trailing_white_space(new_foldername);
    removing_leading_white_space(new_foldername);

    if(rc==1 || new_foldername[0]=='\0' || strcmp(new_foldername, folder)==0){
        q_status_message(0, 0,2, "\007Folder rename cancelled");
        return(0);
    }

    if(ren_cur && ps_global->mail_stream != NULL) {
        mail_close(ps_global->mail_stream);
        ps_global->mail_stream = NULL;
    }

    ren_stream = context_same_stream(cntxt->context, new_foldername,
				     ps_global->mail_stream);

    if(!ren_stream && ps_global->mail_stream != ps_global->inbox_stream)
      ren_stream = context_same_stream(cntxt->context, new_foldername,
				       ps_global->inbox_stream);
      
    if(rc = context_rename(cntxt->context,ren_stream,folder,new_foldername)){
	/* insert new name */
	new_f               = new_folder(new_foldername);
	new_f->prefix[0]    = '\0';
	new_f->msg_count    = 0;
	new_f->unread_count = 0;
	folder_insert(-1, new_f, cntxt->folders);

	if(strcmp(ps_global->VAR_DEFAULT_FCC, folder) == 0
	   || strcmp(DEFAULT_SAVE, folder) == 0) {
	    /* renaming sent-mail or saved-messages */
	    if(context_create(cntxt->context, cntxt->proto, folder)){
		q_status_message3(0,1,3,
		     "Folder \"%s\" renamed to \"%s\". New \"%s\" created",
				  folder, new_foldername,
				  pretty_fn(ps_global->VAR_DEFAULT_FCC));

	    }
	    else{
		if((index = folder_index(folder, cntxt->folders)) >= 0)
		  folder_delete(index, cntxt->folders); /* delete old struct */

		q_status_message1(1, 2, 4, "\007Error creating new \"%s\"",
				  folder);

	    }
	}
	else{
	    q_status_message2(0, 1, 3, "Folder \"%s\" renamed to \"%s\"",
			      pretty_fn(folder), pretty_fn(new_foldername));

	    if((index = folder_index(folder, cntxt->folders)) >= 0)
	      folder_delete(index, cntxt->folders); /* delete old struct */
	}
    }

    if(ren_cur) {
        /* No reopen the folder we just had open */
        do_broach_folder(new_foldername, cntxt);
    }

    return(rc ? new_foldername : NULL);
}



/*----------------------------------------------------------------------
   Confirm and delete a folder

   Args: index -- Index of folder in collection to remove
         cntxt -- The particular collection the folder's to be remove from
         mangled_header -- Pointer to flag to set if the the anchor line
                           needs updating (deleted the open folder)

 Result: return 0 if not delete, 1 if deleted.

 NOTE: Currently disallows deleting open folder...
  ----*/
int
delete_folder(index, cntxt, mangled_header)
    int        index, *mangled_header;
    CONTEXT_S *cntxt;
{
    char       *folder, ques_buf[100];
    MAILSTREAM *del_stream = NULL;
    FOLDER_S   *fp;
    int         ret, close_opened = 0;

    if(!folder_total(cntxt->folders)){
	q_status_message(0,2,4,
			 "\007Empty folder collection.  No folder to delete!");
	return(0);
    }

    fp     = folder_entry(index, cntxt->folders);
    folder = fp->nickname ? fp->nickname : fp->name;

    if(strucmp(folder, ps_global->inbox_name) == 0) {
	q_status_message1(1, 2, 4, "\007Can't delete special folder \"%s\".",
			  ps_global->inbox_name);
	return(0);
    }
    else if(cntxt == ps_global->context_current
	    && strcmp(folder, ps_global->cur_folder) == 0)
      close_opened++;

    sprintf(ques_buf, "Really delete \"%s\"%s", folder, 
	    close_opened ? " (the currently open folder)" : "");

    if((ret=want_to(ques_buf, 'n', 'x', NO_HELP, 0)) != 'y'){
	q_status_message(0,1,3, (ret == 'x') ? "\007Delete cancelled" 
			 		     : "No folder deleted");
	return(0);
    }


    /*
     * Use fp->name since "folder" may be a nickname...
     */
    if(close_opened){
	/*
	 * There *better* be a stream, but check just in case.  Then
	 * close it, NULL the pointer, and let do_broach_folder fixup
	 * the rest...
	 */
	if(ps_global->mail_stream){
	    mail_close(ps_global->mail_stream);
	    ps_global->mail_stream = NULL;
	    do_broach_folder(ps_global->inbox_name, ps_global->context_list);
	}
    }
    else
      del_stream = context_same_stream(cntxt->context, fp->name,
				       ps_global->mail_stream);

    if(!del_stream && ps_global->mail_stream != ps_global->inbox_stream)
      del_stream = context_same_stream(cntxt->context, fp->name,
				       ps_global->inbox_stream);

    if(!context_delete(cntxt->context, del_stream, fp->name)){
/*
 * BUG: what if sent-mail or saved-messages????
 */
	q_status_message1(0,1,3,"Delete of \"%s\" Failed!", folder);
	return(0);
    }

    q_status_message1(0, 1,3,"Folder \"%s\" deleted!", folder);
    return(1);
}




                     

/*----------------------------------------------------------------------
  Search folder list

   Args: fd       -- The folder display structure
         index    -- pointer to index of current folder (new folder if found)
         context  -- pointer to context of current folder (new folder if found)
         ask_line -- Screen line to prompt on

 Result: returns 
                -1 if aborted
                 0 if NOT found
		 1 if found
		 2 if found and wrapped
  ----------------------------------------------------------------------*/
int
search_folders(fd, ask_line)
    FSTATE_S   *fd;
    int         ask_line;
{
    char            prompt[MAX_SEARCH+50], nsearch_string[MAX_SEARCH+1];
    static char     search_string[MAX_SEARCH+1];
    HelpType        help;
    FOLDER_S       *f;
    int             rc, t_index;

    help              = NO_HELP;
    nsearch_string[0] = '\0';
    if(!folder_total(fd->context->folders)){
	q_status_message(0,2,4,
			"\007Empty folder collection.  No folders to search!");
	return(0);
    }

    t_index           = fd->folder_index;
    sprintf(prompt, "Folder name to search for %s%s%s: ", 
	    (*search_string == '\0') ? "" : "[", 
	    search_string,
	    (*search_string == '\0') ? "" : "] ");

    while(1) {
        rc = optionally_enter(nsearch_string, ask_line, 0, MAX_SEARCH, 1,
                               0, prompt, NULL, help,0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_foldsearch : NO_HELP;
            continue;
        }
        if(rc != 4)
          break;
    }

    if(rc == 1 || (search_string[0] == '\0' && nsearch_string[0] == '\0'))
      return(-1);			/* abort */

    if(nsearch_string[0] != '\0')
      strcpy(search_string, nsearch_string);

    /*----- Search the bottom half of list ------*/
    rc = 0;
    while(1){
	if(t_index + 1 >= folder_total(fd->context->folders)){
	    t_index = 0;
	    rc = 1;			/* wrapped the search */
	}
	else
	  t_index++;

	if(t_index == fd->folder_index)
	  return(0);

	f = folder_entry(t_index, fd->context->folders);
        if(srchstr((f->nickname) ? f->nickname : f->name,
		   search_string)){
	    fd->folder_index   = t_index;
	    return(rc + 1);
	}
    }
}

    


/*----------------------------------------------------------------------
      compare two names for qsort, case independent

   Args: pointers to strings to compare

 Result: integer result of strcmp of the names.  Uses simple 
         efficiency hack to speed the string comparisons up a bit.

  ----------------------------------------------------------------------*/
int
compare_names(x, y)
     QSType *x, *y;
{
    char *a = *(char **)x, *b = *(char **)y;
    int r;
#define	STRCMP	strcmp
#define	CMPI(X,Y)	((X)[0] - (Y)[0])
#define	UCMPI(X,Y)	((isupper((X)[0]) ? (X)[0] - 'A' + 'a' : (X)[0])  \
			 - (isupper((Y)[0]) ? (Y)[0] - 'A' + 'a' : (Y)[0]))

    /*---- Inbox always sorts to the top ----*/
    if((UCMPI(b, ps_global->inbox_name)) == 0
       && strucmp(b, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : 1);
    else if(CMPI(b, ps_global->VAR_DEFAULT_FCC) == 0
	    && STRCMP(b, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : 1);
    else if(CMPI(b, DEFAULT_SAVE) == 0
	    && STRCMP(b, DEFAULT_SAVE) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : 1);
    else if(UCMPI(a, ps_global->inbox_name) == 0
	    && strucmp(a, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : -1);
    /*----- The sent-mail folder, is always next ---*/
    else if(CMPI(a, ps_global->VAR_DEFAULT_FCC) == 0
	    && STRCMP(a, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : -1);
    /*----- The saved-messages folder, is always next ---*/
    else if(CMPI(a, DEFAULT_SAVE) == 0
	    && STRCMP(a, DEFAULT_SAVE) == 0)
      return((CMPI(a, b) == 0 && STRCMP(a, b) == 0) ? 0 : -1);
    else
      return((r = CMPI(a, b)) ? r : STRCMP(a, b));
}



/*----------------------------------------------------------------------
  This code calculate the screen arrangement for the folders screen.
It fills in the line and col number for each entry in the list.

Args: f_list  -- The folder list
      f_list_size -- The number of entries in the folder list 
      screen_cols -- The number of columns on the screen


BUG - test this with one, two and three folders
Returns: The folder display structure
  ----*/


/*----------------------------------------------------------------------
   Calculate the arrangement on the screen. This fills in the rwo and column
 in the struct folders in the global Pine folder list. 

Args: fold_disp  -- folder menu state 
      screen_cols -- The width of the screen

Names are passed pre-sorted.
  ---*/

void
create_folder_display(fold_disp, screen_cols)
     FSTATE_S *fold_disp;
     int       screen_cols;
{
    register int       index;
    register FOLDER_S *f;
    CONTEXT_S         *c_list;
    int                length = 0, row, col, goal;

    if(!fold_disp){			/* what? */
	q_status_message(0,2,2,"Programmer BOTCH: No folder state struct!");
	return;
    }

    row                      = 0;
    c_list                   = fold_disp->context_list;
    fold_disp->top_row       = 0;
    fold_disp->display_cols  = screen_cols;
    fold_disp->display_rows  = ps_global->ttyo->screen_rows
                                 - FOLDER_LIST_BOTTOM_LINES - 1
                                 - FOLDER_LIST_HEADER_LINES;

    /*---
      Figure out the column width to use for display. What we want is a
      column width that looks nice for most of the folder names, but
      not to make the columns super wide because of a few folder names
      of exceptional length. This is done by sorting the lengths of the
      existing folders and using a width that will suit 95% of the
      entries.

      The miminum columns width used is 20.
     ----*/
    if(!ALL_FOUND(c_list)){
	length = strlen(CLICKHERE);
    }
    else if(!folder_total(c_list->folders) || (c_list->use&CNTXT_PSEUDO)){
	length = strlen(CLICKHERETOO);
    }
    else{
	length = 0;		/* start from scratch */
	for(index = 0; index < folder_total(c_list->folders); index++){
	    length = max(length,
			 (int)folder_entry(index,
					   c_list->folders)->name_len + 1);
	}
    }

    length = max(20, length);

    /*----
      Now we go through the display list and fill in the rows and columns.
      If the entry is just a text entry then it is centered. If it is
      a folder name it is fit in after the preceed one if possible. If
      not it is put on the line boundrieds
     ---*/

    /* then assign positions for each folder name */
    col = 0;
    if(!ALL_FOUND(c_list)){
	if(c_list->use & CNTXT_PSEUDO){
	    f = folder_entry(0, c_list->folders);
	}
	else{
	    f = new_folder(CLICKHERE);
	    folder_insert(0, f, c_list->folders);
	    c_list->use |= CNTXT_PSEUDO;
	}

	f->d_line = row;
	f->d_col  = max(0, (screen_cols - (int)f->name_len)/2);
    }
    else if(!folder_total(c_list->folders) || (c_list->use&CNTXT_PSEUDO)){
	if(c_list->use & CNTXT_PSEUDO)
	  folder_delete(0, c_list->folders); /* may be CLICKHERE */

	f = new_folder(CLICKHERETOO);
	folder_insert(0, f, c_list->folders);
	c_list->use |= CNTXT_PSEUDO; /* let others know entry's bogus */
	f->d_line    = row;
	f->d_col     = max(0, (screen_cols - (int)f->name_len)/2);
    }
    else{
	for(index = 0; index < folder_total(c_list->folders); index++){
	    f = folder_entry(index, c_list->folders);

	    if(col + (int)f->name_len >= screen_cols){
		col = 0;
		row++;
	    }

	    f->d_line = (unsigned int) row;
	    f->d_col  = (unsigned int) col;

	    for(goal = 0; goal < (int) f->name_len; goal += length)
	      col += length;
	}
    }

    fold_disp->last_row = row;
}



/*----------------------------------------------------------------------
      Format the given folder name for display for the user

   Args: folder -- The folder name to fix up

Not sure this always makes it prettier. It could do nice truncation if we
passed in a length. Right now it adds the path name of the mail 
subdirectory if appropriate.
 ----*/
      
char *
pretty_fn(folder)
     char *folder;
{
    static char  pfn[MAXFOLDER * 2 + 1];
    char        *p;

    if(!ps_global->show_folders_dir || *folder == '/' || *folder == '~' ||
       *folder == '{' || *folder == '\0'
       || !strucmp(folder, ps_global->inbox_name))  {
	if(!strucmp(folder, ps_global->inbox_name)){
	    strcpy(pfn, ps_global->inbox_name);
	    return(pfn);
	}
	else
          return(folder);

    } else {
        build_path(pfn, ps_global->VAR_MAIL_DIRECTORY, folder);
        return(pfn);
    }
}



/*----------------------------------------------------------------------
       Check to see if folder exists in given context

  Args: c_string -- context to check for folder in
        file  -- name of folder to check
 
 Result: returns 1 if the folder exists
                 0 if not

  Uses mail_find to sniff out the existance of the requested folder.
  The context string is just here for convenience.  Checking for
  folder's existance within a given context is probably more efficiently
  handled outside this function for now using find_folders_in_context().

    ----*/
int
folder_exists(c_string, file)
    char *c_string, *file;
{
    char         fn[MAXPATH+1], *find_string;
    int          rv = 0;
    MAILSTREAM  *find_stream = NULL;

    if(c_string && *c_string && context_isambig(file))
      context_apply(fn, c_string, file);
    else {
	/*
	 * file is a caller-typed folder/Fcc name with no length cap
	 * upstream (e.g. straight from the composer's Fcc field) -- a
	 * name too long to fit fn can't be a real folder anyway, so
	 * reject rather than overflow.
	 */
	if(strlen(file) >= sizeof(fn))
	  return(0);
	strcpy(fn, file);
    }

    find_stream = mail_open(NULL, fn, OP_PROTOTYPE);
    find_string = fn;

    find_folder_list = new_folder_list();

    context_find_all(NULL, find_stream, find_string);
    rv = folder_total(find_folder_list);

    free_folder_list(&find_folder_list);

    return(rv);
}



/*----------------------------------------------------------------------
 Initialize global list of contexts for folder collections.

 Interprets collections defined in the pinerc and orders them for
 pine's use.  Parses user-provided context labels and sets appropriate 
 use use flags and the default prototype for that collection. 
 (See find_folders for how the actual folder list is found).

  ----*/
void
init_folders(void)
{
    CONTEXT_S  *tc;
    FOLDER_S   *f;

    if(!ps_global->VAR_FOLDER_SPEC){		/* shouldn't happen */
	q_status_message(0,2,2,"No Folder Collections Specified");
	return;
    }

    if(!ps_global->VAR_FOLDER_SPEC[0]){
	/*
	 * THIS MAY NEED TUNING!  In the absence of any other info,
	 * assume all opens and saves are done relative to the
	 * home directory.  So, insert such a context here...
	 */
	ps_global->VAR_FOLDER_SPEC[0] = cpystr("[]");
    }

    /*
     * This build supports exactly one folder collection; only the
     * first folder-collections value is used.  new_context() has the
     * job of parsing it, and writes any bogus format messages.
     */
    if(tc = new_context(ps_global->VAR_FOLDER_SPEC[0])){
	tc->proto   = default_driver(tc->context);
	tc->folders = new_folder_list();

	/* associate the name "INBOX" with the collection */
	if(ps_global->VAR_INBOX_PATH){
	    f = new_folder(ps_global->VAR_INBOX_PATH);
	    f->nickname = cpystr(ps_global->inbox_name);
	    f->name_len = strlen(f->nickname);
	}
	else
	  f = new_folder(ps_global->inbox_name);

	folder_insert(-1, f, tc->folders);
    }

    ps_global->context_list    = tc;	/* init pointers */
    ps_global->context_current = tc;
}




/*
 * new_context - creates and fills in a new context structure, leaving
 *               blank the actual folder list (to be filled in later as
 *               needed).  Also, parses the context string provided 
 *               picking out any user defined label.  Context lines are
 *               of the form:
 *
 *               [ ["] <string> ["] <white-space>] <context>
 *
 */
CONTEXT_S *
new_context(cntxt_string)
    char *cntxt_string;
{
    CONTEXT_S  *c;
    int         quoted = 0;
    char        rcontext[MAXPATH], *p, *nickname = NULL;

    /*
     * do any context string parsing (like splitting user-supplied 
     * label from actual context)...
     */
    for(p = cntxt_string; *p ; p++){
	if(*p == '"')			/* quoted label? */
	  quoted = (quoted) ? 0 : 1;

	if(*p == '\\' && *(p+1) == '"')	/* escaped quote? */
	  p++;				/* skip it... */

	if(isspace(*p) && !quoted){	/* space delimits label & context */
	    char *p1;
	    nickname = p1 = (char *)fs_get(((p-cntxt_string)+1)*sizeof(char));
	    for(; cntxt_string < p ; cntxt_string++){
		if(*cntxt_string == '\\' && *(cntxt_string+1) == '"')
		  *p1++ = *++cntxt_string;
		else if(*cntxt_string != '"')
		  *p1++ = *cntxt_string;
	    }

	    *p1 = '\0';			/* tie off nickname */
	    while(isspace(*p))		/* find start of context */
	      p++;

	    cntxt_string = p;		/* point to context start */
	    break;			/* pop out of here */
	}
    }

    if(p = context_digest(cntxt_string, NULL, rcontext, NULL)){
	q_status_message2(0,3,4,"\007Bad context, %s : %s", p, cntxt_string);
	if(nickname)
	  fs_give((void **)&nickname);

	return(NULL);
    }

    c = (CONTEXT_S *) fs_get(sizeof(CONTEXT_S)); /* get new context   */
    memset((void *) c, 0, sizeof(CONTEXT_S));    /* and initialize it */

    if(c->label[0] == NULL)
      c->label[0] = nickname ? nickname : cpystr(rcontext);

    c->context        = cpystr(cntxt_string);
    c->use            = CNTXT_NOFIND;		/* do find later! */


    return(c);
}



/*
 * find_folders_in_context - find the folders associated with the given context
 *                     and search pattern.  If the search pattern is not
 *                     the wild card ("*"), then some subset of all folders
 *                     is specified. So, don't mark context has completely
 *                     searched, but do set the bit to avoid recursive
 *                     calls...
 *
 *        If no search_string, we're being called to search the entire
 *        view within the given context.
 *
 */
void
find_folders_in_context(context, search_string)
    CONTEXT_S *context;
    char      *search_string;
{
    char  rcontext[MAXPATH], view[MAXPATH],
         *search_context, *rv;

    if((context->use&CNTXT_NOFIND) == 0 || (context->use&CNTXT_PARTFIND))
      return;				/* find already done! */

    if(rv = context_digest(context->context, NULL, rcontext, view)){
	q_status_message2(0,2,2,"\007Bad context, %s : %s", rv,
			  context->context);
	return;
    }

    if(!search_string || (search_string[0]=='*' && search_string[1]=='\0')){
	context->use &= ~CNTXT_NOFIND;	/* let'em know we tried  */
	search_string = view;		/* return full view of context */
    }
    else
      context->use |= CNTXT_PARTFIND;	/* or are in a partial find */

    /* let context_mailbox know context! */
    current_context   = context->context;
    find_folder_list  = context->folders;
    sort_folder_entry = 1;


    /*
     * Here's where we have to be careful.  C-client requires different
     * search strings based and inconsistently returns results based on 
     * what sort of technology you're interested in... (would be nice to 
     * see this cleaned up)
     *
     *     Type      Search           Search returns
     *     ----      ------           --------------
     * local mail    mail/*           /usr/staff/mikes/mail/foo
     * local news    *                foo.bar (if news spool local!)
     *
     */

    find_folder_stream = NULL;
    search_context = rcontext;

    /*
     * For now use find_all as there's no distinguishing in pine
     * between subscribed and not with regard to mailboxes.  At some
     * point we may want to use the subscription mechanism to help
     * performance, but implicit subscription needs to be added and
     * folders created outside pine won't be automatically visible
     *
     * THOUGHT: the first find as pine starts does a find_all and then
     * a find then reconciles the two automatically subscribing
     * any new folders.  We can then easily rebuild folder list on
     * every folder menu access much cheaper (locally anyway)
     * (probably only necessary to rebuild the section that has
     * to do with the current context)
     */
    context_find_all(search_context, find_folder_stream, search_string);

    context->use &= ~CNTXT_PARTFIND;	/* unset partial find bit */
}



/*
 * free_folders_in_context - loop thru the context's lists of folders
 *                     clearing all entries without nicknames
 *                     (as those were user provided) AND reset the 
 *                     context's find flag.
 *
 * NOTE: if fetched() information (e.g., like message counts come back
 *       in bboard collections), we may want to have a check before
 *       executing the loop and setting the FIND flag.
 */
void
free_folders_in_context(cntxt)
    CONTEXT_S *cntxt;
{
    int i, j, total = folder_total(cntxt->folders);

    j = 0;				/* current niknamed folder */
    for(i = 0; i < total; i++){
	if(folder_entry(j, cntxt->folders)->nickname)
	  j++;
	else
	  folder_delete(j, cntxt->folders);
    }

    cntxt->use |= CNTXT_NOFIND;		/* do find next time...  */
    cntxt->use &= ~CNTXT_PSEUDO;	/* or add the fake entry */
}





/*
 * folder_complete - foldername completion routine
 *              returns:
 *
 *   Result: returns 0 if the folder doesn't have a unique completetion
 *                   1 if so, and replaces name with completion
 */
int
folder_complete(context, name)
    CONTEXT_S *context;
    char      *name;
{
    int  i, match = -1, did_find;
    char tmp[MAXFOLDER], tmpname[MAXFOLDER], *a, *b; 
    FOLDER_S *f;
#define	STRUCMP	strncmp

    if(*name == '\0' || !context_isambig(name))
      return(0);

    if(did_find = !ALL_FOUND(context)){
	sprintf(tmpname, "%s*", name);
	find_folders_in_context(context, tmpname);
    }
    else if(context->use & CNTXT_PSEUDO)
      return(0);			/* no folders to complete */

    *tmp = '\0';			/* find uniq substring */
    for(i = 0; i < folder_total(context->folders); i++){
	f = folder_entry(i, context->folders);
	if(STRUCMP(name,f->nickname ? f->nickname:f->name,strlen(name)) == 0){
	    if(match != -1){		/* oh well, do best we can... */
		a = f->nickname ? f->nickname : f->name;
		if(match >= 0){
		    f = folder_entry(match, context->folders);
		    strcpy(tmp, f->nickname ? f->nickname : f->name);
		}

		match = -2;
		b     = tmp;		/* remember largest common text */
		while(*a && *b && *a == *b)
		  *b++ = *a++;

		*b = '\0';
	    }
	    else		
	      match = i;		/* bingo?? */
	}
    }

    if(match >= 0){			/* found! */
	f = folder_entry(match, context->folders);
	strcpy(name, f->nickname ? f->nickname : f->name);
    }
    else if(match == -2)		/* closest we could find */
      strcpy(name, tmp);

    if(did_find){
	if(context->use & CNTXT_PSEUDO){
	    while(folder_total(context->folders) > 1)
	      folder_delete(strcmp(folder_entry(0, context->folders)->name,
				   CLICKHERE) ? 0 : 1,
			    context->folders);
	}
	else
	  free_folders_in_context(context);
    }

    return((match >= 0) ? 1 : 0);
}


/*
 *           FOLDER MANAGEMENT ROUTINES
 */


/*
 * Folder List Structure - provides for two ways to access and manage
 *                         folder list data.  One as an array of pointers
 *                         to folder structs or
 */
typedef struct folder_list {
    unsigned   used;
    unsigned   allocated;
    FOLDER_S **folders;		/* array of pointers to folder structs */
} FLIST;

#define FCHUNK  64


/*
 * new_folder_list - return a piece of memory suitable for attaching 
 *                   a list of folders...
 *
 */
void *
new_folder_list(void)
{
    FLIST *flist = (FLIST *) fs_get(sizeof(FLIST));
    flist->folders = (FOLDER_S **) fs_get(FCHUNK * sizeof(FOLDER_S *));
    memset((void *)flist->folders, 0, (FCHUNK * sizeof(FOLDER_S *)));
    flist->allocated = FCHUNK;
    flist->used      = 0;
    return((void *)flist);
}



/*
 * new_folder - return a brand new folder entry, with the given name
 *              filled in.
 *
 * NOTE: THIS IS THE ONLY WAY TO PUT A NAME INTO A FOLDER ENTRY!!!
 * STRCPY WILL NOT WORK!!!
 */
FOLDER_S *
new_folder(name)
    char *name;
{
    FOLDER_S *tmp;
    size_t    l = strlen(name);

    tmp = (FOLDER_S *)fs_get(sizeof(FOLDER_S) + (l * sizeof(char)));
    memset((void *)tmp, 0, sizeof(FOLDER_S));
    strcpy(tmp->name, name);
    tmp->name_len = (unsigned char) l;
    return(tmp);
}



/*
 * folder_entry - folder struct access routine.  Permit reference to
 *                folder structs via index number. Serves two purposes:
 *            1) easy way for callers to access folder data 
 *               conveniently
 *            2) allows for a custom manager to limit memory use
 *               under certain rather limited "operating systems"
 *               who shall renameless, but whose initials are DOS
 *
 *
 */
FOLDER_S *
folder_entry(i, flist)
    int   i;
    void *flist;
{
    return((i >= ((FLIST *)flist)->used) ? NULL:((FLIST *)flist)->folders[i]);
}



/*
 * free_folder_list - release all resources associated with the given 
 *                    folder list
 */
void
free_folder_list(flist)
    void **flist;
{
    register int i = (*((FLIST **)flist))->used;

    while(i--)
      fs_give((void **)&((*((FLIST **)flist))->folders[i]));

    fs_give((void **)&((*((FLIST **)flist))->folders));
    fs_give(flist);
}



/*
 * return the number of folders associated with the given folder list
 */
int
folder_total(flist)
    void *flist;
{
    return((int)((FLIST *)flist)->used);
}


/*
 * return the index number of the given name in the given folder list
 */
int
folder_index(name, flist)
    char *name;
    void *flist;
{
    register  int i = 0;
    FOLDER_S *f;
    char     *fname;

    while(f = folder_entry(i, flist)){
	fname = (f->nickname) ? f->nickname : f->name;
	if(*name == *fname && strcmp(name, fname) == 0)
	  return(i);
	else
	  i++;
    }

    return(-1);
}



/*
 * folder_is_nick - check to see if the given name is a nickname
 *                  for some folder in the given context...
 *
 *  NOTE: no need to check if find_folder_names has been done as 
 *        nicknames can only be set by configuration...
 */
char *
folder_is_nick(nickname, flist)
    char *nickname;
    void *flist;
{
    register  int  i = 0;
    FOLDER_S *f;

    while(f = folder_entry(i, flist)){
	if(f->nickname && strcmp(nickname, f->nickname) == 0)
	  return(f->name);
	else
	  i++;
    }

    return(NULL);
}



/*----------------------------------------------------------------------
  Insert the given folder name into the sorted folder list
  associated with the given context.  Only allow ambiguous folder
  names IF associated with a nickname.

   Args: index  -- Index to insert at, OR insert in sorted order if -1
         folder -- folder structure to insert into list
	 flist  -- folder list to insert folder into

  **** WARNING ****
  DON'T count on the folder pointer being valid after this returns
  *** ALL FOLDER ELEMENT READS SHOULD BE THRU folder_entry() ***

  ----*/
int
folder_insert(index, folder, flist)
    int       index;
    FOLDER_S *folder;
    void     *flist;
{
    int i;

    if(index < 0){			/* add to sorted list */
	char     *sortname, *fsortname;
	FOLDER_S *f;

	sortname = folder->nickname ? folder->nickname : folder->name;
	index    = 0;
	while(f = folder_entry(index, flist)){
	    fsortname = f->nickname ? f->nickname : f->name;
	    if((i = compare_names(&sortname, &fsortname)) < 0)
	      break;
	    else if(i == 0)		/* same folder? just return index */
	      return(index);
	    else
	      index++;
	}
    }

    folder_insert_index(folder, index, flist);
    return(index);
}


/* 
 * folder_insert_index - Insert the given folder struct into the global list
 *                 at the given index.
 */
void
folder_insert_index(folder, index, flist)
    FOLDER_S *folder;
    int       index;
    void     *flist;
{
    register FOLDER_S **flp, **iflp;

    /* if index is beyond size, place at end of list */
    index = min(index, ((FLIST *)flist)->used);

    /* grow array ? */
    if(((FLIST *)flist)->used + 1 > ((FLIST *)flist)->allocated){
	((FLIST *)flist)->allocated += FCHUNK;
	fs_resize((void **)&(((FLIST *)flist)->folders),
		  (((FLIST *)flist)->allocated) * sizeof(FOLDER_S *));
    }

    /* shift array left */
    iflp = &(((FOLDER_S **)((FLIST *)flist)->folders)[index]);
    for(flp = &(((FOLDER_S **)((FLIST *)flist)->folders)[((FLIST *)flist)->used]); 
	flp > iflp; flp--)
      flp[0] = flp[-1];

    ((FLIST *)flist)->folders[index] = folder;
    ((FLIST *)flist)->used          += 1;
}


/*----------------------------------------------------------------------
    Removes a folder at the given index in the given context's
    list.

Args: index -- Index in folder list of folder to be removed
      flist -- folder list
 ----*/
void
folder_delete(index, flist)
    int   index;
    void *flist;
{
    register int  i;
    FOLDER_S     *f;

    if(((FLIST *)flist)->used 
       && (index < 0 || index >= ((FLIST *)flist)->used))
      return;				/* bogus call! */

    if((f = folder_entry(index, flist))->nickname)
      fs_give((void **)&(f->nickname));

    fs_give((void **)&(((FLIST *)flist)->folders[index]));
    for(i = index; i < ((FLIST *)flist)->used - 1; i++)
      ((FLIST *)flist)->folders[i] = ((FLIST *)flist)->folders[i+1];


    ((FLIST *)flist)->used -= 1;
}
