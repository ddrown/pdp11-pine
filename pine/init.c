#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: init.c,v 4.102 1993/11/23 00:04:05 mikes Exp $";
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
     init.c
     Routines for pine start up and initialization
       init_vars
       init_username
       init_hostname
       init_mail_dir

       open debug files

      - open and set up the debug files
      - fetch username, password, and home directory
      - get host and domain name
      - read the user's .pinerc config file
      - create the "mail" subdirectory

  ====*/


#include "headers.h"
#include <time.h>


extern char version_buff[];

void set_current_val(struct variable *, int, int);
/* AIX gives warning here 'cause it can't quite cope with enums */
void process_init_cmds(struct pine *, char **);
int  decode_sort(struct pine *, char *);


/*------------------------------------
Some definitions to keep the static variable array below 
a bit more readable...
  ----*/
#define PERSONAL_NAME_TEXT   "personal-name specifies your full name as it should appear on outgoing mail.\n# If unset, Unix Pine will obtain your full name from the system passwd file."

#define USER_DOMAIN_TEXT   "user-domain specifies the domain part of your return address on outgoing\n# email and is also used as the default domain for email composed to a\n# local user.  If unset, Unix Pine will obtain the domain from the system."


#define	INBOX_TEXT	"inbox-path specifies the name/path/location of your INBOX.  Example:\n# inbox-path={carson.u.washington.edu}inbox (INBOX on a remote computer)\n# Normal Unix default is the local INBOX (usually /user/spool/mail/$USER)"

#define	FOLDER_TEXT	"folder-collections specifies the single folder collection wherein saved\n# messages are stored (only the first value is used; this build supports\n# exactly one collection).\n# Collection syntax: optnl-label optnl-directory-path[]\n# Example:\n# folder-collections=Saved-Email    mail/[]"

#define	DEFAULT_FCC_TEXT "default-fcc specifies where a copy of outgoing mail is saved.  If this is\n# not a path name, it will be in the the default collection for saves.\n# Unix Pine default is normally \"sent-mail\" in the default folder collection.\n# PC-Pine default is \"SENTMAIL\" (stored as SENTMAIL.MTX)\n# Example of remote case: {carson.u.washington.edu}sent-mail\n# To suppress saving of outgoing mail, set: default-fcc=\"\""

#define	READ_MSG_TEXT "read-message-folder specifies where already-read messages will be stored.\n# If set, you will be asked when you quit Pine if you want read messages\n# to be moved to this folder and deleted from your INBOX."

#define	SIG_FILE_TEXT "signature-file specifies the name or path of a file containing text that\n# will automatically be inserted in outgoing mail."

#define	SAVE_MSG_TEXT	"saved-msg-name-rule= determines default folder name for Saves...\n# Choose one: default-folder, by-sender, by-from, by-recipient,\n# last-folder-used.  Normal default is \"default-folder\", which on Unix\n# means \"saved-messages\" and in PC-Pine means \"SAVEMAIL\""

#define	SORT_TEXT	"sort-key= order in which messages will be presented...\n# Choose one: subject, from, arrival, date, size.\n# Normal default is \"arrival\""


#define ONLY_DOMAIN_TEXT     "Used if \"user-domain\" not set.  Strips hostname from user\'s return address.\n# Not applicable for PC-Pine",




/*----------------------------------------------------------------------
These are the variables that control a number of pine functions.  They
come out of the .pinerc and the /usr/local/lib/pine.conf files.  Some can
be set by the user while in Pine.  Eventually all the local ones should
be so and maybe the global ones too.

Each variable can have a command-line, user, global, and current value.
All of these values are malloc'd.  The user value is the one read out of
the user's .pinerc, the global value is the one from the system pine
configuration file.  There are often defaults for the global values, set
at the start of init_vars().  Perhaps someday there will be group values.
The current value is the one that is actually in use.
  ----*/
/* name::is_obsolete::is_used::been_written::is_user::is_global::is_list::desc*/
static struct variable variables[] = {
{"personal-name",              0, 1, 0, 1, 0, 0,
  PERSONAL_NAME_TEXT},
{"user-id",                    0, 0, 0, 1, 0, 0,	/* Don't allow on UNIX machines for some security */
  "Your login/e-mail user name"},
{"user-domain",                0, 1, 0, 1, 1, 0,
  USER_DOMAIN_TEXT},
{"inbox-path",                 0, 1, 0, 1, 1, 0,
  INBOX_TEXT},
{"folder-collections",         0, 1, 0, 1, 1, 1,
  FOLDER_TEXT},
{"default-fcc",                0, 1, 0, 1, 1, 0,
  DEFAULT_FCC_TEXT},
{"mail-directory",             0, 1, 0, 1, 1, 0,
  "mail-directory is where postponed & interrupted msgs are held temporarily."},
{"read-message-folder",        0, 1, 0, 1, 1, 0,
  READ_MSG_TEXT},
{"signature-file",             0, 1, 0, 1, 1, 0,
  SIG_FILE_TEXT},
{"initial-keystroke-list",     0, 1, 0, 1, 1, 1,
 "Pine executes these keys upon startup (e.g. to view msg 13: i,j,1,3,CR,v)"},
{"saved-msg-name-rule",        0, 1, 0, 1, 1, 0,
  SAVE_MSG_TEXT},
{"sort-key",                   0, 1, 0, 1, 1, 0,
  SORT_TEXT},
{"use-only-domain-name",       0, 1, 0, 1, 1, 0,
  ONLY_DOMAIN_TEXT},
{NULL,                         0, 0, 0, 0, 0, 0,
   NULL}
};


int
init_init_vars(struct pine *ps)
{
    ps->vars = variables;
    return(0);
}

    
/*----------------------------------------------------------------------
     Initialize the variables

 Args:   ps   -- The usual pine structure

 Result: 

  This reads the system pine configuration file and the user's pine
configuration file ".pinerc" and places the results in the variables 
structure.  It sorts out what was read and sets a few other variables 
based on the contents.
  ----*/
void 
init_vars(ps)
     struct pine *ps;
{
    register struct variable *vars = ps->vars;

    /*--- The defaults here are defined in os-xxx.h so they can vary
          per machine ---*/
    vars[V_INBOX_PATH].global_val.p       = cpystr("inbox");
    vars[V_DEFAULT_FCC].global_val.p      = cpystr(DF_DEFAULT_FCC);
    vars[V_USE_ONLY_DOMAIN_NAME].global_val.p = cpystr(DF_USE_ONLY_DOMAIN_NAME);
    vars[V_SORT_KEY].global_val.p         = cpystr(DF_SORT_KEY);
    vars[V_SAVED_MSG_NAME_RULE].global_val.p = cpystr(DF_SAVED_MSG_NAME_RULE);
    vars[V_SIGNATURE_FILE].global_val.p   = cpystr(DF_SIGNATURE_FILE);
    vars[V_MAIL_DIRECTORY].global_val.p   = cpystr(DF_MAIL_DIRECTORY);

    set_current_val(vars, V_INBOX_PATH, TRUE);

    set_current_val(vars, V_USER_DOMAIN, TRUE);
    set_current_val(vars,V_USE_ONLY_DOMAIN_NAME, TRUE);

    set_current_val(vars,V_DEFAULT_FCC, TRUE);
    set_current_val(vars,V_READ_MESSAGE_FOLDER, TRUE);

    set_current_val(vars, V_SIGNATURE_FILE, TRUE);

    /*
     * mail-directory variable is obsolete, put its value in
     * folder-collection list if that list is blank...
     */
    set_current_val(vars, V_MAIL_DIRECTORY, TRUE);
    set_current_val(vars, V_FOLDER_SPEC, TRUE);
    if(VAR_FOLDER_SPEC == NULL){
	char *lvalue[2];
	sprintf(tmp_20k_buf, "%s%c[]", VAR_MAIL_DIRECTORY, '/');
	lvalue[0] = tmp_20k_buf;
	lvalue[1] = NULL;
	set_variable_list(V_FOLDER_SPEC, lvalue);
    }

    set_current_val(vars, V_SORT_KEY, TRUE);
    if(decode_sort(ps, VAR_SORT_KEY) == -1) {
        if(!struncmp(VAR_SORT_KEY, "to", 2) ||
		!struncmp(VAR_SORT_KEY, "cc", 2)) {
	   fprintf(stderr, "Sort type \"%s\" is not implemented yet\n",
						    VAR_SORT_KEY);
	   sleep(5);
	}else {
	   fprintf(stderr, "Sort type \"%s\" is invalid\n", VAR_SORT_KEY);
	   exit(-1);
	}
    }

    set_current_val(vars, V_SAVED_MSG_NAME_RULE, TRUE);
    if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "by-sender"))
        ps->save_by_sender = 1;
    else if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "by-from"))
        ps->save_by_from = 1;
    else if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "by-recipient"))
        ps->save_by_recipient = 1;
    else if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "last-folder-used"))
        ps->save_by_last_saved = 1;
    else if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "default-folder"))
        ps->save_by_sender = 0;

    if (!ps->start_in_index)
        set_current_val(vars, V_INIT_CMD_LIST, FALSE);
    if (VAR_INIT_CMD_LIST)
        process_init_cmds(ps, VAR_INIT_CMD_LIST);

#ifdef DEBUG
#define STRING(vv) ((vv) ? (vv) : "<unset>")
#define LIST(vv)   (((vv) && ((vv)[0])) ? (vv)[0] : "<unset>")
    for(vars = variables; vars->name != NULL; vars++) {
        dprint(2, (debugfile, "%17.17s : %18.18s %18.18s %18.18s\n",
                   vars->name, 
		   (vars->is_list) ? LIST(vars->user_val.l) 
		                   : STRING(vars->user_val.p),
                   (vars->is_list) ? LIST(vars->global_val.l) 
		                   : STRING(vars->global_val.p),
		   (vars->is_list) ? LIST(vars->current_val.l) 
		                   : STRING(vars->current_val.p)));
    }
#endif
}


/*
 * Process the command list, changing function key notation into
 * lexical equivalents.
 */
void
process_init_cmds(ps, list)
    struct pine *ps;
    char **list;
{
    char **p;
    int i = 0;
    int j;
#define MAX_INIT_CMDS 500
    /* this is just a temporary stack array, the real one is allocated below */
    int i_cmds[MAX_INIT_CMDS];
    int fkeys = 0;
    int not_fkeys = 0;
  
    if(list) {
      for(p = list; *p && i < MAX_INIT_CMDS; p++) {

	/* regular character commands */
	if(strlen(*p) == 1) {

	  i_cmds[i++] = **p;
	  not_fkeys++;

	}else if(strucmp(*p, "SPACE") == 0) {
	    i_cmds[i++] = ' ';
	    not_fkeys++;

	}else if(strucmp(*p, "CR") == 0) {
	    i_cmds[i++] = '\n';
	    not_fkeys++;

	/* function keys */
	}else {

	  fkeys++;

	  if(**p == 'F' || **p == 'f') {
	    int v;

	    v = atoi((*p)+1);
	    if(v >= 1 && v <= 12)
	      i_cmds[i++] = PF1 + v - 1;
	    else
	      i_cmds[i++] = KEY_JUNK;

	  }else if(strucmp(*p, "UP") == 0) {
	    i_cmds[i++] = KEY_UP;
	  }else if(strucmp(*p, "DOWN") == 0) {
	    i_cmds[i++] = KEY_DOWN;
	  }else if(strucmp(*p, "LEFT") == 0) {
	    i_cmds[i++] = KEY_LEFT;
	  }else if(strucmp(*p, "RIGHT") == 0) {
	    i_cmds[i++] = KEY_RIGHT;
	  }else {
	    i_cmds[i++] = KEY_JUNK;
	  }
	}
      }
    }

    /*
     * We don't handle the case where function keys are used to specify the
     * commands but some non-function key input is also required.  For example,
     * you might want to jump to a specific message number and view it
     * on start up.  To do that, you need to use character commands instead
     * of function key commands in the initial-keystroke-list.
     */
    if(fkeys && not_fkeys) {
      fprintf(stderr, "Can't mix characters and function keys in\n");
      fprintf(stderr, "\"initial-keystroke-list\", skipping.\n\n");
      sleep(5);
      i = 0;
    }

    ps->initial_cmds = (int *)fs_get((i+1) * sizeof(int));
    for(j = 0; j < i; j++)
	ps->initial_cmds[j] = i_cmds[j];
    ps->initial_cmds[i] = 0;
    if(i)
      ps->in_init_seq = ps->save_in_init_seq = 1;
}


void    
set_current_val(vars, v_no, expand)
  struct variable *vars;
  int              v_no, expand;
{
    if(vars[v_no].is_list){		/* expand vars in list? */
	char **list = NULL, **tmp;
	int    i;

	/*
	 * It's possible for non-null list elements to expand to
	 * nothing, so we have to be a little more intelligent about
	 * pre-expanding the lists to make sure such lists are
	 * treated like empty lists...
	 */
	if(vars[v_no].command_line_val.l){
	    if(!expand)
	      list = vars[v_no].command_line_val.l;
	    else
	      for(i = 0; vars[v_no].command_line_val.l[i]; i++)
		if(expand_variables(tmp_20k_buf,
				    vars[v_no].command_line_val.l[i])){
		    list = vars[v_no].command_line_val.l;
		    break;
		}
	}

	if(!list && vars[v_no].user_val.l){
	    if(!expand)
	      list = vars[v_no].user_val.l;
	    else
	      for(i = 0; vars[v_no].user_val.l[i]; i++)
		if(expand_variables(tmp_20k_buf, vars[v_no].user_val.l[i])){
		    list = vars[v_no].user_val.l;
		    break;
		}
	}

	if(!list && vars[v_no].global_val.l){
	    if(!expand)
	      list = vars[v_no].global_val.l;
	    else
	      for(i = 0; vars[v_no].global_val.l[i]; i++)
		if(expand_variables(tmp_20k_buf, vars[v_no].global_val.l[i])){
		    list = vars[v_no].global_val.l;
		    break;
		}
	}

	if(!list)
	  return;

	for(i = 0; list[i] ; i++)		/* minimum item count */
	  ;

	vars[v_no].current_val.l = (char **)fs_get((i+1) * sizeof(char *));
	tmp = vars[v_no].current_val.l;

	for(i = 0; list[i] ; i++){
	    if(!expand)
	      *tmp++ = cpystr(list[i]);
	    else if(expand_variables(tmp_20k_buf, list[i]))
	      *tmp++ = cpystr(tmp_20k_buf);
	}

	*tmp = NULL;
    }
    else {
	char *p;

	if((!(p = vars[v_no].command_line_val.p) || (expand &&
	    !expand_variables(tmp_20k_buf, vars[v_no].command_line_val.p)))
	   && (!(p = vars[v_no].user_val.p) || (expand &&
	       !expand_variables(tmp_20k_buf, vars[v_no].user_val.p)))
	   && (!(p = vars[v_no].global_val.p) || (expand &&
	       !expand_variables(tmp_20k_buf, vars[v_no].global_val.p))))
	  vars[v_no].current_val.p = NULL;
	else
	  vars[v_no].current_val.p = cpystr(expand ? tmp_20k_buf : p);
    }
}



/*----------------------------------------------------------------------

	Expand Metacharacters/variables in file-names

   Read input line and expand shell-variables/meta-characters

	<input>		<replaced by>
	${variable}	getenv("variable")
	$variable	getenv("variable")
	~		getenv("HOME")
	\c		c
	<others>	<just copied>

NOTE handling of braces in ${name} doesn't check much or do error recovery
	
  ----*/

char *
expand_variables(lineout, linein)
char *linein, *lineout;
{
    char *src = linein, *dest = lineout, *p;
    char word[128];
    int  envexpand = 0;

    if(!linein)
      return(NULL);

    while ( *src ) {			/* something in input string */
        if (*src == '\\' && *(src+1) == '$') {
	    /*
	     * backslash to escape chars we're interested in, else
	     * it's up to the user of the variable to handle the 
	     * backslash...
	     */
            *dest++ = *++src;		/* copy next as is */
        } else if (*src == '~' && src == linein) {
	    char buf[MAXPATH];
	    int  i;

	    for(i = 0; src[i] && src[i] != '/'; i++)
	      buf[i] = src[i];

	    src    += i;		/* advance src pointer */
	    buf[i]  = '\0';		/* tie off buf string */
	    fnexpand(buf, MAXPATH);	/* expand the path */

	    for(p = buf; *dest = *p; p++, dest++)
	      ;

	    continue;
        } else if (*src == '$') {	/* shell variable */
            int found_brace=0;

	    envexpand++;		/* signal that we've expanded a var */
            src++;			/* skip dollar */
            p = word;
            if (*src == '{') {		/* starts with brace? */
                src++;        
                found_brace=1;
            }

            while (isalnum(*src))	/* copy to word */
                *p++ = *src++;

            if ( found_brace ) {	/* look for closing  brace */
                while (*src && *src != '}')
                    src++;		/* skip until brace */
                if(*src == '}')
                  src++;
            }
            *p = '\0';

	    if (p = getenv(word)) 	/* check for word in environment */
	      while (*p)
		*dest++ = *p++;

            continue;
        } else {			/* other cases: just copy */
            *dest++ = *src;
        }
        if (*src)			/* next character (if any) */
            src++;
    }

    *dest = '\0';
    return((envexpand && lineout[0] == '\0') ? NULL : lineout);
}


/*----------------------------------------------------------------------
    Sets  login, full_username and home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the fullname, login and home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/

int
init_username(ps)
     struct pine *ps;
{
    char fld_dir[MAXPATH+1];
    int  rv;

    rv = 0;

    /* Look up password first because it forces a real read of passwd file */
    if(get_system_passwd() != NULL)
        ps->passwd = cpystr(get_system_passwd());
    else
        ps->passwd = NULL;

    if(get_system_login() != NULL) {
        ps->VAR_USER_ID = cpystr(get_system_login());
    } else {
        fprintf(stderr, "Who are you? (Unable to look up login name)\n");
        ps->VAR_USER_ID = cpystr("");
        rv = -1;
    }

    if(ps->vars[V_PERSONAL_NAME].user_val.p != NULL) {
        ps->VAR_PERSONAL_NAME = cpystr(expand_variables(tmp_20k_buf,
                                        ps->vars[V_PERSONAL_NAME].user_val.p));
    } else if(get_system_fullname() != NULL) {
        ps->VAR_PERSONAL_NAME = cpystr(get_system_fullname());
    } else {
        ps->VAR_PERSONAL_NAME   = cpystr("");
    }


    if(strlen(ps->home_dir) + strlen(ps->VAR_MAIL_DIRECTORY)+2 > MAXPATH) {
        printf("Folders directory name is longer than %d\n", MAXPATH);
        printf("Directory name: \"%s/%s\"\n",ps->home_dir,
               ps->VAR_MAIL_DIRECTORY);
        return(-1);
    }
    build_path(fld_dir, ps->home_dir, ps->VAR_MAIL_DIRECTORY);
    ps->folders_dir = cpystr(fld_dir);

    dprint(1, (debugfile, "Userid: %s\nFullname: \"%s\"\n",
               ps->VAR_USER_ID, ps->VAR_PERSONAL_NAME));
    return(rv);
}


/*----------------------------------------------------------------------
        Fetch the hostname of the current system and put it in pine struct

   Args: ps -- The pine structure to put the hostname, etc in

  Result: hostname, localdomain, userdomain and maildomain are set


** Pine uses the following set of names:
  hostname -    The fully-qualified hostname.  Obtained with
                gethostbyname() which reads /etc/hosts or does a DNS
                lookup.  This may be blank.
  localdomain - The domain name without the host.  Obtained from the
                above hostname if it has a "." in it.  Removes first
                segment.  If hostname has no "." in it then the hostname
                is used.  This may be blank.
  userdomain -  Explicitly configured domainname.  This is read out of
                the global or  users .pinerc.  The users entry in the .pinerc
                overrides.


** Pine has the following uses for such names:

  1. On outgoing messages in the From: line
         Uses userdomain if there is one.  If not uses localdomain
         unless Pine has been configured to use full hostnames

  2. When expanding/fully-qualifying unqualified addresses during
     composition
         (same as 1)
         Uses userdomain if there is one.  If not uses localdomain
         unless Pine has been configured to use full hostnames

  3. When expanding/fully-qualifying unqualified addresses during
     composition when a local entry in the password file exists for
     name.
         If no userdomain is given, then this lookup is always done.
         Localdomain or hostname is used to qualify this address.
         If userdomain matches localdomain then the local lookup is
         done and localdomain is used on these addresses.  Pine may
         also be configured to do local lookup even if the user
         has placed himself in another domain.  In that case the lookup
         is kind of a strange thing to do, but if so desirest...

  4. In determining if an address is that of the current pine user for
     formatting index and filtering addresses when replying
         If a userdomain is specified the address must match the
         userdomain exactly.  If a userdomain is not specified or the
         userdomain is the same as the hostname or domainname, then
         an address will be considered the users if it matches either
         the domainname or the hostname.  Of course, the userid must match
         too. 
        
  5. In Message ID's
         The fully-qualified hostname is always users here.


** Setting the domain names
  To set the domain name for all Pine users on the system to be
different from what Pine figures out from DNS, set the domain name in
the "user-domain" variable in pine.conf.  To set the domain name for an
individual user, set the "user-domain" variable in his .pinerc.
The .pinerc setting overrides any other setting.
 ----*/
int
init_hostname(ps)
     struct pine *ps;
{
    char            hostname[MAX_ADDRESS+1], domainname[MAX_ADDRESS+1];

    ps->use_full_hostname = strucmp(ps->VAR_USE_ONLY_DOMAIN_NAME,"yes");

    getdomainnames(hostname, MAX_ADDRESS, domainname, MAX_ADDRESS);
    ps->hostname    = cpystr(hostname);
    ps->localdomain = cpystr(domainname);
    ps->userdomain  = NULL;

    if(ps->VAR_USER_DOMAIN && ps->VAR_USER_DOMAIN[0]){
        ps->maildomain = ps->userdomain = cpystr(ps->VAR_USER_DOMAIN);
        if(strucmp(ps->userdomain, ps->localdomain) == 0 ||
           strucmp(ps->userdomain, ps->hostname) == 0)
          ps->do_local_lookup = 1;
        else
          ps->do_local_lookup = 0;
    } else {
        ps->do_local_lookup = 1;
        ps->maildomain = ps->use_full_hostname ? ps->hostname :
                                                 ps->localdomain;
    }

    /* Set c-client idea of the host name for unqualifed addresses  */
    lhostn = ps_global->maildomain;

    dprint(1, (debugfile,"User domain name being used \"%s\"\n",
               ps->userdomain == NULL ? "" : ps->userdomain));
    dprint(1, (debugfile,"Local Domain name being used \"%s\"\n",
               ps->localdomain));
    dprint(1, (debugfile,"Host name being used \"%s\"\n",
               ps->hostname));
    dprint(1, (debugfile,"Mail Domain name being used \"%s\"\n",
               ps->maildomain));

    if(ps->maildomain == NULL || strlen(ps->maildomain) == 0) {
        fprintf(stderr, "No host name or domain name set\n");
        return(-1);
    } else {
        return(0);
    }
}


/*
 * parse_list - takes a comma delimited list of "count" elements and
 *              returns an array of pointers to each element neatly
 *              malloc'd in its own array.  Any errors are returned
 *              in the string pointed to by "error"
 *
 *  NOTE: only recognizes escaped quotes
 */
char **
parse_list(list, count, error)
    char *list, **error;
    int   count;
{
    char **lvalue, *p2, *p3, *p4;
    int    was_quoted = 0;

    lvalue = (char **)fs_get((count+1)*sizeof(char *));
    count  = 0;
    while(*list){			/* pick elements from list */
	p2 = list;		/* find end of element */
	while(1){
	    if(*p2 == '"')	/* ignore ',' if quoted   */
	      was_quoted = (was_quoted) ? 0 : 1 ;

	    if(*p2 == '\\' && *(p2+1) == '"')
	      p2++;		/* preserve escaped quotes, too */

	    if((*p2 == ',' && !was_quoted) || *p2 == '\0')
	      break;

	    p2++;
	}

	if(was_quoted){		/* unbalanced parens! */
	    if(error)
	      *error = "Unbalanced parentheses";

	    break;
	}

	/*
	 * if element found, eliminate trailing 
	 * white space and tie into variable list
	 */
	if(p2 != list){
	    for(p3 = p2 - 1; isspace(*p3) && list < p3; p3--)
	      ;

	    p4 = fs_get(((p3 - list) + 2) * sizeof(char));
	    lvalue[count++] = p4;
	    while(list <= p3)
	      *p4++ = *list++;

	    *p4 = '\0';
	}

	if(*(list = p2) != '\0'){	/* move to beginning of next val */
	    while(*list == ',' || isspace(*list))
	      list++;
	}
    }

    lvalue[count] = NULL;		/* tie off pointer list */
    return(lvalue);
}




/*
 * This is only used at startup time.  It sets ps->sort_order and
 * ps->reverse_sort.  The syntax of the sort_spec is type[/reverse].
 * A reverse without a type is the same as arrival/reverse.  A blank
 * argument also means arrival/reverse.
 */
int
decode_sort(ps, sort_spec)
     struct pine *ps;
     char        *sort_spec;
{
    char *sep;
    int    x, reverse;

    if(*sort_spec == '\0' ||
		   struncmp(sort_spec, "reverse", strlen(sort_spec)) == 0) {
        ps->reverse_sort = 1; 
        return(0);
    }
     
    reverse = 0;
    if((sep = strindex(sort_spec, '/')) != NULL) {
        *sep = '\0';
        sep++;
        if(struncmp(sep, "reverse", strlen(sep)) == 0)
          reverse = 1;
        else
          return(-1);
    }

    for(x = 0; ps_global->sort_types[x] != EndofList; x++)
      if(struncmp(sort_name(ps_global->sort_types[x]),
                  sort_spec, strlen(sort_spec)) == 0)
        break;

    if(ps_global->sort_types[x] == EndofList)
      return(-1);

    ps->reverse_sort = reverse;
    ps->sort_order   = ps_global->sort_types[x];
    return(0);
}



/*----------------------------------------------------------------------
      Set a user variable list in memory (pinerc is never rewritten)

  Args:  var -- The index of the variable to set from pine.h (V_....)
         lvalue -- The list to set the value to

 Result: 0 is always returned

 The vars data structure is updated; the change does not persist to disk.
 ----*/
int
set_variable_list(var, lvalue)
    int    var;
    char **lvalue;
{
    int              i;
    struct variable *v = &ps_global->vars[var];

    if(!v->is_user || !v->is_list)
      panic1("BOTCH: Trying to set non-user or non-list variable %s", v->name);

    if(v->user_val.l){
	for(i = 0; v->user_val.l[i] ; i++)
	  fs_give((void **) &v->user_val.l[i]);

	fs_give((void **) &v->user_val.l);
    }

    if(v->current_val.l){
	for(i = 0; v->current_val.l[i] ; i++)
	  fs_give((void **) &v->current_val.l[i]);

	fs_give((void **) &v->current_val.l);
    }

/* BUG: HAVING MULTIPLE COPIES OF CONFIG DATA IS BOGUS */
    if(lvalue){
	for(i = 0; lvalue[i] ; i++)	/* count elements */
	  ;

	v->user_val.l    = (char **) fs_get((i+1) * sizeof(char *));
	v->current_val.l = (char **) fs_get((i+1) * sizeof(char *));

	for(i = 0; lvalue[i] ; i++){
	    v->user_val.l[i]    = cpystr(lvalue[i]);
	    v->current_val.l[i] = cpystr(lvalue[i]);
	}

	v->user_val.l[i]    = NULL;
	v->current_val.l[i] = NULL;
    }


    return(0);
}
           

/*----------------------------------------------------------------------
    Make sure the pine folders directory exists, with proper folders

   Args: dir -- name of directory to initialize as mail directory

  Result: returns 0 if it exists or it is created and all is well
                  1 if it is missing and can't be created.
  ----*/
int
init_mail_dir(dir)
     char *dir;
{
    switch(is_writable_dir(dir)) {
      case 0:
        /* --- all is well --- */
        return(0);

        
      case 1:
        printf("The \"mail\" subdirectory already exists, but it is not writable by pine\n");
    	printf("so pine cannot run. Please correct the permissions and restart pine.\n");
    	return(-1);

      case 2:
    	printf("Pine requires a directory called \"mail\" and usualy creates it.\n");
    	printf("You already have a regular file called \"mail\" which is in the way.\n");
    	printf("Please move or remove \"mail\" and start pine again.\n");
        return(-1);
    	
  
      case 3:
        printf("Creating subdirectory \"%s\" where pine will store\nits mail folders.\n", dir);
    	sleep(4);
        if(create_mail_dir(dir) < 0) {
            printf("\007Error creating subdirectory \"%s\" : %s\n", dir, 
                   error_description(errno));
            return(-1);
        }
    }

    return(0);
}


#ifdef DEBUG
/*----------------------------------------------------------------------
     Initialize debugging - open the debug log file

  Args: none

 Result: opens the debug logfile for dprints

   Opens the file "~/.pine.debug1. Also maintains .pine.debug[2-4]
by renaming them each time so the last 4 sessions are saved.
  ----*/

int debug = DEFAULT_DEBUG;
FILE *debugfile = NULL;

void
init_debug()
{
    char nbuf[12];		/* enough for any int + sign + nul */
    char newfname[MAXPATH+1], filename[MAXPATH+1];

    /*---- Get rid of very old pine debug files ----*/
    build_path(filename, ps_global->home_dir, "pine-debug-0.9");
    unlink(filename);
    build_path(filename, ps_global->home_dir, ".pine.debug.last");
    unlink(filename);
    
    if (debug) {		/* setup for dprint() statements! */
      int i;

      build_path(filename, ps_global->home_dir, DEBUGFILE);
      (void)unlink(filename); /* Just do it even though it may fail */

      for(i = NUMDEBUGFILES - 1; i > 0; i--) {
        build_path(filename, ps_global->home_dir, DEBUGFILE);
        strcpy(newfname, filename);
        sprintf(nbuf, "%d", i);
        strcat(filename, nbuf);
        sprintf(nbuf, "%d", i+1);
        strcat(newfname, nbuf);
        (void) rename_file(filename, newfname);
      }


      build_path(filename, ps_global->home_dir, DEBUGFILE);
      strcat(filename, "1");

      debugfile = fopen(filename, "w");
      if(debugfile != NULL){
          long now = time(0);
          fprintf(debugfile, 
 "Debug output of the Pine program (at debug level %d).  Version %s\n%s\n",
    	   debug, version_buff, ctime(&now));
      }
    }
}
#endif
