#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: send.c,v 4.75 1993/12/07 01:42:03 mikes Exp $";
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
       send.c
       Functions for composing and sending mail

 ====*/

#include "headers.h"
#include "../pico/pico.h"

#ifndef TCPSTREAM
#define TCPSTREAM void
#endif

#ifdef ANSI
void     outgoing2strings(ENVELOPE *,BODY *,char **,char **,char **,void **,
			  PATMT **);
void      strings2outgoing(ENVELOPE *,BODY **,char *,char *,char *,PATMT *);
int       append_message2(char *,ENVELOPE *,BODY *,AppendTypes,char *,PATMT *);
void      parse_postponed_header(char *, char *, int, PATMT **);
void      create_message_body(BODY **, PATMT *);
void      free_attachment_list(PATMT *);
#ifndef	DOS
static    char *call_sendmail(ENVELOPE *, BODY *);
static    int   call_system(char *);
#endif

/*
 * Pines version of c-client routines, but changed to allow
 * piping attachments thru encoding filters on the way out the
 * SMTP mail slot or into file handed to sendmail... (mss, 4 Nov 1992)
 */
void      pine_write_body_header(char **, BODY  *);
void      pine_encode_body(ENVELOPE *, BODY *);
void      pine_rfc822_header(char *, ENVELOPE *, BODY *);
long      pine_rfc822_output(char *,ENVELOPE *,BODY *,soutr_t,TCPSTREAM *);
long      pine_rfc822_output_body(BODY *,soutr_t,TCPSTREAM *);
void      pine_free_body(BODY **);
void      pine_free_body_data(BODY *);
void      pine_free_body_part(PART **);
void      set_mime_types(BODY *);
int       call_mailer(char *, ENVELOPE *, BODY *);
FileTypes file_type(void *, long);
char     *mime_stats(BODY *);
void      mime_recur(BODY *);

#else

void      outgoing2strings();
void	  strings2outgoing();
void      create_message_body();
void      free_attachment_list();
int       append_message2();
void      parse_postponed_header();
#ifndef	DOS
static char   *call_sendmail();
static int     call_system();
#endif	/* DOS */
void      pine_write_body_header();
void      pine_encode_body();
void      pine_rfc822_header();
long      pine_rfc822_output();
long      pine_rfc822_output_body();
void      pine_free_body();
void      pine_free_body_data();
void      pine_free_body_part();
void      set_mime_types();
int       call_mailer();
FileTypes file_type();
char     *mime_stats();
void      mime_recur();
#endif


/*
 * Buffer to hold pointers into pine data that's needed by pico. 
 * Defined here so as not to take up room on the stack.  Better malloc'd?
 */
static	PICO	pbuf;


/* 
 * Storage object where the FCC is to be written.
 * This is amazingly bogus.  Much work was done to put messages 
 * together and encode them as they went to the tmp file for sendmail
 * or into the SMTP slot (especially for for DOS, to prevent a temporary
 * file (and needlessly copying the message).
 * 
 * HOWEVER, since there's no piping into c-client routines
 * (particularly mail_append() which copies the fcc), the fcc will have
 * to be copied to disk.  This global tells pine's copy of the rfc822
 * output functions where to also write the message bytes for the fcc.
 * With piping in the c-client we could just have two pipes to shove
 * down rather than messing with damn copies.  FIX THIS!
 */
static STORE_S *fcc_so = NULL;


#ifdef	DOS
/*
 * This variable is about the only place we can get the user's
 * IMAP server login name.  DOS requires it for the "From:" address
 * and pseudo-authenticaton warning...
 */
extern char mm_login_user[];
#endif


/*----------------------------------------------------------------------
    Compose screen (not forward or reply). Set up envelope, call composer
  
   Args: pine_state -- The usual pine structure
 
  Little front end for the compose screen
  ---*/
void
compose_screen(pine_state)
    struct pine *pine_state;
{
    ps_global = pine_state;
    pine_state->next_screen = pine_state->prev_screen;
    compose_mail(NULL);
}



/*----------------------------------------------------------------------
     Format envelope for outgoing message and call editor

 Args:  given_to -- An address to send mail to (usually from command line 
                       invocation)
 
 If a "To" line is given format that into the envelope and get ready to call
           the composer
 If there's a message postponed, offer to continue it, and set it up

 Otherwise just fill in the outgoing envelope as blank
 ----*/
void 
compose_mail(given_to)
    char *given_to;
{
    ENVELOPE  *outgoing;
    BODY      *body;
    long       start_in_body = 0L;
    char      *p, *fcc2, *file, *enc_error,
               fcc[MAXFOLDER+1], file_path[MAXPATH+1], *sig = NULL;
    PATMT     *attachments;
    STRING     string_struct;
    FILE      *fp;
    STORE_S   *so = NULL;
    gf_io_t    gc, pc;
    fpos_t     body_start;

    dprint(1, (debugfile,
                 "\n\n    ---- COMPOSE SCREEN (not in pico yet) ----\n"));

    fcc2 = NULL;
    file_path[0] = '\0';
    attachments = NULL;

    {
	int ret=0;
	build_path(file_path, ps_global->folders_dir, INTERRUPTED_MAIL);
	if(given_to != NULL || can_access(file_path, ACCESS_EXISTS) != 0 ||
	   (ret=want_to("Continue interrupted composition (answering \"n\" won't erase it)", 'y', 'x', NO_HELP, 0)) == 'n')
	  file_path[0] = '\0';

	if(file_path[0] == '\0') {
	    build_path(file_path, ps_global->folders_dir, POSTPONED_MAIL);
	    if(given_to != NULL || can_access(file_path, ACCESS_EXISTS) != 0 ||
	       (ret=want_to("Continue postponed composition (answering \"No\" won't erase it)", 'y', 'x', NO_HELP, 0)) == 'n')
	      file_path[0] = '\0';
	}
	if(ret == 'x') {
            q_status_message(1, 3, 4, "\007Message cancelled");
	    return;
	}

    }

    if(file_path[0] != '\0') {
        /*============== Continue postponed compostion ==============*/
#ifdef	DOS
	if((fp=fopen(file_path, "rb")) == NULL){
#else
	if((fp=fopen(file_path, "r")) == NULL){
#endif
            q_status_message2(1, 3,4, 
                "\007Error \"%s\" reading postponed composition in \"%s\"",
                              error_description(errno), file_path);
            return;
	}

	p = tmp_20k_buf;
	while(fgets(p, 1024, fp) != NULL	/* slurp up the header */
	      && strncmp(p, NEWLINE, strlen(NEWLINE))){
	    while(*p != '\0')
	      p++;
	}

	fget_pos(fp, &body_start);		/* remember where we're at */

	strcat(tmp_20k_buf, NEWLINE);
	file = cpystr(tmp_20k_buf);		/* copy header */
	p    = tmp_20k_buf;
	/* have c-client parse the rest */
	INIT(&string_struct, mail_string, (void *)"", 0L);
	rfc822_parse_msg(&outgoing, &body, file, (unsigned long)strlen(file),
			 &string_struct, ps_global->maildomain, tmp_20k_buf);
	/* grab what we want from the header */
        parse_postponed_header(file, fcc, sizeof(fcc), &attachments);
        fcc2 = fcc;

	/*
	 * Now deal with the body.  The parse discovered the body's
	 * encoding and left the file pointer at the beginning of the body.
	 * Create a space for the text, and read in with any necessary
	 * decoding...
	 */
	if((so = (void *)so_get(PicoText, NULL, EDIT_ACCESS)) == NULL){
	    q_status_message(1, 2, 4, 
			     "\007Problem creating space for message text.");
	    fclose(fp);
	    return;
	}

	gf_filter_init();
	gf_set_so_writec(&pc, so);		/* write into decoded text */
	gf_set_readc(&gc, fp, 0L, FileStar);
	fset_pos(fp, &body_start);		/* reset to start of body */

	/*
	 * decoding filters expect endoflines to be CRLF so we need to
	 * convert what's on disk in the local convention to CRLF.
	 * We don't need to worry about what comes out of the pipe since
	 * we're writing into a PicoText storage object which treats
	 * CRLF and CR the same.  Bug or feature, you decide...
	 */
	gf_link_filter(gf_local_nvtnl);
	if(body->encoding == ENCBASE64)
	  gf_link_filter(gf_b64_binary);
	else if(body->encoding == ENCQUOTEDPRINTABLE)
	  gf_link_filter(gf_qp_8bit);

	if(enc_error = gf_pipe(gc, pc)){
	    q_status_message1(1, 3, 4, "\007Decoding Error \"%s\"", enc_error);
	    display_message('x');
	    so_give(&so);
	    mail_free_envelope(&outgoing);
	    pine_free_body(&body);
	    return;
	}

	fs_give((void **)&file);		/* return temp header */
	fclose(fp);				/* close postponed file */

        body->type            = TYPETEXT;
	body->contents.binary = (void *)so;
	start_in_body         = -1L;
        if(unlink(file_path) < 0){		/* and blast it */
            q_status_message2(1, 2, 4, 
                "Error \"%s\" unlinking postponed composition in \"%s\"",
                              error_description(errno), file_path);
        }
    } else {
        /*=================  Compose new message ===============*/
        body         = mail_newbody();
        outgoing     = mail_newenvelope();

        if(given_to != NULL) {
            rfc822_parse_adrlist(&(outgoing->to), given_to,
                                               ps_global->maildomain);
        }
        outgoing->message_id = cpystr(generate_message_id(ps_global));

	/*
	 * The type of storage object allocated below is vitally
	 * important.  See SIMPLIFYING ASSUMPTION #37
	 */
	if((body->contents.binary = (void *)so_get(PicoText, NULL,
						   EDIT_ACCESS)) == NULL){
	    q_status_message(1, 2, 4, "\007Problem creating space for message text.");
	    return;
	}

	if((sig = get_signature()) && *sig){
	    so_puts(body->contents.binary, sig);
	    fs_give((void **)&sig);
	}

	body->type = TYPETEXT;
	
    }

    ps_global->prev_screen = compose_screen;

    pine_send(outgoing, &body, "COMPOSE MESSAGE", fcc2,
	      start_in_body, (void *)attachments);

    mail_free_envelope(&outgoing);
    pine_free_body(&body);
}



/*----------------------------------------------------------------------
    Parse the postponed message for the Pine specific fields

Args:  text -- Text to parse for fcc and attachments refs
       fcc  -- Pointer to buffer to return fcc in 
       atmt -- Attachments structure to return result in

The current strategy on postponing a message with attachments or one that
is multipart is this:  If the parts are attachments of local files then just 
store a reference to the file in the postponed message file. If the 
parts are not local files and came about by MIME style forwarding, replying
or inclusion or some other way, then we would like to store the 
whole part, contents and all. Currently this not done because the code
to parse a whole MIME format message is not written. The code to write
it out does work. Now when a message with attachments is postponed any
MIME style forwarding parts or such will be dropped and only references
to files attached will be included.
 ----*/
void
parse_postponed_header(text, fcc, fcc_size, atmt)
    char   *text, *fcc;
    PATMT **atmt;
    int     fcc_size;
{
    register char *p, *q, *a;
    PATMT         *pa;
    char           tmp;

    *fcc = '\0';
    for(pa = *atmt; pa != NULL && pa->next != NULL; pa = pa->next);

    /*---- Loop until the end of the header ----*/
    for(p = text; *p && !(*p == '\n' && *(p+1) != '\n'); p++) {
        if(struncmp(p, "Attachment-file:", 16) == 0) {
            p += 16;
            while(*p && *p != '\n' && isspace(*p))
              p++;
            if(*p == '\n')
              continue; /* Blank field, skip this */

            /*---- Got a field, now allocate a structure for it -----*/
            if(pa == NULL) {
                /* empty list */
                *atmt = (PATMT *)fs_get(sizeof(PATMT));
                pa    = *atmt;
            } else {
                pa->next = (PATMT *)fs_get(sizeof(PATMT));
                pa       = pa->next;
            }
            pa->id = NULL;
            pa->description = cpystr("");
            pa->size = cpystr(""); /* BUG these should be NULL, 
                                      but pico can't handle NULL's */
            pa->next = NULL;
            a = p;
            while(*p && *p != '\n' && !isspace(*p))
              p++;
            tmp = *p;
            *p = '\0';
            pa->filename = cpystr(a);
            *p = tmp;
#ifdef X_NEVER
        } else if(struncmp(p, "Attachment-id:", 14) == 0) {
            if(pa != NULL){
                p += 14;
                while(*p && *p != '\n' && isspace(*p))
                  p++;
                if(*p == '\n')
                  continue; /* Blank field, skip this */
                a = p;
                while(*p && *p != '\n' && !isspace(*p))
                  p++;
                tmp = *p;
                *p = '\0';
                pa->id = cpystr(a);
                *p = tmp;
            }
#endif
        } else if(struncmp(p, "Attachment-description:", 23) == 0) {
            if(pa != NULL){
                p += 23;
                while(*p && *p != '\n' && isspace(*p))
                  p++;
                if(*p == '\n')
                  continue; /* Blank field, skip this */
                a = p;
                while(*p && *p != '\n')
                  p++;
                tmp = *p;
                *p = '\0';
                pa->description = cpystr(a);
                *p = tmp;
            }
        } else if(struncmp(p, "Fcc:", 4) == 0) {
            /*-- Skip past white space and field tag --*/
            p += 4; 
            while(*p && *p != '\n' && isspace(*p))
              p++;
            /*-- copy the result in --*/
            for(q=fcc; q < &fcc[fcc_size-1] && *p && *p!='\n' && !isspace(*p);)
              *q++ = *p++;
            *q = '\0';
        }

        /* Skip to end of line, what ever it was */
        while(*p != '\n')
          p++;  
    }
}
 

#ifdef	DOS
/*----------------------------------------------------------------------
    Verify that the necessary pieces are around to allow for
    message sending under DOS

Args:  none

The idea is to make sure pine knows enough to put together a valid 
from line.  The things we MUST know are a user-id, user-domain and
smtp server to dump the message off on.  Typically these are 
provided in pine's configuration file, but if not, the user is
queried here.
 ----*/
int
dos_valid_from()
{
    char        prompt[80], answer[60];
    int         rc, i;
    HelpType    help;

    /*
     * query for user name portion of address, use IMAP login
     * name as default
     */
    if(!ps_global->VAR_USER_ID || ps_global->VAR_USER_ID[0] == '\0'){
	if(mm_login_user[0])
	  strcpy(answer, mm_login_user);
	else
	  answer[0] = '\0';

	sprintf(prompt,"User-id for From address : "); 

	help = NO_HELP;
	while(1) {
	    rc = optionally_enter(answer,-3,0,79,1,0,prompt,NULL,help,0);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_user_id : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer)) {
	    q_status_message(0, 0,2 ,"\007Send message cancelled");
	    return(0);
	}

	/* save the name */
	sprintf(prompt, "Preserve %s as \"user-id\" in PINERC", answer);
	if(ps_global->blank_user_id && want_to(prompt,'y','n',NO_HELP,0)=='y'){
	    set_variable(V_USER_ID, answer);
	}
	else{
            fs_give((void **)&(ps_global->VAR_USER_ID));
	    ps_global->VAR_USER_ID = cpystr(answer);
	}
    }

    /* query for personal name */
    if(!ps_global->VAR_PERSONAL_NAME || ps_global->VAR_PERSONAL_NAME[0]=='\0'){
	answer[0] = '\0';
	sprintf(prompt,"Personal name for From address : "); 

	help = NO_HELP;
	while(1) {
	    rc = optionally_enter(answer,-3,0,79,1,0,prompt,NULL,help,0);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_personal_name : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
    	}

	if(rc == 0 && answer){		/* save the name */
	    sprintf(prompt, "Preserve %s as \"personal-name\" in PINERC",
		    answer);
	    if(ps_global->blank_personal_name
	       && want_to(prompt, 'y', 'n', NO_HELP, 0) == 'y'){
		set_variable(V_PERSONAL_NAME, answer);
	    }
	    else{
        	fs_give((void **)&(ps_global->VAR_PERSONAL_NAME));
		ps_global->VAR_PERSONAL_NAME = cpystr(answer);
	    }
	}
    }

    /* 
     * query for host/domain portion of address, using IMAP
     * host as default
     */
    if(ps_global->blank_user_domain 
       || ps_global->maildomain == ps_global->localdomain
       || ps_global->maildomain == ps_global->hostname){
	if(ps_global->inbox_name[0] == '{'){
	    for(i=0;ps_global->inbox_name[i+1] != '}'; i++)
		answer[i] = ps_global->inbox_name[i+1];

	    answer[i] = '\0';
	}
	else
	  answer[0] = '\0';

	sprintf(prompt,"Host/domain for From address : "); 

	help = NO_HELP;
	while(1) {
	    rc = optionally_enter(answer,-3,0,79,1,0,prompt,NULL,help,0);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_domain : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer)) {
	    q_status_message(0, 0,2 ,"\007Send message cancelled");
	    return(0);
	}

	/* save the name */
	sprintf(prompt, "Preserve %s as \"user-domain\" in PINERC",
		answer);
	if(!ps_global->userdomain && !ps_global->blank_user_domain
	   && want_to(prompt, 'y', 'n', NO_HELP, 0) == 'y'){
	    set_variable(V_USER_DOMAIN, answer);
	    fs_give((void **)&(ps_global->maildomain));	/* blast old val */
	    ps_global->userdomain      = cpystr(answer);
	    ps_global->maildomain      = ps_global->userdomain;
	    ps_global->do_local_lookup = 0;
	    lhostn                     = ps_global->maildomain;
	}
	else{
            fs_give((void **)&(ps_global->maildomain));
            ps_global->userdomain = cpystr(answer);
	    ps_global->maildomain = ps_global->userdomain;
	}
    }

    return(1);
}
#endif



/*----------------------------------------------------------------------
     Prepare data structures for pico, call pico, then post message

  Args:  outgoing -- Partially formatted outgoing ENVELOPE
         textbuf  -- Address of text of outgoing message
         editor_title -- Title for anchor line in composer
         fcc      -- The file carbon copy field
	 are_replying -- 0 if originating mail, number of message
	                 being replied to otherwise, or -1 if calling
			 the composer with cursor in the body is wanted.
         att      -- Possible list of attachment references
                     (declared void * so we don't have to include
                      pico.h everywhere else in Pine)

  Result: message is edited, then postponed, cancelled or sent.

  Fields:
     remail                -
     return_path           -
     date                 added here
     from                 added here
     sender                -
     reply_to              -
     subject              passed in, edited and cannonized here
     to                   possibly passed in, edited and cannonized here
     cc                   possibly passed in, edited and cannonized here
     bcc                  edited and cannonized here
     in_reply_to          generated in reply() and passed in
     message_id            -
  
 Storage for these fields comes from anywhere outside. It is remalloced
 here so the composer can realloc them if needed. The copies here are also 
 freed here.


Can only deal with message bodies that are either TYPETEXT or TYPEMULTIPART
with the first part TYPETEXT! All newlines in the text here also end with
CRLF.

There's a further assumption that the text in the TYPETEXT part is 
stored in a storage object (see filter.c).
  ----*/

void
pine_send(outgoing, body, editor_title, fcc, are_replying, att)
    ENVELOPE  *outgoing;  /* envelope for outgoing message */
    BODY     **body;   
    char      *editor_title, *fcc;
    long	are_replying;
    void      *att;
{
    int   editor_result, result, orig_cols, second_time = 0;
    long  real_current_msgno, i;
    extern char version_buff[];
#ifdef	DOS
    char *reserve;
#endif

    dprint(1, (debugfile,"\n === send called === \n"));
    /*
     * Remember the real current message number, as new mail arrival
     * may invalidate the current sorted message number.  Restore it
     * before we return...
     */
    real_current_msgno = ps_global->sort[ps_global->current_sorted_msgno];

    /*
     * Cancel any pending initial commands since pico uses a different
     * input routine.  If we didn't cancel them, they would happen after
     * we returned from the editor.
     */
    if (ps_global->in_init_seq) {
	ps_global->in_init_seq = 0;
	ps_global->save_in_init_seq = 0;
        if (ps_global->initial_cmds)
	    *ps_global->initial_cmds = 0;
	F_SET(F_USE_FK,ps_global,ps_global->orig_use_fkeys);
    }

#ifdef	DOS
    if(!dos_valid_from()){
        q_status_message(1, 2, 4,
             "Must provide Username for sending!");
        return;
    }
#endif

    pbuf.attachments = (PATMT *)att;

    /*----- Fill in a few general parts of the envelope ----*/
    pbuf.fccbuf = cpystr(fcc != NULL ? fcc : ps_global->VAR_DEFAULT_FCC);
    pbuf.fcclen = strlen(pbuf.fccbuf);

    if(!outgoing->subject)
	outgoing->subject = cpystr("");

    if(!outgoing->from) {
        outgoing->from             = mail_newaddr();
        outgoing->from->personal   = cpystr(ps_global->VAR_PERSONAL_NAME);
        outgoing->from->adl        = NULL;
        outgoing->from->mailbox    = cpystr(ps_global->VAR_USER_ID);
        outgoing->from->host       = cpystr(ps_global->maildomain);
        outgoing->from->error      = NULL;
        outgoing->from->next       = NULL;
    }

    outgoing->return_path = rfc822_cpy_adr(outgoing->from);

    /*----------------------------------------------------------------------
       Loop calling the editor until everything goes well
     ----*/
    do {
        flush_status_messages();

        /*-----------------------------------------------------
          Convert the envelope and body to the string format that
            pico can edit
         */
        outgoing2strings(outgoing, *body, &pbuf.tobuf, &pbuf.ccbuf,
                         &pbuf.bccbuf, &pbuf.msgtext, &pbuf.attachments);

	pbuf.pine_anchor = set_titlebar(editor_title, 0, FolderName, 0, 0, 0),
	pbuf.pine_version = version_buff;
	pbuf.tolen = strlen(pbuf.tobuf);
	pbuf.cclen = strlen(pbuf.ccbuf);
	pbuf.bcclen = strlen(pbuf.bccbuf);
	pbuf.subbuf = outgoing->subject;
	pbuf.sublen = strlen(pbuf.subbuf);

	pbuf.raw_io = Raw;
	pbuf.showmsg = display_message;
	pbuf.clearcur = clear_cursor_pos;
	pbuf.newmail = new_mail;
	pbuf.folders = folders_for_fcc;
	pbuf.buildaddr = build_address;
	pbuf.keybinit = init_keyboard;
	pbuf.helper = helper;
	pbuf.ins_help = h_composer_ins;
	pbuf.search_help = h_composer_search;
	pbuf.to_help = h_composer_to;
	pbuf.cc_help = h_composer_cc;
	pbuf.bcc_help = h_composer_bcc;
	pbuf.fcc_help = h_composer_fcc;
	pbuf.subject_help = h_composer_subject;
	pbuf.attachment_help = h_composer_attachment;
	pbuf.browse_help = h_composer_browse;
	pbuf.composer_help = h_composer;


      	pbuf.pine_flags  = 0;
	pbuf.pine_flags |= (F_ON(F_CAN_SUSPEND,ps_global)) ? P_SUSPEND : 0;
	pbuf.pine_flags |= (F_ON(F_USE_FK,ps_global)) ? P_FKEYS : 0;
	pbuf.pine_flags |= (ps_global->restricted ? P_SECURE :
                                                                   0);
	pbuf.pine_flags |= (are_replying || second_time) ? P_BODY : 0;

        dprint(9, (debugfile, "flags: %x\n", pbuf.pine_flags));

        flush_status_messages();
        clear_cursor_pos();

#ifdef	DOS
/*
 * dumb hack to help ensure we've got something left
 * to send with if the user runs out of precious memory
 * in the composer...	(FIX THIS!!!)
 */
	if((reserve = (char *)malloc(16384)) == NULL){
	    q_status_message(1, 2, 4,
			"\007LOW MEMORY!  May be unable to complete send!");
	    flush_status_messages();
	}
#endif

	editor_result = pico(&pbuf);

#ifdef	DOS
	free((char *)reserve);
#endif
        /*-- Text returned here has network EOL's, CRLF's that is --*/

        if(outgoing->subject != pbuf.subbuf) 	/* subject string realloc'd */
	  outgoing->subject = pbuf.subbuf; 	/* forget old subj. pointer */

        init_signals();        /* Pico has it's own signal stuff */
        mark_status_dirty();
	orig_cols = ps_global->ttyo->screen_cols;
        get_windsize(ps_global->ttyo); /* Might have changed, we won't know */
	if(orig_cols != ps_global->ttyo->screen_cols)
	  clear_index_cache();

        clear_cursor_pos();

        /*-------------------------------------------------------------
          Turn strings back into structures
          ----*/
        strings2outgoing(outgoing, body, pbuf.tobuf, pbuf.ccbuf,
                         pbuf.bccbuf, pbuf.attachments);

        fs_give((void **)&pbuf.tobuf);
        fs_give((void **)&pbuf.ccbuf);
        fs_give((void **)&pbuf.bccbuf);

        removing_trailing_white_space(pbuf.fccbuf);

	/*-------- Stamp it with a current date -------*/
	if(outgoing->date)			/* update old date */
	  fs_give((void **)&(outgoing->date));

	rfc822_date(tmp_20k_buf);		/* format and copy new date */
	outgoing->date = cpystr(tmp_20k_buf);

        /*----- Message is edited, now decide what to do with it ----*/
        if(editor_result & COMP_CANCEL) {
            /*================ Cancel the message ===================*/
            q_status_message(0, 1, 3, "Message Cancelled");
            break;

        } else if(editor_result & COMP_SUSPEND) {
            /*================= Postpone message ====================*/
            PART *p_tmp;
            BODY *b_tmp;

            if((*body)->type != TYPETEXT) {
                /* Leave this question off until we can actually send something
                   with MIME parts that aren't just attachments. Right now the
                   attachments will be postponed by reference */
                if(want_to("Attachments won't be included in message. Postpone anyway",'n', 'n', NO_HELP, 0) == 'n')
                  continue;

                /* Hide all but first text part */
                p_tmp = (*body)->contents.part->next;
                (*body)->contents.part->next = NULL;
                b_tmp = &((*body)->contents.part->body);
            } else {
                b_tmp = *body;
            }

            result = append_message2(POSTPONED_MAIL, outgoing, b_tmp,
                                     AppendPostpone, pbuf.fccbuf,
				     (void *)pbuf.attachments);

            if((*body)->type != TYPETEXT) {
                (*body)->contents.part->next = p_tmp;
            }
            if(result< 0) {
                q_status_message(1, 1, 3, "\007Continuing composition; message not postponed or sent");
                display_message('x');
                sleep(3);
		second_time++;
                continue; /* postpone failed, jump back in to composer */
            } else {
                q_status_message(1, 1,3,
                          "Composition postponed. Select Compose to resume.");
                break; /* postpone went OK, get out of here */
            }
        } else if(editor_result & COMP_GOTHUP) {
            /*----- Disconnected, save the interrupted composition ----*/
            result = append_message2(INTERRUPTED_MAIL,outgoing,*body,
                                     AppendInterrupted,
                                     pbuf.fccbuf, pbuf.attachments);
            dprint(1, (debugfile, "Save composition on HUP %s\n",
                       result < 0 ? "FAILED" : "succeeded"));
            hup_signal(); /* Do what we normally do on SIGHUP */

        } else {
            /*------ Must be sending mail ! -----*/

	    if(!outgoing->to && !outgoing->cc && !outgoing->bcc &&
				    (!pbuf.fccbuf || pbuf.fccbuf[0] == '\0')) {
               q_status_message(1, 2, 4, "No recipients specified!");
               display_message('x');
               sleep(3);
               continue;
	    }
	       
            if(check_addresses(outgoing)) {
                /*--- Addresses didn't check out---*/
               display_message('x');
               sleep(3);
               continue;
            }

            if(call_mailer(pbuf.fccbuf, outgoing, *body)){
		/*
		 * If message successfully sent, we're replying
		 * AND we're allowed to write back state, do it.
		 * But also protect against shifted message message
		 * numbers due to new mail arrival.  Since the number passed
		 * is based on the real imap msg no, AND we're sure no
		 * expunge has been done, just fix up the sorted number...
		 */
		if(are_replying > 0 && !READONLY_FOLDER){
		    char seq[8];

		    sprintf(seq, "%ld", are_replying);
		    mail_setflag(ps_global->mail_stream, seq, "\\ANSWERED");
		    check_point_change();
		    clear_index_cache_ent(sorted_msgno(ps_global,
						       are_replying));
		}

		break;
	    }

            /*--- Send failed, loop on back ----*/
        }

    } while(1);

    /*
     * Make sure our sorted number is the same as it was when we 
     * entered.  We know the old number is in the list somewhere
     * since only new mail (no expunges) could have happened while
     * we were away...
     */
    ps_global->current_sorted_msgno = sorted_msgno(ps_global,
						   real_current_msgno);

    fs_give((void **)&pbuf.fccbuf);
    free_attachment_list(pbuf.attachments);
    display_message('\0');
}



/*----------------------------------------------------------------------
   Check for addresses the user is not permitted to send to, or probably
   doesn't want to send to
   
Returns: 0 if OK, and 1 if the message shouldn't be sent

Alls queues a message indicating what happened
  ---*/

check_addresses(e)
    ENVELOPE *e;
{
    ADDRESS *a;

    /*---- Restricted mode can only send mail back -----*/
    if(ps_global->restricted) {
        if(!address_is_us(e->to, ps_global) ||
          (e->cc != NULL &&
           !address_is_us(e->cc,ps_global)) ||
          (e->bcc != NULL &&
           !address_is_us(e->bcc,ps_global))){
            q_status_message(1, 2, 4,
                             "Restricted demo version of Pine. You may only send mail to yourself");
            return(1);
        }
    }

    /*---- Is he/she trying to send mail to the mailer-daemon ----*/
    for(a = e->to; a != NULL; a = a->next) 
      if(strucmp(a->mailbox, "mailer-daemon") == 0)
        goto really_send;
    for(a = e->cc; a != NULL; a = a->next) 
      if(strucmp(a->mailbox, "mailer-daemon") == 0)
        goto really_send;
    for(a = e->bcc; a != NULL; a = a->next) 
      if(strucmp(a->mailbox, "mailer-daemon") == 0)
          goto really_send;

    return(0); /* All is OK */

  really_send:
    if(want_to("Really send this message to the MAILER-DAEMON", 'n', 'n',
						       NO_HELP, 0) == 'n')
      return(1);
    else
      return(0);
}



/*----------------------------------------------------------------------
     Call the mailer, SMTP, sendmail or whatever
     
Args: fcc      -- The fcc to write a copy of the message to
      envelope -- The full envelope structure
      body     -- The full body of the message including text
      header   -- a text version of the header LF format
      text     -- Text of the message if text only, LF format

Returns: 0 if failed, 1 if succeeded
----*/      
int
call_mailer(fcc, envelope, body)
    char     *fcc;
    ENVELOPE *envelope;
    BODY     *body;
{
    char         error_buf[100], *error_mess = NULL;
    int          rv = 1, rrv = 1, real_send = 0;
    CONTEXT_S   *fcc_cntxt = NULL;

    q_status_message(0, 2, 4, "Sending mail.....");
    display_message('x'); 

    /*---- recompute message-id to encode body info stats ----*/
    if(!envelope->message_id)		/* just in case! */
      envelope->message_id = cpystr(generate_message_id(ps_global));

    update_message_id(envelope, mime_stats(body));

    /*----- write message to sent-mail file ----*/
    /*
     * At this point, just check to make sure the fcc's available.
     * Actually take care of it below.
     *
     * WARNING!  There are hooks in append_message2 and
     * pine_rfc822_output_body that cause the FCC storage object to
     * get written into.  MAKE SURE THOSE ONLY GET CALLED ONCE WHILE
     * IN THIS ROUTINE!
     */
    fcc_so = NULL;
    if(fcc && *fcc){
#ifdef	DOS
#define	FCC_SOURCE	FileStar
#else
#define	FCC_SOURCE	CharStar
#endif

	if((fcc_cntxt = default_save_context(ps_global->context_list)) == NULL)
	  fcc_cntxt = ps_global->context_list;

	/* 
	 * check for fcc's existance...
	 */
	if(context_isambig(fcc)){
	    int  ok = 1;
	    find_folders_in_context(fcc_cntxt, fcc);
	    if(folder_index(fcc, fcc_cntxt->folders) < 0){
		if(ps_global->context_list->next){
		    sprintf(tmp_20k_buf,
		      "Folder \"%.20s\" in <%.30s> doesn't exist. Create",
			    fcc, fcc_cntxt->label[0]);
		}
		else
		  sprintf(tmp_20k_buf,
			  "Folder \"%.40s\" doesn't exist.  Create", fcc);

		if(want_to(tmp_20k_buf, 'y', 'n', NO_HELP, 0) != 'y'){
		    q_status_message(0, 1, 3, "\007Fcc of message rejected");
		    ok = 0;
		}
		else if(!context_create(fcc_cntxt->context,
					fcc_cntxt->proto, fcc))
		  ok = 0;
	    }

	    free_folders_in_context(fcc_cntxt);
	    if(!ok)
	      return(0);
	}
	else if(!folder_exists(NULL, fcc)){
	    MAILSTREAM *create_stream;
	    sprintf(tmp_20k_buf,"Folder \"%.40s\" doesn't exist.  Create",fcc);

	    if(want_to(tmp_20k_buf, 'y', 'n', NO_HELP, 0) != 'y'){
		q_status_message(0, 1, 3, "\007Fcc of message rejected");
		return(0);
	    }

	    if((create_stream = default_driver(fcc)) == NULL){
		/*
		 * See if an already open stream will service the create
		 */
		create_stream = same_stream(fcc, ps_global->mail_stream);

		if(!create_stream 
		   && ps_global->mail_stream != ps_global->inbox_stream)
		  create_stream = same_stream(fcc, ps_global->inbox_stream);
	    }

	    if(!mail_create(create_stream, fcc))
	      return(0);
	}

	if((fcc_so = so_get(FCC_SOURCE, NULL, WRITE_ACCESS)) == NULL){
	    q_status_message(1, 2, 4, 
			     "\007Problem creating space for Fcc.");
	    return(0);
	}
    }

    /*----- dump message off to the mailer ----*/
    if(envelope->to || envelope->cc || envelope->bcc){
	real_send++;

	/*----- Send mail ------*/
	error_mess = call_sendmail(envelope, body);
    }
    else if(fcc_so){
	/*
	 * If there aren't any real addressees, but there is an fcc, 
	 * go thru the motions of building the message...
	 */
	if(pine_rfc822_output(tmp_20k_buf, envelope, body, NULL, NULL) < 1){
	    q_status_message(0,2,3, "Fcc Failed!.  No message saved.");
	    return(0);
	}
    }
    else {				/* shouldn't happen */
	q_status_message(0,2,3,"Can't send message. No recipients specified!");
	return(0);
    }

    /*-------- Did message make it ? ----------*/
    if(error_mess) {
        /*---- Error sending mail -----*/
        q_status_message(1, 4, 7, error_mess);
        rv = 0;				/* fall thru to clean up fcc */
    }
    else if(fcc_so){			/* if sent or only fcc, save it */
	STRING      msg;
	MAILSTREAM *fcc_stream = NULL;
#ifdef	DOS
	struct {			/* hack! stolen from dawz.c */
	    int fd;
	    unsigned long pos;
	} d;
	extern STRINGDRIVER dawz_string;
#endif

	so_seek(fcc_so, 0L, 0);
#ifdef	DOS
	d.fd  = fileno((FILE *)so_text(fcc_so));
	d.pos = 0L;
	INIT(&msg, dawz_string, (void *)&d, filelength(d.fd));
#else
	INIT(&msg, mail_string, (void *)so_text(fcc_so), 
	     strlen((char *)so_text(fcc_so)));
#endif

	fcc_stream = context_same_stream(fcc_cntxt->context, fcc,
					 ps_global->mail_stream);

	if(!fcc_stream && ps_global->mail_stream != ps_global->inbox_stream)
	  fcc_stream = context_same_stream(fcc_cntxt->context, fcc,
					   ps_global->inbox_stream);

	if(!(rrv = context_append(fcc_cntxt->context, fcc_stream, fcc, &msg))){
	    q_status_message1(0,2,2,"\007Fcc to \"%s\" FAILED!!!", fcc);
            sprintf(error_buf,"\007Error appending %.20s in %.40s", 
		    fcc, fcc_cntxt->context);
            dprint(1, (debugfile, error_buf));
	}

    }

    error_buf[0] = '\0';

    /*--- Blat out the result ---*/
    sprintf(error_buf, "Message %s%s%s%s%s%s%s.",
	    (real_send && !rv) ? "NOT " : "",
	    (real_send) ? "sent" : "",
	    (real_send && fcc_so) ? " and " : "",
	    (fcc_so && !rrv) ? "NOT " : "",
	    (fcc_so) ? "copied to \"" : "",
	    (fcc_so) ? fcc : "",
	    (fcc_so) ? "\"" : "");

    q_status_message(0, 1, 3, error_buf);

    if(fcc_so)				/* always clean this up */
      so_give(&fcc_so);

    return(rv && rrv);
}



#ifndef	DOS	
/* ----------------------------------------------------------------------
      Fork off mailer process and pipe the message into it

  Args: envelope -- The envelope for the BCC and debugging
        header   -- The text of the message header
        text     -- The message text
     
   Run the mailer process and pipe the message down to it. Using -t
   option for sendmail so it reads the header and decides what to do.
    Returns string error message or NULL if all is OK
   For now this only drives sendmail.

  ----*/
static char *
call_sendmail(envelope, body)
    ENVELOPE *envelope;
    BODY     *body;
{
    char        mail_cmd[MAXPATH+1];
    static char error_mess[100];
    char       *tmpfile;
    int         rv;

    dprint(1, (debugfile, "=== call_mailer ===\n"));

    if(!envelope->to && !envelope->cc && !envelope->bcc)
      return(NULL);

    tmpfile = temp_nam(NULL, "pinesend");

    rv = append_message2(tmpfile, envelope, body, AppendSendmail, NULL, NULL);
    if(rv < 0)
      goto io_error;

    sprintf(mail_cmd, "( ( %s %s ; /bin/rm -f %s ) < %s & )",
            SENDMAIL, SENDMAILFLAGS, tmpfile, tmpfile);

    dprint(6, (debugfile, "Send command \"%s\"\n", mail_cmd));

    (void)call_system(mail_cmd);

    dprint(1, (debugfile, "\n***** MAIL SENT ******\nTo: %s@%s",
	       (envelope->to && envelope->to->mailbox) ?
	        envelope->to->mailbox : "NULL", 
	       (envelope->to && envelope->to->host) ?
	        envelope->to->host : "NULL"));
    dprint(1, (debugfile, "Subject: %s\n", envelope->subject != NULL ?
                envelope->subject : ""));
    dprint(1, (debugfile, "Message ID: %s\n\n", envelope->message_id));

    fs_give((void **)&tmpfile);
    return(NULL);

  io_error:
    sprintf(error_mess,"\007Error starting mailer process: %s",
                error_description(errno));
    dprint(1, (debugfile, "Error writing %s: %s\n", tmpfile,
                error_description(errno)));

    fs_give((void **)&tmpfile);
    return(error_mess);
}



/*----------------------------------------------------------------------

  ----*/
static
call_system(command)
    char *command;
{
    SigType (*hsig)(), (*isig)(), (*qsig)();
    int status, pid, r;

    dprint(9, (debugfile, "Command: \"%s\"\n", command));

    isig = signal(SIGINT, SIG_IGN);
    qsig = signal(SIGQUIT, SIG_IGN);
    hsig = signal(SIGHUP, SIG_IGN);
    if((pid = vfork()) == 0){
        execl("/bin/sh", "sh", "-c", command, 0);
        _exit(-1);
    }
    dprint(9, (debugfile, "PID: %d\n", pid));
    if(pid == -1) {
        q_status_message1(1, 2, 3, "Error creating mailer process: %s",
                         error_description(errno));
        return(-1);
    }
#ifdef HAVE_WAIT_UNION
    while((r =  wait((union wait *)&status)) && r != -1 && r != pid);
#else
    while((r =  wait(&status)) && r != -1 && r != pid);
#endif
    signal(SIGINT,  isig);
    signal(SIGHUP,  hsig);
    signal(SIGQUIT, qsig);
    dprint(9, (debugfile, "STATUS: %d\n", status));
    return(0);
}
#endif	/* !DOS */


/*----------------------------------------------------------------------
    Set up fields for passing to pico.  Assumes first text part is
    intended to be passed along for editing, and is in the form of
    of a storage object brought into existence sometime before pico_send().
 -----*/
void
outgoing2strings(env, bod, to, cc, bcc, text, pico_a)
    ENVELOPE  *env;
    BODY      *bod;
    char     **to, **cc, **bcc;
    void     **text;
    PATMT    **pico_a;
{
    PART  *part;
    PATMT *pa;
    char  *type;

    /*
     * SIMPLIFYING ASSUMPTION #37: the first TEXT part's storage object
     * is guaranteed to be of type PicoText!
     */
    if(bod->type == TYPETEXT){
	*text = so_text(bod->contents.binary);
    } else if(bod->type == TYPEMULTIPART){
        if(bod->contents.part->body.type != TYPETEXT) {
            panic1("Can't compose message with first type %s",
                   int2string(bod->contents.part->body.type));
        }
	*text = so_text(bod->contents.part->body.contents.binary);

	/*
	 * If we already had a list, blast it now, so we can build a new
	 * attachment list that reflects what's really there...
	 */
	if(*pico_a)
	  free_attachment_list(*pico_a);

	pa = NULL;

        for(part = bod->contents.part->next; part != NULL; part = part->next) {
            if(pa == NULL) {
                /* empty list */
                *pico_a = (PATMT *)fs_get(sizeof(PATMT));
                pa = *pico_a;
            } else {
                pa->next = (PATMT *)fs_get(sizeof(PATMT));
                pa = pa->next;
            }
            pa->description = part->body.description == NULL ? cpystr("") : 
                                              cpystr(part->body.description);
            
            type         = type_desc(part->body.type,
                                     part->body.subtype,part->body.parameter,0);
            pa->filename = fs_get(strlen(type) + 4);
            sprintf(pa->filename, "[%s]", type);
            pa->flags    = A_FLIT;
            pa->size     = cpystr(byte_string(part->body.size.bytes));
            if(part->body.id == NULL)
              part->body.id = cpystr(generate_message_id(ps_global));
            pa->id       = cpystr(part->body.id);
            pa->next     = NULL;
        }
    } else {
        panic1("Can't compose message of MIME type %s\n",
               int2string(bod->type));
    }
        

    /*------------------------------------------------------------------
       Malloc strings to pass to composer editor because it expects
       such strings so it can realloc them
      -----------------------------------------------------------------*/
    *to  = addr_list_string(env->to, 0);
    *cc  = addr_list_string(env->cc, 0);
    *bcc = addr_list_string(env->bcc, 0);
}



/*----------------------------------------------------------------------
    Restore fields returned from pico to form useful to sending
    routines.
 -----*/
void
strings2outgoing(env, bod, to, cc, bcc, attach)
    ENVELOPE *env;
    BODY    **bod;
    char     *to, *cc, *bcc;
    PATMT    *attach;
{
    char *maildomain;

    maildomain = ps_global->maildomain;

    mail_free_address(&(env->to));
    env->to = NULL;
    /* rfc822_parse... doesn't like trailing space on it's addresses;
       it gives "junk at end of address" messages. Pico always 
       seems to leave a space so we hack it off here.
     */
    removing_trailing_white_space(to);
    rfc822_parse_adrlist(&(env->to), to, maildomain);

    if(env->to != NULL) {
       dprint(9, (debugfile, "to->personal %x\n", env->to->personal));
       dprint(9, (debugfile, "to->route_list %x\n", env->to->adl));
       dprint(9, (debugfile, "to->mailbox %x\n", env->to->mailbox));
       dprint(9, (debugfile, "to->host %x\n", env->to->host));
       dprint(9, (debugfile, "to->next %x\n", env->to->next));
       dprint(4, (debugfile, "cannonized To \"%s\"\n",
                addr_list_string(env->to, 1)));
    }

    mail_free_address(&(env->cc));
    env->cc = NULL; 
    removing_trailing_white_space(cc);
    rfc822_parse_adrlist(&(env->cc), cc, maildomain);


    mail_free_address(&(env->bcc));
    env->bcc = NULL;
    removing_trailing_white_space(bcc);
    rfc822_parse_adrlist(&(env->bcc), bcc, maildomain);

    create_message_body(bod, attach);
    pine_encode_body(env, *bod);
}



/*----------------------------------------------------------------------

 The head of the body list here is always either TEXT or MULTIPART. It may be
changed from TEXT to MULTIPART if there are attachments to be added
and it is not already multipart. 
  ----*/
void
create_message_body(b, attach)
    BODY  **b;
    PATMT  *attach;
{
    PART         *p, *p_trail;
    PATMT        *pa;
    BODY         *b1;
    void         *file_contents;
    long          file_len, new_len;
    PARAMETER    *pm;
#ifndef	DOS
    char          full_filename[MAXPATH + 1];
#endif

    if((*b)->type == TYPETEXT && attach == NULL) {
	(*b)->encoding = ENCOTHER;
	set_mime_types(*b);
        return;
    }

    if((*b)->type == TYPETEXT) {
        /*-- Current type is text, but there are attachments to add --*/
        /*-- Upgrade to a TYPEMULTIPART --*/
        b1                                  = (BODY *)mail_newbody();
        b1->type                            = TYPEMULTIPART;
        b1->contents.part                   = mail_newbody_part();
        b1->contents.part->body             = **b;

        (*b)->subtype = (*b)->id = (*b)->description = NULL;
	(*b)->parameter = NULL;
	(*b)->contents.binary               = NULL;
	pine_free_body(b);
        *b = b1;
    }

    /*-- Now type must be MULTIPART with first part text --*/
    (*b)->contents.part->body.encoding = ENCOTHER;
    set_mime_types(&((*b)->contents.part->body));

    /*------ Go through the parts list remove those to be deleted -----*/
    for(p = p_trail = (*b)->contents.part->next; p != NULL;) {
        for(pa = attach; pa != NULL; pa = pa->next) {
            if(pa->id == NULL)
              continue;
            if(p->body.id == NULL || strcmp(pa->id, p->body.id) == 0)
              break; /* Found it */
        }
        if(pa == NULL) {
            /* attachment wasn't in the list; zap it */
            if(p == (*b)->contents.part->next) {
                /* Beginning of list */
                (*b)->contents.part->next = p->next;
                p->next = NULL;  /* Don't free the whole chain */
                pine_free_body_part(&p);
                p = p_trail = (*b)->contents.part->next;
            } else {
                p_trail->next = p->next;
                p->next = NULL;  /* Don't free the whole chain */
                pine_free_body_part(&p);
                p = p_trail->next;
            }
        } else {
            p_trail = p;
            p       = p->next;
        }
    }

    /*---------- Now add any new attachments ---------*/
    for(p = (*b)->contents.part ; p->next != NULL; p = p->next);
    for(pa = attach; pa != NULL; pa = pa->next) {
        if(pa->id != NULL)
          continue; /* Has an ID, it's old */
	/*
	 * the idea is handle ALL attachments as open FILE *'s.  Actual
         * encoding and such is handled at the time the message
         * is shoved into the mail slot or written to disk...
	 *
         * Also, we never unlink a file, so it's up to whoever opens
         * it to deal with tmpfile issues.
	 */
	if((file_contents = (void *)so_get(FileStar, pa->filename,
					   READ_ACCESS)) == NULL){
            q_status_message2(1, 3, 4,
                              "\007Error \"%s\", couldn't attach file \"%s\"",
                              error_description(errno), pa->filename);
            display_message('x');
            continue;
        }
        
        p->next                      = mail_newbody_part();
        p                            = p->next;
        p->body.id                   = cpystr(generate_message_id(ps_global));
        p->body.contents.binary      = file_contents;
	/*
	 * Set type to unknown and let set_mime_types figure it out.
	 * Always encode attachments we add as BINARY.
	 */
	p->body.type		     = TYPEOTHER;
	p->body.encoding	     = ENCBINARY;
	p->body.size.bytes           = file_size(pa->filename);
	set_mime_types(&(p->body));
	so_release((STORE_S *)p->body.contents.binary);
        p->body.description          = cpystr(pa->description);
	/* add name attribute */
	if (p->body.parameter == NULL) {
	    pm = p->body.parameter = mail_newbody_parameter();
	    pm->attribute = cpystr("name");
	}else {
            for (pm = p->body.parameter;
			strucmp(pm->attribute, "name") && pm->next != NULL;
								pm = pm->next);
	    if (strucmp(pm->attribute, "name") != 0) {
		pm->next = mail_newbody_parameter();
		pm = pm->next;
		pm->attribute = cpystr("name");
	    }
	}
	pm->value = cpystr(last_cmpnt(pa->filename));

        p->next = NULL;
        pa->id = cpystr(p->body.id);
    }
}



/*
 * free_attachment_list - free attachments in given list
 */
void
free_attachment_list(alist)
    PATMT  *alist;
{
    PATMT  *trailing = alist;

    while(trailing = alist){
	alist = alist->next;
	if(trailing->description)
          fs_give((void **)&trailing->description);

	if(trailing->filename)
          fs_give((void **)&trailing->filename);

	if(trailing->size)
          fs_give((void **)&trailing->size);

	if(trailing->id)
          fs_give((void **)&trailing->id);

	fs_give((void **)&trailing);
    }
}



/*----------------------------------------------------------------------

Returns IsText:
  If lines are less than 500 characters and thereis nothing with the 
  8th bit on

Returns IsText8:
  If lines are less than 500 characters and less than 10% of characters
  have 8th bit on

Returns IsBinary
  All other cases;

 ---*/
FileTypes   
file_type(f, len)
    void *f;
    long  len;
{
    long      max_line, eight_bit_chars;
    char     *p, *line_start;
    FileTypes rv;

    max_line        = 0L;
    line_start      = (char *)f;
    eight_bit_chars = 0L;
    
    for(p = (char *)f; p < &(((char *)f)[len]); p++) {
        if(*p == '\n') {
            max_line = max(max_line, p - line_start);
            line_start = p;
        } else if(*p & 0x80){
            eight_bit_chars++;
        } else if(!*p) {
            /* A NULL, must be binary */
            return(IsBinary);
        }
    }
    if(max_line > 500L)
      rv = IsBinary;  /* Very long lines */
    else if(eight_bit_chars == 0L)
      rv = IsText;    /* short lines, no 8 bit */
    else if((eight_bit_chars * 100L)/len < 10L)
      rv = IsText8;   /* Short lines, < 10% 8 bit chars */
    else
      rv = IsBinary;   /* Short lines, > 10% 8 bit chars */
    
    dprint(4, (debugfile,
               "file_type -- len: %ld max_line: %ld  8 bit chars: %ld  %s\n",
               len, max_line, eight_bit_chars,
               rv==IsText ? "IsText" : rv==IsBinary ? "IsBinary" : "IsText8"));
    return(rv);
}



/*----------------------------------------------------------------------
    Output routine for rfc822_output_body

Normally this is used for writing a message down a TCP stream, but
writing a MIME body out is complex business so we use it here.
We must convert the network CRLF to the local new line convention, in this
case on UNIX it's LF.
 ----*/
unix_puts(stream_x, string)
    void   *stream_x;
    char   *string;
{
    FILE *stream = (FILE *)stream_x;
    char *p;

    /* Need error checking */
    for(p = string; *p; p++) {
#ifdef	DOS
        putc(*p, stream);
#else
        if(*p == '\015' && *(p+1) == '\012') {
            putc('\n', stream);
            p++;
        } else {
            putc(*p, stream);
        }
#endif
    }
    return(1); /* T for c-client */
}



/*----------------------------------------------------------------------
    General function to write a message to Berkeley format folder

Args:  folder -- The name of the folder
       env    -- The envelope (header) of the message to save
       bod    -- The message body
       type   -- Type of save operation
       fcc    -- name of file for fcc
       a      -- Attachemnts for Postponing

Returns: 0 if susccessful, -1 if not

The message can be written in three slightly different formats:

AppendPostponed:
  Includes: Fcc, Bcc, header, body
  confirms if folder *does* exist

AppendInterrupted:
  Includes: Fcc, Bcc, header, body
  overwrites existing folder without confirmation

AppendSendmail:
  Includes: Bcc, header, body
  overwrites existing folder without confirmation
----*/           
append_message2(folder, env, bod, type, fcc, a)
    char       *folder, *fcc;
    ENVELOPE   *env;
    BODY       *bod;
    AppendTypes type;
    PATMT      *a;
{
    char   filename[MAXPATH+1];
    int    rv, save_errno;
    FILE  *folder_stream;
    long   start_of_append;
#ifdef	DOS
#define	OTYPE	"w+b"
#define	OMODE	(S_IREAD | S_IWRITE)
#else
#define	OTYPE   "w"
#define	OMODE   0600
#endif

    strcpy(filename, folder);

    if (! expand_foldername(filename)) {
        /* expand_foldername generates it's own error message */
        dprint(1, (debugfile,
              "Error: Failed on expansion of filename %s (save)\n", filename));
        return(-1);
    }

    if(file_size(filename) > 0) {
        /*--- Folder exists and has stuff in it ----*/
	if(type == AppendPostpone) {
            if(want_to("One message already postponed. Overwrite it", 'n',
                        'n', NO_HELP, 0) == 'n')
              return(-1);
        }
    }

    if(folder_stream = fopen(filename, OTYPE)){
	if(type == AppendSendmail)
	  (void) chmod(filename, OMODE);
    }
    else{
        q_status_message2(1, 2, 4, "\007Error opening folder \"%s\", %s",
                         pretty_fn(filename), error_description(errno));
        return(0);
    }

    start_of_append = ftell(folder_stream);

    if(type == AppendPostpone || type == AppendInterrupted) {
        while(a) {
            if(a->flags & A_FLIT) {
                a = a->next;
                continue;
            }
            rv = fprintf(folder_stream, "Attachment-file: %s%s", 
			a->filename, NEWLINE);
            if (rv == EOF) 
              goto done;

            rv = fprintf(folder_stream, "Attachment-description: %s%s",
                         a->description, NEWLINE);
            if (rv == EOF) 
              goto done;
            a = a->next;
        }

        if(fcc) {
            rv = fprintf(folder_stream, "Fcc: %s%s", fcc, NEWLINE);
            if (rv == EOF) 
              goto done;
        }
    }

    pine_encode_body(env, bod);

    /*---- Output the header ----*/
    pine_rfc822_header(tmp_20k_buf, env, bod);
    removing_trailing_white_space(tmp_20k_buf);
    strcat(tmp_20k_buf, "\015\012");
    if(fcc_so){
	so_puts(fcc_so, tmp_20k_buf);
	so_puts(fcc_so, "\015\012");
    }

    if(unix_puts(folder_stream, tmp_20k_buf) == EOF)
      goto done;

    fputs(NEWLINE, folder_stream); /* blank line to separate the header */

    /*---- Output the message body ----*/
    rv = pine_rfc822_output_body(bod, (soutr_t)unix_puts,
				 (TCPSTREAM *) folder_stream);

    if (rv == EOF)
      goto done;

    rv = fclose(folder_stream);
    folder_stream = NULL;

  done:
    if(rv == EOF) {
        if(folder_stream)
          fclose(folder_stream);

        save_errno = errno;
#ifndef	DOS
        truncate(filename, start_of_append);
#endif
        q_status_message2(1, 2, 4, "\007Error \"%s\" writing \"%s\"",
                          error_description(save_errno), pretty_fn(folder));
        dprint(1, (debugfile, "Error writing %s: %s\n",
                   filename, error_description(save_errno)));
        return(-1);
    }
    else 
      return(0);
}



/*----------------------------------------------------------------------
  Insert the addition into the message id before first "@"
  ----*/
update_message_id(e, addition)
    ENVELOPE *e;
    char *addition;
{
    char *p, *q, *r, *new;

    new = fs_get(strlen(e->message_id) + strlen(addition) + 5);
    for(p = new, q = e->message_id; *q && *q != '@'; *p++ = *q++);
    *p++ = '-';
    for(r = addition; *r ; *p++ = *r++);
    for(; *q; *p++ = *q++);
    *p = *q;
    fs_give((void **)&(e->message_id));
    e->message_id = new;
}




static struct mime_count {
    long size;
    int text_parts;
    int image_parts;
    int message_parts;
    int application_parts;
    int audio_parts;
    int  video_parts;
} mc;

char *
mime_stats(body)
    BODY *body;
{
    static char id[10];
    mc. size = 0;
    mc.text_parts = 0;
    mc.image_parts = 0;
    mc.message_parts = 0;
    mc.application_parts = 0;
    mc.audio_parts = 0;
    mc.video_parts = 0;

    mime_recur(body);

    mc.text_parts        = min(8, mc.text_parts );
    mc.image_parts       = min(8, mc.image_parts );
    mc.message_parts     = min(8, mc.message_parts );
    mc.application_parts = min(8, mc.application_parts );
    mc.audio_parts       = min(8, mc.audio_parts );
    mc.video_parts       = min(8, mc.video_parts );


    id[0] = encode_bits(logbase2(mc.size));
    id[1] = encode_bits(mc.text_parts);
    id[2] = encode_bits(mc.message_parts);
    id[3] = encode_bits(mc.application_parts);
    id[4] = encode_bits(mc.video_parts);
    id[5] = encode_bits(mc.audio_parts);
    id[6] = encode_bits(mc.image_parts);
    id[7] = '\0';
    return(id);
}
    


/*----------------------------------------------------------------------
   ----*/
void
mime_recur(body)
    BODY *body;
{
    PART *part;
    switch (body->type) {
      case TYPETEXT:
        mc.text_parts++;
        break;
      case TYPEIMAGE:
        mc.image_parts++;
        break;
      case TYPEMESSAGE:
        mc.message_parts++;
        break;
      case TYPEAUDIO:
        mc.audio_parts++;
        break;
      case TYPEAPPLICATION:
        mc.application_parts++;
        break;
      case TYPEVIDEO:
        mc.video_parts++;
        break;
      case TYPEMULTIPART:
        for(part = body->contents.part; part != NULL; part = part->next) 
          mime_recur(&(part->body));
        break;
    }
    if(body->type != TYPEMULTIPART)
      mc.size += body->size.bytes;
}
        
int        
encode_bits(bits)
    int bits;
{
    if(bits < 10)
      return(bits + '0');
    else if(bits < 36)
      return(bits - 10 + 'a');
    else if (bits < 62)
      return(bits - 36 + 'A');
    else
      return('.');
}

int
logbase2(x)
    long x;
{
    int base2;

    for(base2 = 0; x != 0; base2++)
      x /= 2;
    return(base2);
}


/*
 * set_mime_types - sniff the given storage object to determine its 
 *                  type, subtype and encoding
 *
 *		"Type" and "encoding" must be set before calling this routine.
 *		If "type" is set to something other than TYPEOTHER on entry,
 *		then that is the "type" we wish to use.  Same for "encoding"
 *		using ENCOTHER instead of TYPEOTHER.  Otherwise, we
 *		figure them out here.  If "type" is already set, we also
 *		leave subtype alone.  If not, we figure out subtype here.
 *		There is a chance that we will upgrade "encoding" to a "higher"
 *		level.  For example, if it comes in as 7BIT we may change
 *		that to 8BIT if we find a From_ we want to escape.
 *
 * NOTE: this is rather inefficient if the store object is a CharStar
 *       but the win is all types are handled the same
 */
void
set_mime_types(body)
    BODY *body;
{
#define RBUFSZ	(8193)
    unsigned char   *buf, *p, *bol;
    register size_t  n;
    long             max_line = 0L,
                     eight_bit_chars = 0L,
                     line_so_far = 0L,
                     len = 0L,
                     can_be_ascii = 1L;
    STORE_S         *so = (STORE_S *)body->contents.binary;
    unsigned short   new_encoding = ENCOTHER;
#ifdef ENCODE_FROMS
    short            froms = 0,
                     bmap  = 0x1;
#endif

#ifndef DOS
    buf = (unsigned char *)fs_get(RBUFSZ);
#else
    buf = (unsigned char *)tmp_20k_buf;
#endif
    so_seek(so, 0L, 0);

    for(n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
      ;

    buf[n] = '\0';

    if (n) {    /* check first few bytes to look for magic numbers */
	if (body->type == TYPEOTHER) {
	    if (!strncmp((char *)buf, "GIF", 3)) {
		body->type = TYPEIMAGE;
		body->subtype = cpystr("GIF");
	    }
	    else if ((n > 9) && buf[0] == 0xFF && buf[1] == 0xD8 &&
		           buf[2] == 0xFF && buf[3] == 0xE0 && 
		           !strncmp((char *)&buf[6], "JFIF", 4)) {
	        body->type = TYPEIMAGE;
	        body->subtype = cpystr("JPEG");
	    }
	    else if (!strncmp((char *)buf, "MM", 2)
					|| !strncmp((char *)buf, "II", 2)) {
		body->type = TYPEIMAGE;
		body->subtype = cpystr("TIFF");
	    }
	    else if ((buf[0] == '%' && buf[1] == '!')
		     || (buf[0] == '\004' && buf[1] == '%' && buf[2] == '!')){
		body->type = TYPEAPPLICATION;
		body->subtype = cpystr("PostScript");
	    }
	    else if (!strncmp((char *)buf, ".snd", 4)) {
		body->type = TYPEAUDIO;
		body->subtype = cpystr("Basic");
	    }
	    else if ((n > 3) && buf[0] == 0x00 && buf[1] == 0x05 &&
		           buf[2] == 0x16 && buf[3] == 0x00) {
	        body->type = TYPEAPPLICATION;
		body->subtype = cpystr("APPLEFILE");
	    }
	    else if ((n > 3) && buf[0] == 0x04 && buf[1] == 0x03 &&
		           buf[2] == 0x4b && buf[3] == 0x50) {
	        body->type = TYPEAPPLICATION;
		body->subtype = cpystr("ZIP");
	    }

	    /*
	     * if type was set above, but no encoding specified, go
	     * ahead and make it BASE64...
	     */
	    if(body->type != TYPEOTHER && body->encoding == ENCOTHER)
	      body->encoding = ENCBINARY;
	}
    }else {
	/* PROBLEM !!! */
	if (body->type == TYPEOTHER) {
	    body->type = TYPEAPPLICATION;
	    body->subtype = cpystr("octet-stream");
	    if (body->encoding == ENCOTHER)
		body->encoding = ENCBINARY;
	}
    }

    if (body->encoding == ENCOTHER || body->type == TYPEOTHER) {
#ifdef DOS /* for binary file detection */
	int lastchar = '\0';
#endif
	p = bol = buf;
	len = n;
	while (n--) {
/* Some people don't like quoted-printable caused by leading Froms */
#ifdef ENCODE_FROMS
	    Find_Froms(froms, bmap, *p);
#endif
	    if (*p == '\n') {
		max_line = max(max_line, line_so_far + p - bol);
		bol = p;
		line_so_far = 0L;
	    }else if (*p == ctrl('O') || *p == ctrl('N') || *p == ESCAPE) {
		can_be_ascii--;
	    }else if (*p & 0x80) {
		eight_bit_chars++;
	    }else if (!*p) {
		/* NULL found. Unless we're told otherwise, must be binary */
		if (body->type == TYPEOTHER) {
		    body->type = TYPEAPPLICATION;
		    body->subtype = cpystr("octet-stream");
		}

		/*
		 * The "TYPETEXT" here handles the case that the NULL
		 * comes from imported text generated by some external
		 * editor that permits or inserts NULLS.  Otherwise,
		 * assume it's a binary segment...
		 */
		new_encoding = (body->type==TYPETEXT) ? ENC8BIT : ENCBINARY;

		/*
		 * Since we've already set encoding, count this as a 
		 * hi bit char and continue.  The reason is that if this
		 * is text, there may be a high percentage of encoded 
		 * characters, so base64 may get set below...
		 */
		if(body->type == TYPETEXT)
		  eight_bit_chars++;
		else
		  break;
	    }
#ifdef DOS /* for binary file detection */
#define BREAKOUT 300   /* a value that a character can't be */
	    /* LF with no preceding CR, so binary */
	    else if (*p == '\n' && lastchar != '\r') {
		    lastchar = BREAKOUT;
	    }
#endif

#ifdef DOS /* for binary file detection */
	    if (lastchar != BREAKOUT)
		lastchar = *p;
#endif

	    /* read another buffer in */
	    if (n == 0) {
		line_so_far += p - bol;
		for (n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
		  ;
		len += n;
		p = buf;
	    }else {
		p++;
	    }
#ifdef DOS /* for binary file detection */
	    /* either a lone \r or lone \n indicate binary file */
	    if (lastchar == '\r' || lastchar == BREAKOUT) {
		if (lastchar == BREAKOUT || n == 0 || *p != '\n') {
		    if (body->type == TYPEOTHER) {
			body->type = TYPEAPPLICATION;
			body->subtype = cpystr("octet-stream");
		    }

		    new_encoding = ENCBINARY;
		    break;
		}
	    }
#endif
	}
    }

    if (body->encoding == ENCOTHER || body->type == TYPEOTHER) {
	/*
	 * Since the type or encoding aren't set yet, fall thru a 
	 * series of tests to make sure an adequate type and 
	 * encoding are set...
	 */

	if (max_line >= 1000L) { 	/* 1000 comes from rfc821 */
	    if (body->type == TYPEOTHER) {
		/*
		 * Since the types not set, then we didn't find a NULL.
		 * If there's no NULL, then this is likely text.  However,
		 * since we can't be *completely* sure, we set it to
		 * the generic type.
		 */
		body->type = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    if (new_encoding != ENCBINARY)
	      /*
	       * As with NULL handling, if we're told it's text, 
	       * qp-encode it, else it gets base 64...
	       */
	      new_encoding = (body->type == TYPETEXT) ? ENC8BIT : ENCBINARY;
	}

	if (eight_bit_chars == 0L) {
	    if (body->type == TYPEOTHER)
		body->type = TYPETEXT;

	    if (new_encoding == ENCOTHER)
		new_encoding = ENC7BIT;  /* short lines, no 8 bit */
	}
	else if ((eight_bit_chars * 100L)/len < 30L) {
	    /*
	     * The 30% threshold is based on qp encoded readability
	     * on non-MIME UA's.
	     */
	    can_be_ascii--;
	    if (body->type == TYPEOTHER)
		body->type = TYPETEXT;

	    if (new_encoding != ENCBINARY)
		new_encoding = ENC8BIT;  /* short lines, < 30% 8 bit chars */
	}else {
	    can_be_ascii--;
	    if (body->type == TYPEOTHER) {
		body->type = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    /*
	     * Apply maximal encoding regardless of previous
	     * setting.  This segment's either not text, or is 
	     * unlikely to be readable with > 30% of the
	     * text encoded anyway, so we might as well save space...
	     */
	    new_encoding = ENCBINARY;   /*  > 30% 8 bit chars */
	}
    }

#ifdef ENCODE_FROMS
    /* If there were From_'s at the beginning of a line */
    if (froms && new_encoding != ENCBINARY)
	new_encoding = ENC8BIT;
#endif

    /* need to set the subtype, and possibly the charset */
    if(body->type == TYPETEXT && body->subtype == NULL) {
        PARAMETER *pm;

	body->subtype = cpystr("PLAIN");

	/* need to add charset */
	if(can_be_ascii > 0) {
	    if(body->parameter == NULL) {
	        pm = body->parameter = mail_newbody_parameter();
	        pm->attribute = cpystr("charset");
	    }else {
                for(pm = body->parameter;
			strucmp(pm->attribute, "charset") && pm->next != NULL;
								pm = pm->next);
	        if(strucmp(pm->attribute, "charset") != 0) {
		    pm->next = mail_newbody_parameter();
		    pm = pm->next;
		    pm->attribute = cpystr("charset");
	        }else if(pm->value) {
			fs_give((void **)&pm->value);
		}
	    }

	    pm->value = cpystr("US-ASCII");
	}
    }

    if(body->encoding == ENCOTHER)
      body->encoding = new_encoding;

#ifndef	DOS
    fs_give((void **)&buf);
#endif
}



/*
 * these functions provide for attachment handling in DOS.  They
 * may at a later date be exended to pine in general.  It pretty
 * much depends on how the piping performs.
 */


/*
 * pine version of c-client's rfc822_header() function.  Same
 * function, actually, except pine's body_header write function
 * is called, because encoding is delayed until output_body() is
 * called
 */
void
pine_rfc822_header(header, env, body)
    char *header;
    ENVELOPE *env;
    BODY *body;
{
#ifdef	DOS
  NETMBX netmbox;
#endif

  if (env->remail) {		/* if remailing */
    long i = strlen (env->remail);
				/* flush extra blank line */
    if (i > 4 && env->remail[i-4] == '\015') env->remail[i-2] = '\0';
    strcpy (header,env->remail);/* start with remail header */
  }
  else *header = '\0';		/* else initialize header to null */
  rfc822_header_line (&header,"Newsgroups",env,env->newsgroups);
  rfc822_header_line (&header,"Date",env,env->date);
  rfc822_address_line (&header,"From",env,env->from);
  rfc822_address_line (&header,"Sender",env,env->sender);
  rfc822_address_line (&header,"Reply-To",env,env->reply_to);
  rfc822_header_line (&header,"Subject",env,env->subject);
  rfc822_address_line (&header,"To",env,env->to);
  rfc822_address_line (&header,"cc",env,env->cc);
#if	defined(DOS) && !defined(NOAUTH)
  /*
   * Add comforting "X-" header line indicating what sort of 
   * authenticity the receiver can expect...
   */

  if(ps_global->mail_stream 
     && mail_valid_net_parse(ps_global->mail_stream->mailbox, &netmbox)
     && !netmbox.anoflag){
      char   tmp[MAILTMPLEN];	/* place to write  */
      char   last_char = netmbox.host[strlen(netmbox.host) - 1];

      sprintf(tmp, "%s@%s%s%s", mm_login_user, isdigit(last_char) ? "[" : "",
	      netmbox.host, isdigit(last_char) ? "]" : "");
      rfc822_header_line (&header,"X-Sender",env,tmp);
  }
  else
    rfc822_header_line (&header,"X-Warning",env,"UNAuthenticated Sender");
#endif
  rfc822_address_line (&header,"bcc",env,env->bcc);
  rfc822_header_line (&header,"In-Reply-To",env,env->in_reply_to);
  rfc822_header_line (&header,"Message-ID",env,env->message_id);
  if (body && !env->remail) {	/* not if remail or no body structure */
    strcat (header,"MIME-Version: 1.0\015\012");
    pine_write_body_header (&header,body);
  }
}


/*
 * since encoding happens on the way out the door, this is basically
 * just needed to handle TYPEMULTIPART
 */
void
pine_encode_body (env,body)
    ENVELOPE *env;
    BODY *body;
{
  void *f;
  PART *part;
  if (body) switch (body->type) {
  case TYPEMULTIPART:		/* multi-part */
    if (!body->parameter) {	/* cookie not set up yet? */
      char tmp[MAILTMPLEN];	/* make cookie not in BASE64 or QUOTEPRINT*/
      sprintf (tmp,"%ld-%ld-%ld:#%ld",gethostid (),random (),time (0),
	       getpid ());
      body->parameter = mail_newbody_parameter ();
      body->parameter->attribute = cpystr ("BOUNDARY");
      body->parameter->value = cpystr (tmp);
    }
    part = body->contents.part;	/* encode body parts */
    do pine_encode_body (env,&part->body);
    while (part = part->next);	/* until done */
    break;
/* case MESSAGE:	*/	/* here for documentation */
    /* Encapsulated messages are always treated as text objects at this point.
       This means that you must replace body->contents.msg with
       body->contents.text, which probably involves copying
       body->contents.msg.text to body->contents.text */
  default:			/* all else has some encoding */
    /*
     * but we'll delay encoding it until the message is on the way
     * into the mail slot...
     */
    break;
  }
}


/*
 * pine_rfc822_output - pine's version of c-client call.  Necessary here
 *			since we're not using its structures as intended!
 */
long
pine_rfc822_output(t,env,body,f,s)
    char *t;
    ENVELOPE *env;
    BODY *body;
    soutr_t f;
    TCPSTREAM *s;
{
    pine_encode_body (env,body);	/* encode body as necessary */
    pine_rfc822_header (t,env,body);	/* build RFC822 header */
					/* output header and body */
    if(fcc_so)
      so_puts(fcc_so, t);

    return (f ? (*f) (s,t) : 1) && 
            (body ? pine_rfc822_output_body (body,f,s) : 1);
}


static soutr_t    l_f;
static TCPSTREAM *l_stream;
static unsigned   c_in_buf = 0;

/*
 * l_flust_net - empties gf_io terminal function's buffer
 */
void
l_flush_net()
{
    if(c_in_buf){
	if(l_f)
	  (*l_f)(l_stream, tmp_20k_buf);

	if(fcc_so)
	  so_puts(fcc_so, tmp_20k_buf);
    }

    c_in_buf = 0;
}


/*
 * l_putc - gf_io terminal function that calls the soutr_t output function
 *          (e.g. unix_puts for call_sendmail).
 *
 */
int
l_putc(c)
    int c;
{
    tmp_20k_buf[c_in_buf++] = (char) c;
    tmp_20k_buf[c_in_buf]   = '\0';

    if(c_in_buf > 16384){
	if(fcc_so)
	  so_puts(fcc_so, tmp_20k_buf);

        c_in_buf = 0;
        return(l_f ? (int)(*l_f)(l_stream, tmp_20k_buf) : 1);
    }
    else
	return(TRUE);
}


/*
 * pine_rfc822_output_body - pine's version of c-client call.  Again, 
 *                necessary since c-client doesn't know about how
 *                we're treating attachments
 */
long
pine_rfc822_output_body(body, f, s)
    BODY *body;
    soutr_t f;
    TCPSTREAM *s;
{
    PART *part;
    PARAMETER *param;
    char *cookie = NIL, *t, *encode_error;
    char tmp[MAILTMPLEN];
    gf_io_t            gc, pc;

    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	part = body->contents.part;	/* first body part */
					/* find cookie */
	for (param = body->parameter; param && !cookie; param = param->next)
	  if (!strcmp (param->attribute,"BOUNDARY")) cookie = param->value;
	if (!cookie) cookie = "-";	/* yucky default */

	/*
	 * Output a bit of text before the first multipart delimiter
	 * to warn unsuspecting users of non-mime-aware ua's that
	 * they should expect weirdness...
	 */
	if(!(*f)(s, "  This message is in MIME format.  The first part should be readable text,\015\012  while the remaining parts are likely unreadable without MIME-aware tools.\015\012\015\012"))
	  return(0);

	do {				/* for each part */
					/* build cookie */
	    sprintf (t = tmp,"--%s\015\012",cookie);
					/* append mini-header */
	    pine_write_body_header (&t,&part->body);
				/* output cookie, mini-header, and contents */
	    if(fcc_so)
	      so_puts(fcc_so, tmp);

	    if (!((f ? (*f) (s,tmp) : 1)
		  && pine_rfc822_output_body (&part->body,f,s)))
	      return(0);
	} while (part = part->next);	/* until done */
					/* output trailing cookie */
	sprintf (t = tmp,"--%s--",cookie);
	if(fcc_so){
	    so_puts(fcc_so, t);
	    so_puts(fcc_so, "\015\012");
	}

	return(f ? ((*f) (s,t) && (*f) (s,"\015\012")) : 1);
    }

    l_f      = f;			/* set up for writing chars...  */
    l_stream = s;			/* out other end of pipe...     */
    pc       = l_putc;			/* using our output function	*/
    gf_filter_init();

    if(body->contents.binary)
      gf_set_so_readc(&gc, (STORE_S *)body->contents.binary);
    else
      return(1);

    so_seek((STORE_S *)body->contents.binary, 0L, 0);

    if(body->type != TYPEMESSAGE){ 	/* NOT encapsulated message */
	/*
	 * Convert text pieces to canonical form
	 * BEFORE applying any encoding (rfc1341: appendix G)...
	 */
	if(body->type == TYPETEXT){
	    gf_link_filter(gf_local_nvtnl);
	}

	switch (body->encoding) {	/* all else needs filtering */
	  case ENC8BIT:			/* encode 8BIT into QUOTED-PRINTABLE */
	    gf_link_filter(gf_8bit_qp);
	    break;

	  case ENCBINARY:		/* encode binary into BASE64 */
	    gf_link_filter(gf_binary_b64);
	    break;

	  default:			/* otherwise text */
	    break;
	}
    }

    if(encode_error = gf_pipe(gc, pc)){ /* shove body part down pipe */
	q_status_message1(1, 3, 4,
			  "\007Encoding Error \"%s\"", encode_error);
	display_message('x');
	return(0);
    }
    else
      l_flush_net();

    so_release((STORE_S *)body->contents.binary);

    if(fcc_so)
      so_puts(fcc_so, "\015\012");

    return(f ? (*f)(s, "\015\012") : 1);	/* output final stuff */
}


/*
 * pine_write_body_header - another c-client clone.  This time only
 *                          so the final encoding labels get set 
 *                          correctly since it hasn't happened yet.
 */
void
pine_write_body_header(dst, body)
    char **dst;
    BODY  *body;
{
    char *s;
    PARAMETER *param = body->parameter;
    extern const char *tspecials;

    sprintf (*dst += strlen (*dst),"Content-Type: %s",body_types[body->type]);
    s = body->subtype ? body->subtype : rfc822_default_subtype (body->type);
    sprintf (*dst += strlen (*dst),"/%s",s);
    if (param) do {
	sprintf (*dst += strlen (*dst),"; %s=",param->attribute);
	rfc822_cat (*dst,param->value,tspecials);
    } while (param = param->next);
    else if (body->type == TYPETEXT) strcat (*dst,"; charset=US-ASCII");
    strcpy (*dst += strlen (*dst),"\015\012");
    if (body->encoding)		/* note: encoding 7BIT never output! */
      sprintf (*dst += strlen (*dst),"Content-Transfer-Encoding: %s\015\012",
	       body_encodings[body->encoding == ENCBINARY ? ENCBASE64 :
			      body->encoding == ENC8BIT ? ENCQUOTEDPRINTABLE :
			      body->encoding]);
    if (body->id) sprintf (*dst += strlen (*dst),"Content-ID: %s\015\012",
			   body->id);
    if (body->description)
      sprintf (*dst += strlen (*dst),"Content-Description: %s\015\012",
	       body->description);
    strcat (*dst,"\015\012");	/* write terminating blank line */
}


/*
 * pine_free_body - yet another c-client clone just so the body gets
 *                  free'd appropriately
 */
void
pine_free_body(body)
    BODY **body;
{
    if (*body) {			/* only free if exists */
	pine_free_body_data (*body);	/* free its data */
	fs_give ((void **) body);	/* return body to free storage */
    }
}


/* 
 * pine_free_body_data - not just releasing strings anymore!
 */
void
pine_free_body_data(body)
    BODY *body;
{
    if (body->subtype) fs_give ((void **) &body->subtype);
    mail_free_body_parameter (&body->parameter);
    if (body->id) fs_give ((void **) &body->id);
    if (body->description) fs_give ((void **) &body->description);
    if(body->type == TYPEMULTIPART){
	pine_free_body_part (&body->contents.part);
    }
    else if(body->contents.binary){
	so_give((STORE_S **)&body->contents.binary);
	body->contents.binary = NULL;
    }
}


/*
 * pine_free_body_part - c-client clone to call the right routines
 *             for cleaning up.
 */
void
pine_free_body_part(part)
    PART **part;
{
    if (*part) {		/* only free if exists */
	pine_free_body_data (&(*part)->body);
				/* run down the list as necessary */
	pine_free_body_part (&(*part)->next);
	fs_give ((void **) part); /* return body part to free storage */
    }
}
