/*----------------------------------------------------------------------
  $Id: os-dos.h,v 5.10 1993/11/16 00:32:46 hubert Exp $

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

#ifndef _OS_INCLUDED
#define _OS_INCLUDED


/*----------------------------------------------------------------------

   This first section has some constants that you may want to change
   for your configuration.  This is the DOS version of the os.h file.
   Further down in the file are os-dependent things that need to be set up
   correctly for each os.  They aren't interesting, they just have to be
   right.  There are also a few constants down there that may be of
   interest to some.

 ----*/

/*----------------------------------------------------------------------
 Define this if you want the disk quota to be checked on startup.
 Of course, this only makes sense if your system has quotas.  If it doesn't,
 there should be a dummy disk_quota() routine in os-xxx.c so that defining
 this won't be harmful anyway.
 ----*/
/* #define USE_QUOTAS */



/*----------------------------------------------------------------------
    Turn this on if you want to disable the keyboard lock function.
 ----*/
/* #define NO_KEYBOARD_LOCK */



/*----------------------------------------------------------------------
    Turn this on if it's OK to encode sent message text containing
  "From " at the beginning of any line.
 ----*/
/* #define ENCODE_FROMS */



/*----------------------------------------------------------------------
    Turn this on if you want to disable PC-Pine's including of 
  "X-Warning" and "X-Sender" header lines in outgoing messages.
 ----*/
/* #define NOAUTH */



/*----------------------------------------------------------------------
    Timeouts (seconds)
 ----*/
#define NEW_MAIL_TIME      (150) /* How often to check for new mail. There's 
                                   some expense in doing this so it shouldn't
                                   be done too frequently.  */

/*----------------------------------------------------------------------
    Check pointing (seconds)
 ----*/
#define CHECK_POINT_TIME (7*60) /* Check point the mail file (write changes
				   to disk) if more than CHECK_POINT_TIME
				   seconds have passed since the first
				   change was made.  Depending on what is
				   happening, we may wait up to three times
				   this long, since we don't want to do the
				   slow check pointing and irritate the user. */
                                     
#define CHECK_POINT_FREQ   (12) /* Check point the mail file if there have been
                                   at least this many (status) changes to the
				   current mail file.  We may wait longer if
				   it isn't a good time to do the checkpoint. */



/*----------------------------------------------------------------------
 In scrolling through text, the number of lines from the previous
 screen to overlap when showing the next screen.  Usually set to two.
 ----*/
#define	OVERLAP		(2) 



/*----- System-wide config file ----------------------------------------*/
#define SYSTEM_PINERC "pinerc"



/*----- Used only in DOS -----------------------------------------------*/
#define DF_PINEDIR      "\\pine"
#define HELPFILE        "pine.hlp"
#define HELPINDEX       "pine.ndx"
  
  
  
/*----------------------------------------------------------------------
   The default folder names and folder directories (some for backwards
   compatibility).  Think hard before changing any of these.
 ----*/
#define DF_DEFAULT_FCC            "sentmail"
#define DEFAULT_SAVE              "savemail"
#define POSTPONED_MAIL            "postpone"
#define INTERRUPTED_MAIL          "intruptd"
#define DF_MAIL_DIRECTORY         "mail"
#define INBOX_NAME                "INBOX"
#define DF_SIGNATURE_FILE         "pine.sig"
#define DF_ELM_STYLE_SAVE         "no"
#define DF_HEADER_IN_REPLY        "no"
#define DF_OLD_STYLE_REPLY        "no"
#define DF_USE_ONLY_DOMAIN_NAME   "no"
#define DF_FEATURE_LEVEL          "sapling"
#define DF_SAVE_BY_SENDER         "no"
#define DF_LOCAL_MAIL_FORMAT      "pine"
#define DF_SORT_KEY               "arrival"
#define DF_SAVED_MSG_NAME_RULE    "default-folder"
#define	DF_DRIVER_PROTO		  (&dawzproto)
#define DF_STANDARD_PRINTER       "lpr"
#define ANSI_PRINTER              "attached-to-ansi"
#define DF_ADDRESSBOOK            "addrbook"

/*----------------------------------------------------------------------
   The default printer when pine starts up for the first time with no printer
 ----*/
#define DF_DEFAULT_PRINTER        ANSI_PRINTER



/*----------------------------------------------------------------------

   OS dependencies, DOS version.  See also the os-dos.c files.
   The following stuff may need to be changed for a new port, but once
   the port is done it won't change.  Further down in the file are a few
   constants that you may want to configure differently than they
   are configured, but probably not.

 ----*/



/*----------------- Are we ANSI? ---------------------------------------*/
#define ANSI

/*------ If our compiler doesn't understand type void ------------------*/
/* #define void char */

/*-------- Standard ANSI functions usually defined in stdlib.h ---------*/
#include <stdlib.h>
/*---- Declare getenv() if not already declared ------------------------*/
/* char *getenv(); */


/*------- Standard string functions ------------------------------------*/
#include <string.h>
/*---- Declare these if not already declared ---------------------------*/
/* char *strcpy(), *strncpy(); */


/*------- Include types.h if you have it -------------------------------*/
#include <sys/types.h>


/*------- Some systems need this ---------------------------------------*/
/* #include <stdarg.h> */


#include <io.h>
#include <sys/stat.h>
#ifdef	PCTCP
#include <4bsddefs.h>			/* PC/TCP Include Files */
#include <netdb.h>
#undef	signal				/* override their stupid def */
#define	sleep	_sleep			/* use pico's sleep function */
#else
#ifdef	LWP
#include <sys/socket.h>			/* LanWorkplace Include Files */
#include <netinet/in.h>
#include <netdb.h>
#else
#ifdef	PCNFS
#include <tklib.h>
#include <netdb.h>
#else
#include <tcp.h>			/* WATTCP Include File */
#endif
#endif
#endif
#include <dos.h>
#include <direct.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/timeb.h>


/*---- Declare sys_errlist() if not already declared -------------------*/
/* extern char *sys_errlist[]; */



/*----------------- time.h ---------------------------------------------*/
#include <time.h>
/* plain time.h isn't enough on some systems */
/* #include <sys/time.h>  /* For struct timeval usually in time.h */ 



/*--------------- signal.h ---------------------------------------------*/
#include <signal.h>
/* #include <sys/signal.h> */

#define SigType void
/* #define SigType int */

/* #define POSIX_SIGNALS */

/*--- For AT&T-derviced systems ----------------------------------------*/
/* #define signal(a,b) sigset (a, b) */
/* extern void (*sigset())(); */



/*-------------- qsort argument type -----------------------------------*/
#define QSType void
/* #define QSType char */



/*------ how help text is referenced (always char ** on Unix) ----------*/
#define HelpType short
#define NO_HELP (-1)



/*--------- System V terminal driver -----------------------------------*/
/* #define HAVE_TERMIO   this is for pure System V */
/* #define HAVE_TERMIOS  this is an alternative    */
/*- Use the poll() system call instead of select() */
/* #define USE_POLL */
/*---- Use terminfo instead of termcap ---------------------------------*/
/* #define USE_TERMINFO */
/* #include <sys/stream.h>	/* for ptem.h */
/* #include <sys/ptem.h>	/* for struct winsize */



/*-- What argument does wait(2) take? Define this if it is a union -----*/
/* #define HAVE_WAIT_UNION */


/*-------- Is window resizing available? -------------------------------*/
/* #define RESIZING */



/*-------- If no vfork, use regular fork -------------------------------*/
/* #define vfork fork */



/*----------------------------------------------------------------------
    This is DOS specific and is used to determine if a mail message is
 too big to fit in core.  It's just a static value now, but could be
 turned into a fuller macro that takes an adaptive approach to figure
 out what can fit into available free storage.
 Examining mailboxes around here, it looks like 90-something percent of
 messages come in under 8K so that seems like an OK limit on what can
 be dealt with in core.
 ----*/
#define MAX_MSG_INCORE  (8192L)



/*----------------------------------------------------------------------
    This is DOS specific and is used to pass the results of dos_gets
 calls back to functions that had previously installed it as the
 c-client "mailgets" allocation function.  It's basically a way around
 c-client's exclusive use of malloc'd strings which is quite
 impractical if we want to deal with text objects bigger than 64K on
 DOS (assuming we even have 64 contiguous K anyway!).
 ----*/
extern  FILE    *append_file;



/*----- The usual sendmail configuration for sending mail on Unix ------*/
/* #define SENDMAIL	"/usr/lib/sendmail" */
/* #define SENDMAILFLAGS	"-oi -oem -t"	/* ignore dots in incoming mail,
					   mail back errors, and
					   get recipients from To, cc,
					   and bcc header lines.      */


/*-------------- A couple constants used to size arrays ----------------*/
/* #include <sys/param.h>          /* Get it from param.h if available */
/* #define MAXPATH MAXPATHLEN      /* Longest pathname we ever expect */
#define MAXPATH        (128)    /* Longest pathname we ever expect */
#define MAXFOLDER      (64)     /* Longest foldername we ever expect */  


/*-- Max screen pine will display on. Used to define some array sizes --*/
#define MAX_SCREEN_COLS  (170) 
#define MAX_SCREEN_ROWS  (200) 


/*---- When no screen size can be discovered this is the size used -----*/
#define DEFAULT_LINES_ON_TERMINAL	(25)
#define DEFAULT_COLUMNS_ON_TERMINAL	(80)


/*----------------------------------------------------------------------
    Where to put the output of pine in debug mode. Files are created
 in the user's home directory and have a number appended to them when
 there is more than one.
 ----*/
#define DEBUGFILE	".pine-debug"

/*----------------------------------------------------------------------
    The number of debug files to save in the user's home diretory. The files
 are useful for figuring out what a user did when he complains that something
 went wrong. It's important to keep a bunch around, usually 4, so that the
 debug file in question will still be around when the problem gets 
 investigated. Users tend to go in and out of Pine a few times and there
 is one file for each pine invocation
 ----*/
#define NUMDEBUGFILES 4

/*----------------------------------------------------------------------
   The default debug level to set (approximate meanings):
       1 logs only highest level events and errors
       2 logs events like file writes
       3
       4 logs each command
       5
       6 
       7 logs details of command execution (7 is highest to run any production)
       8
       9 logs gross details of command execution
 ----*/
#define DEFAULT_DEBUG 2



/*----------------------------------------------------------------------
    Various maximum field lengths, probably shouldn't be changed.
 ----*/
#define MAX_FULLNAME      (50) 
#define MAX_NICKNAME      (20)
#define MAX_ADDRESS      (100)
#define MAX_NEW_LIST     (100)  /* Max addrs to be added when creating list */
#define MAX_SEARCH       (100)  /* Longest string to search for             */
#define MAX_ADDR_EXPN   (1000)  /* Longest expanded addr                    */
#define MAX_ADDR_FIELD (10000)  /* Longest fully-expanded addr field        */


#endif /* _OS_INCLUDED */
