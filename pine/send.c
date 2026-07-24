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

void     outgoing2strings(ENVELOPE *,BODY *,char **,char **,char **,void **);
void      strings2outgoing(ENVELOPE *,BODY **,char *,char *,char *);
int       append_message2(char *,ENVELOPE *,BODY *,AppendTypes,char *);
void      parse_postponed_header(char *, char *, int);
void      create_message_body(BODY **);
static    char *call_sendmail(ENVELOPE *, BODY *);
static    int   call_system(char *);

/*
 * Pines version of c-client routines, but changed to allow
 * piping attachments thru encoding filters on the way out the
 * SMTP mail slot or into file handed to sendmail... (mss, 4 Nov 1992)
 */
void      pine_write_body_header(char **, BODY  *);
void      pine_encode_body(ENVELOPE *, BODY *);
void      pine_rfc822_header(char *, ENVELOPE *, BODY *);
size_t    pine_rfc822_header_size(ENVELOPE *, BODY *);
long      pine_rfc822_output(char *,ENVELOPE *,BODY *,soutr_t,TCPSTREAM *);
long      pine_rfc822_output_body(BODY *,soutr_t,TCPSTREAM *);
void      pine_free_body(BODY **);
void      pine_free_body_data(BODY *);
void      pine_free_body_part(PART **);
void      set_mime_types(BODY *);
int       call_mailer(char *, ENVELOPE *, BODY *);


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
    STRING     string_struct;
    FILE      *fp;
    STORE_S   *so = NULL;
    gf_io_t    gc, pc;
    fpos_t     body_start;

    dprint(1, (debugfile,
                 "\n\n    ---- COMPOSE SCREEN (not in pico yet) ----\n"));

    fcc2 = NULL;
    file_path[0] = '\0';

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
	if((fp=fopen(file_path, "r")) == NULL){
            q_status_message2(1, 3,4, 
                "\007Error \"%s\" reading postponed composition in \"%s\"",
                              error_description(errno), file_path);
            return;
	}

	{
	    /*
	     * Accumulate the whole header in tmp_20k_buf, growing it to
	     * stay ahead of hdr_off instead of assuming a fixed capacity
	     * is big enough -- a postponed message can have a header
	     * (e.g. a long To: list) well past the 2K baseline.
	     */
	    size_t hdr_off = 0;

	    grow_tmp_20k_buf(hdr_off + 1025);
	    p = tmp_20k_buf + hdr_off;
	    while(fgets(p, 1024, fp) != NULL	/* slurp up the header */
		  && strncmp(p, NEWLINE, strlen(NEWLINE))){
		hdr_off += strlen(p);
		grow_tmp_20k_buf(hdr_off + 1025);
		p = tmp_20k_buf + hdr_off;
	    }
	}

	fget_pos(fp, &body_start);		/* remember where we're at */

	strcat(tmp_20k_buf, NEWLINE);
	file = cpystr(tmp_20k_buf);		/* copy header */
	/*
	 * rfc822_parse_msg() reuses tmp_20k_buf as scratch space for one
	 * unfolded logical header line at a time, which in the worst
	 * case (e.g. one very long address list folded across many
	 * physical lines) can be nearly as long as the whole header.
	 */
	grow_tmp_20k_buf(strlen(file) + 128);
	p    = tmp_20k_buf;
	/* have c-client parse the rest */
	INIT(&string_struct, mail_string, (void *)"", 0L);
	rfc822_parse_msg(&outgoing, &body, file, (unsigned long)strlen(file),
			 &string_struct, ps_global->maildomain, tmp_20k_buf);
	/* grab what we want from the header */
        parse_postponed_header(file, fcc, sizeof(fcc));
        fcc2 = fcc;

	/*
	 * Now deal with the body.  The parse discovered the body's
	 * encoding and left the file pointer at the beginning of the body.
	 * Create a space for the text, and read in with any necessary
	 * decoding...
	 */
	if((so = (void *)so_get(CharStar, NULL, EDIT_ACCESS)) == NULL){
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
	 * decoding filters expect endoflines to be CRLF, so convert
	 * what's on disk in the local convention to CRLF going in, then
	 * (following the same canonical model detach()/mailcmd.c uses
	 * for attachments) convert back from CRLF to local convention
	 * coming out -- CharStar storage, unlike the PicoText storage
	 * this used before the composer-as-a-separate-process cut (see
	 * TODO-pdp11-done.md), keeps CR bytes verbatim rather than
	 * silently dropping them.
	 */
	gf_link_filter(gf_local_nvtnl);
	if(body->encoding == ENCBASE64)
	  gf_link_filter(gf_b64_binary);
	else if(body->encoding == ENCQUOTEDPRINTABLE)
	  gf_link_filter(gf_qp_8bit);

	gf_link_filter(gf_nvtnl_local);

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
	if((body->contents.binary = (void *)so_get(CharStar, NULL,
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

    pine_send(outgoing, &body, "COMPOSE MESSAGE", fcc2, start_in_body);

    mail_free_envelope(&outgoing);
    pine_free_body(&body);
}



/*----------------------------------------------------------------------
    Parse the postponed message for the Pine specific fields

Args:  text -- Text to parse for fcc
       fcc  -- Pointer to buffer to return fcc in
 ----*/
void
parse_postponed_header(text, fcc, fcc_size)
    char   *text, *fcc;
    int     fcc_size;
{
    register char *p, *q;

    *fcc = '\0';

    /*---- Loop until the end of the header ----*/
    for(p = text; *p && !(*p == '\n' && *(p+1) != '\n'); p++) {
        if(struncmp(p, "Fcc:", 4) == 0) {
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
 

/*----------------------------------------------------------------------
    Support for running the composer as a separate exec'd process (see
    TODO-pdp11.md / TODO-pdp11-done.md). pine_send() below writes a
    small handoff file (plain strings it already builds via
    outgoing2strings(), plus the body as a separate plain-text temp
    file) and execs "pico -P <handoff> <result>"; the child (see
    pico/compose_stub.c) reads it, runs the ordinary composer, and
    writes an edited version back. None of this needs c-client in the
    child -- see compose_stub.c's header comment.
  ----------------------------------------------------------------------*/

/*
 * write_field/read_field - length-prefixed field I/O, matching
 * compose_stub.c's format exactly (kept as a separate copy rather than
 * a shared helper -- pine and pico are different link units and this
 * is only a few lines each way).
 */
static void
write_field(FILE *fp, char *tag, char *buf)
{
    long len = buf ? strlen(buf) : 0L;

    fprintf(fp, "%s %ld\n", tag, len);
    if(len)
      fwrite(buf, 1, (size_t)len, fp);
    fputc('\n', fp);
}


static char *
read_field(FILE *fp, char *tag)
{
    char got_tag[32];
    char linebuf[56];
    long len;
    char *buf;

    if(fgets(linebuf, sizeof(linebuf), fp) == NULL)
      return(NULL);

    if(sscanf(linebuf, "%31s %ld", got_tag, &len) != 2
       || strcmp(got_tag, tag) != 0 || len < 0)
      return(NULL);

    if((buf = (char *)fs_get((size_t)len + 1)) == NULL)
      return(NULL);

    if(len && fread(buf, 1, (size_t)len, fp) != (size_t)len){
	fs_give((void **)&buf);
	return(NULL);
    }

    buf[len] = '\0';
    (void)fgetc(fp);
    return(buf);
}


/*
 * compose_body_store - the STORE_S* holding the message text, same
 * branch outgoing2strings()/create_message_body() already use.
 */
static STORE_S *
compose_body_store(BODY *bod)
{
    return (STORE_S *)((bod->type == TYPETEXT) ? bod->contents.binary
			: bod->contents.part->body.contents.binary);
}


/*
 * dump_body_text - copy the composer body's current text out to a
 * plain file, for handing to the composer subprocess.
 */
static int
dump_body_text(BODY *bod, char *path)
{
    STORE_S      *so = compose_body_store(bod);
    FILE         *fp;
    unsigned char c;

    if((fp = fopen(path, "w")) == NULL)
      return(-1);

    so_seek(so, 0L, 0);
    while(so_readc(&c, so))
      putc((int)c, fp);

    fclose(fp);
    return(0);
}


/*
 * reload_body_text - replace the composer body's STORE_S with a fresh
 * one built from the composer subprocess's edited body file.
 */
static int
reload_body_text(BODY *bod, char *path)
{
    FILE    *fp;
    STORE_S *newso, *oldso;
    int      c;

    if((fp = fopen(path, "r")) == NULL)
      return(-1);

    if((newso = so_get(CharStar, NULL, EDIT_ACCESS)) == NULL){
	fclose(fp);
	return(-1);
    }

    while((c = getc(fp)) != EOF)
      so_writec(c, newso);

    fclose(fp);

    oldso = compose_body_store(bod);
    so_give(&oldso);

    if(bod->type == TYPETEXT)
      bod->contents.binary = (void *)newso;
    else
      bod->contents.part->body.contents.binary = (void *)newso;

    return(0);
}


/*
 * pico_child_path - the composer binary to exec: the same directory
 * pine's own binary lives in (matches both a real install, where pine
 * and pico are installed together, and the local ./build lnx dev/test
 * layout, where both sit in ./bin/) -- falling back to a bare "pico"
 * (relying on $PATH) if pine's own argv[0] had no directory part.
 */
static char *
pico_child_path(void)
{
    static char path[MAXPATH + 1];
    char       *slash;

    if(ps_global->pine_exe
       && (slash = strrchr(ps_global->pine_exe, '/')) != NULL){
	int dirlen = slash - ps_global->pine_exe + 1;

	strncpy(path, ps_global->pine_exe, dirlen);
	path[dirlen] = '\0';
	strcat(path, "pico");
    }
    else
      strcpy(path, "pico");

    return(path);
}


/*
 * run_composer_subprocess - write the handoff file, fork/exec/wait for
 * "pico -P", then read the result back. Leaves new_to/new_cc/new_bcc/
 * new_sub/new_fcc pointing at fresh fs_get'd strings (falling back to
 * copies of the unedited input on any failure, so callers never see a
 * NULL) and returns the equivalent of pico()'s old return value --
 * COMP_CANCEL/COMP_SUSPEND/COMP_GOTHUP/0-meaning-send.
 */
static int
run_composer_subprocess(char *anchor, char *to, char *cc, char *bcc,
			 char *sub, char *fcc, char *bodyfile, unsigned flags,
			 char **new_to, char **new_cc, char **new_bcc,
			 char **new_sub, char **new_fcc)
{
    extern char version_buff[];
    char     handoff_path[MAXPATH + 1], result_path[MAXPATH + 1];
    FILE    *fp;
    int      pid, r, status, editor_result;
    SigType (*isig)(int), (*qsig)(int), (*hsig)(int);

    strcpy(handoff_path, temp_nam(NULL, "pinecmph"));
    strcpy(result_path, temp_nam(NULL, "pinecmpr"));

    if((fp = fopen(handoff_path, "w")) == NULL){
	q_status_message(1, 2, 4, "\007Can't create composer temp file");
	goto fallback;
    }

    write_field(fp, "ANCHOR", anchor);
    write_field(fp, "VERSION", version_buff);
    write_field(fp, "TO", to);
    write_field(fp, "CC", cc);
    write_field(fp, "BCC", bcc);
    write_field(fp, "SUBJECT", sub);
    write_field(fp, "FCC", fcc);
    write_field(fp, "BODYFILE", bodyfile);
    { char flagbuf[16]; sprintf(flagbuf, "%u", flags); write_field(fp, "FLAGS", flagbuf); }

    fclose(fp);

    isig = signal(SIGINT, SIG_IGN);
    qsig = signal(SIGQUIT, SIG_IGN);
    hsig = signal(SIGHUP, SIG_IGN);

    if((pid = fork()) == 0){
	execl(pico_child_path(), "pico", "-P", handoff_path, result_path,
	      (char *)0);
	_exit(-1);
    }

    if(pid == -1){
	signal(SIGINT,  isig);
	signal(SIGQUIT, qsig);
	signal(SIGHUP,  hsig);
	unlink(handoff_path);
	q_status_message1(1, 2, 4, "\007Error starting composer: %s",
			  error_description(errno));
	goto fallback;
    }

    /*
     * Retry on EINTR -- unlike call_system()'s short-lived wait()
     * (which this loop is otherwise modeled on), the composer
     * subprocess can run for as long as the user takes to compose,
     * during which SIGWINCH (terminal resize) or other signals can
     * interrupt wait() well before the child actually exits. Giving
     * up on any wait() != pid (the old idiom) misreads that as "no
     * result" and abandons the composition.
     */
#ifdef HAVE_WAIT_UNION
    while((r = wait((union wait *)&status)) != pid)
      if(r == -1 && errno != EINTR)
	break;
#else
    while((r = wait(&status)) != pid)
      if(r == -1 && errno != EINTR)
	break;
#endif

    signal(SIGINT,  isig);
    signal(SIGQUIT, qsig);
    signal(SIGHUP,  hsig);

    unlink(handoff_path);

    if((fp = fopen(result_path, "r")) != NULL){
	char line[32];

	editor_result = COMP_CANCEL;
	if(fgets(line, sizeof(line), fp) != NULL)
	  sscanf(line, "STATUS %d", &editor_result);

	if((*new_to  = read_field(fp, "TO"))      == NULL)
	  *new_to  = cpystr(to);
	if((*new_cc  = read_field(fp, "CC"))      == NULL)
	  *new_cc  = cpystr(cc);
	if((*new_bcc = read_field(fp, "BCC"))     == NULL)
	  *new_bcc = cpystr(bcc);
	if((*new_sub = read_field(fp, "SUBJECT")) == NULL)
	  *new_sub = cpystr(sub);
	if((*new_fcc = read_field(fp, "FCC"))     == NULL)
	  *new_fcc = cpystr(fcc);

	fclose(fp);
	unlink(result_path);
	return(editor_result);
    }

    q_status_message(1, 2, 4, "\007Composer subprocess produced no result");

  fallback:
    *new_to  = cpystr(to);
    *new_cc  = cpystr(cc);
    *new_bcc = cpystr(bcc);
    *new_sub = cpystr(sub);
    *new_fcc = cpystr(fcc);
    return(COMP_CANCEL);
}



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
pine_send(outgoing, body, editor_title, fcc, are_replying)
    ENVELOPE  *outgoing;  /* envelope for outgoing message */
    BODY     **body;
    char      *editor_title, *fcc;
    long	are_replying;
{
    int      editor_result, result, orig_cols, second_time = 0;
    long     real_current_msgno, i;
    char    *tobuf, *ccbuf, *bccbuf, *fccbuf, *anchor;
    void    *msgtext;
    unsigned pine_flags;
    char     bodyfile_path[MAXPATH + 1];

    dprint(1, (debugfile,"\n === send called === \n"));
    /*
     * Remember the real current message number, as new mail arrival
     * may invalidate the current sorted message number.  Restore it
     * before we return...
     */
    real_current_msgno = (ps_global->current_sorted_msgno <= 0L) ? 0L :
			   ps_global->sort[ps_global->current_sorted_msgno];

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

    /*----- Fill in a few general parts of the envelope ----*/
    fccbuf = cpystr(fcc != NULL ? fcc : ps_global->VAR_DEFAULT_FCC);
    strcpy(bodyfile_path, temp_nam(NULL, "pinecmpb"));

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
            the composer can edit
         */
        outgoing2strings(outgoing, *body, &tobuf, &ccbuf, &bccbuf, &msgtext);

	anchor = set_titlebar(editor_title, 0, FolderName, 0, 0, 0);

	pine_flags  = 0;
	pine_flags |= (F_ON(F_CAN_SUSPEND,ps_global)) ? P_SUSPEND : 0;
	pine_flags |= (F_ON(F_USE_FK,ps_global)) ? P_FKEYS : 0;
	pine_flags |= (are_replying || second_time) ? P_BODY : 0;

        dprint(9, (debugfile, "flags: %x\n", pine_flags));

        flush_status_messages();
        clear_cursor_pos();

	if(dump_body_text(*body, bodyfile_path) < 0){
	    char *nt, *nc, *nb, *ns, *nf;

	    q_status_message(1, 2, 4,
			     "\007Can't create composer temp file");
	    editor_result = COMP_CANCEL;
	    nt = cpystr(tobuf); nc = cpystr(ccbuf); nb = cpystr(bccbuf);
	    ns = cpystr(outgoing->subject); nf = cpystr(fccbuf);
	    fs_give((void **)&tobuf); tobuf = nt;
	    fs_give((void **)&ccbuf); ccbuf = nc;
	    fs_give((void **)&bccbuf); bccbuf = nb;
	    fs_give((void **)&outgoing->subject); outgoing->subject = ns;
	    fs_give((void **)&fccbuf); fccbuf = nf;
	}
	else{
	    char *new_to, *new_cc, *new_bcc, *new_sub, *new_fcc;

	    editor_result = run_composer_subprocess(anchor, tobuf, ccbuf,
			bccbuf, outgoing->subject, fccbuf, bodyfile_path,
			pine_flags, &new_to, &new_cc, &new_bcc, &new_sub,
			&new_fcc);

	    fs_give((void **)&tobuf);
	    fs_give((void **)&ccbuf);
	    fs_give((void **)&bccbuf);
	    fs_give((void **)&outgoing->subject);
	    fs_give((void **)&fccbuf);

	    tobuf = new_to;
	    ccbuf = new_cc;
	    bccbuf = new_bcc;
	    outgoing->subject = new_sub;
	    fccbuf = new_fcc;

	    if(reload_body_text(*body, bodyfile_path) < 0)
	      q_status_message(1, 2, 4,
			 "\007Couldn't read back edited message text");
	}

        init_signals();        /* the composer subprocess has its own */
        mark_status_dirty();
	orig_cols = ps_global->ttyo->screen_cols;
        get_windsize(ps_global->ttyo); /* Might have changed, we won't know */
	if(orig_cols != ps_global->ttyo->screen_cols)
	  clear_index_cache();

        clear_cursor_pos();

        /*-------------------------------------------------------------
          Turn strings back into structures
          ----*/
        strings2outgoing(outgoing, body, tobuf, ccbuf, bccbuf);

        fs_give((void **)&tobuf);
        fs_give((void **)&ccbuf);
        fs_give((void **)&bccbuf);

        removing_trailing_white_space(fccbuf);

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
                                     AppendPostpone, fccbuf);

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
                                     fccbuf);
            dprint(1, (debugfile, "Save composition on HUP %s\n",
                       result < 0 ? "FAILED" : "succeeded"));
            hup_signal(SIGHUP); /* Do what we normally do on SIGHUP */

        } else {
            /*------ Must be sending mail ! -----*/

	    if(!outgoing->to && !outgoing->cc && !outgoing->bcc &&
				    (!fccbuf || fccbuf[0] == '\0')) {
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

            if(call_mailer(fccbuf, outgoing, *body)){
		/*
		 * If message successfully sent, we're replying
		 * AND we're allowed to write back state, do it.
		 * But also protect against shifted message message
		 * numbers due to new mail arrival.  Since the number passed
		 * is based on the real imap msg no, AND we're sure no
		 * expunge has been done, just fix up the sorted number...
		 */
		if(are_replying > 0 && !READONLY_FOLDER){
		    char seq[24];	/* room for any long */

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

    fs_give((void **)&fccbuf);
    unlink(bodyfile_path);
    display_message('\0');
}



/*----------------------------------------------------------------------
   Check for addresses the user is not permitted to send to, or probably
   doesn't want to send to
   
Returns: 0 if OK, and 1 if the message shouldn't be sent

Alls queues a message indicating what happened
  ---*/

int
check_addresses(ENVELOPE *e)
{
    ADDRESS *a;

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

    if(!envelope->message_id)		/* just in case! */
      envelope->message_id = cpystr(generate_message_id(ps_global));

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
#define	FCC_SOURCE	CharStar

	fcc_cntxt = ps_global->context_list;

	/*
	 * check for fcc's existance...
	 */
	if(context_isambig(fcc)){
	    int  ok = 1;
	    find_folders_in_context(fcc_cntxt, fcc);
	    if(folder_index(fcc, fcc_cntxt->folders) < 0){
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
	grow_tmp_20k_buf(pine_rfc822_header_size(envelope, body));
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

	so_seek(fcc_so, 0L, 0);
	INIT(&msg, mail_string, (void *)so_text(fcc_so),
	     strlen((char *)so_text(fcc_so)));

	fcc_stream = context_same_stream(fcc_cntxt->context, fcc,
					 ps_global->mail_stream);

	if(!fcc_stream && ps_global->mail_stream != ps_global->inbox_stream)
	  fcc_stream = context_same_stream(fcc_cntxt->context, fcc,
					   ps_global->inbox_stream);

	if(!(rrv = context_append(fcc_cntxt->context, fcc_stream, fcc, &msg))){
	    q_status_message1(0,2,2,"\007Fcc to \"%s\" FAILED!!!", fcc);
            sprintf(error_buf,"\007Error appending %.20s in %.40s", 
		    fcc, fcc_cntxt->context);
            dprint(1, (debugfile, "%s", error_buf));
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

    rv = append_message2(tmpfile, envelope, body, AppendSendmail, NULL);
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
int
call_system(command)
    char *command;
{
    SigType (*hsig)(int), (*isig)(int), (*qsig)(int);
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


/*----------------------------------------------------------------------
    Set up fields for passing to pico.  Assumes first text part is
    intended to be passed along for editing, and is in the form of
    of a storage object brought into existence sometime before pico_send().
 -----*/
void
outgoing2strings(env, bod, to, cc, bcc, text)
    ENVELOPE  *env;
    BODY      *bod;
    char     **to, **cc, **bcc;
    void     **text;
{
    /*
     * SIMPLIFYING ASSUMPTION #37: the first TEXT part's storage object
     * is guaranteed to be of type CharStar!
     */
    if(bod->type == TYPETEXT){
	*text = so_text(bod->contents.binary);
    } else if(bod->type == TYPEMULTIPART){
        if(bod->contents.part->body.type != TYPETEXT) {
            panic1("Can't compose message with first type %s",
                   int2string(bod->contents.part->body.type));
        }
	*text = so_text(bod->contents.part->body.contents.binary);
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
strings2outgoing(env, bod, to, cc, bcc)
    ENVELOPE *env;
    BODY    **bod;
    char     *to, *cc, *bcc;
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
       dprint(9, (debugfile, "to->personal %p\n", env->to->personal));
       dprint(9, (debugfile, "to->route_list %p\n", env->to->adl));
       dprint(9, (debugfile, "to->mailbox %p\n", env->to->mailbox));
       dprint(9, (debugfile, "to->host %p\n", env->to->host));
       dprint(9, (debugfile, "to->next %p\n", env->to->next));
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

    create_message_body(bod);
    pine_encode_body(env, *bod);
}



/*----------------------------------------------------------------------

 The head of the body list here is always either TEXT or MULTIPART.
  ----*/
void
create_message_body(b)
    BODY  **b;
{
    if((*b)->type == TYPETEXT) {
	(*b)->encoding = ENCOTHER;
	set_mime_types(*b);
        return;
    }

    /*-- Now type must be MULTIPART with first part text --*/
    (*b)->contents.part->body.encoding = ENCOTHER;
    set_mime_types(&((*b)->contents.part->body));
}



/*----------------------------------------------------------------------
    Output routine for rfc822_output_body

Normally this is used for writing a message down a TCP stream, but
writing a MIME body out is complex business so we use it here.
We must convert the network CRLF to the local new line convention, in this
case on UNIX it's LF.
 ----*/
int
unix_puts(void *stream_x, char *string)
{
    FILE *stream = (FILE *)stream_x;
    char *p;

    /* Need error checking */
    for(p = string; *p; p++) {
        if(*p == '\015' && *(p+1) == '\012') {
            putc('\n', stream);
            p++;
        } else {
            putc(*p, stream);
        }
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
int
append_message2(folder, env, bod, type, fcc)
    char       *folder, *fcc;
    ENVELOPE   *env;
    BODY       *bod;
    AppendTypes type;
{
    char   filename[MAXPATH+1];
    int    rv, save_errno;
    FILE  *folder_stream;
    long   start_of_append;
#define	OTYPE   "w"
#define	OMODE   0600

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
        if(fcc) {
            rv = fprintf(folder_stream, "Fcc: %s%s", fcc, NEWLINE);
            if (rv == EOF) 
              goto done;
        }
    }

    pine_encode_body(env, bod);

    /*---- Output the header ----*/
    grow_tmp_20k_buf(pine_rfc822_header_size(env, bod));
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
        truncate(filename, start_of_append);
        q_status_message2(1, 2, 4, "\007Error \"%s\" writing \"%s\"",
                          error_description(save_errno), pretty_fn(folder));
        dprint(1, (debugfile, "Error writing %s: %s\n",
                   filename, error_description(save_errno)));
        return(-1);
    }
    else 
      return(0);
}



/*
 * set_mime_types - determine the encoding (and PLAIN/US-ASCII subtype)
 *                  for the outgoing text body
 *
 *		Every caller hands this a TYPETEXT body with encoding
 *		still ENCOTHER -- the composer only ever produces the
 *		user's own typed text, never arbitrary binary content, so
 *		unlike the original multi-format sniffer this no longer
 *		guesses at body->type (removed: magic-number detection for
 *		GIF/JPEG/TIFF/PostScript/.snd/AppleFile/ZIP content, which
 *		dates from when the composer could attach arbitrary files).
 *		This still has to inspect the actual bytes to pick 7BIT vs.
 *		8BIT vs. BINARY encoding, since typed/pasted text can
 *		contain high-bit characters, long lines, or stray NULs.
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

    buf = (unsigned char *)fs_get(RBUFSZ);
    so_seek(so, 0L, 0);

    for(n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
      ;

    buf[n] = '\0';

    p = bol = buf;
    len = n;
    while (n--) {
	if (*p == '\n') {
	    max_line = max(max_line, line_so_far + p - bol);
	    bol = p;
	    line_so_far = 0L;
	}else if (*p == ctrl('O') || *p == ctrl('N') || *p == ESCAPE) {
	    can_be_ascii--;
	}else if (*p & 0x80) {
	    eight_bit_chars++;
	}else if (!*p) {
	    /*
	     * NULL found -- typed text shouldn't have these, but some
	     * external editor imports permit/insert them.  Treat it as
	     * high-bit content so it's at least 8BIT-encoded below.
	     */
	    new_encoding = ENC8BIT;
	    eight_bit_chars++;
	}

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
    }

    if (max_line >= 1000L) { 	/* 1000 comes from rfc821 */
	if (new_encoding != ENCBINARY)
	  new_encoding = ENC8BIT;
    }

    if (eight_bit_chars == 0L) {
	if (new_encoding == ENCOTHER)
	    new_encoding = ENC7BIT;  /* short lines, no 8 bit */
    }
    else if ((eight_bit_chars * 100L)/len < 30L) {
	/*
	 * The 30% threshold is based on qp encoded readability
	 * on non-MIME UA's.
	 */
	can_be_ascii--;

	if (new_encoding != ENCBINARY)
	    new_encoding = ENC8BIT;  /* short lines, < 30% 8 bit chars */
    }else {
	can_be_ascii--;

	/*
	 * Apply maximal encoding regardless of previous
	 * setting.  This segment's unlikely to be readable with
	 * > 30% of the text encoded anyway, so we might as well
	 * save space...
	 */
	new_encoding = ENCBINARY;   /*  > 30% 8 bit chars */
    }

    /* need to set the subtype, and possibly the charset */
    if(body->subtype == NULL) {
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

    fs_give((void **)&buf);
}



/*
 * addr_list_size - conservative upper bound on the RFC822-formatted
 *                  size of an address list, for pine_rfc822_header_size()
 *                  below.  rfc822_cat() may quote/escape personal names,
 *                  so double their length rather than trying to predict
 *                  exactly how much escaping will happen.
 */
static size_t
addr_list_size(ADDRESS *adr)
{
    size_t n = 0;

    for(; adr; adr = adr->next)
      n += 2 * ((adr->personal ? strlen(adr->personal) : 0)
                + (adr->mailbox ? strlen(adr->mailbox) : 0)
                + (adr->host   ? strlen(adr->host)   : 0)
                + (adr->adl    ? strlen(adr->adl)    : 0))
           + 32;    /* "<>", "@", quotes, ", " separator, CRLF fold */

    return(n);
}


/*
 * pine_rfc822_header_size - conservative upper bound on the buffer
 *                  space pine_rfc822_header() will need to format
 *                  env/body into, so callers can grow_tmp_20k_buf()
 *                  before calling it instead of assuming a fixed-size
 *                  buffer is always big enough (env->to/cc/bcc can
 *                  hold arbitrarily many addresses).
 */
size_t
pine_rfc822_header_size(env, body)
    ENVELOPE *env;
    BODY     *body;
{
    size_t n = 512;    /* header labels, MIME-Version, blank lines, slop */

    if(env->remail)      n += strlen(env->remail);
    if(env->date)        n += strlen(env->date) + 16;
    if(env->subject)     n += strlen(env->subject) + 16;
    if(env->in_reply_to) n += strlen(env->in_reply_to) + 16;
    if(env->message_id)  n += strlen(env->message_id) + 16;
    if(env->newsgroups)  n += strlen(env->newsgroups) + 16;

    n += addr_list_size(env->from);
    n += addr_list_size(env->sender);
    n += addr_list_size(env->reply_to);
    n += addr_list_size(env->to);
    n += addr_list_size(env->cc);
    n += addr_list_size(env->bcc);

    if(body) n += 512; /* Content-Type/-Transfer-Encoding/-ID/-Description */

    return(n);
}


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
      sprintf (tmp,"%ld-%ld-%ld:#%ld",gethostid (),(long) random (),time (0),
	       (long) getpid ());
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
 * l_out_buf is a fixed, dedicated buffer -- unlike tmp_20k_buf, its
 * job is purely to batch bytes of the (already-formatted) outgoing
 * message body to cut down on write() calls, so its size is a
 * self-contained performance tradeoff, not a function of message
 * content, and it never needs to grow.
 */
#define L_OUT_BUF_LEN 1024
static char       l_out_buf[L_OUT_BUF_LEN + 2];

/*
 * l_flust_net - empties gf_io terminal function's buffer
 */
void
l_flush_net(void)
{
    if(c_in_buf){
	if(l_f)
	  (*l_f)(l_stream, l_out_buf);

	if(fcc_so)
	  so_puts(fcc_so, l_out_buf);
    }

    c_in_buf = 0;
}


/*
 * l_putc - gf_io terminal function that calls the soutr_t output function
 *          (e.g. unix_puts for call_sendmail).
 *
 */
int
l_putc(int c)
{
    l_out_buf[c_in_buf++] = (char) c;
    l_out_buf[c_in_buf]   = '\0';

    if(c_in_buf > L_OUT_BUF_LEN){
	if(fcc_so)
	  so_puts(fcc_so, l_out_buf);

        c_in_buf = 0;
        return(l_f ? (int)(*l_f)(l_stream, l_out_buf) : 1);
    }
    else
	return(TRUE);
}


/*
 * pine_body_header_size - conservative upper bound on the buffer space
 *                  pine_write_body_header() will need to format a single
 *                  MIME part's mini-header, so callers can fs_get() a
 *                  buffer sized to fit instead of assuming a fixed-size
 *                  one is always big enough (subtype/parameter values/
 *                  id/description can all be arbitrarily long -- see
 *                  the caller, pine_rfc822_output_body()).
 */
static size_t
pine_body_header_size(BODY *body)
{
    size_t n = 128;    /* Content-Type/-Transfer-Encoding labels, CRLFs, slop */
    PARAMETER *param;

    n += strlen(body_types[body->type]);
    n += body->subtype ? strlen(body->subtype)
                        : strlen(rfc822_default_subtype(body->type));
    for(param = body->parameter; param; param = param->next)
      /* rfc822_cat() can quote/backslash-escape every character of
	 param->value, up to doubling its length; value can be NULL
	 (see pine_write_body_header()'s matching guard) */
      n += strlen(param->attribute) +
	   2 * (param->value ? strlen(param->value) : 0) + 8;
    if(body->id)          n += strlen(body->id) + 32;
    if(body->description) n += strlen(body->description) + 32;

    return(n);
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
    char *tmp;
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
	 *
	 * f is NULL when there's no real recipient and this is only
	 * being written to the Fcc copy (pine_rfc822_output(), called
	 * with f=NULL,s=NULL) -- every other callback invocation in
	 * this function already guards against that with "f ? ... : 1",
	 * this one didn't and called through a NULL function pointer.
	 */
	if(f && !(*f)(s, "  This message is in MIME format.  The first part should be readable text,\015\012  while the remaining parts are likely unreadable without MIME-aware tools.\015\012\015\012"))
	  return(0);

	do {				/* for each part */
					/* build cookie */
	    /*
	     * cookie/subtype/parameters/id/description all come
	     * straight from parsing a message (possibly received,
	     * then forwarded/copied into this outgoing part) with no
	     * length cap anywhere upstream -- a fixed MAILTMPLEN
	     * buffer here overflowed on a long enough part. Size to
	     * fit instead.
	     */
	    tmp = (char *) fs_get(strlen(cookie) +
				   pine_body_header_size(&part->body) + 16);
	    sprintf (t = tmp,"--%s\015\012",cookie);
					/* append mini-header */
	    pine_write_body_header (&t,&part->body);
				/* output cookie, mini-header, and contents */
	    if(fcc_so)
	      so_puts(fcc_so, tmp);

	    if (!((f ? (*f) (s,tmp) : 1)
		  && pine_rfc822_output_body (&part->body,f,s))) {
		fs_give((void **) &tmp);
		return(0);
	    }
	    fs_give((void **) &tmp);
	} while (part = part->next);	/* until done */
					/* output trailing cookie */
	tmp = (char *) fs_get(strlen(cookie) + 16);
	sprintf (t = tmp,"--%s--",cookie);
	if(fcc_so){
	    so_puts(fcc_so, t);
	    so_puts(fcc_so, "\015\012");
	}

	{
	    long rv = f ? ((*f) (s,t) && (*f) (s,"\015\012")) : 1;
	    fs_give((void **) &tmp);
	    return(rv);
	}
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
	/* param->value can be NULL (an empty "attr=;" parameter value
	   parses to no value at all, mail_newbody_parameter()'s
	   default) -- rfc822_cat() has no NULL check of its own */
	rfc822_cat (*dst,param->value ? param->value : "",tspecials);
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
