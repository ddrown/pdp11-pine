/*----------------------------------------------------------------------
  $Id: pine.h,v 4.83 1993/12/06 20:07:32 mikes Exp $

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

     pine.h 

     Definitions here are fundamental to pine. 

    No changes should need to be made here to configure pine for one
  site or another.  That is, no changes for local preferences such as
  default directories and other parameters.  Changes might be needed here
  for porting pine to a new machine, but we hope not.

   Includes
     - Various convenience definitions and macros
     - macros for debug printfs
     - data structures used by Pine
     - declarations of all Pine functions

  ====*/


#ifndef _PINE_INCLUDED
#define _PINE_INCLUDED


#define BREAK		'\0'  		/* default interrupt    */
#define BACKSPACE	'\b'     	/* backspace character  */
#define TAB		'\t'            /* tab character        */
#define RETURN		'\r'     	/* carriage return char */
#define LINE_FEED	'\n'     	/* line feed character  */
#define FORMFEED	'\f'     	/* form feed (^L) char  */
#define COMMA		','		/* comma character      */
#define SPACE		' '		/* space character      */
#define DOT		'.'		/* period/dot character */
#define BANG		'!'		/* exclaimation mark!   */
#define AT_SIGN		'@'		/* at-sign character    */
#define PERCENT		'%'		/* percent sign char.   */
#define COLON		':'		/* the colon ..		*/
#define BACKQUOTE	'`'		/* backquote character  */
#define TILDE_ESCAPE	'~'		/* escape character~    */
#define ESCAPE		'\033'		/* the escape		*/

#ifdef	DOS
#define	NEWLINE		"\r\n"		/* local EOL convention...  */
#else
#define	NEWLINE		"\n"		/* necessary for gf_* funcs */
#endif

#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif


#define EXECUTE_ACCESS	01		/* These five are 	   */
#define WRITE_ACCESS	02		/*    for the calls	   */
#define READ_ACCESS	04		/*       to access()       */
#define ACCESS_EXISTS	00		/*           <etc>         */
#define EDIT_ACCESS	06		/*  (this is r+w access)   */

#define	FM_DO_PRINT	01		/* flag for format_message */
#define	FM_NEW_MESS	02		/* ditto		   */

#define	GF_NOOP		0x01		/* flags used by generalized */
#define GF_EOD		0x02		/* filters                   */
#define GF_DATA		0x04		/* See filter.c for more     */
#define GF_ERROR	0x08		/* details                   */
#define GF_RESET	0x10

#define KEY_HELP_LEN   (2 * MAX_SCREEN_COLS)
                          /*When saving escape sequences, etc length of string
                                for the usual key menu at the bottom. used with
                                begin_output_store(), end_output_store() */


#define equal(s,w)	(strcmp(s,w) == 0)
#undef  min
#define min(a,b)	((a) < (b) ? (a) : (b))
#undef  max
#define max(a,b)	((a) > (b) ? (a) : (b))
#define ctrl(c)	        ((c) & 0x1f)	/* control character mapping */
#define plural(n)	((n) == 1 ? "" : "s")
#define lastch(s)	s[strlen(s)-1]
#define no_ret(s)	{ register int xyz; /* varname is for lint */	      \
		          for (xyz=strlen(s)-1; xyz >= 0 && 		      \
				(s[xyz] == '\r' || s[xyz] == '\n'); )	      \
			     s[xyz--] = '\0';                                 \
			}
#define first_word(s,w) (strncmp(s,w, strlen(w)) == 0)
#define ClearLine(n)	MoveCursor(n,0) ; CleartoEOLN()
#define whitespace(c)	((c) == ' ' || (c) == '\t')
#define ok_char(c)	(isalnum(c) || (c) == '-' || (c) == '_')
#define quote(c)	((c) == '"' || (c) == '\'') 
#define onoff(n)	(n == 0 ? "OFF" : "ON")

/*
 * froms and bmap may be any integer type.  C is the next char.
 * bmap should be initialized to 1.
 * froms is incremented by 1 whenever a From_ is found.
 */
#define Find_Froms(froms,bmap,c) { int x;				\
				switch (c) {				\
				  case '\n': case '\r':			\
				    x = 0x1;				\
				    bmap = 0;				\
				    break;				\
				  case 'F':				\
				    x = 0x3;				\
				    break;				\
				  case 'r':				\
				    x = 0x7;				\
				    break;				\
				  case 'o':				\
				    x = 0xf;				\
				    break;				\
				  case 'm':				\
				    x = 0x1f;				\
				    break;				\
				  case ' ':				\
				    x = 0x3f;				\
				    break;				\
				  default:				\
				    x = 0;				\
				    break;				\
				}					\
				bmap = ((x >> 1) == bmap) ? x : 0;	\
				froms += (bmap == 0x3f ? 1 : 0);	\
			     }

/*
 * Useful macro to test if current folder is a bboard type (meaning
 * netnews for now) collection...
 */
#define	IS_NEWS	(ps_global->nr_mode					     \
		 || (context_isambig(ps_global->cur_folder)                  \
		     && ps_global->context_current                           \
		     && (ps_global->context_current->type&FTYPE_BBOARD))     \
		 || (!context_isambig(ps_global->cur_folder)                 \
		     && ps_global->cur_folder[0] == '*'))

#define	READONLY_FOLDER  (ps_global->mail_stream 			     \
			  && ((ps_global->mail_stream->readonly && !IS_NEWS) \
			      || ps_global->mail_stream->anonymous))

/*
 * Macro used to fetch all flags.  Used when counting deleted messages
 * and finding next message with a particular flag set.  The idea is to
 * minimize the number of times we have to fetch all of the flags for all
 * messages in the folder.
 */
#define	FETCH_ALL_FLAGS(s) {long i;					      \
			    if(s == ps_global->mail_stream){		      \
				i = max(ps_global->last_msgno_flagged,1L);    \
				ps_global->last_msgno_flagged = s->nmsgs;     \
			    }						      \
			    else					      \
			      i = 1L;					      \
									      \
			    if(i < s->nmsgs){				      \
				char         sequence[16];		      \
									      \
				sprintf(sequence,"%ld:%ld", i, s->nmsgs);     \
				mail_fetchflags(s, sequence);		      \
			    }						      \
			   }


/*======================================================================
        Key code definitions
  ===*/
#define PF1           0x0100
#define PF2           0x0101
#define PF3           0x0102
#define PF4           0x0103
#define PF5           0x0104
#define PF6           0x0105
#define PF7           0x0106
#define PF8           0x0107
#define PF9           0x0108
#define PF10          0x0109
#define PF11          0x010A
#define PF12          0x010B
      
#define OPF1          0x0110
#define OPF2          0x0111
#define OPF3          0x0112
#define OPF4          0x0113
#define OPF5          0x0114
#define OPF6          0x0115
#define OPF7          0x0116
#define OPF8          0x0117
#define OPF9          0x0118
#define OPF10         0x0119
#define OPF11         0x011A
#define OPF12         0x011B

#define OOPF1          0x0120
#define OOPF2          0x0121
#define OOPF3          0x0122
#define OOPF4          0x0123
#define OOPF5          0x0124
#define OOPF6          0x0125
#define OOPF7          0x0126
#define OOPF8          0x0127
#define OOPF9          0x0128
#define OOPF10         0x0129
#define OOPF11         0x012A
#define OOPF12         0x012B

#define PF2OPF(x)      (x + 0x10)
#define PF2OOPF(x)     (x + 0x20)


/*-- some control codes for arrow keys.. see read_char */
#define KEY_UP        0x0130
#define KEY_DOWN      0x0131
#define KEY_RIGHT     0x0132
#define KEY_LEFT      0x0133
#define KEY_JUNK      0x0134
#define KEY_RESIZE    0x0135  /* Fake key to cause resize */
#define	KEY_HOME      0x0136  /* Extras that aren't used outside DOS */
#define	KEY_END       0x0137
#define	KEY_PGUP      0x0138
#define	KEY_PGDN      0x0139
#define	KEY_DEL       0x013A

#define NO_OP_COMMAND '\0'    /* no-op for short timeouts   */
#define NO_OP_IDLE    0x200   /* no-op for timeouts longer than 25 seconds  */



/*
 * The array is initialized in init.c so the order of that initialization
 * must correspond to the order of the values here.  The order is
 * significant only in that it determines the order that the variables
 * are written into the pinerc file.
 */
#define V_PERSONAL_NAME            0
#define V_USER_ID                  1
#define V_USER_DOMAIN              2
#define V_SMTP_SERVER              3
#define V_INBOX_PATH               4
#define V_INCOMING_FOLDERS         5
#define V_FOLDER_SPEC              6
#define V_NEWS_SPEC                7
#define V_DEFAULT_FCC              8
#define V_MAIL_DIRECTORY           9
#define V_READ_MESSAGE_FOLDER      10
#define V_SIGNATURE_FILE           11
#define V_ADDRESSBOOK              12  /* disabled, address book removed */
#define V_FEATURE_LIST             13
#define V_INIT_CMD_LIST            14
#define V_SAVED_MSG_NAME_RULE      15
#define V_SORT_KEY                 16
#define V_CHAR_SET                 17
#define V_EDITOR                   18
#define V_IMAGE_VIEWER             19
#define V_USE_ONLY_DOMAIN_NAME     20
#define V_PRINTER                  21
#define V_PERSONAL_PRINT_COMMAND   22
#define V_STANDARD_PRINTER         23
#define V_LAST_TIME_PRUNE_QUESTION 24
#define V_LAST_VERS_USED           25
#define V_BUGS_NICKNAME            26
#define V_BUGS_FULLNAME            27
#define V_BUGS_ADDRESS             28
#define V_ELM_STYLE_SAVE           29  /* obsolete */
#define V_HEADER_IN_REPLY          30  /* obsolete */
#define V_FEATURE_LEVEL            31  /* obsolete */
#define V_OLD_STYLE_REPLY          32  /* obsolete */
#define V_COMPOSE_MIME             33  /* obsolete */
#define V_SHOW_ALL_CHARACTERS      34  /* obsolete */
#define V_SAVE_BY_SENDER           35  /* obsolete */


#define VAR_PERSONAL_NAME            vars[V_PERSONAL_NAME].current_val.p
#define VAR_USER_ID                  vars[V_USER_ID].current_val.p
#define VAR_USER_DOMAIN              vars[V_USER_DOMAIN].current_val.p
#define VAR_SMTP_SERVER              vars[V_SMTP_SERVER].current_val.l
#define VAR_INBOX_PATH               vars[V_INBOX_PATH].current_val.p
#define VAR_INCOMING_FOLDERS         vars[V_INCOMING_FOLDERS].current_val.l
#define VAR_FOLDER_SPEC              vars[V_FOLDER_SPEC].current_val.l
#define VAR_NEWS_SPEC                vars[V_NEWS_SPEC].current_val.l
#define VAR_DEFAULT_FCC              vars[V_DEFAULT_FCC].current_val.p
#define VAR_MAIL_DIRECTORY           vars[V_MAIL_DIRECTORY].current_val.p
#define VAR_READ_MESSAGE_FOLDER      vars[V_READ_MESSAGE_FOLDER].current_val.p
#define VAR_SIGNATURE_FILE           vars[V_SIGNATURE_FILE].current_val.p
#define VAR_FEATURE_LIST             vars[V_FEATURE_LIST].current_val.l
#define VAR_INIT_CMD_LIST            vars[V_INIT_CMD_LIST].current_val.l
#define VAR_SAVED_MSG_NAME_RULE      vars[V_SAVED_MSG_NAME_RULE].current_val.p
#define VAR_SORT_KEY                 vars[V_SORT_KEY].current_val.p
#define VAR_CHAR_SET                 vars[V_CHAR_SET].current_val.p
#define VAR_EDITOR                   vars[V_EDITOR].current_val.p
#define VAR_IMAGE_VIEWER             vars[V_IMAGE_VIEWER].current_val.p
#define VAR_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].current_val.p
#define VAR_PRINTER                  vars[V_PRINTER].current_val.p
#define VAR_PERSONAL_PRINT_COMMAND   vars[V_PERSONAL_PRINT_COMMAND].current_val.p
#define VAR_STANDARD_PRINTER         vars[V_STANDARD_PRINTER].current_val.p
#define VAR_LAST_TIME_PRUNE_QUESTION vars[V_LAST_TIME_PRUNE_QUESTION].current_val.p
#define VAR_LAST_VERS_USED           vars[V_LAST_VERS_USED].current_val.p
#define VAR_BUGS_NICKNAME            vars[V_BUGS_NICKNAME].current_val.p
#define VAR_BUGS_FULLNAME            vars[V_BUGS_FULLNAME].current_val.p
#define VAR_BUGS_ADDRESS             vars[V_BUGS_ADDRESS].current_val.p
  /* Elm style save is obsolete in Pine 3.81 (see saved msg name rule) */   
#define VAR_ELM_STYLE_SAVE           vars[V_ELM_STYLE_SAVE].current_val.p
  /* Header in reply is obsolete in Pine 3.83 (see feature list) */   
#define VAR_HEADER_IN_REPLY          vars[V_HEADER_IN_REPLY].current_val.p
  /* Feature level is obsolete in Pine 3.83 (see feature list) */   
#define VAR_FEATURE_LEVEL            vars[V_FEATURE_LEVEL].current_val.p
  /* Old style reply is obsolete in Pine 3.83 (see feature list) */   
#define VAR_OLD_STYLE_REPLY          vars[V_OLD_STYLE_REPLY].current_val.p
  /* Compose MIME is obsolete in Pine 3.81 */
#define VAR_COMPOSE_MIME             vars[V_COMPOSE_MIME].current_val.p
  /* Show all characters is obsolete in Pine 3.83 */   
#define VAR_SHOW_ALL_CHARACTERS      vars[V_SHOW_ALL_CHARACTERS].current_val.p
  /* Save by sender is obsolete in Pine 3.83 (see saved msg name rule) */   
#define VAR_SAVE_BY_SENDER           vars[V_SAVE_BY_SENDER].current_val.p

/* Feature list support (can have up to LARGEST_BITMAP features) */
/* Is feature "feature" turned on? */
#define F_ON(feature,ps)   (bitnset((feature),(ps)->feature_list))
#define F_OFF(feature,ps)  (!F_ON(feature,ps))
#define F_TURN_ON(feature,ps)   (setbitn((feature),(ps)->feature_list))
#define F_TURN_OFF(feature,ps)  (clrbitn((feature),(ps)->feature_list))
/* turn off or on depending on value */
#define F_SET(feature,ps,value) ((value) ? F_TURN_ON((feature),(ps))       \
					 : F_TURN_OFF((feature),(ps)))

/* list of feature numbers (which bit goes with which feature) */
#define F_OLD_GROWTH                0
#define F_ENABLE_FULL_HDR           1
#define F_ENABLE_PIPE               2
#define F_ENABLE_TAB_COMPLETE       3
#define F_QUIT_WO_CONFIRM           4
#define F_ENABLE_JUMP               5
#define F_ENABLE_ALT_ED             6
#define F_ENABLE_BOUNCE             7
#define F_ENABLE_APPLY              8
#define F_ENABLE_FLAG               9
#define F_ENABLE_ZOOM              10
#define F_MIME_FORWARD             11
#define F_CAN_SUSPEND              12
#define F_EXPANDED_FOLDERS         13
#define F_USE_FK                   14
#define F_INCLUDE_HEADER           15
#define F_SIG_AT_BOTTOM            16
#define F_DEL_SKIPS_DEL            17
#define F_AUTO_EXPUNGE             18
#define F_AUTO_READ_MSGS           19



/*======================================================================
       Macros for debug printfs 
   n is debugging level:
       1 logs only highest level events and errors
       2 logs events like file writes
       3
       4 logs each command
       5
       6 
       7 logs details of command execution (7 is highest to run any production)
         allows core dumps without cleaning up terminal modes
       8
       9 logs gross details of command execution

 the following macro is as suggested by Larry McVoy.  Thanks! *
  ====*/
# ifdef DEBUG
#  define   dprint(n,x)		{ 				\
				   if (debugfile && debug >= (n))  { \
				     fprintf x ; 		\
                                     if(debug > 7)              \
                                       fflush(debugfile);       \
				   }				\
				}
# else
#  define   dprint(n,x)
# endif



/*======================================================================
            Various structures that Pine uses
 ====*/

struct ttyo {
    int  screen_rows,
         screen_cols;
};


/*
 * Flags to indicate context (i.e., folder collection) use
 */
#define	CNTXT_PRIME	0x01			/* primary collection	    */
#define	CNTXT_SECOND	0x02			/* secondary collection     */
#define	CNTXT_NEWS	0x04			/* news group collection    */
#define	CNTXT_PSEUDO	0x08			/* fake folder entry exists */
#define	CNTXT_INCMNG	0x10			/* inbox collection	    */
#define	CNTXT_SAVEDFLT	0x20			/* default save collection  */
#define	CNTXT_PARTFIND	0x40			/* partial find done        */
#define	CNTXT_NOFIND	0x80			/* no find done in context  */


/*
 * Flags indicating folder collection type
 */
#define	FTYPE_LOCAL	0x01			/* Local folders	  */
#define	FTYPE_REMOTE	0x02			/* Remote folders	  */
#define	FTYPE_SHARED	0x04			/* Shared folders	  */
#define FTYPE_BBOARD    0x08			/* Bulletin Board folders */
#define	FTYPE_OLDTECH	0x10			/* Not accessed via IMAP  */
#define	FTYPE_ANON	0x20			/* anonymous access       */


/*------------------------------
    Stucture to keep track of the various folder collections being
    dealt with.
  ----*/
typedef struct context {
    char           *context;			/* context string	   */
    char           *label[4];			/* description lines	   */
    char           *nickname;			/* user provided nickname  */
    char            last_folder[MAXFOLDER+1];	/* last folder used        */
    void           *folders;			/* folder data             */
    unsigned short  type;			/* type of collection      */
    unsigned short  use;			/* use flags for context   */
    unsigned short  num;			/* context number in list  */
    int		    d_line;			/* display line for labels */
    MAILSTREAM     *proto;			/* prototype driver        */
    struct context *next;			/* next context struct	   */
} CONTEXT_S;


/*------------------------------
   Used for displaying as well as
   keeping track of folders. 
   Currently about 25 bytes.
  ----*/
typedef struct folder {
    char     prefix[8];				/* news prefix?		   */
    unsigned char   name_len;			/* name length		   */
    unsigned short msg_count;			/* Up to 65,000 messages   */
    unsigned short unread_count;
    unsigned short d_col;
    unsigned short d_line;
    char     *nickname;				/* folder's short name     */
    char     name[1];				/* folder's name           */
} FOLDER_S;


typedef long MsgNo;

struct variable {
    char *name;
    unsigned  is_obsolete:1;	/* variable read in, not written unless set */
    unsigned  is_used:1;	/* Some variables are disabled */
    unsigned  been_written:1;
    unsigned  is_user:1;
  /*unsigned  is_group:1; */
    unsigned  is_global:1;
    unsigned  is_list:1;	/* flag indicating variable is a list */
    char     *description;
    union {
	char *p;		/* pointer to single string value */
	char **l;		/* pointer to list of string values */
    } current_val;
    union {
	char *p;		/* pointer to single string value */
	char **l;		/* pointer to list of string values */
    } user_val;
  /*char     *group_val;*/
    union {
	char *p;		/* pointer to single string value */
	char **l;		/* pointer to list of string values */
    } global_val;
    union {
	char *p;		/* pointer to single string value */
	char **l;		/* pointer to list of string values */
    } command_line_val;
};


typedef enum {ShowAll, ShowParts, ShowNone} MimeShow;

struct attachment {
    char           *description;
    BODY           *body;
    unsigned int    can_display:1;
    unsigned int    shown:1;
    MimeShow        show;
    char            number[20];
    char            size[25];
};


/*------
   A key menu has two ways to turn on and off individual items in the menu.
   If there is a null entry in the key_menu structure for that key, then
   it is off.  Also, if the passed bitmap has a zero in the position for
   that key, then it is off.  This means you can usually set all of the
   bitmaps and only turn them off if you want to kill a key that is normally
   there otherwise.
   Each key_menu is an array of keys with a multiple of 12 number of keys.
  ------*/
#define LARGEST_BITMAP 48
#define BM_SIZE (LARGEST_BITMAP / 8)
#define BM_MENUS (LARGEST_BITMAP / 12)
#define _BITCHAR(bit) ((bit) / 8)
#define _BITBIT(bit) (1 << ((bit) % 8))
typedef unsigned char BITMAP[BM_SIZE];
/* is bit set? */
#define bitnset(bit,map) ((map)[_BITCHAR(bit)] & _BITBIT(bit))
/* set bit */
#define setbitn(bit,map) ((map)[_BITCHAR(bit)] |= _BITBIT(bit))
/* clear bit */
#define clrbitn(bit,map) ((map)[_BITCHAR(bit)] &= ~_BITBIT(bit))
/* clear entire bitmap */
#define clrbitmap(map)   memset((void *)(map), 0, BM_SIZE)
/* set entire bitmap */
#define setbitmap(map)   memset((void *)(map), 0xff, BM_SIZE)
/*------
  Argument to draw_keymenu().  These are to identify which of the possibly
  multiple sets of twelve keys should be shown in the keymenu.  That is,
  a keymenu may have 24 or 36 keys, so that there are 2 or 3 different
  screens of key menus for that keymenu.  FirstMenu means to use the
  first twelve, NextTwelve uses the one after the previous one, SameTwelve
  uses the same one, AParticularTwelve uses the value in the "which"
  argument to draw_keymenu().
  ------*/
typedef enum {FirstMenu, NextTwelve, SameTwelve, AParticularTwelve} OtherMenu;
/*
 * In the next iteration we want to move column out of this structure since
 * it is the only dynamic data here.  That way we should be able to
 * consolidate some of the static data that initializes the keymenus.
 */
struct key {
       char *name;  /* the short name */
       char *label; /* the descriptive label */
       short column;
};
struct key_menu {
    unsigned char how_many;  /* how many separate sets of 12                */
    unsigned char which;     /* which of the sets of twelve are we using    */
    short width[BM_MENUS];   /* this ought to be of size how_many           */
    struct key *keys;  /* points to an array of structs of size how_many*12 */
    BITMAP bitmap;
};


typedef enum {SortDate, SortArrival, SortFrom, SortSubject,
                SortTo, SortCc, SortSize, EndofList}   SortOrder;
typedef enum {Sapling, Seedling, Seasoned} FeatureLevel;
/*----------------------------------------------------------------------
   This structure sort of takes the place of global variables or perhaps
is the global variable.  (It can be accessed globally as ps_global.  One
advantage to this is that as soon as you see a reference to the structure
you know it is a global variable. 
   In general it is treated as global by the lower level and utility
routines, but it is not treated so by the main screen driving routines.
Each of them receives it as an argument and then sets ps_global to the
argument they received.  This is sort of with the thought that things
might be coupled more loosely one day and that Pine might run where there
is more than one window and more than one instance.  But we haven't kept
up with this convention very well.
 ----*/
  
struct pine {
    /*-- see the loop at the end of main() for how these are used --*/
    void       (*next_screen)();   /* NULL means stay in current screen */
    void       (*prev_screen)();
    void       (*redrawer)();

    char        *return_string;
    char        *error_message;

    CONTEXT_S   *context_list;		/* list of user defined contexts */
    CONTEXT_S   *context_current;	/* default open context          */
    CONTEXT_S   *context_last;		/* most recently open context    */
    CONTEXT_S   *context_write;		/* default save context          */
    unsigned     context_index;		/* current folder's index        */

    char         inbox_name[MAXFOLDER+1];
    MAILSTREAM  *inbox_stream;
    long 	 inbox_raw_msgno; 	/* msgno in imap stream when we left */
    long         inbox_max_msgno;	/* current folder is *not* inbox  */
    long         inbox_new_mail_count;
    long         inbox_expunge_count;
    int          inbox_changed;
    SortOrder	 inbox_sort_order;
    
    MAILSTREAM  *mail_stream;
    long         current_sorted_msgno;
    long         new_current_sorted_msgno;
    long         max_msgno;
    long         last_msgno_flagged;
    long         new_mail_count;
    long         expunge_count;
    int          mail_box_changed;
    long         top_of_screen_sorted_msgno;
    char         cur_folder[MAXPATH+1];
    struct attachment
                *atmts;
    int          atmts_allocated;

    long        *sort;
    long         sort_allocated;

    char        *folders_dir;
    unsigned     mangled_footer:1; 	/* footer needs repainting */
    unsigned     mangled_header:1;	/* header needs repainting */
    unsigned     mangled_body:1;	/* body of screen needs repainting */
    unsigned     mangled_screen:1;	/* whole screen needs repainting */
    unsigned     in_init_seq:1;		/* executing initial cmd list */
    unsigned     save_in_init_seq:1;
    unsigned     dont_use_init_cmds:1;	/* use keyboard input when true */
    unsigned     scroll_text_level:2;	/* # of times scrolltool rentered */
    unsigned     status_changed:1;	/* So above func. knows status change*/
    unsigned     show_folders_dir:1;	/* show folders dir path when showing
                                                   folder names */
    unsigned     io_error_on_stream:1;	/* last write on mail_stream failed */
    unsigned     restricted:1;
    unsigned     reverse_sort:1;
    unsigned     current_reverse_sort:1;
    unsigned     nr_mode:1;
    unsigned     anonymous:1;           /* for now implys nr_mode */
    unsigned     save_by_from:1;
    unsigned     save_by_sender:1;
    unsigned     save_by_recipient:1;
    unsigned     save_by_last_saved:1;
    unsigned     full_header:1;         /* display full headers */
    unsigned     orig_use_fkeys:1;

    unsigned     try_to_create:1;     /* flag to try mail_create during save */
    unsigned     use_full_hostname:1; /* Use full hostname vs just domain */
    unsigned     low_speed:1;         /* Special screen painting 4 low speed */
    unsigned     dead_inbox:1;
    unsigned     dead_stream:1;
    unsigned     noticed_dead_inbox:1;
    unsigned     noticed_dead_stream:1;
    unsigned     compose_mime:1;
    unsigned     show_new_version:1;
    unsigned     first_time_user:1;
    unsigned     do_local_lookup:1;  /* look up in passwd even if using
                                           user defined domain */
    BITMAP       feature_list;       /* a bitmap of all the features */

    /*--- Command line flags, modify only on start up ---*/
    unsigned     start_in_index:1;
    unsigned     start_entry;

    unsigned     noshow_error:1;
    unsigned     noshow_warn:1;

    unsigned     painted_body_on_startup:1;
    unsigned     painted_footer_on_startup:1;
    unsigned     open_readonly_on_startup:1;

#ifdef	DOS
    unsigned     blank_user_id:1;
    unsigned     blank_personal_name:1;
    unsigned     blank_user_domain:1;
#endif

    int         *initial_cmds;         /* cmds to execute on startup */

    char         c_client_error[300];  /* when nowhow_error is set and PARSE */

    struct ttyo *ttyo;

    char        *passwd,         /* Encrypted login passwd for keyboard lock */
                *home_dir,
                *hostname,       /* Fully qualified hostname */
                *localdomain,    /* The (DNS) domain this host resides in */
                *userdomain,     /* The per user domain from .pinerc or */
                *maildomain,     /* Domain name for most uses */
#ifdef	DOS
                *pine_exe,	 /* argv[0] as provided by DOS */
#else
                *pine_conf,	 /* Location of global pine.conf */
#endif
                *pinerc;	 /* Location of user's pinerc */

    SortOrder    sort_order, current_sort_order,
                 sort_types[20];

    int          last_expire_year, last_expire_month;

    char         last_error[500];
    struct variable *vars;
};


struct outgoing_message {
    ADDRESS *to,
            *from,
            *cc,
            *bcc,
            *sender,
            *reply_to,
            *return_path;
    char    *date,
            *subject,
            *message_id,
            **optional_lines;
};


/*------------------------------
  Structure to pass optionally_enter to tell it what keystrokes
  are special
  ----*/

typedef struct esckey {
    char *name;
    char *label;
    int  ch;
    int  rval;
} ESCKEY_S;


struct date {
    int  sec, minute, hour, day, month, year, hours_off_gmt, min_off_gmt, wkday;
    char zone_name[15];
};



typedef enum {OpenFolder, SaveMessage, FolderMaint, GetFcc} FolderFun;
typedef enum {FolderName, MessageNumber, TextPercent, PageNumber} TitleBarType;
typedef enum {ComposerHelpText, HelpText, NewsText, MessageText, AttachText,
		WhoText, NetNewsText} TextType;
typedef enum {CharStarStar, CharStar, FileStar, 
		TmpFileStar, PicoText} SourceType;
typedef enum {GoodTime, BadTime, VeryBadTime, DoItNow} CheckPointTime;
typedef enum {IsText, IsText8, IsBinary} FileTypes;
typedef enum {AppendPostpone, AppendInterrupted, AppendSendmail} AppendTypes;
typedef enum {InLine, QStatus} DetachErrStyle;

/*
 * typedefs of generalized filters used by gf_pipe
 */
typedef int (*gf_io_t)();	/* type of get and put char function     */
typedef void (*filter_t)();	/* type of filters for piping            */
typedef struct filter_s {	/* type to hold data for filter function */
    filter_t f;			/* next function in the pipe             */
    struct filter_s *next;	/* next filter to call                   */
    long     n;			/* number of chars seen                  */
    short    f1;		/* flags                                 */
    short    f2;		/* second place for flags                */
    unsigned char t;		/* temporary char                        */
    char     *line;		/* place for temporary storage           */
    char     *linep;		/* pointer into storage space            */
    int       fd;               /* File descriptor, if needed            */
} FILTER_S;

/*
 * typedef used by storage object routines
 */
typedef struct store_object {
    void          *txt;		/* pointer to start of it		 */
    unsigned char *txtp;	/* pointer to something inside it	 */
    SourceType     src;		/* what we're copying into		 */
    long           size;	/* it's current size			 */
    long           used;	/* amount used so far			 */
    char          *name;	/* optional object name			 */
    short          flags;	/* flags relating to object use		 */
} STORE_S;


/*======================================================================
    Declarations of all the Pine functions.
 ====*/
  
#ifdef ANSI

/*-- standard libraries --*/
FILE *popen(const char *, const char *);

/*-- addrutil.c --*/
int  build_address(char *, char *, char **);
char *addr_list_string(ADDRESS *, int);
char *addr_string(ADDRESS *);
int   address_is_us(ADDRESS *, struct pine *);

/*-- args.c --*/
char *pine_args(struct pine *, int, char **, char ***);

/*--- filter.c ---*/
STORE_S *so_get(SourceType, char *, int);
void	 so_give(STORE_S **);
int      so_readc(unsigned char *, STORE_S *);
int      so_writec(int, STORE_S *);
int	 so_puts(STORE_S *, char *);
int      so_seek(STORE_S *, long, int);
int	 so_release(STORE_S *);
void	*so_text(STORE_S *);
void     gf_filter_init(void);
char	*gf_pipe(gf_io_t, gf_io_t);
void	 gf_set_so_readc(gf_io_t *, STORE_S *);
void	 gf_set_so_writec(gf_io_t *, STORE_S *);
void	 gf_set_readc(gf_io_t *, void *, unsigned long, SourceType);
void	 gf_set_writec(gf_io_t *, void *, unsigned long, SourceType);
int      gf_puts(char *, gf_io_t);
void	 gf_set_terminal(gf_io_t);
void	 gf_binary_b64(FILTER_S *, int, int);
void	 gf_b64_binary(FILTER_S *, int, int);
void	 gf_qp_8bit(FILTER_S *, int, int);
void	 gf_8bit_qp(FILTER_S *, int, int);
void	 gf_rich2plain(FILTER_S *, int, int);
void	 gf_rich2plain_opt(int);
void	 gf_iso_filter(FILTER_S *, int, int);
void	 gf_iso_filter_opt(int);
void	 gf_wrap(FILTER_S *, int, int);
void	 gf_wrap_filter_opt(int);
void	 gf_busy(FILTER_S *, int, int);
void	 gf_nvtnl_local(FILTER_S *, int, int);
void	 gf_local_nvtnl(FILTER_S *, int, int);
void	 gf_prefix(FILTER_S *, int, int);
void	 gf_prefix_opt(char *);
#ifdef	DOS
void	 gf_translate(FILTER_S *, int, int);
void	 gf_translate_opt(unsigned char *, unsigned);
#endif

/*--- folder.c ---*/
void        folder_screen(struct pine *);
int         folders_for_fcc(char *);
int         folder_lister(struct pine *,FolderFun,CONTEXT_S *,CONTEXT_S **,
                          char *);
char       *pretty_fn(char *);
int         folder_exists(char *, char *);
int         folder_create(char *, CONTEXT_S *);
int         folder_complete(CONTEXT_S *, char *);
void        init_folders();
void       *find_folders(char *, char *);
void        find_folders_in_context(CONTEXT_S *, char *);
CONTEXT_S  *default_save_context(CONTEXT_S *);
FOLDER_S   *folder_entry(int, void *);
FOLDER_S   *new_folder(char *);
int         folder_insert(int, FOLDER_S *, void *);
int         folder_index(char *, void *);
char       *folder_is_nick(char *, void *);
char       *next_folder(char *, char *, CONTEXT_S *);

/*-- help.c --*/
#ifdef	DOS
void helper(short, char *title, int); 
char **get_help_text(short, int *lines); 
#else
void helper(char *text[], char *title, int); 
#endif
void print_all_help(void);

/*-- init.c --*/
void init_vars(struct pine *);
char *expand_variables(char *, char *);
int  init_username(struct pine *);  
int  init_hostname(struct pine *);  
int  write_pinerc(struct pine *);
void dump_global_conf(void);
int  set_variable(int, char *);
int  set_variable_list(int, char **);
int  init_mail_dir(char *); 
void init_save_defaults();
int  expire_sent_mail(void);
char **parse_list(char *, int, char **);
#ifdef DEBUG
void init_debug(void);
#endif

/*---- mailcmd.c ----*/
int   process_cmd(int, int, int, int *);
int   save_msg(int, MESSAGECACHE *, int *, long);
char *detach(long, BODY *, char *, long *, gf_io_t, filter_t *);
long  jump_to(int, int);
char *broach_folder(int, int, CONTEXT_S **);
int   do_broach_folder(char *, CONTEXT_S *);
void  expunge_and_close(MAILSTREAM *, char *);
int   export_message(int, long);
int   print_message(long);
long  search_headers(int, long, long);

/*--- mailindx.c ---*/
void  mail_index_screen(struct pine *);
void  clear_index_cache(void);
void  clear_index_cache_ent(long);
int   build_index_cache(long);
#ifdef	DOS
void  flush_index_cache(void);
#endif
char *build_header_line(long);
void  build_header_cache(void);
void  redraw_index_body(void);
void  do_index_border(int, int, int, int *);
char *sort_name(SortOrder);
void  sort_current_folder();


/*--- mailview.c ---*/
void mail_view_screen(struct pine *);
int  match_fields(char *, char **);
void scrolltool(void *, char *, int *, TextType, SourceType,
							struct attachment *);
void end_scroll_text(void);
void format_scroll_text(void);
int  get_scroll_text_lines(void);
void  redraw_scroll_text(void);
void  scroll_scroll_text(int, int);
int   search_scroll_text(int, char *);
int   search_text(int, int);
void  set_scroll_text(void *, int, int, int, SourceType);
char *body_type_names(int);
char *type_desc(int, char *, PARAMETER *, int);
int  format_message(long, ENVELOPE *, BODY *, int, gf_io_t);
int  decode_text(struct attachment *, long, gf_io_t, DetachErrStyle, int);


/*--newmail.c --*/
long new_mail(int *, int, int);
int  check_point(CheckPointTime);
void check_point_change();
void reset_check_point(void);
void zero_new_mail_count(void);

/*-- os.c --*/
int can_access(char *, int);
long file_size(char *);
int is_writable_dir(char *);
int create_mail_dir(char *);
int rename_file(char *, char *);
void build_path(char *, char *, char *);
char *last_cmpnt(char *);
int   expand_foldername(char *);
char *fnexpand(char *, int);
char *filter_filename(char *);
long  disk_quota(char *, int *);
char *read_file(char *);
FILE *create_tmpfile(void);
char *temp_nam(char *, char *);
void  coredump(void);
void  getdomainnames(char *, int, char *, int);
char *canonical_name(char *);
int   have_job_control(void);
int   stop_process(void);
char *error_description(int);
char *get_system_login(void);
char *get_system_fullname(void);
char *get_system_homedir(void);
char *get_system_passwd(void);
char *local_name_lookup(char *);
int   change_passwd(void);
int   mime_can_display(int, char *, PARAMETER *);
void  gf_play_sound(FILTER_S *, int, int);
#ifdef	DOS
void *dos_cache(MAILSTREAM *, long, long);
char *dos_gets(readfn_t, void *, unsigned long);
#endif

/*--- other.c ---*/
int  lock_keyboard(void);

/*-- pine.c --*/
void main_menu_screen(struct pine *);
void news_screen(struct pine *);
void quit_screen(struct pine *);
int  count_deleted(MAILSTREAM *);
int  count_seen_not_deleted(MAILSTREAM *);
void who_screen(struct pine *);
void panic(char *);
void panic1(char *, char *);
MAILSTREAM *same_stream(char *, MAILSTREAM *);
MAILSTREAM *default_driver(char *);
MAILSTREAM *create_proto(MAILSTREAM *, CONTEXT_S *, char *);
MsgNo       first_sorted_unread(MAILSTREAM *);
MsgNo       next_sorted_un(MAILSTREAM *, long, int *, int);
MsgNo       sorted_msgno(struct pine *, MsgNo);
void warn_other_cmds();

/*-- print.c --*/
int  open_printer(char *);
void close_printer(void);
int  print_char(int);
void print_text(char *);
void print_text1(char *, char *);
void print_text2(char *, char *, char *);
void print_text3(char *, char *, char *, char *);
     
/*-- reply.c --*/
void     reply(struct pine *, long, ENVELOPE *, BODY *);
void     forward(struct pine *, long, ENVELOPE *, BODY *);
char    *generate_message_id(struct pine *);
void     pretty_addr_string(char *, ADDRESS *, char *, gf_io_t);
ADDRESS *first_addr(ADDRESS *);
char    *get_signature();
PARAMETER *copy_parameters(PARAMETER *);

/*-- screen.c --*/
void output_keymenu(struct key_menu *, BITMAP, int, int);
void format_keymenu(struct key_menu *, BITMAP, int, OtherMenu, int);
void draw_keymenu(struct key_menu *, BITMAP, int, int, int, OtherMenu, int);
void blank_keymenu(int, int);
void redraw_keymenu(void);
char *status_string(MESSAGECACHE *);
char *set_titlebar(char *, int, TitleBarType, long, int, int);
void push_titlebar_state();
void pop_titlebar_state();
void redraw_titlebar(void);
void update_titlebar_message(int);
void update_titlebar_percent(int);
void update_titlebar_page(int);
int  update_titlebar_status(MESSAGECACHE *);

/*-- send.c --*/
void  compose_screen(struct pine *); 
void  compose_mail(char *);
void  pine_send(ENVELOPE *, BODY **, char *, char *, long, void *);
void  pine_free_body(BODY **);

/*-- signals.c --*/
void init_signals(void); 
void do_suspend(struct pine *);
void end_signals(void);
SigType hup_signal();
SigType term_signal();

/*-- status.c --*/
void display_message(int);
int messages_queued(void);
void d_q_status_message(void);
void q_status_message( int, int, int, char *);
void q_status_message1(int, int, int, char *, void *);
void q_status_message2(int, int, int, char *, void *, void *);
void q_status_message3(int, int, int, char *, void *, void *, void *);
void q_status_message4(int, int, int, char *, void *, void *, void *, void *);
void q_status_message7(int, int, int, char *, void *, void *, void *, void *,
                       void *, void *, void *);
void flush_status_messages(void);
void mark_status_dirty(void);
int want_to(char *, int, int, HelpType, int);
int radio_buttons(char *, int, int, char *, char **, int, int, HelpType, int);


/*-- strings.c --*/
char *rplstr(char *, int, char *);
void sqzspaces(char *);
void removing_trailing_white_space(char *);
void removing_leading_white_space(char *);
int strucmp(char *, char *);
int struncmp(char *, char *, int);
char *srchstr(char *, char *);
char *strindex(char *, int);
char *strrindex(char *, int);
char *strcat_nocr(char *, char *);
void sstrcpy(char **, char *);
void crlf2lf(char *);
char *month_abbrev(int);
char *week_abbrev(int);
int   month_num(char *);
void  parse_date(char *, struct date *);
int   compare_dated(struct date *, struct date *);
void  convert_to_gmt(struct date *);
char *pretty_command(int);
char *repeat_char(int, int);
char *comatose(long);
char *byte_string(long);
char *enth_string(int);
char *long2string(long);
char *int2string(int);
char *rfc822date_to_ctime(char *);
char *lf2crlf(char *);

/*-- ttyin.c--*/
int read_char(int);
int read_command();
int optionally_enter(char *,int,int,int,int,int,char *,ESCKEY_S *,HelpType,int);
int init_tty_driver(struct pine *);
void end_tty_driver(struct pine *);
int Raw(int);
void end_keyboard(int);
void init_keyboard(int);
int validatekeys(int);

/*-- ttyout.c --*/
void get_windsize(struct ttyo *);
int BeginScroll(int, int);
void EndScroll(void);
int  ScrollRegion( int);
int  Writechar(unsigned int, int);
void Write_to_screen(char *);
void PutLine0(int, int, char *);
void PutLine0n8b(int, int, char *, int);
void PutLine1(int, int, char *, void *);
void PutLine2(int, int, char *, void *, void *);
void PutLine3(int, int, char *, void *, void *, void *);
void PutLine4(int, int, char *, void *, void *, void *, void *);
void PutLine5(int, int, char *, void *, void *, void *, void *, void *);
int  CenterLine(int, char *);
void Stripe0(int, int, int,  char *);
void Stripe3(int, int, int, char *, char *, char *, char *);
void CleartoEOLN(void);
int  CleartoEOS(void);
void ClearScreen(void);
void MoveCursor(int, int);
void NewLine(void);
void StartInverse(void);
void EndInverse(void);
void StartUnderline(void);
void EndUnderline(void);
void StartBold(void);
void EndBold(void);
int  config_screen(struct ttyo **);
void init_screen(void);
void end_screen(void);
void outchar(int);
void clear_cursor_pos(void);
int  InsertChar(int);
int  DeleteChar(int);

/*----- our own various library things, some from imap, other from ../lib --*/
char *cpystr(char *);

#define SCREEN_FUN_NULL ((void (*) (void *))NULL)

#else

/*-- addrutil.c --*/
int  build_address();
char *addr_list_string();
char *addr_string();
int   address_is_us();

/*-- args.c --*/
char *pine_args();

/*--- filter.c ---*/
STORE_S *so_get();
void	 so_give();
int      so_readc();
int      so_writec();
int	 so_puts();
int      so_seek();
void	*so_text();
int	 so_release();
void	 gf_set_readc();
void	 gf_set_writec();
void	 gf_set_so_readc();
void	 gf_set_so_writec();
int      gf_puts();
void	 gf_set_terminal();
char	*gf_pipe();
void	 gf_binary_b64();
void	 gf_b64_binary();
void	 gf_qp_8bit();
void	 gf_8bit_qp();
void	 gf_rich2plain();
void	 gf_richtext_filter_opt();
void	 gf_iso_filter();
void	 gf_iso_filter_opt();
void	 gf_wrap();
void	 gf_wrap_filter_opt();
void	 gf_busy();
void	 gf_nvtnl_local();
void	 gf_local_nvtnl();
void	 gf_prefix();
void	 gf_prefix_opt();

/*--- folder.c ---*/
void        folder_screen();
int         folders_for_fcc();
int         folder_lister();
char       *pretty_fn();
int         folder_exists();
void        init_folders();
void       *find_folders();
void        find_folders_in_context();
CONTEXT_S  *default_save_context();
int         folder_create();
int         folder_complete();
FOLDER_S   *folder_entry();
FOLDER_S   *new_folder();
int         folder_insert();
int         folder_index();
char       *folder_is_nick();
char       *next_folder();

/*-- help.c --*/
void helper(); 
void print_all_help();

/*-- init.c --*/
void init_vars();
char *expand_variables();
int  init_username();  
int  init_hostname();  
int  write_pinerc();
void dump_global_conf();
int  set_variable();
int  set_variable_list();
int  init_mail_dir(); 
void init_save_defaults();
int  expire_sent_mail();
char **parse_list();
#ifdef DEBUG
void init_debug();
#endif

/*---- mailcmd.c ----*/
int process_cmd();
int save_msg();
char *detach();
long jump_to();
char *broach_folder();
int do_broach_folder();
void expunge_and_close();
int  export_message();
int  print_message();
int  save_msg();
long search_headers();


/*--- mailindx.c ---*/
void  mail_index_screen();
void  build_header_cache();
void  clear_index_cache();
void  clear_index_cache_ent();
int   build_index_cache();
char *build_header_line();
void  redraw_index_body();
void  do_index_border();
char *sort_name();
void  sort_current_folder();


/*--- mailview.c ---*/
void mail_view_screen();
int  match_fields();
void scrolltool();
void end_scroll_text();
void format_scroll_text();
int  get_scroll_text_lines();
void  redraw_scroll_text();
void  scroll_scroll_text();
int   search_scroll_text();
int   search_text();
void  set_scroll_text();
char *body_type_names();
char *type_desc();
int   format_message();
int   decode_text();

/*--newmail.c --*/
long new_mail();
int check_point();
void check_point_change();
void reset_check_point();
void zero_new_mail_count();

/*-- os.c --*/
int   can_access();
long  file_size();
int   is_writable_dir();
int   create_mail_dir();
int   rename_file();
void  build_path();
char *last_cmpnt();
int   expand_foldername();
char *fnexpand();
char *filter_filename();
long  disk_quota();
char *read_file();
FILE *create_tmpfile();
char *temp_nam();
void  coredump();
void  getdomainnames();
int   have_job_control();
int   stop_process();
char *error_description();
char *get_system_login();
char *get_system_fullname();
char *get_system_homedir();
char *get_system_passwd();
char *local_name_lookup();
int   change_passwd();
int   mime_can_display();
void  gf_play_sound();
char *canonical_name();
#ifdef	DOS
void *dos_cache();
char *dos_gets();
#endif

/*--- other.c ---*/
int  lock_keyboard();

/*-- pine.c --*/
void main_menu_screen();
void news_screen();
void quit_screen();
int  count_deleted();
int  count_seen_not_deleted();
void who_screen();
void panic();
void panic1();
MAILSTREAM *same_stream();
MAILSTREAM *default_driver();
MAILSTREAM *create_proto();
MsgNo       first_sorted_unread();
MsgNo       next_sorted_un();
MsgNo       sorted_msgno();
void warn_other_cmds();

/*-- print.c --*/
int  open_printer();
void close_printer();
int  print_char();
void print_text();
void print_text1();
void print_text2();
void print_text3();
     
/*-- reply.c --*/
void  reply();
void  forward();
char *generate_message_id();
void  pretty_addr_string();
ADDRESS *first_addr();
char *get_signature();
PARAMETER *copy_parameters();

/*-- screen.c --*/
void output_keymenu();
void format_keymenu();
void draw_keymenu();
void blank_keymenu();
void redraw_keymenu();
char *status_string();
char *set_titlebar();
void push_titlebar_state();
void pop_titlebar_state();
void redraw_titlebar();
void update_titlebar_message();
void update_titlebar_percent();
void update_titlebar_page();
int  update_titlebar_status();

/*-- send.c --*/
void  compose_screen(); 
void  pine_send();
void  pine_free_body();
void  compose_mail();

/*-- signals.c --*/
void init_signals(); 
void do_suspend();
void end_signals();
SigType hup_signal();
SigType term_signal();

/*-- status.c --*/
void display_message();
int messages_queued();
void d_q_status_message();
void q_status_message();
void q_status_message1();
void q_status_message2();
void q_status_message3();
void q_status_message4();
void q_status_message7();
void flush_status_messages();
void mark_status_dirty();
int want_to();
int radio_buttons();

/*-- strings.c --*/
char *rplstr();
void sqzspaces();
void removing_trailing_white_space();
void removing_leading_white_space();
int strucmp();
int struncmp();
char *srchstr();
char *strindex();
char *strrindex();
char *strcat_nocr();
void sstrcpy();
void crlf2lf();
char *month_abbrev();
char *week_abbrev();
int   month_num();
char *pretty_command();
char *repeat_char();
char *comatose();
char *byte_string();
char *enth_string();
char *long2string();
char *int2string();
char *rfc822date_to_ctime();
char *lf2crlf();

/*-- ttyin.c--*/
int read_char();
int read_command();
int optionally_enter();
int init_tty_driver();
void end_tty_driver();
int Raw();
void end_keyboard();
void init_keyboard();
int validatekeys();

/*-- ttyout.c --*/
void get_windsize();
int BeginScroll();
void EndScroll();
int  ScrollRegion();
int  Writechar();
void Write_to_screen();
void PutLine0();
void PutLine0n8b();
void PutLine1();
void PutLine2();
void PutLine3();
void PutLine4();
void PutLine5();
int  CenterLine();
void Stripe0();
void Stripe3();
void CleartoEOLN();
int  CleartoEOS();
void ClearScreen();
void MoveCursor();
void NewLine();
void StartInverse();
void EndInverse();
void StartUnderline();
void EndUnderline();
void StartBold();
void EndBold();
int  config_screen();
void init_screen();
void end_screen();
void outchar();
void clear_cursor_pos();
int  InsertChar();
int  DeleteChar();

/*----- our own various library things, some from imap, other from ../lib --*/
char *cpystr();

/*-- Standard system libraries that got missed in standard include files ---*/
FILE *popen();

#define SCREEN_FUN_NULL ((void (*) ())NULL)

#endif

#endif /* _PINE_INCLUDED */
