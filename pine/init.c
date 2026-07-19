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
       read_pinerc
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

typedef enum {ParseLocal, ParseGlobal} ParsePinerc;

#ifdef ANSI
int  read_pinerc(char *, struct variable *, ParsePinerc);
void set_current_val(struct variable *, int, int);
void set_feature_list_current_val(struct variable *, int);
/* AIX gives warning here 'cause it can't quite cope with enums */
void process_init_cmds(struct pine *, char **);
void process_feature_list(struct pine *, char **, int , int, int);
int  decode_sort(struct pine *, char *);
#else /* !ANSI */
int  read_pinerc();
void set_current_val();
void set_feature_list_current_val();
void process_init_cmds();
void process_feature_list();
int  decode_sort();
#endif /* !ANSI */


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

#define	FEATURE_LIST_TEXT	"feature-list= comma-separated list of features from the following set:\n# Note: the value \"old-growth\" is shorthand for the indicated features...\n# enable-full-header-cmd        (included in old-growth set)\n# enable-unix-pipe-cmd          (included in old-growth set)\n# enable-bounce-cmd             (included in old-growth set)\n# enable-suspend                (included in old-growth set)\n# \
enable-tab-completion         (included in old-growth set)\n# enable-jump-shortcut          (included in old-growth set)\n# quit-without-confirm          (included in old-growth set)\n# enable-apply-cmd              (included in old-growth set)\n# enable-flag-cmd               (included in old-growth set)\n# enable-zoom-cmd               (included in old-growth set)\n# expanded-view-of-folders\n# use-function-keys\n# include-header-in-reply\n# signature-at-bottom\n# delete-skips-deleted\n# expunge-without-confirm\n# auto-move-read-msgs\n# e.g.\n# feature-list= old-growth, signature-at-bottom\n# \
The feature-list that is in effect when you are running Pine is the\n# combination of the feature-list set by the system manager in the pine.conf\n# file plus the feature-list set by you here.  (PC-Pine doesn't have a\n# pine.conf so this doesn't apply in that case.)  The feature-list is additive\n# and the order is significant.  First the system-wide pine.conf values are\n# read, then yours.  Any of the features listed above may also have the\n# \
characters \"no-\" prepended to it, to cancel its effect.  So, for example,\n# you could set \"feature-list= old-growth, no-quit-without-confirm\" if you\n# wanted to have all of the old-growth features except for that one.  Or,\n# if the system manager has set \"old-growth\" in the pine.conf file, you\n# can cancel that with \"no-old-growth\".  The default condition for all\n# of the features is no-, that is, off."

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
#ifdef DOS		/* Have to have this on DOS, PC's, Macs, etc... */
{"user-id",                    0, 1, 0, 1, 0, 0,
#else			/* Don't allow on UNIX machines for some security */
{"user-id",                    0, 0, 0, 1, 0, 0,
#endif
  "Your login/e-mail user name"},
{"user-domain",                0, 1, 0, 1, 1, 0,
  USER_DOMAIN_TEXT},
{"smtp-server",                1, 1, 0, 1, 1, 1,
  "Smtp-server is disabled, outgoing mail is always sent via local sendmail"},
{"inbox-path",                 0, 1, 0, 1, 1, 0, 
  INBOX_TEXT},
{"incoming-folders",           1, 1, 0, 1, 1, 1,
  "Incoming-folders is disabled, multi-collection support has been removed"},
{"folder-collections",         0, 1, 0, 1, 1, 1,
  FOLDER_TEXT},
{"news-collections",           1, 1, 0, 1, 1, 1,
  "News-collections is disabled, news reading support has been removed"},
{"default-fcc",                0, 1, 0, 1, 1, 0,
  DEFAULT_FCC_TEXT},
{"mail-directory",             0, 1, 0, 1, 1, 0,
  "mail-directory is where postponed & interrupted msgs are held temporarily."},
{"read-message-folder",        0, 1, 0, 1, 1, 0,
  READ_MSG_TEXT},
{"signature-file",             0, 1, 0, 1, 1, 0,
  SIG_FILE_TEXT},
{"address-book",               1, 1, 0, 1, 1, 0,
  "Address-book is disabled, address book support has been removed"},
{"feature-list",               0, 1, 0, 1, 1, 1,
  FEATURE_LIST_TEXT},
{"initial-keystroke-list",     0, 1, 0, 1, 1, 1,
 "Pine executes these keys upon startup (e.g. to view msg 13: i,j,1,3,CR,v)"},
{"saved-msg-name-rule",        0, 1, 0, 1, 1, 0,
  SAVE_MSG_TEXT},
{"sort-key",                   0, 1, 0, 1, 1, 0,
  SORT_TEXT},
{"character-set",              1, 1, 0, 1, 1, 0,
  "Character-set is disabled, this build only supports US-ASCII"},
{"editor",                     1, 1, 0, 1, 1, 0,
  "Alternate editor is disabled, this build only supports the built-in composer"},
{"image-viewer",               1, 1, 0, 1, 1, 0,
  "Image-viewer is disabled, this build has no attachment display support"},
{"use-only-domain-name",       0, 1, 0, 1, 1, 0,
  ONLY_DOMAIN_TEXT},
{"printer",                    1, 1, 0, 1, 1, 0,
  "Printing is disabled, this build has no printer support"},
{"personal-print-command",     1, 1, 0, 1, 0, 0,
  "Printing is disabled, this build has no printer support"},
{"standard-printer",           1, 1, 0, 0, 1, 0,
  "Printing is disabled, this build has no printer support"},
{"last-time-prune-questioned" ,1, 1, 0, 1, 0, 0,
  "Sent-mail pruning is disabled, this build has no automatic expiration"},
{"last-version-used",          0, 1, 0, 1, 0, 0,
  "Version of Pine used last session (set automatically)"},
{"bugs-nickname",              0, 1, 0, 0, 1, 0,
  "Nickname of bugs entry to be automatically placed in all address books"},
{"bugs-fullname",              0, 1, 0, 0, 1, 0,
  "Full name of bugs entry to be automatically placed in all address books"},
{"bugs-address",               0, 1, 0, 0, 1, 0,
  "Email address of bugs entry to be automatically placed in all address books"},
{"elm-style-save",             1, 1, 0, 1, 1, 0,
  "Elm-style-save is obsolete, use saved-msg-name-rule"},
{"header-in-reply",            1, 1, 0, 1, 1, 0,
  "Header-in-reply is obsolete, use include-header-in-reply in feature-list"},
{"feature-level",              1, 1, 0, 1, 1, 0,
  "Feature-level is obsolete, use feature-list"},
{"old-style-reply",            1, 1, 0, 1, 1, 0,
  "Old-style-reply is obsolete, use signature-at-bottom in feature-list"},
{"compose-mime",               1, 1, 0, 0, 1, 0,
  "Compose-mime is obsolete"},
{"show-all-characters",        1, 1, 0, 1, 1, 0,
  "Show-all-characters is obsolete"},
{"save-by-sender",             1, 1, 0, 1, 1, 0,
  "Save-by-sender is obsolete, use saved-msg-name-rule"},
{NULL,                         0, 0, 0, 0, 0, 0,
   NULL}
};


static struct pinerc_line {
  char *line;
  struct variable *var;
  unsigned int  is_var:1;
  unsigned int  is_quoted:1;
#ifdef DONT_DO_THIS
  unsigned int  no_such_var:1;
#endif
  unsigned int  obsolete_var:1;
} *pinerc_lines = NULL;


init_init_vars(ps)
     struct pine *ps;
{
    ps->vars = variables;
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
    char	 buf[MAXPATH +1];
    register struct variable *vars = ps->vars;
    int		 obs_header_in_reply,     /* the obs_ variables are to       */
		 obs_old_style_reply,     /* support backwards compatibility */
		 obs_save_by_sender;
    FeatureLevel obs_feature_level;

    /*--- The defaults here are defined in os-xxx.h so they can vary
          per machine ---*/
    vars[V_ELM_STYLE_SAVE].global_val.p   = cpystr(DF_ELM_STYLE_SAVE);
    vars[V_SAVE_BY_SENDER].global_val.p   = cpystr(DF_SAVE_BY_SENDER);
    vars[V_HEADER_IN_REPLY].global_val.p  = cpystr(DF_HEADER_IN_REPLY);
    vars[V_INBOX_PATH].global_val.p       = cpystr("inbox");
    vars[V_DEFAULT_FCC].global_val.p      = cpystr(DF_DEFAULT_FCC);
    vars[V_USE_ONLY_DOMAIN_NAME].global_val.p = cpystr(DF_USE_ONLY_DOMAIN_NAME);
    vars[V_FEATURE_LEVEL].global_val.p    = cpystr(DF_FEATURE_LEVEL);
    vars[V_OLD_STYLE_REPLY].global_val.p  = cpystr(DF_OLD_STYLE_REPLY);
    vars[V_SORT_KEY].global_val.p         = cpystr(DF_SORT_KEY);
    vars[V_SAVED_MSG_NAME_RULE].global_val.p = cpystr(DF_SAVED_MSG_NAME_RULE);
#ifdef FIRST_FOLDER_TRY
    vars[V_LOCAL_MAIL_FORMAT].global_val.p= cpystr(DF_LOCAL_MAIL_FORMAT);
#endif
    vars[V_SIGNATURE_FILE].global_val.p   = cpystr(DF_SIGNATURE_FILE);
    vars[V_MAIL_DIRECTORY].global_val.p   = cpystr(DF_MAIL_DIRECTORY);

#ifdef	DOS
    /*
     * Rules for the config/support file locations under DOS are:
     *
     * 1) The location of the PINERC is searched for in the following
     *    order of precedence:
     *       - File pointed to by PINERC environment variable
     *       - $HOME\pine
     *       - same dir as argv[0]
     *
     * 2) The HOME environment variable, if not set, defaults to 
     *    root of the current working drive (see pine.c)
     * 
     * 3) The default for external files (PINE.SIG and ADDRBOOK) is the
     *    same directory as the pinerc
     *
     * 4) The support files (PINE.HLP and PINE.NDX) are searched for
     *    in:
     *       - the directory pointed to by PINEHOME environment variable
     *       - the same directory as PINERC
     *       - the same directory as PINE.EXE.
     *
     */
    if(!ps_global->pinerc) {
	char *p;
	int   l;

	if(p = getenv("PINERC")){
	    ps_global->pinerc = cpystr(p);
	}
	else{
	    char buf2[MAXPATH];
	    build_path(buf2, ps_global->home_dir, DF_PINEDIR);
	    build_path(buf, buf2, SYSTEM_PINERC);
	    ps_global->pinerc = cpystr(buf);
	}
    }
#else
    if(ps_global->pine_conf)
      read_pinerc(ps_global->pine_conf, vars, ParseGlobal);
    else
      read_pinerc(SYSTEM_PINERC, vars, ParseGlobal);

    if(!ps_global->pinerc) {
      build_path(buf, ps->home_dir, ".pinerc");
      ps_global->pinerc = cpystr(buf);
    }
#endif
    read_pinerc(ps_global->pinerc, vars, ParseLocal);

    set_current_val(vars, V_INBOX_PATH, TRUE);

    set_current_val(vars, V_USER_DOMAIN, TRUE);
    set_current_val(vars,V_USE_ONLY_DOMAIN_NAME, TRUE);

    /* obsolete, backwards compatibility */
    set_current_val(vars,V_HEADER_IN_REPLY, TRUE);
    obs_header_in_reply=!strucmp(VAR_HEADER_IN_REPLY, "yes");

    set_current_val(vars,V_BUGS_NICKNAME, TRUE);
    set_current_val(vars,V_BUGS_FULLNAME, TRUE);
    set_current_val(vars,V_BUGS_ADDRESS, TRUE);
    set_current_val(vars,V_DEFAULT_FCC, TRUE);
    set_current_val(vars,V_READ_MESSAGE_FOLDER, TRUE);

    /* obsolete, backwards compatibility */
    set_current_val(vars, V_FEATURE_LEVEL, TRUE);
    if(strucmp(VAR_FEATURE_LEVEL, "seedling") == 0)
      obs_feature_level = Seedling;
    else if(strucmp(VAR_FEATURE_LEVEL, "old-growth") == 0)
      obs_feature_level = Seasoned;
    else
      obs_feature_level = Sapling;

    /* obsolete, backwards compatibility */
    set_current_val(vars, V_OLD_STYLE_REPLY, TRUE);
    obs_old_style_reply = !strucmp(VAR_OLD_STYLE_REPLY, "yes");

    set_current_val(vars, V_SIGNATURE_FILE, TRUE);

    set_current_val(vars, V_LAST_VERS_USED, TRUE);
    if(VAR_LAST_VERS_USED && strcmp(VAR_LAST_VERS_USED, version_buff) == 0)
      ps->show_new_version = 0;
    else
      ps->show_new_version = 1;
    /* want to write new version to pinerc */
    if(ps->show_new_version)
      set_variable(V_LAST_VERS_USED, version_buff);
    
    /* Obsolete, backwards compatibility */
    set_current_val(vars, V_ELM_STYLE_SAVE, TRUE);
    /* Also obsolete */
    set_current_val(vars, V_SAVE_BY_SENDER, TRUE);
    if(!strucmp(VAR_ELM_STYLE_SAVE, "yes"))
      set_variable(V_SAVE_BY_SENDER, "yes");
    obs_save_by_sender = !strucmp(VAR_SAVE_BY_SENDER, "yes");

    /*
     * mail-directory variable is obsolete, put its value in
     * folder-collection list if that list is blank...
     */
    set_current_val(vars, V_MAIL_DIRECTORY, TRUE);
    set_current_val(vars, V_FOLDER_SPEC, TRUE);
    if(VAR_FOLDER_SPEC == NULL){
	char *lvalue[2];
	sprintf(tmp_20k_buf, "%s%c[]", VAR_MAIL_DIRECTORY,
#ifdef	DOS
		'\\');
#else
		'/');
#endif
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
    else if(!strucmp(VAR_SAVED_MSG_NAME_RULE, "default-folder")) {
        ps->save_by_sender = 0;
	/* if user overrode explicitly, don't change back */
        if(!strucmp(vars[V_SAVED_MSG_NAME_RULE].user_val.p, "default-folder"))
	    obs_save_by_sender = 0;  /* don't overwrite */
    }
    /* backwards compatibility */
    if(obs_save_by_sender) {
        ps->save_by_from = 1;
	set_variable(V_SAVED_MSG_NAME_RULE, "by-from");
    }

    set_feature_list_current_val(vars, V_FEATURE_LIST);
    process_feature_list(ps, VAR_FEATURE_LIST,
           (obs_feature_level == Seasoned) ? 1 : 0,
	   obs_header_in_reply, obs_old_style_reply);

    /* this should come after process_feature_list because of use_fkeys */
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


struct features {
    char *name;        /* the name that goes in the config file */
    int   var_number;  /* the internal bit number */
};
static struct features feat_list[] = {
    {"old-growth", F_OLD_GROWTH},
    {"enable-full-header-cmd", F_ENABLE_FULL_HDR},
    {"enable-unix-pipe-cmd", F_ENABLE_PIPE},
    {"enable-tab-completion", F_ENABLE_TAB_COMPLETE},
    {"quit-without-confirm", F_QUIT_WO_CONFIRM},
    {"enable-jump-shortcut", F_ENABLE_JUMP},
    {"enable-bounce-cmd", F_ENABLE_BOUNCE},
    {"enable-apply-cmd", F_ENABLE_APPLY},
    {"enable-flag-cmd", F_ENABLE_FLAG},
    {"enable-zoom-cmd", F_ENABLE_ZOOM},
    {"enable-forward-as-mime", F_MIME_FORWARD},
    {"enable-suspend", F_CAN_SUSPEND},
    {"expanded-view-of-folders", F_EXPANDED_FOLDERS},
    {"use-function-keys", F_USE_FK},
    {"include-header-in-reply", F_INCLUDE_HEADER},
    {"signature-at-bottom", F_SIG_AT_BOTTOM},
    {"delete-skips-deleted", F_DEL_SKIPS_DEL},
    {"expunge-without-confirm", F_AUTO_EXPUNGE},
    {"auto-move-read-msgs", F_AUTO_READ_MSGS},
    {NULL, 0}
};

/*
 * All the arguments past "list" are the backwards compatibility hacks.
 */
void
process_feature_list(ps, list, old_growth, hir, osr)
    struct pine *ps;
    char **list;
    int old_growth, hir, osr;
{
    register struct variable *vars = ps->vars;
    register char *q;
    char **p;
    char *lvalue[LARGEST_BITMAP];
    int   i, yorn;
    struct features *feat;


    /* backwards compatibility */
    if(hir)
	F_TURN_ON(F_INCLUDE_HEADER, ps);

    /* ditto */
    if(osr)
	F_TURN_ON(F_SIG_AT_BOTTOM, ps);

    /* ditto */
    if(old_growth) {
	F_TURN_ON(F_ENABLE_FULL_HDR, ps);
	F_TURN_ON(F_ENABLE_PIPE, ps);
	F_TURN_ON(F_ENABLE_TAB_COMPLETE, ps);
	F_TURN_ON(F_QUIT_WO_CONFIRM, ps);
	F_TURN_ON(F_ENABLE_JUMP, ps);
	F_TURN_ON(F_ENABLE_BOUNCE, ps);
	F_TURN_ON(F_ENABLE_APPLY, ps);
	F_TURN_ON(F_ENABLE_FLAG, ps);
	F_TURN_ON(F_ENABLE_ZOOM, ps);
	F_TURN_ON(F_CAN_SUSPEND, ps);
    }


    /* now run through the list (both global and user lists are in list) */
    if(list) {
      for(p = list; (q = *p) != NULL; p++) {
	if(struncmp(q, "no-", 3) == 0) {
	  yorn = 0;
	  q += 3;
	}else {
	  yorn = 1;
	}

	for(feat=feat_list; feat->name; feat++) {
	  if(strucmp(q, feat->name) == 0) {
	    if(feat->var_number == F_OLD_GROWTH) {
	      F_SET(F_ENABLE_FULL_HDR, ps, yorn);
	      F_SET(F_ENABLE_PIPE, ps, yorn);
	      F_SET(F_ENABLE_TAB_COMPLETE, ps, yorn);
	      F_SET(F_QUIT_WO_CONFIRM, ps, yorn);
	      F_SET(F_ENABLE_JUMP, ps, yorn);
	      F_SET(F_ENABLE_BOUNCE, ps, yorn);
	      F_SET(F_ENABLE_APPLY, ps, yorn);
	      F_SET(F_ENABLE_FLAG, ps, yorn);
	      F_SET(F_ENABLE_ZOOM, ps, yorn);
	      F_SET(F_CAN_SUSPEND, ps, yorn);
	    }else {
	      F_SET(feat->var_number, ps, yorn);
	    }
	    break;
	  }
	}
	/* if it wasn't in that list */
	if(feat->name == NULL) {
          dprint(1, (debugfile, "Unrecognized feature in feature-list (%s%s)", (yorn ? "" : "no-"), q));
	}
      }
    }

    /* Will we have to build a new list? */
    if(!(old_growth || hir || osr))
	return;

    /*
     * Build a new list for feature-list.  The only reason we ever need to
     * do this is if one of the obsolete options is being converted
     * into a feature-list item, and it isn't already included in the user's
     * feature-list.
     */
    i = 0;
    for(p = vars[V_FEATURE_LIST].user_val.l; p && (q = *p); p++) {
      /* already have it or cancelled it, don't need to add later */
      if(hir && (strucmp(q, "include-header-in-reply") == 0 ||
                             strucmp(q, "no-include-header-in-reply") == 0)) {
	hir = 0;
      }else if(osr && (strucmp(q, "signature-at-bottom") == 0 ||
                             strucmp(q, "no-signature-at-bottom") == 0)) {
	osr = 0;
      }else if(old_growth && (strucmp(q, "old-growth") == 0 ||
                             strucmp(q, "no-old-growth") == 0)) {
	old_growth = 0;
      }
      lvalue[i++] = cpystr(q);
    }

    /* check to see if we still need to build a new list */
    if(!(old_growth || hir || osr))
	return;

    if(hir)
      lvalue[i++] = "include-header-in-reply";
    if(osr)
      lvalue[i++] = "signature-at-bottom";
    if(old_growth)
      lvalue[i++] = "old-growth";
    lvalue[i] = NULL;
    set_variable_list(V_FEATURE_LIST, lvalue);
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

    if(fkeys && !not_fkeys)
      F_TURN_ON(F_USE_FK,ps);
    if(!fkeys && not_fkeys)
      F_TURN_OFF(F_USE_FK,ps);

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


/*
 * Feature-list has to be handled separately from the other variables
 * because it is additive.  The other variables choose one of command line,
 * or pine.conf, or pinerc.  Feature list adds them (except there is not
 * a command-line option).  This could easily be converted to a general
 * purpose routine if we add more additive variables.
 */
void    
set_feature_list_current_val(vars, v_no)
  struct variable *vars;
  int              v_no;
{
    char **list;
    int    i, j,
	   elems = 0;

    /* count the lists so I can allocate */
    if(list=vars[v_no].global_val.l)
      for(i = 0; list[i]; i++)
        elems++;
    if(list=vars[v_no].user_val.l)
      for(i = 0; list[i]; i++)
        elems++;

    vars[v_no].current_val.l = (char **)fs_get((elems+1) * sizeof(char *));

    j = 0;
    if(list=vars[v_no].global_val.l)
      for(i = 0; list[i]; i++)
        vars[v_no].current_val.l[j++] = cpystr(list[i]);
    if(list=vars[v_no].user_val.l)
      for(i = 0; list[i]; i++)
        vars[v_no].current_val.l[j++] = cpystr(list[i]);

    vars[v_no].current_val.l[j] = NULL;
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
#ifndef	DOS
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
#else
        if (*src == '$') {		/* shell variable */
#endif
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

#ifdef DOS
    if(ps->vars[V_USER_ID].user_val.p != NULL) {
        ps->VAR_USER_ID =  cpystr(expand_variables(tmp_20k_buf,
				             ps->vars[V_USER_ID].user_val.p));
    } else {
        ps->VAR_USER_ID   = cpystr("");
	ps->blank_user_id = 1;
    }
#else
    if(get_system_login() != NULL) {
        ps->VAR_USER_ID = cpystr(get_system_login());
    } else {
        fprintf(stderr, "Who are you? (Unable to look up login name)\n");
        ps->VAR_USER_ID = cpystr("");
        rv = -1;
    }
#endif

    if(ps->vars[V_PERSONAL_NAME].user_val.p != NULL) {
        ps->VAR_PERSONAL_NAME = cpystr(expand_variables(tmp_20k_buf,
                                        ps->vars[V_PERSONAL_NAME].user_val.p));
    } else if(get_system_fullname() != NULL) {
        ps->VAR_PERSONAL_NAME = cpystr(get_system_fullname());
    } else {
        ps->VAR_PERSONAL_NAME   = cpystr("");
#ifdef	DOS
	ps->blank_personal_name = 1;
#endif
    }


    if(strlen(ps->home_dir) + strlen(ps->VAR_MAIL_DIRECTORY)+2 > MAXPATH) {
        printf("Folders directory name is longer than %d\n", MAXPATH);
        printf("Directory name: \"%s/%s\"\n",ps->home_dir,
               ps->VAR_MAIL_DIRECTORY);
        return(-1);
    }
#ifdef	DOS
    if(ps->VAR_MAIL_DIRECTORY[1] == ':')
      strcpy(fld_dir, ps->VAR_MAIL_DIRECTORY);
    else
#endif
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
#ifndef	DOS
        if(strucmp(ps->userdomain, ps->localdomain) == 0 ||
           strucmp(ps->userdomain, ps->hostname) == 0)
          ps->do_local_lookup = 1;
        else
#endif
          ps->do_local_lookup = 0;
    } else {
#ifdef	DOS
	if(ps->VAR_USER_DOMAIN)
	  ps->blank_user_domain = 1;	/* user domain set to null string! */

        ps->do_local_lookup   = 0;
        ps->maildomain        = ps->localdomain[0] ? ps->localdomain 
						   : ps->hostname; 
#else
        ps->do_local_lookup = 1;
        ps->maildomain = ps->use_full_hostname ? ps->hostname :
                                                 ps->localdomain;
#endif
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
#ifdef	DOS
	if(ps->blank_user_domain)
	  return(0);		/* prompt for this in send.c:dos_valid_from */
#endif
        fprintf(stderr, "No host name or domain name set\n");
        return(-1);
    } else {
        return(0);
    }
}


/*----------------------------------------------------------------------
         Read and parse a pinerc file
  
   Args:  Filename   -- name of the .pinerc file to open and read
          vars       -- The vars structure to store values in
          which_vars -- Whether the local or global values are being read

   Result: 

 This may be the local file or the global file.  The values found are
merged with the values currently in vars.  All values are strings and
are malloced; and existing values will be freed before the assignment.
Those that are <unset> will be left unset; their values will be NULL.
  ----*/
int
read_pinerc(filename, vars, which_vars)
     char *filename;
     ParsePinerc which_vars;
     struct variable *vars;
{
    char               *file, *value, **lvalue, *line, *error;
    register char      *p, *p1;
    struct variable    *v;
    struct pinerc_line *pline;
    int                 line_count, was_quoted;

    dprint(1, (debugfile, "reading_pinerc \"%s\"\n", filename));

    file = read_file(filename);
    if(file == NULL) {
        dprint(1, (debugfile, "Open failed: %s\n", error_description(errno)));
	if(which_vars == ParseLocal)
	    ps_global->first_time_user = 1;
        return(-1);
    }

    dprint(1, (debugfile, "Read %ld characters\n", (long)strlen(file)));

    /*--- Count up lines and allocate structures */
    for(line_count = 0, p = file; *p != '\0'; p++)
      if(*p == '\n')
        line_count++;
    pinerc_lines = (struct pinerc_line *)
               fs_get((3 + line_count) * sizeof(struct pinerc_line));
    memset((void *)pinerc_lines, 0,
			    (3 + line_count) * sizeof(struct pinerc_line));
    pline = pinerc_lines;

    for(p = file, line = file; *p != '\0';) {
        /*----- Grab the line ----*/
        line = p;
        while(*p && *p != '\n')
          p++;
        if(*p == '\n') {
            *p++ = '\0';
        }

        /*----- Comment Line -----*/
        if(*line == '#') {
            if(which_vars == ParseLocal){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	if(*line == '\0' || *line == '\t' || *line == ' ') {
            p1 = line;
            while(*p1 == '\t' || *p1 == ' ')
               p1++;
	    if(*p1 != '\0') {
	      fprintf(stderr,
      "Syntax error in %s: continuation line with nothing to continue: %s\n",
			      filename, line);
	    } /* else blank line */
            if(which_vars == ParseLocal){
                pline->is_var = 0;
                pline->line = "";
                pline++;
            }
            continue;
	}
	
        /*----- look up the variable ----*/
        for(v = vars; *line && v->name != NULL; v++) {
            if(strncmp(v->name, line, strlen(v->name)) == 0)
              break;
        }

        /*----- Didn't match any variable -----*/
        if(v->name == NULL){
            if(which_vars == ParseLocal){           
                pline->is_var = 0;
#ifdef DONT_DO_THIS
                pline->no_such_var = 1;
#endif
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

        /*----- Obsolete variable, read it anyway below, might use it -----*/
        if(v->is_obsolete){
            if(which_vars == ParseLocal){           
                pline->obsolete_var = 1;
                pline->line = cpystr(line);
                pline->var = v;
            }
        }

        /*----- Variable is in the list but unused for some reason -----*/
        if(!v->is_used){
            if(which_vars == ParseLocal){           
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	/*----- Skip to '=' -----*/
        p1 = line + strlen(v->name);
        while(*p1 && (*p1 == '\t' || *p1 == ' '))
          p1++;

        /*---- Rest of the line is garbage ----*/
        if(*p1 != '=') {
	    fprintf(stderr, "Syntax error in %s: \"%s\"\n", filename, line);
            if(which_vars == ParseLocal){           
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
	    continue;
        }
        p1++;

        /*----- Matched a variable, get its value ----*/
        while(*p1 == ' ' || *p1 == '\t')
          p1++; /* skipping leading space */
        value = p1;

        /*---- variable is unset ----*/
        if(*value == '\0') {
            if(v->is_user && which_vars == ParseLocal) {
                pline->is_var   = 1;
                pline->var = v;
                pline++;
            }
            continue;
        }

        /*--value is non-empty, store it handling quotes and trailing space--*/
        if(*value == '"' && !v->is_list) {
            was_quoted = 1;
            value++;
            for(p1 = value; *p1 && *p1 != '"'; p1++);
            if(*p1 == '"')
              *p1 = '\0';
            else
              removing_trailing_white_space(value);
        } else {
            removing_trailing_white_space(value);
            was_quoted = 0;
        }

	/*
	 * List Entry Parsing
	 *
	 * The idea is to parse a comma separated list of 
	 * elements, preserving quotes, and understanding
	 * continuation lines (that is ',' == "\n ").
	 * Quotes must be balanced within elements.  Space 
	 * within elements is preserved, but leading and trailing 
	 * space is trimmed.  This is a generic function, and it's 
	 * left to the the functions that use the lists to make sure
	 * they contain valid data...
	 */
	if(v->is_list){

	    was_quoted = 0;
	    line_count = 0;
	    p1         = value;
	    while(1){			/* generous count of list elements */
		if(*p1 == '"')		/* ignore ',' if quoted   */
		  was_quoted = (was_quoted) ? 0 : 1 ;

		if((*p1 == ',' && !was_quoted) || *p1 == '\n' || *p1 == '\0')
		  line_count++;		/* count this element */

		if(*p1 == '\0' || *p1 == '\n'){	/* deal with EOL */
		    if(p1 < p || *p1 == '\n'){
			*p1++ = ','; 	/* fix null or newline */

			if(*p1 != '\t' && *p1 != ' '){
			    *(p1-1) = '\0'; /* tie off list */
			    p       = p1;   /* reset p */
			    break;
			}
		    }
		    else{
			p = p1;		/* end of pinerc */
			break;
		    }
		}
		else
		  p1++;
	    }

	    error  = NULL;
	    lvalue = parse_list(value, line_count, &error);
	    if(error){
		dprint(1, (debugfile,
		       "read_pinerc: ERROR: %s in %s = \"%s\"\n", 
			   error, v->name, value));
	    }
	}

        if(which_vars == ParseLocal){
            if(v->is_user) {
		if(v->is_list){
		    if(v->user_val.l){
			char **p;
			for(p = v->user_val.l; *p ; p++)
			  fs_give((void **)p);

			fs_give((void **)&(v->user_val.l));
		    }

		    v->user_val.l = lvalue;
		}
		else{
		    if(v->user_val.p != NULL)
		      fs_give((void **) &(v->user_val.p));

		    v->user_val.p = cpystr(value);

		}

		pline->is_var    = 1;
		pline->var  = v;
		pline->is_quoted = was_quoted;
		pline++;
            }
        } else {
            if(v->is_global) {
		if(v->is_list){
		    if(v->global_val.l){
			char **p;
			for(p = v->global_val.l; *p ; p++)
			  fs_give((void **)p);

			fs_give((void **)&(v->global_val.l));
		    }

		    v->global_val.l = lvalue;
		}
		else{
		    if(v->global_val.p != NULL)
		      fs_give((void **) &(v->global_val.p));
		    v->global_val.p = cpystr(value);
		}
            }
        }

        dprint(1,(debugfile, "pinerc : %12.12s : %20.20s : \"%s\"\n",
                filename, v->name, which_vars == ParseLocal ? 
		  (v->is_list) ? v->user_val.l[0] : v->user_val.p : 
		  (v->is_list) ? v->global_val.l[0] : v->global_val.p));
    }
    pline->line = NULL;
    pline->is_var = 0;
    fs_give((void **)&file);
    return(0);
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
      Set a user variable in memory (pinerc is never rewritten)

  Args:  var -- The index of the variable to set from pine.h (V_....)
         value -- The string to set the value to

 Result: 0 is always returned

 The vars data structure is updated; the change does not persist to disk.
 ----*/
set_variable(var, value)
     int   var;
     char *value;
{
    struct variable *v;

    v = &ps_global->vars[var];

    if(!v->is_user) 
      panic1("Trying to set non-user variable %s", v->name);

    if(v->user_val.p != NULL)
      fs_give((void **) &v->user_val.p);

    if(v->current_val.p != NULL)
      fs_give((void **) &v->current_val.p);

    v->user_val.p    = value == NULL ? NULL : cpystr(value);
    v->current_val.p = value == NULL ? NULL : cpystr(value);

    return(0);
}


/*----------------------------------------------------------------------
      Set a user variable list in memory (pinerc is never rewritten)

  Args:  var -- The index of the variable to set from pine.h (V_....)
         lvalue -- The list to set the value to

 Result: 0 is always returned

 The vars data structure is updated; the change does not persist to disk.
 ----*/
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
#ifdef	DOS
    char filename[MAXPATH+1];

    strcpy(filename, DEBUGFILE);
    unlink(filename);

    if(debug){
	int i;
#else
    char nbuf[5];
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
#endif	/* DOS */

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
