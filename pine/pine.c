#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pine.c,v 4.134 1993/12/06 17:26:11 mikes Exp $";
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

#include "headers.h"


#define VERSION "3.89"

extern DRIVER bezerkdriver;

char         version_buff[50];
struct pine *ps_global;               /* sort of THE global variable */
int          timeout = NEW_MAIL_TIME; /* referenced in weemacs */


/*----------------------------------------------------------------------
  General use big buffer. It is used in the following places:
    compose_mail:    while parsing header of postponed message
    append_message2: while writing header into folder
    q_status_messageX: while doing printf formatting
    pine.c: When address specified on command line
    init.c: When expanding variable values
    send.c: while building the outgoing RFC822 header/address list

  Dynamically sized instead of a fixed 20K: most uses are short
  (a status line, a single address, a config value) and fit well
  within TMP_20K_BUF_BASELINE, but the RFC822 header/address-list
  builders in send.c have no fixed upper bound (arbitrarily many
  recipients), so they call grow_tmp_20k_buf() with a size estimate
  before writing into it instead of assuming a fixed capacity is
  always big enough.
 ----*/
#define TMP_20K_BUF_BASELINE 2048

char         *tmp_20k_buf = NULL;
static size_t tmp_20k_buf_size = 0;

void
grow_tmp_20k_buf(need)
    size_t need;
{
    size_t newsize;

    if(need <= tmp_20k_buf_size)
      return;

    newsize = tmp_20k_buf_size ? tmp_20k_buf_size : TMP_20K_BUF_BASELINE;
    while(newsize < need)
      newsize *= 2;

    if(tmp_20k_buf)
      fs_resize((void **) &tmp_20k_buf, newsize);
    else
      tmp_20k_buf = (char *) fs_get(newsize);

    tmp_20k_buf_size = newsize;
}



#ifdef ANSI
static int setup_mini_menu(int);
void do_menu(int);
void main_redrawer();
void show_main_screen(struct pine *, int, OtherMenu, struct key_menu *);
void new_user_or_version(char **);
#else
static int setup_mini_menu();
void do_menu();
void main_redrawer();
void show_main_screen();
void new_user_or_version();
#endif
static void do_setup_task();

static char *first_time_message[] = {
#ifdef DOS
"                   Welcome to PC-Pine",
"",
"a Program for Internet News and Email.  Pine offers the ability to:",
"  -Access local and remote message folders using a simple user-interface",
"  -Send documents, graphics, etc (via the MIME standard for attachments)",
"",
"COMMANDS IN PINE:  The last two lines on the screen tell you what",
"commands are available for the current situation.  Usually there",
"are more commands than can be shown on two lines, so use the \"O\"",
"key to see what OTHER COMMANDS are available.  The \"O\" is optional;",
"it is not necessary for a command to be visible before using it",
"(except when Pine is used in function-key mode; then F2 is required.)",
"",
"PINE CONFIGURATION:  If you haven't yet filled out your Pine configuration",
"file, Pine will be asking you questions about your configuration as you",
"go.  If you plan to use Pine regularly, take the time to edit the PINERC",
"file.  Use a standard text editor outside of Pine to edit this file.",
"",

#else /* UNIX */

"                      Welcome to Pine...",
" ",
"a Program for Internet News and Email.  Pine offers the ability to:",
"  -Access local and remote message folders using a simple user-interface",
"  -Send documents, graphics, etc (via the MIME standard for attachments)",
"",
"COMMANDS IN PINE:  The last two lines on the screen tell you what", 
"commands are available for the current situation.  Usually there",
"are more commands than can be shown on two lines, so use the \"O\"",
"key to see what OTHER COMMANDS are available.  The \"O\" is optional;",
"it is not necessary for a command to be visible before using it",
"(except when Pine is used in function-key mode; then F2 is required.)",
"",
"PINE CONFIGURATION:  Pine will create a default configuration file,",
".pinerc, in your home directory.  You may edit this file to select",
"various options.  It will also create a \"mail\" subdirectory for your",
"saved-message folders.  PLEASE type \"?\" from the Main Menu for help.",
#endif
NULL};


static char *new_version_message[] = {
#ifdef DOS
"   This is a new version of PC-Pine!",
#else /* UNIX */
"   This is a new version of Pine!",
#endif
"",
"   Your Pine configuration file shows that you have not used ",
"   Pine 3.89 before.  There are a small number of visible changes",
"   since Pine 3.88, but this is primarily a \"bug fix\" release.",
"",
"   Details of changes in this, and previous, versions of Pine can",
"   be found in the Release Notes (\"R\" command off Main Menu).",
"",
"   This message will appear only once.",
"",
NULL};

static struct key main_keys[] =
       {{"?","Help",0},        {"O","OTHER CMDS",0},  {NULL,NULL,0},    
        {NULL,NULL, 0},        {"P","PrevCmd",0},     {"N","NextCmd",0},
	{NULL,NULL,0},         {NULL,NULL,0},         {"R","RelNotes",0},
	{"K","KBLock",0},      {NULL,NULL,0},         {NULL,NULL,0},
	{"?","Help",0},        {"O","OTHER CMDS",0},  {"Q","Quit",0},
        {"C","Compose",0},     {"L","ListFldrs",0},   {"G","GotoFldr",0},
	{"I","Index",0},       {NULL,NULL,0},         {"S","Setup",0},
	{NULL,NULL,0},         {NULL,NULL,0},         {NULL,NULL,0}};
static struct key_menu main_keymenu =
	{sizeof(main_keys)/(sizeof(main_keys[0])*12), 0, 0,0,0,0, main_keys};
#define HELP_KEY         0
#define OTHER_KEY        1
#define DEFAULT_KEY      3
#define KBLOCK_KEY       9
#define HELP_KEY2        12
#define OTHER_KEY2       13
#define QUIT_KEY         14
#define COMPOSE_KEY      15
#define FOLDER_KEY       16
#define INDEX_KEY        18
#define SETUP_KEY        20
/*
 * length of longest label from keymenu, of labels corresponding to
 * commands in the middle of the screen.  9 is length of ListFldrs
 */
#define LONGEST_LABEL 9  /* length of longest label from keymenu */
#define LONGEST_NAME 1   /* length of longest name from keymenu */


/*----------------------------------------------------------------------
     main routine -- entry point

  Args: argv, argc -- The command line arguments


 Initialize pine, parse arguments and so on

 If there is a user address on the command line go into send mode and exit,
 otherwise loop executing the various screens in Pine.
  ----*/

main(argc, argv)
     int   argc;
     char *argv[];
{
    char         *folder_to_open;
    int           rv;
    struct pine  *pine_state;
    char          int_mail[MAXPATH+1];
#ifdef DYN
    char stdiobuf[64];
#endif

    /*----------------------------------------------------------------------
          Set up buffering and some data structures
      ----------------------------------------------------------------------*/

    grow_tmp_20k_buf(TMP_20K_BUF_BASELINE);

    pine_state                 = (struct pine *)fs_get(sizeof (struct pine));
    memset((void *)pine_state, 0, sizeof(struct pine));
    ps_global                  = pine_state;
    ps_global->mail_stream     = NULL; /* In case of SIGHUP before open */
    ps_global->inbox_stream    = NULL; /* In case of SIGHUP before open */
    ps_global->ttyo            = NULL;
    ps_global->cur_folder[0]   = '\0';
    ps_global->context_list    = NULL;
    ps_global->context_current = NULL;
    ps_global->sort            = (long *)fs_get(2 * sizeof(long));
    ps_global->sort_allocated  = 2;
    ps_global->sort_order      = SortArrival;
    ps_global->reverse_sort    = 0;
    ps_global->new_current_sorted_msgno = -1L;
    ps_global->sort_types[0]   = SortSubject;
    ps_global->sort_types[1]   = SortArrival;
    ps_global->sort_types[2]   = SortFrom;
    ps_global->sort_types[3]   = SortDate;
    ps_global->sort_types[4]   = SortSize;
    ps_global->sort_types[5]   = EndofList;
    ps_global->atmts           = (struct attachment *)
                                       fs_get(sizeof(struct attachment));
    ps_global->atmts_allocated = 1;
    ps_global->atmts->description = NULL;
    pine_state->full_header    = 0;
    ps_global->low_speed       = 1;
    ps_global->noticed_dead_stream = 0;
    ps_global->noticed_dead_inbox  = 0;
    ps_global->dead_stream = 0;
    ps_global->dead_inbox  = 0;

    ps_global->max_msgno     = 0;
    init_init_vars(ps_global);

#ifndef	DOS
    srandom(getpid());  /* for new mail notification which uses random */
#endif

#ifdef DYN
    /*-------------------------------------------------------------------
      There's a bug in DYNIX that causes the terminal driver to lose
      characters when large I/O writes are done on slow lines. Like
      a 1Kb write(2) on a 1200 baud line. Usually CR is output which
      causes a flush before the buffer is too full, some the pine composer
      doesn't output newlines a lot. Either stdio should be fixed to
      continue with more writes when the write request is partial, or
      fix the tty driver to always complete the write.
     */
    setbuffer(stdout, stdiobuf, 64);
#endif	

    /*----------------------------------------------------------------------
           Parse arguemnts and initialize debugging
      ----------------------------------------------------------------------*/
    folder_to_open = pine_args(pine_state, argc, argv, &argv);

#ifdef DEBUG
    /* Since this is specific debugging we don't mind if the
       ifdef is the type of system. See conf/templets.h
     */
#ifdef HAVE_SMALLOC 
    if(debug > 8)
      malloc_debug(2);
#endif 
#ifdef NXT
    if(debug > 8)
      malloc_debug(32); 
#endif 

#endif  /* DEBUG */


    strcpy(version_buff, VERSION);


    /* Must do this here, so we can get the .pinerc file and debug file */
    if(getenv("HOME") != NULL) { 
        pine_state->home_dir = cpystr(getenv("HOME"));
    } else if(get_system_homedir() != NULL) {
        pine_state->home_dir = cpystr(get_system_homedir());
    } else {
        pine_state->home_dir = cpystr("");
    }


#ifdef DEBUG
    init_debug();
#endif

#ifdef	DOS
    /*
     * install c-client callback to manage cache data outside
     * free memory
     */
    mailcache = dos_cache;
#endif

    /*------- Set up c-client drivers -------------- */
    mail_link((DRIVER *)&bezerkdriver);

    init_vars(pine_state);

    if(init_username(pine_state) < 0)
      exit(-1);

    if(init_hostname(pine_state) < 0)
      exit(-1);

    if(*argv == NULL)
      /* do mail dir if we're not in send only mode */
      if(init_mail_dir(pine_state->folders_dir) < 0)
        exit(-1);

    init_signals();

    /*--- input side ---*/
    if(init_tty_driver(pine_state)){
#ifdef	DOS
    /* always succeeds under DOS! */
#else
        fprintf(stderr, "Can't access terminal or input is not a terminal. Redirection of\n");
        fprintf(stderr, "standard input is not allowed. For example \"pine < file\" doesn't work.\n\007");
        exit(-1);
#endif
    }
        

    /*--- output side ---*/
    rv = config_screen(&(pine_state->ttyo));
#ifdef	DOS
    /* always succeeds under DOS! */
#else
    if(rv) {
        switch(rv) {
          case -1: printf("Terminal type (envronment variable TERM) not set.\n");
            break;
          case -2: printf("Terminal type \"%s\", is unknown.\n", getenv("TERM"));
            break;
          case -3:
            printf("Can't open termcap file; check TERMCAP variable and/or system manager.\n");
            break;
          case -4:
            printf("Your terminal, of type \"%s\", is lacking functions needed to run pine.\n", getenv("TERM"));
            break;
        }
        printf("\r");
        end_tty_driver(pine_state);
        exit(-1);
    }
#endif
    init_screen();
    pine_state->in_init_seq = 0;  /* so output (and ClearScreen) will show up */
    pine_state->dont_use_init_cmds = 1;  /* don't use up initial_commands yet */
    ClearScreen();
    if(pine_state->first_time_user || pine_state->show_new_version){
	pine_state->mangled_header = 1;
	show_main_screen(pine_state, 0, 0, NULL);
        if(pine_state->first_time_user)
	    new_user_or_version(first_time_message);
	else
	    new_user_or_version(new_version_message);
        ClearScreen();
    }
    
    /* put back in case we need to suppress output */
    pine_state->in_init_seq = pine_state->save_in_init_seq;
            
    init_keyboard(pine_state->orig_use_fkeys);

    strcpy(pine_state->inbox_name, INBOX_NAME);
    init_folders();		/* digest folder spec's */

    if(*argv == NULL) {
        struct key_menu *km;

	km = &main_keymenu;

        /*======================== Normal pine mail reading mode ==========*/
            
        pine_state->mail_stream = NULL;
        pine_state->inbox_stream = NULL;
        pine_state->mangled_screen = 1;
    
        if(!pine_state->start_in_index) {

	    /* flash message about executing initial commands */
	    if(pine_state->in_init_seq) {
	        pine_state->in_init_seq = 0;
		pine_state->mangled_header = 1;
		pine_state->mangled_footer = 1;
		pine_state->mangled_screen = 0;
		/* show that this is Pine */
		show_main_screen(pine_state, 0, FirstMenu, km);
		pine_state->mangled_screen = 1;
		pine_state->painted_footer_on_startup = 1;
	        PutLine0(4, 0,
		         "           Executing initial-keystroke-list......");
	        pine_state->in_init_seq = 1;
	    }else {
                show_main_screen(pine_state, 0, FirstMenu, km);
		pine_state->painted_body_on_startup   = 1;
		pine_state->painted_footer_on_startup = 1;
	    }
        } else {
	    /* cancel any initial commands, overridden by cmd line */
	    if(pine_state->in_init_seq) {
		pine_state->in_init_seq = 0;
		pine_state->save_in_init_seq = 0;
		if (pine_state->initial_cmds)
		    *pine_state->initial_cmds = 0;
		F_SET(F_USE_FK,pine_state,pine_state->orig_use_fkeys);
	    }
            do_index_border(1, 1, 1, NULL);
	    pine_state->painted_footer_on_startup = 1;
            PutLine1(4, 0, "           Please wait, opening %s......",
                     "mail folder");
        }
        fflush(stdout);

	pine_state->in_init_seq = 0;

        if(folder_to_open != NULL) {
            if(do_broach_folder(folder_to_open, 
				pine_state->context_current) <= 0) {
                display_message('x');
                sleep(4);
                end_screen();
                end_keyboard(F_ON(F_USE_FK,pine_state));
                end_tty_driver(pine_state);
            
                clear_index_cache();
                end_scroll_text();
                printf("\n\nUnable to open %s \"%s\". Pine finished\n\n",
                       "folder",
                       folder_to_open);
                exit(-1);
            }
        } else {
#ifdef	DOS
            /*
	     * need to ask for the inbox name if no default under DOS
	     * since there is no "inbox"
	     */

	    if(!pine_state->VAR_INBOX_PATH || !pine_state->VAR_INBOX_PATH[0]
	       || strucmp(pine_state->VAR_INBOX_PATH, "inbox") == 0){
		HelpType help = NO_HELP;
		static   ESCKEY_S ekey[] = {{"^T", "To Fldrs", ctrl(T), 2},
					  {NULL,NULL,-1,0}};

		pine_state->mangled_footer = 1;
		int_mail[0] = '\0';
    		while(1) {
        	    rv = optionally_enter(int_mail, -3, 0, MAXPATH, 1, 0,
				      "No inbox!  Folder to open as inbox : ",
				      ekey, help, 0);
        	    if(rv == 3){
			help = (help == NO_HELP) ? h_sticky_inbox : NO_HELP;
			continue;
		    }

        	    if(rv != 4)
			break;
    		}

    		if(rv == 1){
		    q_status_message(0, 0,2 ,"\007Folder open cancelled");
		    rv = 0;		/* reset rv */
		} 
		else if(rv == 2){
        	    if(!folder_lister(pine_state, OpenFolder, NULL,
			       &(pine_state->context_current), int_mail))
		      *int_mail = '\0';	/* user cancelled! */

                    show_main_screen(pine_state, 0, FirstMenu, km);
		}

		if(*int_mail){
		    removing_trailing_white_space(int_mail);
		    removing_leading_white_space(int_mail);
		    if((!pine_state->VAR_INBOX_PATH 
			|| strucmp(pine_state->VAR_INBOX_PATH, "inbox") == 0)
		     && want_to("Preserve folder as \"inbox-path\" in PINERC", 
				'y', 'n', NO_HELP, 0) == 'y'){
			set_variable(V_INBOX_PATH, int_mail);
		    }
		    else{
			if(pine_state->VAR_INBOX_PATH)
			  fs_give((void **)&pine_state->VAR_INBOX_PATH);

			pine_state->VAR_INBOX_PATH = cpystr(int_mail);
		    }

		    do_broach_folder(pine_state->inbox_name, 
				     pine_state->context_list);
    		}
		else
		  q_status_message(0, 0,2 ,"\007No folder opened");

	    }
	    else

#endif
            do_broach_folder(pine_state->inbox_name, pine_state->context_list);
        }

        if(pine_state->mangled_footer)
	  pine_state->painted_footer_on_startup = 0;

        build_path(int_mail, pine_state->folders_dir, INTERRUPTED_MAIL);
        if(can_access(int_mail, ACCESS_EXISTS) == 0) {
            q_status_message(1, 4, 5, "\007Use compose command to continue composing last message. It was interrupted.");
        }

#if defined(USE_QUOTAS)
	{
	    long q;
	    int  over;
	    q = disk_quota(pine_state->home_dir, &over);
	    if(q > 0 && over) {
		q_status_message2(1, 4,5,
			"\007WARNING! Over your disk quota by %s bytes (%s)",
			comatose(q),byte_string(q));
	    }
	}
#endif

	pine_state->in_init_seq = pine_state->save_in_init_seq;
	pine_state->dont_use_init_cmds = 0;

        /*-------------------------------------------------------------------
                         Loop executing the commands
    
            This is done like this so that one command screen can cause
            another one to execute it with out going through the main
            menu. 
            BUG- need for comments
    
          ------------------------------------------------------------------*/
        pine_state->next_screen = pine_state->start_in_index ?
                                         mail_index_screen : 
                                         main_menu_screen;
        while(1) {
            if(pine_state->next_screen == SCREEN_FUN_NULL) 
              pine_state->next_screen = main_menu_screen;

            (*(pine_state->next_screen))(pine_state);
        }

    } else {
        /*======= address on command line/send one message mode ============*/
        char *to, **t, *error;
        int   len, good_addr;

        /*----- Format the To: line with commas for the composer ---*/
        for(t = argv, len = 0; *t != NULL; len += strlen(*t++) + 2);
        to = fs_get(len + 5);
        to[0] = '\0';
        for(t = argv, len = 0; *t != NULL; t++) {
            if(to[0] != '\0')
              strcat(to, ", ");
            strcat(to, *t);
        }

        good_addr = (build_address(to, tmp_20k_buf, &error) >= 0);

        if(good_addr) {
            char *addr = cpystr(tmp_20k_buf);
            compose_mail(addr);
            fs_give((void **)&addr);
        }

        fs_give((void **)&to);
        end_screen();
        end_keyboard(F_ON(F_USE_FK,pine_state));
        end_tty_driver(pine_state);
        if(!good_addr) {
            fprintf(stderr, "Bad address: %s\n", error);
            exit(-1);
        }
        exit(0);
    }
}


/* this default is from the array of structs below */
#define DEFAULT_MENU_ITEM 6  /* LIST FOLDERS */
#define MAX_DEFAULT_MENU_ITEM 10
#define UNUSED 0
static unsigned char current_default_menu_item = DEFAULT_MENU_ITEM;
/*
 * One of these for each line that gets printed in the middle of the
 * screen in the main menu.
 */
struct menu_key {
    char         *descrip;
    unsigned int f_key;           /* function key that invokes this action */
    unsigned int key;             /* alpha key that invokes this action */
    unsigned int keymenu_number;  /* index into keymenu array for this cmd */
};
static struct menu_key mkeys[] = {
  {"   %s     HELP               -  Get help using Pine", 
            PF1, '?', HELP_KEY},
  {"", UNUSED, UNUSED, UNUSED},
  {"   %s     COMPOSE MESSAGE    -  Compose and send a message", 
            OPF4, 'C', COMPOSE_KEY},
  {"", UNUSED, UNUSED, UNUSED},
  {"   %s     FOLDER INDEX       -  View messages in current folder", 
            OPF7, 'I', INDEX_KEY},
  {"", UNUSED, UNUSED, UNUSED},
  {"   %s     FOLDER LIST        -  Select a folder to view",
            OPF5, 'L', FOLDER_KEY},
  {"", UNUSED, UNUSED, UNUSED},
  {"   %s     SETUP              -  Configure or update Pine",
            OPF9, 'S',SETUP_KEY},
  {"", UNUSED, UNUSED, UNUSED},
  {"   %s     QUIT               -  Exit the Pine program", 
            OPF3, 'Q', QUIT_KEY},
  {NULL, UNUSED, UNUSED, UNUSED}
};

/*----------------------------------------------------------------------
      display main menu and execute main menu commands

    Args: The usual pine structure

  Result: main menu commands are executed


              M A I N   M E N U    S C R E E N

   Paint the main menu on the screen, get the commands and either execute
the function or pass back the name of the function to execute for the menu
selection. Only simple functions that always return here can be executed
here.

This functions handling of new mail, redrawing, errors and such can 
serve as a template for the other screen that do much the same thing.

There is a loop that fetchs and executes commands until a command to leave
this screen is given. Then the name of the next screen to display is
stored in next_screen member of the structure and this function is exited
with a return.

First a check for new mail is performed. This might involve reading the new
mail into the inbox which might then cause the screen to be repainted.

Then the general screen painting is done. This is usually controlled
by a few flags and some other position variables. If they change they
tell this part of the code what to repaint. This will include cursor
motion and so on.
  ----*/
void
main_menu_screen(pine_state)
     struct pine *pine_state;
{
    int  ch, orig_ch, question_line, setup_command, just_a_navigate_cmd;
    char            *new_folder;
    CONTEXT_S       *tc;
    struct key_menu *km;
    OtherMenu       what;
#ifdef	DOS
    extern void (*while_waiting)();
#endif

    ps_global                 = pine_state;
    question_line             = -3;
    just_a_navigate_cmd       = 0;
    current_default_menu_item = DEFAULT_MENU_ITEM;
    what                      = FirstMenu;  /* which keymenu to display */
    ch                        = 'x'; /* For display_message 1st time through */
    pine_state->prev_screen   = main_menu_screen;
    pine_state->redrawer      = main_redrawer;

    if(!pine_state->painted_body_on_startup 
       && !pine_state->painted_footer_on_startup){
	ClearScreen();
	pine_state->mangled_screen = 1;
    }

    km = &main_keymenu;

    dprint(1, (debugfile, "\n\n    ---- MAIN_MENU_SCREEN ----\n"));

    while (1) {
	/*----------- Check for new mail -----------*/
        if(new_mail(NULL, 0,ch==NO_OP_IDLE ? 0 : ch==NO_OP_COMMAND ?1 :2) >= 0)
          pine_state->mangled_header = 1;

        if(streams_died())
          pine_state->mangled_header = 1;

        show_main_screen(pine_state, just_a_navigate_cmd, what, km);
        just_a_navigate_cmd = 0;
	what = SameTwelve;

	/*---- This displays new mail notification, or errors ---*/
        display_message(ch);

        /*------ Read the command from the keyboard ----*/      
        MoveCursor(min(0, pine_state->ttyo->screen_rows - 3), 0);
#ifdef	DOS
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
#endif
        orig_ch = ch;

        if(ch <= 0xff && isupper(ch))
          ch = tolower(ch);
	if(km->which == 1)
	  if(ch >= PF1 && ch <= PF12)
	    ch = PF2OPF(ch);

	/*----- Validate the command ----*/
	if(ch == ctrl('M') || ch == ctrl('J') || ch == PF4){/* set up default */
	  ch = F_ON(F_USE_FK,pine_state)
                                       ? mkeys[current_default_menu_item].f_key
                                       : mkeys[current_default_menu_item].key;
          if(ch <= 0xff && isupper(ch))
            ch = tolower(ch);
	}


        /*
	 * 'q' is always valid as a way to exit pine even in function key mode
         */
        if(ch != 'q')
          ch = validatekeys(ch);

	/*------ Execute the command ------*/
	switch (ch)
	{
#ifdef	DOS
/* while we're testing DOS */
 case 'h': 
     {
#include <malloc.h>
       int    heapresult;
       int    totalused = 0;
       int    totalfree = 0;
       long   totalusedbytes = 0L;
       long   totalfreebytes = 0L;
       long   largestinuse = 0L;
       long   largestfree = 0L, freeaccum = 0L;
       _HEAPINFO hinfo;
       extern long coreleft();
       extern void dumpmetacache();

       hinfo._pentry = NULL;
       while((heapresult = _heapwalk(&hinfo)) == _HEAPOK){
        if(hinfo._useflag == _USEDENTRY){
          totalused++;
          totalusedbytes += (long)hinfo._size; 
          if(largestinuse < (long)hinfo._size)
             largestinuse = (long)hinfo._size;
        }
        else{
          totalfree++;
          totalfreebytes += (long)hinfo._size;
        }

        if(hinfo._useflag == _USEDENTRY){
	    if(freeaccum > largestfree)		/* remember largest run */
	      largestfree = freeaccum;

	    freeaccum = 0L;
	}
	else
	  freeaccum += (long)hinfo._size;
      }

      sprintf(tmp_20k_buf,
	      "use: %d (%ld, %ld lrg), free: %d (%ld, %ld lrg), DOS: %ld", 
		totalused, totalusedbytes, largestinuse,
		totalfree, totalfreebytes, largestfree, coreleft());
      q_status_message(0, 5, 7, tmp_20k_buf);

      switch(heapresult/* = _heapchk()*/){
       case _HEAPBADPTR:
        q_status_message(0,1,2 ,"\007ERROR - Bad start of heap");
        break;
       case _HEAPBADBEGIN:
        q_status_message(0,1,2 ,"\007ERROR - Bad start of heap");
        break;
       case _HEAPBADNODE:
        q_status_message(0,1,2 ,"\007ERROR - Bad node in heap");
        break;
       case _HEAPEMPTY:
        q_status_message(0,1,2 ,"Heap OK - empty");
        break;
       case _HEAPEND:
        q_status_message(0,1,2 ,"Heap checks out!");
        break;
       case _HEAPOK:
        q_status_message(0,1,2 ,"Heap checks out!");
        break;
       default:
        q_status_message1(0,1,2 ,"\007BS from heapchk: %d", (void *)heapresult);
        break;
      }

/*       dumpmetacache(ps_global->mail_stream);*/
/* DEBUG: heapcheck() */
/*       q_status_message1(0, 1, 3,
                         " * * There's %ld bytes of core left for Pine * * ", 
                         (void *)coreleft());*/
     }
     break;
#endif	/* END: for testing */

	  /*---------- help ------*/
	  case PF1:
	  case OPF1:
	  case ctrl('G'):
	  case '?':
            helper(main_menu_tx, "HELP FOR MAIN MENU", 0);
	    pine_state->mangled_screen = 1;
	    break;
  
	  /*---------- display other key bindings ------*/
	  case PF2:
	  case OPF2:
          case 'o' :
            if(ch == 'o')
	      warn_other_cmds();
	    what = NextTwelve;
	    pine_state->mangled_footer = 1;
	    break;

	  /* case PF4: */
	  /* PF4 is handled above switch */

          /*---------- Previous item in menu ----------*/
	  case PF5:
	  case 'p':
	  case ctrl('P'):
	  case KEY_UP:
	    if(current_default_menu_item > 1) {
	      current_default_menu_item -= 2;  /* 2 to skip the blank lines */
	      pine_state->mangled_body = 1;
	      if(km->which == 0)
	        pine_state->mangled_footer = 1;
	      just_a_navigate_cmd++;
	    }else {
	      q_status_message(0, 0, 2, "\007Already at top of list");
	    }
	    break;

          /*---------- Next item in menu ----------*/
	  case PF6:
	  case 'n':
	  case ctrl('N'):
	  case KEY_DOWN:
	    if(current_default_menu_item < (unsigned)(MAX_DEFAULT_MENU_ITEM-1)){
	      current_default_menu_item += 2;
	      pine_state->mangled_body = 1;
	      if(km->which == 0)
	        pine_state->mangled_footer = 1;
	      just_a_navigate_cmd++;
	    }else {
	      q_status_message(0, 0, 2, "\007Already at bottom of list");
	    }
	    break;

          /*---------- Release Notes ----------*/
	  case PF9:
	  case 'r':
	    helper(h_news, "NEWS ABOUT PINE", 0);
	    pine_state->mangled_screen = 1;
	    break;

#ifndef NO_KEYBOARD_LOCK
          /*---------- Keyboard lock ----------*/
	  case PF10:
	  case 'k':
            (void)lock_keyboard();
	    ClearScreen();
	    pine_state->mangled_screen = 1;
	    break;
#endif /* !NO_KEYBOARD_LOCK */

          /*---------- Quit pine ----------*/
	  case OPF3: 
	  case 'q':
	    pine_state->next_screen = quit_screen;
	    return;
  
          /*---------- Go to composer ----------*/
	  case OPF4:
	  case 'c':
	    pine_state->next_screen = compose_screen;
	    return;
  
          /*---------- Folders ----------*/
	  case OPF5: 
	  case 'l':
	    pine_state->next_screen = folder_screen;
	    return;

          /*---------- Old Folders Command ----------*/
	  case 'f':
	    q_status_message(0, 0, 2, "\007Use \"L\" to list Folders");
	    break;

          /*---------- Goto new folder ----------*/
	  case OPF6:
	  case 'g':
	    tc = ps_global->context_current;
            new_folder = broach_folder(question_line, 1, &tc);
#ifdef	DOS
	    if(new_folder && *new_folder == '{' && coreleft() < 20000){
	      q_status_message(0,0,2,
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
	    return;
		  
          /*---------- Go to index ----------*/
	  case OPF7:
	  case 'i':
	    pine_state->next_screen = mail_index_screen;
	    return;
		    
          /*---------- Setup mini menu ----------*/
	  case OPF9:
	  case 's':
	    setup_command = setup_mini_menu(question_line);
	    pine_state->mangled_footer = 1;
	    do_setup_task(setup_command);
            if(ps_global->next_screen != main_menu_screen)
	      return;
	    break;

	  case 'm':
	    q_status_message(0, 1, 3, "\007Already in Main");
	    break;

#ifdef DEBUG
          case '#':
            if(debug > 8 &&
               want_to("Test panic and abort now", 'n','n',NULL,0) == 'y')
              panic("Testing panic");
            break;
#endif

          case ctrl('Z'):
            if(!have_job_control())
              goto bleep;
            if(F_OFF(F_CAN_SUSPEND,pine_state)) {
                q_status_message(1, 1,3,
                           "\007Pine suspension not enabled - see help text");
                break;
            } else {
                do_suspend(pine_state);
            }
            /*-- Fall through to redraw --*/

          case KEY_RESIZE:
	  case ctrl('L'):
	    ClearScreen();
	    pine_state->mangled_screen = 1;
	    if(ch == KEY_RESIZE)
	      clear_index_cache();

	    break;
  
	  case NO_OP_COMMAND :
          case NO_OP_IDLE:
            break;	/* noop for timeout loop mail check */
  
	  default:
          bleep:
            q_status_message2(0, 0, 2,
	      "\007Command \"%s\" not defined for this screen. Use %s for help",
                              (void *)pretty_command(orig_ch),
                              F_ON(F_USE_FK,pine_state) ? "F1" : "?");
	 } /* the switch */
    } /* the BIG while loop! */
}



/*----------------------------------------------------------------------
    Re-Draw the main menu

    Args: none.

  Result: main menu is re-displayed
  ----*/
void
main_redrawer()
{
    struct key_menu *km = &main_keymenu;
    ps_global->mangled_screen = 1;
    show_main_screen(ps_global, 0, FirstMenu, km);
}


	
/*----------------------------------------------------------------------
         Draw the main menu

    Args: pine_state - the usual struct
	  quick_draw - tells do_menu() it can skip some drawing
	  what       - tells which section of keymenu to draw
	  km         - the keymenu

  Result: main menu is displayed
  ----*/
void
show_main_screen(pine_state, quick_draw, what, km)
    struct pine *pine_state;
    int quick_draw;
    OtherMenu       what;
    struct key_menu *km;
{
    if(pine_state->painted_body_on_startup 
       || pine_state->painted_footer_on_startup){

	pine_state->mangled_screen = 0;		/* only worry about it here */
	pine_state->mangled_header = 1;		/* we have to redo header */
	if(!pine_state->painted_body_on_startup)
	  pine_state->mangled_body = 1;		/* make sure to paint body*/

	if(!pine_state->painted_footer_on_startup)
	  pine_state->mangled_footer = 1;	/* make sure to paint footer*/

	pine_state->painted_body_on_startup   = 0;
        pine_state->painted_footer_on_startup = 0;
    }

    if(pine_state->mangled_screen) {
      pine_state->mangled_header = 1;
      pine_state->mangled_body = 1;
      pine_state->mangled_footer = 1;
      pine_state->mangled_screen = 0;
    }

    /* paint the titlebar if needed */
    if(pine_state->mangled_header) {
      set_titlebar("MAIN MENU", 1, FolderName, 0, 0, 0);
      pine_state->mangled_header = 0;
    }

    /* paint the body if needed */
    if(pine_state->mangled_body) {
      do_menu(quick_draw);
      pine_state->mangled_body = 0;
    }

    /* paint the keymenu if needed */
    if(pine_state->mangled_footer) {
      BITMAP          bitmap;
      static char label[LONGEST_LABEL + 2 + 1], /* label + brackets + \0 */
                  name[max(LONGEST_NAME,3)+1];  /* longest name + 1 (3=F12) */

      setbitmap(bitmap);

#ifdef NO_KEYBOARD_LOCK
      clrbitn(KBLOCK_KEY, bitmap);
#endif

      /* put brackets around the default action */
      label[0] = '[';  label[1] = '\0';
      strcat(label,
	      km->keys[mkeys[current_default_menu_item].keymenu_number].label);
      strcat(label, "]");
      km->keys[DEFAULT_KEY].label = label;
      sprintf(name, "%s", F_ON(F_USE_FK,ps_global) ?
		pretty_command(mkeys[current_default_menu_item].f_key) :
		pretty_command(mkeys[current_default_menu_item].key));
      km->keys[DEFAULT_KEY].name = name;
      draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols, -2, 0, what, 0);
      pine_state->mangled_footer = 0;
    }
}


/*----------------------------------------------------------------------
         Actually display the main menu

    Args: quick_draw - just a next or prev command was typed so we only have
		       to redraw the highlighting

  Result: Main menu is displayed
  ---*/
void
do_menu(quick_draw)
    int quick_draw;
{
    int  dline, use_fkeys;
    char buf[MAX_SCREEN_COLS+1];
    static int last_inverse = -1;

    use_fkeys = F_ON(F_USE_FK,ps_global);
    
    /* leave room for keymenu, status line, and trademark message */
    for(dline = 3; dline < ps_global->ttyo->screen_rows - 4; dline++) {
        if(mkeys[dline-3].descrip == NULL)
          break;
	if(quick_draw &&
	     !(dline-3 == last_inverse || dline-3 == current_default_menu_item))
	  continue;
        sprintf(buf, mkeys[dline-3].descrip,
                use_fkeys ? "" : pretty_command(mkeys[dline-3].key));
        buf[ps_global->ttyo->screen_cols] = '\0';
	if(dline-3 == current_default_menu_item)
	  StartInverse();
        PutLine0(dline, 4, buf);
	if(dline-3 == current_default_menu_item)
	  EndInverse();
    }
    last_inverse = current_default_menu_item;

    if(!quick_draw) {
      /*
       * the devi.. uh, I mean, lawyer made me do it.
       */
      PutLine0(ps_global->ttyo->screen_rows - 4, 3,
 "Copyright 1989-1993.  PINE is a trademark of the University of Washington.");
    }
    fflush(stdout);
}


char *setup_names[] =	{"Newpassword",
			 "Options",
			 "Update",
			 "Exit",
			  NULL};
/*----------------------------------------------------------------------

Args: ql -- Line to prompt on
Returns: character selected

  ----*/
static int
setup_mini_menu(ql)
     int ql;
{
    char      prompt[80];
    int       s;
    HelpType  help;

    (void)strcpy(prompt,
         "SETUP (Newpassword,Options,Update,[Exit]; type NOU or E): ");

    help = h_mini_setup;
    s = radio_buttons(prompt, ql, 0, "noue", setup_names, 'e', 'x', help, 0);
    /* ^C */
    if(s == 'x') {
	Writechar('\007');
	s = 'e';
    }

    return(s);
}


/*----------------------------------------------------------------------

Args: command -- command char to perform

  ----*/
static void
do_setup_task(command)
    int command;
{
    switch(command) {
/*
 * We decided to hardwire this to a particular name rather than leave it
 * a config option.  A config option would be reasonable for Unix sites, where
 * the sys mgr could config it, but not for DOS, where it would have to
 * appear in everyone's pinerc.  If the load on pine.cac gets to be too
 * much, we can always turn pine.cac into a multi-address name which
 * randomizes.
 */
#define UPDATE_FOLDER "*{pine.cac.washington.edu/anonymous}updates"
        /*----- UPDATE -----*/
      case 'u':
	q_status_message(1, 3, 5, "Connecting to update server");
        if(do_broach_folder(UPDATE_FOLDER, NULL) > 0)
          ps_global->next_screen = mail_index_screen;
	break;

        /*----- SET OPTIONS -----*/
      case 'o':
	q_status_message(1, 3, 5, "No options screen yet");
	break;

        /*----- EXIT -----*/
      case 'e':
        break;

        /*----- NEW PASSWORD -----*/
      case 'n':
#ifdef	DOS
        q_status_message(1, 3, 5,
	    "Password change not available in the DOS version of Pine.");
	display_message('x');
        sleep(3);
#else
	change_passwd();
	ClearScreen();
	ps_global->mangled_screen = 1;
#endif	/* DOS */
        break;

    }
}



/*
 * Display a new user or new version message.
 */
void
new_user_or_version(message)
    char *message[];
{
    int  dline;

    for(dline = 3; dline < ps_global->ttyo->screen_rows - 4; dline++) {
        if(message[dline-3] == NULL)
            break;
        PutLine0(dline, 4, message[dline-3]);
    }

    PutLine0(ps_global->ttyo->screen_rows - 4, 13,
		 "PINE is a trademark of the University of Washington.");
    StartInverse();
    PutLine0(ps_global->ttyo->screen_rows - 3, 0,
		 "Type any character to continue : ");
    EndInverse();

    /* ignore the character typed, 120 second timeout */
    (void)read_char(120);
}


/*----------------------------------------------------------------------
          Quit pine if the user wants to 

    Args: The usual pine structure

  Result: User is asked if she wants to quit, if yes then execute quit.

       Q U I T    S C R E E N

Not really a full screen. Just count up deletions and ask if we really
want to quit.
  ----*/
void
quit_screen(pine_state)
     struct pine *pine_state;
{
    int cur_is_inbox, ret;

    dprint(1, (debugfile, "\n\n    ---- QUIT SCREEN ----\n"));    

    if(F_OFF(F_QUIT_WO_CONFIRM,pine_state) &&
       (ret=want_to("Really quit pine", 'y', 'x', h_oe_export, 0)) != 'y') {
	if(ret == 'x')
	  Writechar('\007');
        pine_state->next_screen = pine_state->prev_screen;
        return;
    }

    cur_is_inbox = (pine_state->inbox_stream == pine_state->mail_stream);

    expunge_and_close(pine_state->mail_stream, pine_state->cur_folder);

    pine_state->redrawer = (void (*)())NULL;

    if(pine_state->inbox_stream != NULL && !cur_is_inbox){
	pine_state->mail_stream = pine_state->inbox_stream;
        expunge_and_close(pine_state->inbox_stream, pine_state->inbox_name);
    }

    end_screen();
    end_keyboard(F_ON(F_USE_FK,pine_state));
    end_tty_driver(pine_state);

    clear_index_cache();
    end_scroll_text();

    if(pine_state->hostname != NULL)
      fs_give((void **)&pine_state->hostname);
    if(pine_state->localdomain != NULL)
      fs_give((void **)&pine_state->localdomain);
    if(pine_state->userdomain != NULL)
      fs_give((void **)&pine_state->userdomain);
    if(pine_state->ttyo != NULL)
      fs_give((void **)&pine_state->ttyo);
    if(pine_state->home_dir != NULL)
      fs_give((void **)&pine_state->home_dir);
    if(pine_state->folders_dir != NULL)
      fs_give((void **)&pine_state->folders_dir);

    fs_give((void **)&pine_state);

#ifdef DEBUG
    fclose(debugfile);
#endif    

    printf("\n\n\nPine finished\n\n");
    exit(0);
}



/*----------------------------------------------------------------------
     Count up the number of deleted messages on a given mail stream

  Args: stream -- The stream/folder to count deleted messages on

 Result: returns number of messages deleted on that stream
  ----------------------------------------------------------------------*/
count_deleted(stream)
     MAILSTREAM *stream;
{
    int           i, delete_count = 0;
    MESSAGECACHE *mc;

    if(stream->nmsgs <= 0)
      return(0);

    FETCH_ALL_FLAGS(stream);

    for(i = 0 ; i < stream->nmsgs; i++) {
	mc = mail_elt(stream, (long)(i+1));
        if(mc != NULL && mc->deleted)
          delete_count++;
    }
    return(delete_count);
}


/*----------------------------------------------------------------------
     Count up the number of seen but not deleted messages on a given stream

  Args: stream -- The stream

 Result: returns number of seen but not deleted messages
 Bugs: This and the function above should probably be combined into a single
	more general function.
  ----------------------------------------------------------------------*/
count_seen_not_deleted(stream)
     MAILSTREAM *stream;
{
    int           i, cnt = 0;
    MESSAGECACHE *mc;

    if(stream->nmsgs <= 0)
      return(0);

    FETCH_ALL_FLAGS(stream);

    for(i = 0 ; i < stream->nmsgs; i++) {
	mc = mail_elt(stream, (long)(i+1));
        if(mc != NULL && mc->seen && !mc->deleted)
          cnt++;
    }
    return(cnt);
}



/*----------------------------------------------------------------------
     Find the first unread message 

  Args: stream -- The stream/folder to look at message status

 Result: Message number of first unread message
  ----------------------------------------------------------------------*/
MsgNo
first_sorted_unread(stream )
     MAILSTREAM *stream;
{
    MsgNo        i;
    MESSAGECACHE *mc;

    FETCH_ALL_FLAGS(stream);

    for(i = 1 ; i <= stream->nmsgs; i++) {
	mc = mail_elt(stream, (long)(ps_global->sort[i]));
        if(mc != NULL && !mc->seen && !mc->deleted)
          break;
    }
    if(i > stream->nmsgs)
      i = stream->nmsgs;
    dprint(4, (debugfile, "First unseen returning %ld\n", (long)i));
    return(i);
}



/*----------------------------------------------------------------------
     Find the first unread message 

  Args: stream -- The stream/folder to look at message status
        start  -- Place to start looking
        found_is_new -- Set if the message found is actually new.  The only
                        case where this might not be set when the message
                        being returned is the last message in folder.

 Result: Message number of first unread message
  ----------------------------------------------------------------------*/
MsgNo
next_sorted_un(stream, start, found_is_new, read_or_deleted)
     MAILSTREAM *stream;
     long        start;
     int        *found_is_new, read_or_deleted;
{
    MsgNo        i;
    MESSAGECACHE *mc;

    if(found_is_new)
      *found_is_new = 0;

    FETCH_ALL_FLAGS(stream);

    for(i = start ; i <= stream->nmsgs; i++) {
	mc = mail_elt(stream, (long)(ps_global->sort[i]));
        if(mc != NULL && !mc->deleted && (read_or_deleted || !mc->seen)) {
            if(found_is_new)
              *found_is_new = 1;

            break;
        }
    }

    return(min(i, stream->nmsgs));
}



/*----------------------------------------------------------------------
    Return the sorted message number corresponding to 
    the given message number in the c-client stream

  Args: pine_state -- pine state struct containing sort to use
        sortnum    -- sor

 Result: Sorted message number
  ----------------------------------------------------------------------*/
MsgNo
sorted_msgno(pine_state, num)
    struct pine *pine_state;
    MsgNo num;
		     
{
    MsgNo sortnum;

    if(!pine_state->max_msgno)
      return(-1L);

    for(sortnum = 1L;
	sortnum < pine_state->max_msgno && pine_state->sort[sortnum] != num;
	sortnum++)
      ;

    return(sortnum);
}



/*----------------------------------------------------------------------
  See if stream can be used for a mailbox name (courtesy of Mark Crispin)

   Accepts: mailbox name
            candidate stream
   Returns: stream if it can be used, else NIL

  This is called to weed out unnecessary use of c-client streams.
  ----*/
MAILSTREAM *
same_stream(name, stream)
    char *name;
    MAILSTREAM *stream;
{
    char *s,*t,host1[MAILTMPLEN],host2[MAILTMPLEN];

                                /* make sure stream valid */
    if (stream && (s = stream->mailbox) &&
                                /* must be a network stream */
	((*s == '{') || (*s == '*' && s[1] == '{')) &&
                                /* must be a network name */
	((*(t = name) == '{') || (*name == '*' && (t = name)[1] == '{')) &&
                                /* name must be valid for that stream */
	mail_valid_net (t,stream->dtb,host1,NIL) &&
                                /* get host name from stream */
	mail_valid_net (stream->mailbox,stream->dtb,host2,NIL)) {
                                /* return success if same host */
	if (!strucmp (canonical_name(host1),host2))
	  return stream;
    }

    return NIL;                   /* one of the tests failed */
}


/*
 * Simple, handy macro to determine if folder name is remote 
 * (on an imap server)
 */
#define	IS_REMOTE(X)	(*(X) == '{' && *((X) + 1) && *((X) + 1) != '}' \
			 && strchr(((X) + 2), '}'))


/*----------------------------------------------------------------------
  Given a fully qualified folder name, return pointer to
  driver that the name should be created with IF LOCAL

   Accepts: mailbox name
   Returns: stream if it can be used, else NIL

   NOTE: Right now, only used by create when there's nothing else
         there.  open, rename and such just do the usual driver hunt,
         which we specify by handing a NULL stream.

  ----*/
MAILSTREAM *
default_driver(name)
    char *name;
{
#ifdef	DOS
    extern MAILSTREAM dawzproto;	/*  HACK!  */
#else
    extern MAILSTREAM bezerkproto;	/*  HACK!  */
#endif
    /*
     * if the folder name is local, assign default proto
     * BE SURE TO DO THIS if it is local, as {mail,context}_create
     * requires a driver to create in.  For remote cases, NULL is OK
     * as the driver is infered from the name unambiguously.
     */
    return(IS_REMOTE(name) ? NULL : DF_DRIVER_PROTO);
}


/*
 * context_proto - given a folder and context, return the driver to 
 *                 create the folder in.  The stream passed in has to
 *                 be either NULL or a remote stream
 */
MAILSTREAM *
create_proto(stream, context, folder)
    MAILSTREAM *stream;		/* stream to return if IMAP folder */
    CONTEXT_S  *context;
    char       *folder;
{
    if(context_isambig(folder))
      return(IS_REMOTE(context->context) ? stream : context->proto);
    else
      /*
       * Now things get a bit tougher.  If remote, check to see if
       * a stream's already open, otherwise
       */
      return(IS_REMOTE(folder) ? stream : default_driver(folder));
}
	

/*----------------------------------------------------------------------
   Give hint about Other command being optional.  Some people get the idea
   that it is required to use the commands on the 2nd and 3rd keymenus.
   
   Args: none

 Result: message may be printed to status line
  ----*/
void
warn_other_cmds()
{
    static int other_cmds = 0;

    other_cmds++;
    if(((ps_global->first_time_user || ps_global->show_new_version) &&
	      other_cmds % 3 == 0 && other_cmds < 10) || other_cmds % 20 == 0)
        q_status_message(0, 0, 2,
		    "Remember: the \"O\" command is always optional");
}


/*----------------------------------------------------------------------
    Panic pine - call on detected programmatic errors to exit pine

   Args: message -- message to record in debug file and to be printed for user

 Result: The various tty modes are restored
         If debugging is active a core dump will be generated
         Exits Pine

  This is also called from imap routines and fs_get and fs_resize.
  ----*/
void
panic(message)
     char *message;
{
    MoveCursor(ps_global->ttyo->screen_rows -1, 0);
    NewLine();
    end_screen();
    end_keyboard(ps_global != NULL ? F_ON(F_USE_FK,ps_global) : 0);
    end_tty_driver(ps_global);
    end_signals();
    dprint(1, (debugfile, "Pine Panic: %s\n", message));
    fprintf(stderr, "\n\nBug in Pine detected: \"%s\".\nExiting pine.\n",
            message);
#ifdef DEBUG
    if(debug)
      fclose(debugfile);
    coredump();   /*--- If we're debugging get a core dump --*/
#endif

    exit(-1);
    fatal("ffo"); /* BUG -- hack to get fatal out of library in right order*/
}



/*----------------------------------------------------------------------
    Panic pine - call on detected programmatic errors to exit pine, with arg

  Input: message --  printf styule string for panic message (see above)
         arg     --  argument for printf string

 Result: The various tty modes are restored
         If debugging is active a core dump will be generated
         Exits Pine
  ----*/
void
panic1(message, arg)
    char *message;
    char *arg;
{
    char buf[1001];
    if(strlen(message) > 1000) {
        panic("Pine paniced. (Reason for panic is too long to tell)");
    } else {
        sprintf(buf, message, arg);
        panic(buf);
    }
}
