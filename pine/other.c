#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: other.c,v 4.20 1993/11/30 23:48:35 mikes Exp $";
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
      other.c

      This implements the "setup" screen of miscellaneous commands such
  as keyboard lock, and disk usage

  ====*/

#include "headers.h"

static void draw_klocked_body(char *, char *);

static void redraw_kl_body();
static void redraw_klocked_body();


#ifndef NO_KEYBOARD_LOCK
/*----------------------------------------------------------------------
          Execute the lock keyboard command

    Args: None

  Result: keyboard is locked until user gives password
  ---*/

int
lock_keyboard()
{
    struct pine *ps = ps_global;
    char inpasswd[80], passwd[80];
    HelpType help = NO_HELP;

    passwd[0] = '\0';
    redraw_kl_body();
    ps->redrawer = redraw_kl_body;

    inpasswd[0] = '\0';
    while(1){			/* input pasword to use for locking */
        int rc;
        rc =  optionally_enter(inpasswd, -3, 0, 30, 0, 1,
			       "Enter password to LOCK keyboard : ", NULL,
                               help, 0);

	if(rc == 3) {
	    help = NO_HELP;		/* BUG: more help than from kl_body? */
	    continue;
        }
	else if(rc == 1 || inpasswd[0] == '\0'){
	    q_status_message(0, 0, 2, "\007Lock keyboard cancelled");
	    return(-1);
	}
	else if(rc != 4)
          break;
    }

    if(want_to("Really lock keyboard with entered password", 'y', 'n',
	       NO_HELP, 0) != 'y'){
	q_status_message(0, 0, 2, "\007Lock keyboard cancelled");
	return(-1);
    }

    draw_klocked_body(ps->VAR_USER_ID ? ps->VAR_USER_ID : "<no-user>",
		  ps->VAR_PERSONAL_NAME ? ps->VAR_PERSONAL_NAME : "<no-name>");

    ps->redrawer = redraw_klocked_body;
    while(strcmp(inpasswd, passwd)){
	if(passwd[0]){
	    q_status_message(1,2,4,
		"\007Password to UNLOCK doesn't match password used to LOCK");
	    display_message('x');
	    sleep(3);
	}
        
        help = NO_HELP;
        while(1){
	    int rc;
	    rc =  optionally_enter(passwd,  -3, 0, 30, 0, 1, 
				   "Enter password to UNLOCK keyboard : ",NULL,
				   help, 1);
	    if(rc == 3) {
		help = help == NO_HELP ? h_oe_keylock : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
        }
    }

    q_status_message(0, 1,3,"Keyboard Unlocked");
    return(0);
}

static void
redraw_kl_body()
{
    ClearScreen();

    set_titlebar("KEYBOARD LOCK",1, FolderName, 0, 0, 0);

    PutLine0(6,3 ,
       "You may lock this keyboard so that no one else can access your mail");
    PutLine0(8, 3 ,
       "while you are away.  The screen will be locked after entering the ");
    PutLine0(10, 3 ,
       "password to be used for unlocking the keyboard when you return.");
    fflush(stdout);
}


/*
 * login/username come from VAR_USER_ID/VAR_PERSONAL_NAME, cpystr()'d
 * from the user's pinerc with no length cap of its own -- copy into
 * fixed, truncated buffers here (rather than keeping the raw pointers)
 * so redraw_klocked_body()'s PutLine2() below has a proven bound on
 * its %s args instead of an unbounded pinerc value.
 */
static char klockin[128], klockame[128];

static void
draw_klocked_body(login, username)
    char *login, *username;
{
    strlcpy(klockin, login, sizeof(klockin));
    strlcpy(klockame, username, sizeof(klockame));
    redraw_klocked_body();
}

static void
redraw_klocked_body()
{
    ClearScreen();

    set_titlebar("KEYBOARD LOCK",1, FolderName, 0, 0, 0);

    PutLine2(6, 3, "This keyboard is locked by %s <%s>.",klockame, klockin);
    PutLine0(8, 3, "To unlock, enter password used to lock the keyboard.");
    fflush(stdout);
}
#endif /* !NO_KEYBOARD_LOCK */
