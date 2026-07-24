#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: args.c,v 4.24 1993/10/02 07:29:22 hubert Exp $";
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
      args.c
      Command line argument parsing functions 

  ====*/

#include "headers.h"

void args_help(void);


/*
 *  Parse the command line args.
 *
 *  Args: pine_state -- The pine_state structure to put results in
 *        argc, argv -- The obvious
 *        addrs      -- Pointer to address list that we set for caller
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various flags in pine_state set
 *       returns the string name of the first folder to open
 *       addrs is set
 */
char *
pine_args(pine_state, argc, argv, addrs)
     struct pine  *pine_state;
     int           argc;
     char        **argv;
     char       ***addrs;
{
    register int    ac;
    register char **av;
    int   a0_len, c;
    char *str;
    char *folder          = NULL;
    char *cmd_list        = NULL;
    char *sort            = NULL;
    int   do_help         = 0;
    int   usage           = 0;
    struct variable *vars = pine_state->vars;

    
    ac = argc;
    av = argv;

    /*
     * Needed on every platform now, not just DOS: pine_send()'s
     * composer subprocess (see send.c's pico_child_path()) uses this
     * to find the "pico" binary alongside pine's own.
     */
    pine_state->pine_exe = cpystr(argv[0]);

      /* while more arguments with leading - */
Loop: while(--ac > 0 && **++av == '-') {
	 /* while more chars in this argument */
	 while(*++*av) {
	    /* check for multi-char options first */
	    if(strcmp(*av, "sort") == 0) {
	       sort = --ac ? *++av : "<missing arg>";
	       vars[V_SORT_KEY].command_line_val.p = cpystr(sort);
	       goto Loop;

	    /* single char flags */
	    }else {
	       switch(c = **av) {
		 case 'h':
		   do_help = 1;
		   break;  /* break back to inner-while */
		 case 'k':
		   F_TURN_ON(F_USE_FK,pine_state);
		   break;
		 case 'z':
		   F_TURN_ON(F_CAN_SUSPEND,pine_state);
		   break;
		 case 'o':
		   pine_state->open_readonly_on_startup = 1;
		   break;
		 case 'i':
		   pine_state->start_in_index = 1;
		   break;
/* JUST WHILETESTINGFOLDERS */
		 case 'l':
		   F_TURN_ON(F_EXPANDED_FOLDERS,pine_state);
		   break;
/* END OF WHILETESTINGFOLDERS */
		 /* these take arguments */
		 case 'f': case 'p': case 'I':  /* string args */
#ifndef DOS
		 case 'P':  /* also a string */
#endif
		 case 'd': case 'n':  /* integer args */
		   if(*++*av) {
		      str = *av;
		   }else if(--ac) {
		      str = *++av;
		   }else {
		      fprintf(stderr,
			"Error: missing argument for flag \"%c\"\n", c);
		      ++usage;
		      goto Loop;
		   }
		   switch (c) {
		     case 'f':
		       folder = str;
		       break;
		     case 'I':
		       cmd_list = str;
		       break;
		     case 'p':
		       if(str != NULL)
			  pine_state->pinerc = cpystr(str);
		       break;
#ifndef DOS
		     case 'P':
		       if(str != NULL)
			  pine_state->pine_conf = cpystr(str);
		       break;
#endif
#ifdef	DEBUG
		     case 'd':
		       if(!isdigit(str[0])){
			   fprintf(stderr,
			  "Error: Non numeric argument for flag \"%c\"\n", c);
			   ++usage;
			   break;
		       }

		       debug = atoi(str);
		       break;
#endif
		     case 'n':
		       if(!isdigit(str[0])){
			   fprintf(stderr,
			  "Error: Non numeric argument for flag \"%c\"\n", c);
			   ++usage;
			   break;
		       }

		       pine_state->start_entry = atoi(str);
		       if (pine_state->start_entry < 1)
			  pine_state->start_entry = 1;
		       break;
		   }
		   goto Loop;

		 default:
		   fprintf(stderr, "Error: unknown flag \"%c\"\n", c);
		   ++usage;
		   break;
	       }
	    }
	 }
      }

    if(cmd_list != NULL) {
	int    commas         = 0;
	char  *p              = cmd_list;
	char  *error          = NULL;

	while(*p++)
	    if(*p == ',')
		++commas;
	vars[V_INIT_CMD_LIST].command_line_val.l =
			parse_list(cmd_list, commas+1, &error);
	if(error) {
	    fprintf(stderr, "Error in -I argument \"%s\": %s\n", cmd_list,
								    error);
	    exit(-1);
	}
    }

#define PINEF_LEN 5
    a0_len = strlen(argv[0]);
    if(a0_len >= PINEF_LEN && strcmp(argv[0]+a0_len-PINEF_LEN, "pinef") == 0)
        F_TURN_ON(F_USE_FK,pine_state);

    if(do_help || usage)
      args_help(); 
    if(usage)
      exit(-1);

    pine_state->orig_use_fkeys = F_ON(F_USE_FK,pine_state);

    pine_state->show_folders_dir = 0;

    if(ac <= 0)
      *av = NULL;
    *addrs = av;

    return(folder);
}


/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
args_help(void)
{
	/**  print out possible starting arguments... **/

	printf("\nPossible Starting Arguments for Pine program:\n\n");
	printf("\tArgument\t\tMeaning\n");
        printf("\t <addrs>...\tGo directly into composer sending to given address\n");
#ifdef	DEBUG
	printf("\t -d n\t\tDebug - set debug level to 'n'\n");
#endif
	printf("\t -f <folder>\tFolder - give folder name to open\n");
	printf("\t -h \t\tHelp - give this list of options\n");
	printf("\t -k \t\tKeys - Force use of function keys\n");
	printf("\t -z \t\tSuspend - allow use of ^Z suspension\n");
        printf("\t -sort <sort> \tSort - Specify sort order of folder:\n");
        printf("\t\t\t        subject, arrival, date, from, size, /reverse\n");
        printf("\t -i\t\tIndex - Go directly to index, bypassing mail menu\n");
        printf("\t -I <keystroke_list>\tInitial keystrokes to be executed\n");
        printf("\t -n <number>\tEntry in index to begin on\n");
	printf("\t -o \t\tReadOnly - Open first folder read-only\n");
        printf("\t -p <pinerc>\tSpecify alternate .pinerc file\n");
#ifndef DOS
        printf("\t -P <pine.conf>\tSpecify alternate pine.conf file\n");
#endif
/* WHILE TESTING FOLDERS */
printf("\t -l\t\tList - Expand List of folder collections by default\n");
	printf("\n");
	printf("\n");
	exit(1);
}
