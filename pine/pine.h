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

#define	NEWLINE		"\n"		/* necessary for gf_* funcs */

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

#define	READONLY_FOLDER  (ps_global->mail_stream 			     \
			  && ps_global->mail_stream->readonly)

/*
 * Macro used to fetch all flags.  Used when counting deleted messages
 * and finding next message with a particular flag set.  The idea is to
 * minimize the number of times we have to fetch all of the flags for all
 * messages in the folder.
 */
#define	FETCH_ALL_FLAGS(s) {long faf_msgno;				      \
			    if(s == ps_global->mail_stream){		      \
				faf_msgno = max(ps_global->last_msgno_flagged,1L); \
				ps_global->last_msgno_flagged = s->nmsgs;     \
			    }						      \
			    else					      \
			      faf_msgno = 1L;				      \
									      \
			    if(faf_msgno < (long)s->nmsgs){		      \
				char         faf_sequence[48];		      \
									      \
				sprintf(faf_sequence,"%ld:%ld", faf_msgno, s->nmsgs); \
				mail_fetchflags(s, faf_sequence);	      \
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
#define V_INBOX_PATH               3
#define V_FOLDER_SPEC              4
#define V_DEFAULT_FCC              5
#define V_MAIL_DIRECTORY           6
#define V_READ_MESSAGE_FOLDER      7
#define V_SIGNATURE_FILE           8
#define V_INIT_CMD_LIST            9
#define V_SAVED_MSG_NAME_RULE      10
#define V_SORT_KEY                 11
#define V_USE_ONLY_DOMAIN_NAME     12


#define VAR_PERSONAL_NAME            vars[V_PERSONAL_NAME].current_val.p
#define VAR_USER_ID                  vars[V_USER_ID].current_val.p
#define VAR_USER_DOMAIN              vars[V_USER_DOMAIN].current_val.p
#define VAR_INBOX_PATH               vars[V_INBOX_PATH].current_val.p
#define VAR_FOLDER_SPEC              vars[V_FOLDER_SPEC].current_val.l
#define VAR_DEFAULT_FCC              vars[V_DEFAULT_FCC].current_val.p
#define VAR_MAIL_DIRECTORY           vars[V_MAIL_DIRECTORY].current_val.p
#define VAR_READ_MESSAGE_FOLDER      vars[V_READ_MESSAGE_FOLDER].current_val.p
#define VAR_SIGNATURE_FILE           vars[V_SIGNATURE_FILE].current_val.p
#define VAR_INIT_CMD_LIST            vars[V_INIT_CMD_LIST].current_val.l
#define VAR_SAVED_MSG_NAME_RULE      vars[V_SAVED_MSG_NAME_RULE].current_val.p
#define VAR_SORT_KEY                 vars[V_SORT_KEY].current_val.p
#define VAR_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].current_val.p



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
#define	CNTXT_PSEUDO	0x08			/* fake folder entry exists */
#define	CNTXT_PARTFIND	0x40			/* partial find done        */
#define	CNTXT_NOFIND	0x80			/* no find done in context  */


/*------------------------------
    Stucture describing the single folder collection this build
    supports (see TODO-pdp11-done.md: multi-collection support was
    cut, so unlike stock Pine there's no linked list of these).
  ----*/
typedef struct context {
    char           *context;			/* context string	   */
    char           *label[4];			/* description lines	   */
    char           *nickname;			/* user provided nickname  */
    char            last_folder[MAXFOLDER+1];	/* last folder used        */
    void           *folders;			/* folder data             */
    unsigned short  use;			/* use flags for context   */
    int		    d_line;			/* display line for labels */
    MAILSTREAM     *proto;			/* prototype driver        */
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
    char *description;
    /* is_obsolete..is_list below are unsigned:1 bit-fields -- kept
     * after name/description (rather than the closer-to-original
     * order) so that variables[]'s static initializer (init.c) can
     * supply just {name, description} positionally and let these,
     * plus the value unions below, zero-fill implicitly. 2.11BSD's cc
     * can't initialize a bit-field member via an aggregate
     * initializer at all ("No field initialization"), unlike the
     * bezerkproto fix in bezerk.c which had only one bit-field to
     * work around, these flags are set explicitly at runtime for
     * every entry in init_init_vars() (init.c) instead. */
    unsigned  is_obsolete:1;	/* variable read in, not written unless set */
    unsigned  is_used:1;	/* Some variables are disabled */
    unsigned  been_written:1;
    unsigned  is_user:1;
  /*unsigned  is_group:1; */
    unsigned  is_global:1;
    unsigned  is_list:1;	/* flag indicating variable is a list */
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
    void       (*next_screen)(struct pine *);   /* NULL means stay in current screen */
    void       (*prev_screen)(struct pine *);
    void       (*redrawer)(void);

    char        *return_string;
    char        *error_message;

    CONTEXT_S   *context_list;		/* the single folder collection  */
    CONTEXT_S   *context_current;	/* default open context          */
    CONTEXT_S   *context_write;		/* default save context          */

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
    unsigned     reverse_sort:1;
    unsigned     current_reverse_sort:1;
    unsigned     save_by_from:1;
    unsigned     save_by_sender:1;
    unsigned     save_by_recipient:1;
    unsigned     save_by_last_saved:1;
    unsigned     full_header:1;         /* display full headers */

    unsigned     try_to_create:1;     /* flag to try mail_create during save */
    unsigned     use_full_hostname:1; /* Use full hostname vs just domain */
    unsigned     low_speed:1;         /* Special screen painting 4 low speed */
    unsigned     dead_inbox:1;
    unsigned     dead_stream:1;
    unsigned     noticed_dead_inbox:1;
    unsigned     noticed_dead_stream:1;
    unsigned     compose_mime:1;
    unsigned     do_local_lookup:1;  /* look up in passwd even if using
                                           user defined domain */

    /*--- Command line flags, modify only on start up ---*/
    unsigned     start_in_index:1;
    unsigned     start_entry;

    unsigned     noshow_error:1;
    unsigned     noshow_warn:1;

    unsigned     painted_body_on_startup:1;
    unsigned     painted_footer_on_startup:1;

    int         *initial_cmds;         /* cmds to execute on startup */

    char         c_client_error[300];  /* when nowhow_error is set and PARSE */

    struct ttyo *ttyo;

    char        *passwd,         /* Encrypted login passwd for keyboard lock */
                *home_dir,
                *hostname,       /* Fully qualified hostname */
                *localdomain,    /* The (DNS) domain this host resides in */
                *userdomain,     /* The per user domain from .pinerc or */
                *maildomain,     /* Domain name for most uses */
                *pine_exe;	 /* argv[0], used to find sibling pico */

    SortOrder    sort_order, current_sort_order,
                 sort_types[20];

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



typedef enum {OpenFolder, SaveMessage, FolderMaint} FolderFun;
typedef enum {FolderName, MessageNumber, TextPercent, PageNumber} TitleBarType;
typedef enum {ComposerHelpText, HelpText, NewsText, MessageText, AttachText,
		WhoText, NetNewsText} TextType;
typedef enum {CharStarStar, CharStar, FileStar,
		TmpFileStar} SourceType;
typedef enum {GoodTime, BadTime, VeryBadTime, DoItNow} CheckPointTime;
typedef enum {AppendPostpone, AppendInterrupted, AppendSendmail} AppendTypes;
typedef enum {InLine, QStatus} DetachErrStyle;

/*
 * typedefs of generalized filters used by gf_pipe
 *
 * gf_io_t is deliberately left with no fixed parameter list: it's used
 * as *two* distinct signatures by convention -- "get" functions take
 * (unsigned char *) and are called as (*gc)(&c), "put" functions take
 * (int) and are called as (*pc)(c) -- never mixed, but not expressible
 * as a single ANSI prototype without splitting the typedef in two and
 * updating every declaration site referencing it.
 */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
typedef int (*gf_io_t)();	/* type of get and put char function     */
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
struct filter_s;	/* forward decl so filter_t's parameter list binds to
			 * the same tag as the typedef below, instead of an
			 * unrelated type scoped to this prototype alone */
typedef void (*filter_t)(struct filter_s *, int, int);	/* type of filters for piping */
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
int	 gf_link_filter(filter_t);
char	*gf_pipe(gf_io_t, gf_io_t);
void	 gf_set_so_readc(gf_io_t *, STORE_S *);
void	 gf_set_so_writec(gf_io_t *, STORE_S *);
void	 gf_set_readc(gf_io_t *, void *, unsigned long, SourceType);
int      gf_puts(char *, gf_io_t);
void	 gf_set_terminal(gf_io_t);
void	 gf_binary_b64(FILTER_S *, int, int);
void	 gf_b64_binary(FILTER_S *, int, int);
void	 gf_qp_8bit(FILTER_S *, int, int);
void	 gf_8bit_qp(FILTER_S *, int, int);
void	 gf_rich2plain(FILTER_S *, int, int);
void	 gf_rich2plain_opt(int);
void	 gf_wrap(FILTER_S *, int, int);
void	 gf_wrap_filter_opt(int);
void	 gf_nvtnl_local(FILTER_S *, int, int);
void	 gf_local_nvtnl(FILTER_S *, int, int);
void	 gf_prefix(FILTER_S *, int, int);
void	 gf_prefix_opt(char *);

/*--- folder.c ---*/
void        folder_screen(struct pine *);
int         folder_lister(struct pine *,FolderFun,CONTEXT_S *,CONTEXT_S **,
                          char *);
char       *pretty_fn(char *);
int         folder_exists(char *, char *);
int         folder_create(char *, CONTEXT_S *);
int         folder_complete(CONTEXT_S *, char *);
void        init_folders(void);
void       *find_folders(char *, char *);
void        find_folders_in_context(CONTEXT_S *, char *);
FOLDER_S   *folder_entry(int, void *);
FOLDER_S   *new_folder(char *);
int         folder_insert(int, FOLDER_S *, void *);
int         folder_index(char *, void *);
char       *folder_is_nick(char *, void *);

/*-- help.c --*/
void helper(char *text[], char *title, int);

/*-- init.c --*/
int  init_init_vars(struct pine *);
void init_vars(struct pine *);
char *expand_variables(char *, char *);
int  init_username(struct pine *);  
int  init_hostname(struct pine *);
int  set_variable_list(int, char **);
int  init_mail_dir(char *);
char **parse_list(char *, int, char **);

/*---- mailcmd.c ----*/
int   process_cmd(int, int, int, int *);
int   save_msg(int, MESSAGECACHE *, int *, long);
char *detach(long, BODY *, char *, long *, gf_io_t, filter_t *);
long  jump_to(int);
char *broach_folder(int, int, CONTEXT_S **);
int   do_broach_folder(char *, CONTEXT_S *);
void  expunge_and_close(MAILSTREAM *, char *);
int   export_message(int, long);
long  search_headers(int, long, long);

/*--- mailindx.c ---*/
void  mail_index_screen(struct pine *);
void  clear_index_cache(void);
void  clear_index_cache_ent(long);
int   build_index_cache(long);
char *build_header_line(long);
void  build_header_cache(void);
void  redraw_index_body(void);
void  do_index_border(int, int, int, int *);
char *sort_name(SortOrder);
void  sort_current_folder(void);


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
void check_point_change(void);
int  streams_died(void);
void reset_check_point(void);
void zero_new_mail_count(void);

/*-- os.c --*/
int can_access(char *, int);
long file_size(char *);
int is_writable_dir(char *);
int create_mail_dir(char *);
int rename_file(char *, char *);
void build_path(char *, size_t, char *, char *);
char *last_cmpnt(char *);
int   expand_foldername(char *);
char *fnexpand(char *, int);
char *filter_filename(char *);
char *read_file(char *);
FILE *create_tmpfile(void);
char *temp_nam(char *, char *);
int   fget_pos(FILE *, fpos_t *);
int   fset_pos(FILE *, fpos_t *);
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

/*-- pine.c --*/
void main_menu_screen(struct pine *);
void news_screen(struct pine *);
void quit_screen(struct pine *);
int  count_deleted(MAILSTREAM *);
int  count_seen_not_deleted(MAILSTREAM *);
void who_screen(struct pine *);
void panic(char *);
void panic1(char *, char *);
MAILSTREAM *default_driver(char *);
MAILSTREAM *create_proto(CONTEXT_S *, char *);
MsgNo       first_sorted_unread(MAILSTREAM *);
MsgNo       next_sorted_un(MAILSTREAM *, long, int *, int);
MsgNo       sorted_msgno(struct pine *, MsgNo);
void warn_other_cmds(void);

/*-- reply.c --*/
void     reply(struct pine *, long, ENVELOPE *, BODY *);
void     forward(struct pine *, long, ENVELOPE *, BODY *);
char    *generate_message_id(struct pine *);
void     pretty_addr_string(char *, ADDRESS *, char *, gf_io_t);
ADDRESS *first_addr(ADDRESS *);
char    *get_signature(void);
PARAMETER *copy_parameters(PARAMETER *);

/*-- screen.c --*/
void output_keymenu(struct key_menu *, BITMAP, int, int);
void format_keymenu(struct key_menu *, BITMAP, int, OtherMenu, int);
void draw_keymenu(struct key_menu *, BITMAP, int, int, int, OtherMenu, int);
void blank_keymenu(int, int);
void redraw_keymenu(void);
char *status_string(MESSAGECACHE *);
char *set_titlebar(char *, int, TitleBarType, long, int, int);
void push_titlebar_state(void);
void pop_titlebar_state(void);
void redraw_titlebar(void);
void update_titlebar_message(int);
void update_titlebar_percent(int);
void update_titlebar_page(int);
int  update_titlebar_status(MESSAGECACHE *);

/*-- send.c --*/
void  compose_screen(struct pine *); 
void  compose_mail(char *);
void  pine_send(ENVELOPE *, BODY **, char *, char *, long);
void  pine_free_body(BODY **);

/*-- signals.c --*/
void init_signals(void); 
void do_suspend(struct pine *);
void end_signals(void);
SigType hup_signal(int);
SigType term_signal(int);

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
void removing_trailing_white_space(char *);
void removing_leading_white_space(char *);
int strucmp(char *, char *);
int struncmp(char *, char *, int);
char *srchstr(char *, char *);
char *strindex(char *, int);
char *strrindex(char *, int);
void sstrcpy(char **, char *);
char *month_abbrev(int);
char *week_abbrev(int);
void  parse_date(char *, struct date *);
int   compare_dates(struct date *, struct date *);
void  convert_to_gmt(struct date *);
char *pretty_command(int);
char *repeat_char(int, int);
char *comatose(long);
char *byte_string(long);
char *long2string(long);
char *int2string(int);

/*-- ttyin.c--*/
int read_char(int);
int read_command(void);
int optionally_enter(char *,int,int,int,int,int,char *,ESCKEY_S *,HelpType,int);
int init_tty_driver(struct pine *);
void end_tty_driver(struct pine *);
int Raw(int);
int validatekeys(int);

/*-- ttyout.c --*/
void get_windsize(struct ttyo *);
int BeginScroll(int, int);
void EndScroll(void);
int  ScrollRegion( int);
int  Writechar(unsigned int);
void Write_to_screen(char *);
void PutLine0(int, int, char *);
void PutLine0n8b(int, int, char *, int);
void PutLine1(int, int, char *, void *);
void PutLine2(int, int, char *, void *, void *);
void PutLine4(int, int, char *, void *, void *, void *, void *);
void PutLine5(int, int, char *, void *, void *, void *, void *, void *);
int  Centerline(int, char *);
void CleartoEOLN(void);
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

/*----- our own various library things, some from imap, other from ../lib --*/
char *cpystr(char *);

#define SCREEN_FUN_NULL ((void (*) (struct pine *))NULL)


#endif /* _PINE_INCLUDED */
