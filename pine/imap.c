#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: imap.c,v 4.20 1993/11/11 16:41:57 mikes Exp $";
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
    imap.c
    The call back routines for the c-client/imap
       - handles error messages and other notification
       - handles prelimirary notification of new mail and expunged mail
       - prompting for imap server login and password 

 ====*/

#include "headers.h"

/*----------------------------------------------------------------------
      Write imap debugging information into log file

   Args: strings -- the string for the debug file

 Result: message written to the debug log file
  ----*/
void mm_dlog(string)
     char *string;
{
    dprint(0, (debugfile, "IMAP DEBUG: %s\n", string));
}



/*----------------------------------------------------------------------
      Queue imap log message for display in the message line

   Args: string -- The message 
         errflg -- flag set to 1 if pertains to an error

 Result: Message queued for display

 The c-client/imap reports most of it's status and errors here
  ---*/
void mm_log(string, errflg)
     char *string;
     long  errflg;
{
    char        message[300];
    char       *occurance;
    int         was_capitalized;
    long        now;
    struct tm  *tm_now;

    now = time(0);
    tm_now = localtime(&now);

    dprint((errflg == ERROR ? 1 : 2), (debugfile,
                                     "IMAP %d:%d %d/%d mm_log %s: %s\n",
                                     tm_now->tm_hour, tm_now->tm_min,
                                     tm_now->tm_mon+1, tm_now->tm_mday,
                                     errflg == ERROR ? "ERROR" :
                                     errflg == WARN  ? "warn"  :
                                     errflg == PARSE ? "parse" : "babble",
                                     string));

    if(strncmp(string , "APPEND", 6) == 0){
	if(ps_global->try_to_create) /* hide error if creating new folder  */
	  return;

	strncpy(string += 2, "Save", 4); /* replace "APPEND" with "Save" */
    }

    /*---- replace all "mailbox" with "folder" ------*/
    strncpy(message, string, sizeof(message));
    message[sizeof(message) - 1] = '\0';
    occurance = srchstr(message, "mailbox");
    while(occurance != NULL) {
        was_capitalized = isupper(*occurance);
        rplstr(occurance, 7, (errflg == PARSE ? "address" : "folder"));
        if(was_capitalized)
          *occurance = (errflg == PARSE ? 'A' : 'F');
        occurance = srchstr(occurance, "mailbox");
    }

    if(errflg == PARSE || ps_global->noshow_error) 
      strcpy(ps_global->c_client_error, message);

    if(ps_global->noshow_error ||
		       (ps_global->noshow_warn && errflg == WARN) ||
                       (errflg != ERROR && errflg != WARN))
      return; /* Only care about errors; don't print when asked not to */

    /*---- Display the message ------*/
    q_status_message1(1, errflg == ERROR ? 3 : 2, 5, "\007%s", message);
    strcpy(ps_global->last_error, message);
    display_message('x');
}




/*----------------------------------------------------------------------
         recieve notification from IMAP

  Args: stream  --  Mail stream message is relavant to 
        string  --  The message text
        errflag --  Set if it is a serious error

 Result: message displayed in status line

 The facility is for general notices, such as connection to server;
 server shutting down etc... It is used infrequently.
  ----------------------------------------------------------------------*/
void mm_notify(stream, string, errflag)
     MAILSTREAM *stream;
     char       *string;
     long        errflag;
{
    dprint(1, (debugfile, "IMAP mm_notify %s : %s : %s\n",
               errflag == ERROR ? "error" : "warning" ,
               stream != NULL && stream->mailbox != NULL ? stream->mailbox :
               "-no folder-" ,
               string));

    if((strncmp(string, "[TRYCREATE]", 11) == 0)){
	ps_global->try_to_create = 1;
    }
    else{
	q_status_message2(1, errflag == ERROR ? 2 : 1, 6,
			  errflag == ERROR ?"\007%s : %s" : "%s : %s",
			  stream != NULL && stream->mailbox != NULL ?
			  stream->mailbox : "-no folder-", string);

	sprintf(ps_global->last_error,
		"%s : %s", stream != NULL&& stream->mailbox != NULL?
		stream->mailbox : "-no folder-",
		string);
    }
}



/*----------------------------------------------------------------------
       receive notification of new mail from imap daemon

   Args: stream -- The stream the message count report is for.
         number -- The number of messages now in folder.
 
  Result: Sets value in pine state indicating new mailbox size

     Called when the number of messages in the mailbox goes up.  This
 may also be called as a result of an expunge. It increments the
 new_mail_count based on a the difference between the current idea of
 the maximum number of messages and what mm_exists claims. The new mail
 notification and actual incorporation is done in newmail.c

 ----*/

void mm_exists(stream, number_l)
     MAILSTREAM *stream;
     long number_l;
{
    int number = (int)number_l;
    dprint(3, (debugfile, "=== mm_exists(%ld,%s) called ===\n", number,
	       stream == NULL ? "(no stream)" 
	       : stream->mailbox == NULL ? "(null)" 
	       : stream->mailbox));
    if(stream == ps_global->inbox_stream  &&
                            ps_global->inbox_stream != ps_global->mail_stream){
        ps_global->inbox_changed |= (ps_global->inbox_max_msgno != number);
        if(ps_global->inbox_max_msgno < number)
          ps_global->inbox_new_mail_count += number-ps_global->inbox_max_msgno;
        ps_global->inbox_max_msgno = number;
    } else if(stream == ps_global->mail_stream) {
        ps_global->mail_box_changed |= (ps_global->max_msgno != number);
        if(ps_global->max_msgno < number)
          ps_global->new_mail_count +=  number - ps_global->max_msgno;
        ps_global->max_msgno = number;
    } else {
        /*--- ignore mm_exist for other. These are quick opens --*/
    }
}


/*----------------------------------------------------------------------
    Receive notification from IMAP that a message has been expunged

   Args: stream -- The stream/folder the message is expunged from
         number -- The message number that was expunged

mm_expunged is always called on an expunge. mm_exists() which reports
the current number of messages is sometimes called, depending on the
kind of mail file (Bezerk, imap...). For this reason there are two
expunge counts. tot_expunge_count counts the total messages expunged
since the last new mail check. The new mail check will only see a
change in the number of messages and not know how the change was made
up as a result of adds and deletes unless it has the
total_expunge_count. The secound counter, expunge_count, counts
messages expunged between calls to mm_exists handling the case where
mm_expunged is called without mm_exists() being called. See new_mail()
for more details.

  ----*/
void mm_expunged(stream, number_l)
     MAILSTREAM *stream;
     long        number_l;
{
     dprint(3, (debugfile, "mm_expunged called %s %ld\n",
		stream == NULL ? "(no stream)" 
		: stream->mailbox == NULL ? "(null)" 
		: stream->mailbox, 
		number_l));
     if(stream == ps_global->inbox_stream &&
            ps_global->inbox_stream != ps_global->mail_stream) {
         ps_global->inbox_expunge_count++;
         ps_global->inbox_changed = 1;
         ps_global->inbox_max_msgno--;
    } else {
         ps_global->expunge_count++;
         ps_global->mail_box_changed = 1;
         ps_global->max_msgno--;
    }
}


/*
 * these aren't statics defined inside mm_login because pcpine
 * may need to examine them later
 */
char mm_login_user[80] = {'\0'}, mm_login_passwd[80] = {'\0'};

#ifdef	DOS
#define	PASSFILE	"pine.pwd"


#define	FIRSTCH	0x20
#define	LASTCH	0x7e
#define	TABSZ	(LASTCH - FIRSTCH + 1)

int encrypt_key;

unsigned int
xsum(s)
    char *s;
{
    unsigned int sum = 0;

    while(s && *s)
      sum ^= (unsigned char) *s++;

    return(sum % TABSZ);
}


/*
 * encrypt() - encrypt the given character
 */
char
encrypt(c)
    int	c;
{
    register int  eti;
    extern int encrypt_key;

    eti = encrypt_key;
    if((c >= FIRSTCH) && (c <= LASTCH)){
        eti += (c - FIRSTCH);
	eti -= (eti >= 2*TABSZ) ? 2*TABSZ : (eti >= TABSZ) ? TABSZ : 0;
        return((encrypt_key = eti) + FIRSTCH);
    }
    else
      return(c);
}


/*
 * decrypt() - decrypt the given character
 */
char
decrypt(c)
char	c;
{
    register int  dti;
    register int  xch;
    extern int encrypt_key;

    if((c >= FIRSTCH) && (c <= LASTCH)){
        xch  = c - (dti = encrypt_key);
	xch += (xch < FIRSTCH-TABSZ) ? 2*TABSZ : (xch < FIRSTCH) ? TABSZ : 0;
        dti  = (xch - FIRSTCH) + dti;
	dti -= (dti >= 2*TABSZ) ? 2*TABSZ : (dti >= TABSZ) ? TABSZ : 0;
        encrypt_key = dti;
        return(xch);
    }
    else
      return(c);
}


/*
 * passfile - return the password contained in the special passord
 *            cache.  The file is assumed to be in the same directory
 *            as the pinerc with the name defined above.
 */
char *
passfile(pinerc, passwd)
    char *pinerc, *passwd;
{
    static char path[MAXPATH];
    char *p = NULL;
    int   i;
    FILE *fp;

    if(!pinerc || pinerc[0] == '\0')
      return(NULL);

    if((p = strrchr(pinerc, '\\')) == NULL)
      p = strchr(pinerc, ':');

    if(p){
	strncpy(path, pinerc, i = (p - pinerc) + 1);
	path[i] = '\0';
    }

    strcat(path, PASSFILE);

    /* if there's a password to write, but no file to write to... */
    if(passwd && passwd[0] && can_access(path, ACCESS_EXISTS) != 0)
      return(NULL);			/* don't write anything */

    if((fp = fopen(path, passwd ? "wb" : "rb")) == NULL)
      return(NULL);

    if(passwd){
	if(want_to("Preserve password on DISK for next login", 'y', 'x',
		   NO_HELP, 0) != 'y')
	  return(NULL);

	/*** do any necessary ENcryption here ***/
	encrypt_key = xsum(mm_login_user);
	for(i = 0; passwd[i]; i++)
	  path[i] = encrypt(passwd[i]);

	path [i] = '\0';
	if(fwrite(path, sizeof(char) * strlen(path), 1, fp) != 1)
	  passwd = NULL;
    }
    else if((i = fread(path, sizeof(char), (size_t)MAXPATH, fp)) > 0){
	path[i] = 0;
	passwd  = path;

	/*** do any necessary DEcryption here ***/
	encrypt_key = xsum(mm_login_user);
	for(i = 0; path[i]; i++)
	  path[i] = decrypt(path[i]);
    }
    else
      passwd = NULL;

    fclose(fp);
    return(passwd);
}
#endif

/*----------------------------------------------------------------------
      Get login and password from user for IMAP login
  
  Args:  host   -- The host name the user is trying to log in to 
         user   -- Buffer to return the user name in 
         passwd -- Buffer to return the passwd in
         tial   -- The trial number or number of attempts to login

 Result: username and password passed back to imap
  ----*/
void mm_login(host, user, passwd, trial)
     char *host;
     char *user;
     char *passwd;
     long  trial;
{
    static char junk[] = {'?', '\0'};
    char      prompt[80], *p;
    HelpType  help ;
    int  rc, q_line;

    q_line =  -3; /* 3 from bottom */

    if(messages_queued()) {
        display_message(NO_OP_COMMAND);
        sleep(1);
    }

    if(ps_global->anonymous) {
        /*------ Anonymous login mode --------*/
        if(trial >= 1) {
            user[0]   = '\0';
            passwd[0] = '\0';
        } else {
            strcpy(user, "anonymous");
            sprintf(passwd, "%s@%s", get_system_login(), ps_global->hostname);
        }
        return;
    }

        
    if(mm_login_user[0] == '\0')
      strcpy(mm_login_user, ps_global->VAR_USER_ID);

    /* try our old passwd once */
    if(trial==0L && strlen(mm_login_user) > 0 && strlen(mm_login_passwd) > 0) {
	strcpy(user, mm_login_user);
	strcpy(passwd, mm_login_passwd);
	return;
    }

#ifdef	DOS
    /* check to see if there's a password left over from last session */
    if(trial==0L && *mm_login_user && (p = passfile(ps_global->pinerc, NULL))){
	strcpy(user, mm_login_user);
	strcpy(passwd, p);
	return;
    }
#endif

    ps_global->mangled_footer = 1;
    help = NO_HELP;
    sprintf(prompt, "HOST: %s  ENTER LOGIN NAME: ", host);
    while(1) {
        rc = optionally_enter(mm_login_user, q_line, 0, 
			      sizeof(mm_login_user) - 1, 1, 0,
                              prompt, NULL, help, 0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_login : NO_HELP;
            continue;
        }
        if(rc != 4)
          break;
    }

    if(rc == 1) {
        user[0]   = '\0';
        passwd[0] = '\0';
        return;
    }

    help = NO_HELP;
    sprintf(prompt, "HOST: %s  USER: %s  ENTER PASSWORD: ",host,mm_login_user);
    while(1) {
        rc = optionally_enter(mm_login_passwd, q_line, 0, 
			       sizeof(mm_login_passwd) - 1, 0,
                               1, prompt, NULL, help, 0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_passwd : NO_HELP;
            continue;
        }
        if(rc != 4)
          break;
    }
    if(rc == 1) {
        strcpy(user,junk);
        strcpy(passwd, junk);
        return;
    }

    strcpy(user, mm_login_user);
    strcpy(passwd, mm_login_passwd);
#ifdef	DOS
    passfile(ps_global->pinerc, passwd);
#endif
}
    



/*---------------------------------------------------------------------- 
        receive notification that search found something           

 Input:  mail stream and message number of located item

 Result: nothing, not used by pine
  ----*/
void mm_searched(stream, msg_no_l)
     MAILSTREAM *stream;
     long        msg_no_l;
{
    char tmp[20];
    int  msg_no = (int)msg_no_l;

    sprintf(tmp,"%ld",msg_no);
    q_status_message2(0, 2,4, "\007 %s searched %s",
		      stream ? stream->mailbox : "<no folder>", tmp);
}


/*----------------------------------------------------------------------
       Receive notification of an error writing to disk
      
  Args: stream  -- The stream the error occured on
        errcode -- The system error code (errno)
        serious -- Flag indicating error is serious (mail may be lost)

Result: If error is non serious, the stream is marked as having an error
        and deletes are disallowed until error clears
        If error is serious this goes modal, allowing the user to retry
        of get a shell escape to fix the condition. When the condition is
        serious it means that mail existing in the mailbox will be lost
        if Pine exists without writing so we try to induce the user to 
        fix the error, go get some one that can fix the error or whatever
        and don't provide an easy way out.
  ----*/

long
mm_diskerror (stream, errcode, serious)
     MAILSTREAM *stream;
     long        errcode;
     long        serious;
{
    int line, ch;
    char q[200];

    dprint(0, (debugfile,
       "\n***** DISK ERROR on stream %s. Error code %ld. Error is %sserious\n",
               stream ? stream->mailbox : "<no mail folder>", errcode,
               serious ? "" : "not "));
    dprint(0, (debugfile, "***** message: \"%s\"\n\n", ps_global->last_error));

    if(!serious) {
/*        q_status_message2(1, 3, 6,"\007Error saving mail folder \"%s\" : %s",
                    pretty_fn(stream ? stream->mailbox : ""),
                          error_description((int)errcode)); */
        if(stream == ps_global->mail_stream) {
            ps_global->io_error_on_stream = 1;
        }
        return (1) ;
    }

    line = ps_global->ttyo->screen_rows - 3;
    MoveCursor(line, 0);
    CleartoEOLN();
    StartInverse();
    Writechar('\007', 0);
    sprintf(q,  "Serious error saving mail folder \"%s\":",
                    pretty_fn(stream ? stream->mailbox : ""));
    PutLine0(line, 0, q);
    ch = strlen(q); /* Use ch temporarily */
    MoveCursor(line, ch);
    while(ch++ < ps_global->ttyo->screen_cols - 1)
      Writechar(' ', 0);
/*    sprintf(q, "%-*.*s", ps_global->ttyo->screen_cols - 1,
            ps_global->ttyo->screen_cols - 1,
            ps_global->last_error);
    d_q_status_message();
    PutLine0(line+1, 0, q); */
    if(F_ON(F_CAN_SUSPEND,ps_global)) {
        sprintf(q,  "Press return to retry saving or press ^Z to suspend Pine: ");
    } else {
        sprintf(q,  "Press return to retry saving or press ! to get to system prompt: ");
    }
    PutLine0(line+2, 0, q);
    Writechar('\007', 0);
    ch = strlen(q); /* Use ch temporarily */
    MoveCursor(line+2, ch);
    while(ch++ < ps_global->ttyo->screen_cols - 1)
      Writechar(' ', 0);
    MoveCursor(line+2, strlen(q));

    ch = read_char(0);
    while(!(ch == F_ON(F_CAN_SUSPEND,ps_global) ? ctrl('Z') : '!' || ch == '\r')) {
        Writechar('\007', 0);
        ch = read_char(0);
    }
    switch(ch) {
      case '\r':
        Write_to_screen("Retry");
        EndInverse();
        MoveCursor(line -2, 0);
        fflush(stdout);
        return (0L);
       
      case '!':
        EndInverse();
        end_keyboard(ps_global != NULL ? F_ON(F_USE_FK,ps_global) : 0);
        end_tty_driver(ps_global);
        printf("\n\nPlease attempt to correct the error preventing the saving of the mail folder.\n");
        printf("For example if the disk is out of space try removing uneeded files.\n");
        printf("You might also contact your system administrator.\n");
        printf("Give the command \"exit\" to get back to Pine and try saving the folder again.\n");

        system("csh");

        init_tty_driver(ps_global);
        init_keyboard(F_ON(F_USE_FK,ps_global));
        MoveCursor(line - 2, 0);
        return (0L); /* go retry it again */
       
      case ctrl('Z'):
        EndInverse();
        do_suspend(ps_global);
        break;
    }
    return(0);
}



/*----------------------------------------------------------------------
      Receive list of folders from c-client/imap.
 
 Puts an entry into the specified list of mail folders associated with
 the global context (aka collection). 
 ----*/
extern void       *find_folder_list;
extern MAILSTREAM *find_folder_stream;
   
void context_mailbox(name)
    char *name;
{
    FOLDER_S          *new_f;

    if(*name == '.')
      return;

    new_f                    = new_folder(name);
    new_f->prefix[0]         = '\0';
    new_f->msg_count         = 0;
    new_f->unread_count      = 0;
    folder_insert(-1, new_f, find_folder_list);

    dprint(4, (debugfile, "====== context_mailbox: (%s)\n", name));
}



/*----------------------------------------------------------------------
     Receive list of bulliten boards from c-client/imap

 Puts an entry into the specified list of bulletin boards associated with
 the global context (aka collection). 
 ----*/

void context_bboard(name)
char *name;
{
    FOLDER_S          *new_f = new_folder(name);


#ifdef	LATER
/*
 * Right now it's felt that it's too costly to 
 * display the new folder count by default for newsgoups...
 * (930507)
 */
    find_folder_stream = context_open(find_folder_context->context, 
				      find_folder_stream, name, 0);

    if(find_folder_stream != NULL) {
	new_f->msg_count    = find_folder_stream->nmsgs;
	new_f->unread_count = find_folder_stream->recent;
	sprintf(new_f->prefix, "%4.4s ",
		new_f->unread_count ? int2string(new_f->msg_count)  : "");
    } else {
	new_f->msg_count    = 0; /* zero message count different... */
	new_f->unread_count = 0; /* than inaccessible group!        */
	sprintf(new_f->prefix, "* ");
    }

    new_f->name_len += strlen(new_f->prefix);

    dprint(4, (debugfile, "====== context_bboard: (%s) %s, %d of %d unread\n",
	       find_folder_context->context, new_f->name, 
	       new_f->unread_count, new_f->msg_count));
#endif

    folder_insert(folder_total(find_folder_list), new_f, find_folder_list);

    dprint(4, (debugfile, "====== context_bboard: (%s)\n", name));
}


void
mm_fatal(message)
     char *message;
{
    panic(message);
}


void mm_flags (stream,number)
	MAILSTREAM *stream;
	long number;
{
    long i;

    /*
     * The idea here is to clean up any data pine might have cached
     * that has anything to do with the indicated message number.
     * At the momment, this amounts only to cached index lines, but
     * watch out for future changes...
     */
    if(stream != ps_global->mail_stream)
      return;			/* only worry about displayed msgs */

    if(ps_global->sort_order == SortArrival)
      i = number;
    else			/* more work to do */
      for(i=1; number != ps_global->sort[i] && i != ps_global->max_msgno; i++)
	;

    clear_index_cache_ent(i);
}
