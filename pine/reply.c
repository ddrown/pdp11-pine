#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: reply.c,v 4.44 1993/10/08 23:21:57 mikes Exp $";
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
    reply.c
   
   Code here for forward and reply to mail
   A few support routines as well

  This code will forward and reply to MIME messages. The Pine composer
at this time will only support non-text segments at the end of a
message so, things don't always come out as one would like. If you
always forward a message in MIME format, all will be correct. This
code also has a problem forwarding nested MULTIPART messages. If an
item of TYPEMESSAGE is in between nestings all will be well. This is
due to limitation in the c-client. This code also will result in messages 
that have a text segment as the first part. Lastly, this is all new ground
since I've never seen another implementation of this, so there's probably
lots of things that could be done better that will evolve over time.

  =====*/


#include "headers.h"
#include <time.h>


char          *generate_reply_to(ENVELOPE *);
ENVELOPE      *copy_envelope(ENVELOPE *);
BODY          *copy_body(BODY *, BODY *);
int            fetch_contents(MAILSTREAM *, long, BODY *, int, int);
void           format_text_header(ENVELOPE *, gf_io_t, char *);
int            get_body_part_text(BODY *,long,char *,gf_io_t,char *,char *);
char          *reply_delimiter(ENVELOPE *);
char          *forward_delimiter(void);


/*----------------------------------------------------------------------
        Fill in an outgoing message for reply and pass off to send

    Args: pine_state -- The usual pine structure
          message    -- The MESSAGECACHE of entry to reply to 

  Result: Reply is formated and passed off to composer/mailer

Reply

   - put senders address in To field
   - search to and cc fields to see if we aren't the only recepients
   - if other than us, ask if we should reply to all.
   - if answer yes, fill out the To and Cc fields
   - fill in the subject field
   - fill out the body and the attachments
   - pass off to pine_send()
  ---*/
void
reply(pine_state, msgno, env, body)
     struct pine *pine_state;
     long         msgno;
     ENVELOPE     *env;
     BODY	  *body;
{
    char       *sig, *tp;
    ADDRESS    *a, *a_tail, *t1, *t2;
    ENVELOPE   *outgoing;
    BODY       *orig_body;
    PART       *part;
    void       *msgtext;
    gf_io_t     pc;
    char       *prefix;
    int 	include_text;
    int         ret;


    /*---------- Get the envelope of message we're replying to ------*/
    if(env == NULL) {
        q_status_message1(1, 2,4,
                          "Error fetching message %s. Can't reply to it.",
			  long2string(msgno));
	return;
    }
    outgoing     = mail_newenvelope();
    /*
     * For consistency, the first question is always "include text?"
     */
    ret = want_to("Include original message in Reply", 'n', 'x', NO_HELP,0);
    if(ret == 'x') {
        q_status_message(1, 2, 4,"\007Message cancelled");
	goto done_early;
    }
    include_text = ret == 'y';

    /*==================== To and Cc fields =====================*/
    /* Always include the originator of the message */
    if((t1 = first_addr(env->reply_to)) && (t2 = first_addr(env->from))
	&& (strucmp(t1->mailbox, t2->mailbox) || strucmp(t1->host, t2->host))
	&& (ret=want_to(
		 "Use \"Reply to:\" address instead of \"From:\" address",
		 'y', 'x', NO_HELP, 0)) == 'y') {
        outgoing->to = rfc822_cpy_adr(env->reply_to);
    } else {
        outgoing->to = rfc822_cpy_adr(env->from);
    }
    if(ret == 'x') {
        q_status_message(1, 2, 4,"\007Message cancelled");
	goto done_early;
    }


    /*------ check to see if we aren't the only recepient -----------*/
    for(a = env->to; a != NULL; a = a->next) {
	if(!address_is_us(a, pine_state))
	  break;
    }
    if(a == NULL)
      for(a = env->cc; a != NULL; a = a->next) {
	  if(!address_is_us(a, pine_state))
	    break;
      }
    /*------ Ask user and possibly include all other recepients ---*/ 
    outgoing->cc = (ADDRESS *)NULL;
    if(a != NULL &&
         (ret=want_to("Reply to all recipients", 'n', 'x', NO_HELP, 0)) =='y'){
	if(env->to != NULL)
          outgoing->cc = rfc822_cpy_adr(env->to);
	if(env->cc != NULL) {
            if(outgoing->cc == NULL) {
                outgoing->cc = rfc822_cpy_adr(env->cc);
            } else {
                for(a = outgoing->cc; a->next != NULL; a = a->next);
                a->next = rfc822_cpy_adr(env->cc);
            }
        }
    }
    if(ret == 'x') {
        q_status_message(1, 2, 4,"\007Message cancelled");
	goto done_early;
    }
    for(a = a_tail = outgoing->cc; a != NULL; ){
        if(address_is_us(a, pine_state)){ /*-- eliminate sender from list ---*/
            if(a == outgoing->cc){
                a = outgoing->cc = a->next;
                a_tail->next = NULL;  /* so we only free one address */
                mail_free_address(&a_tail);
                a_tail = a;
            } else {
                a_tail->next = a->next;
                a->next = NULL; /* so we only free one address */
                mail_free_address(&a);
                a = a_tail->next;
            }
        } else {
            a_tail = a;
            a = a->next;
        }
    }
    /*----- eliminate us from to, unless thats all there is ------*/
    if(outgoing->to != NULL && address_is_us(outgoing->to, pine_state)) {
        if(outgoing->cc != NULL) {
            mail_free_address(&(outgoing->to));
            outgoing->to = outgoing->cc;
            outgoing->cc = NULL;
        }
    }


    /*------------ Format the subject line ---------------*/
    if(env->subject == NULL || strlen(env->subject) == 0) {
	/*--- blank : make up our own ---*/        
        outgoing->subject = cpystr("Re: your mail");
    } else {
	/* --- already got "Re:", just pass it along ---*/
	if((isupper(env->subject[0])? tolower(env->subject[0]):env->subject[0])
            == 'r' &&
	   (isupper(env->subject[1])? tolower(env->subject[1]):env->subject[1])
            == 'e' &&
           env->subject[2] == ':'){
	    outgoing->subject = cpystr(env->subject);
	} else {
	    /*--- plan : add and "Re: " ----*/
            outgoing->subject = fs_get(strlen(env->subject) + 6);
	    strcpy(outgoing->subject, "Re: ");
	    strcat(outgoing->subject, env->subject);
	}
    }


    /*==========  Other miscelaneous fields ===================*/   
    outgoing->return_path = (ADDRESS *)NULL;
    outgoing->bcc         = (ADDRESS *)NULL;
    outgoing->sender      = (ADDRESS *)NULL;
    outgoing->return_path = (ADDRESS *)NULL;
    outgoing->in_reply_to = cpystr(generate_reply_to(env));
    outgoing->remail      = NULL;
    outgoing->reply_to    = (ADDRESS *)NULL;

    outgoing->message_id  = cpystr(generate_message_id(pine_state));

   /*==================== Now fix up the message body ====================*/

    sig       = get_signature();
    orig_body = body;
    prefix    = "> ";

    /* 
     * create storage object to be used for message text
     */
    if((msgtext = (void *)so_get(CharStar, NULL, EDIT_ACCESS)) == NULL){
        q_status_message(1, 2, 4,"\007Error allocating message text");
	fs_give((void **)&sig);
        goto done_early;
    }

    gf_set_so_writec(&pc, (STORE_S *)msgtext);

    /*---- Include the origina text if requested ----*/
    if(include_text){
	/* write preliminarly envelope info into message text */
	if(sig[0]){
	    so_puts((STORE_S *)msgtext, sig);
	    fs_give((void **)&sig);
	}
	else
	  so_puts((STORE_S *)msgtext, NEWLINE);

	so_puts((STORE_S *)msgtext, NEWLINE);

        if(orig_body == NULL || orig_body->type == TYPETEXT) {

            /*------ Simple text-only message ----*/
            body                  = mail_newbody();
            body->type            = TYPETEXT;
	    body->contents.binary = msgtext;
	    so_puts((STORE_S *)msgtext, reply_delimiter(env));

	    get_body_part_text(orig_body, msgno, "1", pc,
				prefix, ". Text not included");
        } else if(orig_body->type == TYPEMULTIPART) {

            /*------ Message is Multipart ------*/
            /*-- BUG -- need to do a better job for MULTIPART/alternate --*/
            body = copy_body(NULL, orig_body);

            if(orig_body->contents.part != NULL &&
               orig_body->contents.part->body.type == TYPETEXT) {
                /*---- First part of the message is text -----*/
		body->contents.part->body.contents.binary = msgtext;
/* BUG: ? matter that we're not setting body.size.bytes */
		so_puts((STORE_S *)msgtext, reply_delimiter(env));

		get_body_part_text(&orig_body->contents.part->body,
				   msgno, "1", pc, prefix,
				   ". Text not included");
                if(fetch_contents(pine_state->mail_stream, msgno, body, 0, 2)) 
                  q_status_message(1, 2, 4,
                                  "\007Error including all message parts");
            } else {
                /*--- No text part, create a blank one ---*/
                part                     = mail_newbody_part();
                part->next               = body->contents.part;
                body->contents.part      = part;
                part->body.contents.binary = msgtext;
/* BUG: ? matter that we're not setting body.size.bytes */
                if(fetch_contents(pine_state->mail_stream, msgno, body, 1, 2)) 
                  q_status_message(1, 2, 4,
                                   "\007Error including all message parts");
            }
        } else {
            /*---- Single non-text message of some sort ----*/
            body                     = mail_newbody();
            body->type               = TYPEMULTIPART;
            part                     = mail_newbody_part();
            body->contents.part      = part;
    
            /*--- The first part, a blank text part to be edited ---*/
            part->body.type          = TYPETEXT;
            part->body.contents.binary = msgtext;
/* BUG: ? matter that we're not setting body.size.bytes */

            /*--- The second part, what ever it is ---*/
            part->next               = mail_newbody_part();
            part                     = part->next;
            part->body.id            = cpystr(generate_message_id(ps_global));
            copy_body(&(part->body), orig_body);
	    /*
	     * the idea here is to fetch part into storage object
	     */
	    if(part->body.contents.binary = (void *) so_get(CharStar, NULL,
							    EDIT_ACCESS)){
		if((tp=mail_fetchbody(pine_state->mail_stream, msgno, "1",
					&part->body.size.bytes)) == NULL)
		  goto done;
		so_puts((STORE_S *)part->body.contents.binary, tp);
	    }
	    else
	      goto done;
        }
    } else {
        /*--------- No text included --------*/
        body                  = mail_newbody();
        body->type            = TYPETEXT;
	body->contents.binary = msgtext;
    }

    if(sig){
	so_puts(msgtext, sig);
	fs_give((void **)&sig);
    }

    pine_send(outgoing,      /* partially formatted outgoing message */
              &body,
              "COMPOSE MESSAGE REPLY",
              NULL,
              msgno);

  done:
    pine_free_body(&body);
  done_early:
    mail_free_envelope(&outgoing);
}


/*----------------------------------------------------------------------
    Format the header of the text for inclusion in a reply or forward

Args:  env       -- Envelope, must not be NULL
       so        -- storage object collecing reply text
       prefix    -- string to be placed before each line

The parts in this part of the reply are:
  - date
  - subject 
  - from
  - to
  - cc
  ---*/
void
format_text_header(env, pc, prefix)
    ENVELOPE *env;
    gf_io_t   pc;
    char     *prefix;
{
    char    *pfx = (prefix) ? prefix : "";
    char    *p;

    /*---- The Date: field -----*/
    if(env && env->date){
	sprintf(tmp_20k_buf, "%sDate: %s%s", pfx, env->date, NEWLINE);
	gf_puts(tmp_20k_buf, pc);
    }

    /*---- The From: field ----*/
    tmp_20k_buf[0] = '\0';
    if (!env->from) {
	sprintf(tmp_20k_buf,"%sFrom:     (Sender of message unknown)%s",
		  pfx, NEWLINE);
    } else if(env->from->personal) {
        sprintf(tmp_20k_buf, "%sFrom: %s <%s%s%s>%s",pfx,
                    env->from->personal, env->from->mailbox,
                    env->from->host != NULL ? "@" : "",
                    env->from->host != NULL ? env->from->host : "", NEWLINE);
    } else {
        sprintf(tmp_20k_buf, "%sFrom:%s%s%s%s", pfx, env->from->mailbox,
                    env->from->host != NULL ? "@" : "",
                    env->from->host != NULL ? env->from->host : "",
		    NEWLINE);
    }

    gf_puts(tmp_20k_buf, pc);

    /*-------- The To: field --------*/
    if(env->to)
      pretty_addr_string("To: ",env->to, pfx, pc);

    /*-------- The Cc: field -----*/
    if(env->cc)
      pretty_addr_string("Cc: ",env->cc, pfx, pc);

    /*-------- The subject line ----*/
    sprintf(tmp_20k_buf, "%sSubject: %s%s", pfx,
                             env->subject == NULL ? "" : env->subject,
			     NEWLINE);
    gf_puts(tmp_20k_buf, pc);
    gf_puts(pfx, pc);		/* prefix followed by */
    gf_puts(NEWLINE, pc);	/* blank separater!   */
}


/*
 * reply_delimeter - return properly formatted delimiter for
 *                   reply text
 */
char *
reply_delimiter(env)
    ENVELOPE *env;
{
    struct date d;

    parse_date(env->date, &d);
    if(env->from == NULL) {
        sprintf(tmp_20k_buf, "On %s%s%d %s %d, it was written:",
                d.wkday == -1 ? "" : week_abbrev(d.wkday),
                d.wkday == -1 ? "" : ", ",
                d.day, month_abbrev(d.month), d.year);
    } else if (env->from->personal != NULL) {
        sprintf(tmp_20k_buf, "On %s%s%d %s %d, %s wrote:",
                d.wkday == -1 ? "" : week_abbrev(d.wkday),
                d.wkday == -1 ? "" : ", ",
                d.day, month_abbrev(d.month), d.year,
                env->from->personal);
    } else {
        sprintf(tmp_20k_buf, "On %s%s%d %s %d %s%s%s wrote:",
                d.wkday == -1 ? "" : week_abbrev(d.wkday),
                d.wkday == -1 ? "" : ", ",
                d.day, month_abbrev(d.month), d.year,
                env->from->mailbox,
                env->from->host != NULL ? "@" : "", env->from->host);
    }

    strcat(tmp_20k_buf, NEWLINE); 	/* and end with two newlines */
    strcat(tmp_20k_buf, NEWLINE);
    return(tmp_20k_buf);
}


/*
 * forward_delimiter - return delimiter for forwarded text
 */
char *
forward_delimiter()
{
    char *banner ="---------- Forwarded message ----------";
    return(banner);
}


/*----------------------------------------------------------------------
       Partially set up message to forward and pass off to composer/mailer

    Args: pine_state -- The usual pine structure
          message    -- The MESSAGECACHE of entry to reply to 

  Result: outgoing envelope and body created and passed off to composer/mailer

   Create the outgoing envelope for the mail being forwarded, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
void
forward(pine_state, msgno, env, body)
     struct pine   *pine_state;
     long	   msgno;
     ENVELOPE	   *env;
     BODY          *body;
{
    ENVELOPE      *outgoing;
    BODY          *orig_body, *text_body;
    char          *tmp_text, *tp;
    PART          *part;
    MAILSTREAM    *stream;
    void          *msgtext;
    gf_io_t        pc;
    char          *sig;


    stream                = pine_state->mail_stream;
    outgoing              = mail_newenvelope();
    outgoing->message_id  = cpystr(generate_message_id(pine_state));


    /*==================== Subject ====================*/
    if(env->subject == NULL || strlen(env->subject) == 0) {
	/* --- Blank, make up one ---*/
	outgoing->subject = cpystr("Forwarded mail....");
    } else {
        outgoing->subject = fs_get(strlen(env->subject) + strlen(" (fwd)")+ 1);
	strcpy(outgoing->subject, env->subject);
        removing_trailing_white_space(outgoing->subject);
	/* this next strange compare is to see if the last few chars are
	     already '(fwd)' before we tack another on */
	if (strlen(outgoing->subject) < 6 ||
                  (strcmp(outgoing->subject + strlen(outgoing->subject) - 5,
                      "(fwd)") != 0))
	    strcat(outgoing->subject, " (fwd)");
    }


    /*
     * as with all text bound for the composer, build it in 
     * a storage object of the type it understands...
     */
    if((msgtext = (void *)so_get(CharStar, NULL, EDIT_ACCESS)) == NULL){
	q_status_message(1, 2, 4,"\007Error allocating message text");
	goto clean_early;
    }

    orig_body = body;
    sig       = get_signature();
    so_puts((STORE_S *)msgtext, sig[0] ? sig : NEWLINE);
    fs_give((void **)&sig);
    gf_set_so_writec(&pc, (STORE_S *)msgtext);

        if(orig_body == NULL || orig_body->type == TYPETEXT) {
            /*---- Message has a single text part -----*/
            body                  = mail_newbody();
            body->type            = TYPETEXT;
            body->contents.binary = msgtext;
	    so_puts((STORE_S *)msgtext, NEWLINE);
	    so_puts((STORE_S *)msgtext, forward_delimiter());
	    so_puts((STORE_S *)msgtext, NEWLINE);
	    format_text_header(env, pc, "");
	    get_body_part_text(orig_body, msgno, "1", pc,
				    "", ". Text not included");
/* BUG: ? matter that we're not setting body.size.bytes */
        } else if(orig_body->type == TYPEMULTIPART) {
            /*---- Message is multipart ----*/
    
            /*--- Copy the body and entire structure  ---*/
            body = copy_body(NULL, orig_body);
    
            /*--- The text part of the message ---*/
            if(orig_body->contents.part->body.type == TYPETEXT) {
                /*--- The first part is text ----*/
                text_body                  = &body->contents.part->body;
		text_body->contents.binary = msgtext;
		so_puts((STORE_S *)msgtext, NEWLINE);
		so_puts((STORE_S *)msgtext, forward_delimiter());
		so_puts((STORE_S *)msgtext, NEWLINE);
		format_text_header(env, pc, "");
		get_body_part_text(&orig_body->contents.part->body,
				   msgno, "1", pc, "", ". Text not included");
/* BUG: ? matter that we're not setting body.size.bytes */
                if(fetch_contents(stream, msgno, body, 0, 2))
                  goto bomb;
            } else {
                /*--- Create a new blank text part ---*/
                part                       = mail_newbody_part();
                part->next                 = body->contents.part;
                body->contents.part        = part;
                part->body.contents.binary = msgtext;
                if(fetch_contents(stream, msgno, body, 1, 2))
                  goto bomb;
            }
    
        } else {
            /*---- A single part message, not of type text ----*/
            body                     = mail_newbody();
            body->type               = TYPEMULTIPART;
            part                     = mail_newbody_part();
            body->contents.part      = part;
    
            /*--- The first part, a blank text part to be edited ---*/
            part->body.type            = TYPETEXT;
            part->body.contents.binary = msgtext;
    
            /*--- The second part, what ever it is ---*/
            part->next               = mail_newbody_part();
            part                     = part->next;
            part->body.id            = cpystr(generate_message_id(ps_global));
            copy_body(&(part->body), orig_body);
	    /*
	     * the idea here is to fetch part into storage object
	     */
	    if(part->body.contents.binary = (void *) so_get(CharStar, NULL,
							    EDIT_ACCESS)){
		if((tp=mail_fetchbody(stream, msgno, "1",
					&part->body.size.bytes)) == NULL)
		  goto bomb;
		so_puts((STORE_S *)part->body.contents.binary, tp);
	    }
	    else
	      goto bomb;
        }

    pine_send(outgoing,      /* partially formatted outgoing message */
		&body,
		"FORWARD MESSAGE",
		NULL,
		0L);

  clean:
    pine_free_body(&body);
  clean_early:
    mail_free_envelope(&outgoing);
    return;

  bomb:
    q_status_message(1, 4, 5,
           "\007Error fetching message contents. Can't forwarded message");
    goto clean;

}


        
/*----------------------------------------------------------------------
    Fetch and format text for forwarding

Args:  stream      -- Mail stream to fetch text for
       message_no  -- Message number of text for foward
       part_number -- Part number of text to forward
       env         -- Envelope of message being forwarded
       body        -- Body structure of message being forwarded

Returns:  true if OK, false if problem occured while filtering

If the text is richtext, it will be converted to plain text, since there's
no rich text editing capabilities in Pine (yet). The character sets aren't
really handled correctly here. Theoretically editing should not be allowed
if text character set doesn't match what term-character-set is set to.

It's up to calling routines to plug in signature appropriately

As with all internal text, NVT end-of-line conventions are observed.
DOESN'T sanity check the prefix given!!!
  ----*/
int
get_body_part_text(body, msg_no, part_no, pc, prefix, hmm)
    BODY       *body;
    long        msg_no;
    char       *part_no;
    gf_io_t     pc;
    char       *prefix;
    char       *hmm; 
{
    int            i;
    filter_t	   filters[3];
    long	   len;
    char	  *err;

    /* if null body, we must be talking to a non-IMAP2bis server.
     * No MIME parsing provided, so we just grab the message text...
     */
    if(body == NULL){
	char         *text, *decode_error;
	gf_io_t       gc;
	SourceType    src = CharStar;
	int           rv = 0;

	(void)mail_fetchstructure(ps_global->mail_stream, msg_no, NULL);

	if(text = mail_fetchtext(ps_global->mail_stream, msg_no)){
	    gf_set_readc(&gc,text,(src == FileStar) ? 0L : (unsigned long)strlen(text),src);
	    gf_filter_init();
	    /*
	     * Following the same canonical model detach() (mailcmd.c)
	     * uses: convert newlines from crlf to local convention
	     * before this reaches the (now CharStar, not PicoText)
	     * message-text storage object -- see TODO-pdp11-done.md.
	     */
	    gf_link_filter(gf_nvtnl_local);
	    if(decode_error = gf_pipe(gc, pc)){
		sprintf(tmp_20k_buf, "%s%s    [Formatting error: %s]%s",
			NEWLINE, NEWLINE,
			decode_error, NEWLINE);
		gf_puts(tmp_20k_buf, pc);
		rv++;
	    }
	}
	else{
	    gf_puts(NEWLINE, pc);
	    gf_puts("    [ERROR fetching text of message]", pc);
	    gf_puts(NEWLINE, pc);
	    gf_puts(NEWLINE, pc);
	    rv++;
	}

	return(rv == 0);
    }

    filters[i = 0] = NULL;

    /*
     * just use detach, but add an auxiliary filter to insert prefix,
     * and, perhaps, digest richtext
     */
    if(body->subtype != NULL && strucmp(body->subtype,"richtext") == 0){
	gf_rich2plain_opt(1);		/* set up to filter richtext */
	filters[i++] = gf_rich2plain;
    }

/* BUG: is msgno stuff working right ??? */
/* BUG: not using "hmm" !!! */
    gf_prefix_opt(prefix);
    filters[i++] = gf_prefix;
    filters[i++] = NULL;
    err = detach(msg_no, body, part_no, &len, pc, filters);
    if (err != (char *)NULL)
       q_status_message2(1, 2, 4, "%s: message number %ld",err,(void *)msg_no);
    return((int)len);
}



/*----------------------------------------------------------------------
     Get a text body part and decoded it. Used for including text when 
   forwarding or replying

Args: stream     -- The mail stream the body part to fetch is on
      message_no -- The message number for the body part
      part_number-- The actual part number in the message
      body       -- The body structure for the message to know encoding
      is_malloced-- Indicates if part returned is a malloced buffer or not
      hmm        -- Add this to any error messages output

The null terminated fetched text is returned. This function may not work
well if the contents aren't text. The text will be decoded from what ever
encoding it was in, and it will be converted from rich text to plain text.
If there is an error fetching or decoding the text a message will be
printed, and NULL returned. If body it NULL, then plain text is assumed
(non-IMAP2bis server).

The text returned here will have new lines terminated by CRLF!
  ----*/

/*----------------------------------------------------------------------
   Fill in the contents of each body part

Args: stream      -- Stream the message is on
      msgno       -- Message number the body structure is for
      body        -- Body the to fill in
      part_offset -- Part offset between the orignal and the new message
      start_part  -- The part in the new body 'body' to start with

Result: 0 if all went OK, 1 if nothing could be fetched

This function copies the contents from an original message/body to
a new message/body. The the part_offset, is the number of parts 
in the new body that have been added before the original parts from
the original message.

If one or more part (but not all), a status message will be queued.
 ----*/

int
fetch_contents(stream, msgno, body, part_offset, start_part)
     MAILSTREAM *stream;
     long        msgno;
     BODY       *body;
     int         start_part, part_offset;
{
    char part_string[20], e_mess[200], *e, *tp;
    int  part_count, nested_multipart, error_part[50], *ep, got_one;
    PART *part, *ppart;

    nested_multipart = 0;
    got_one          = 0;
    ep               = error_part;
    *ep              = 0;

    for(ppart = part = body->contents.part, part_count = 1;
        part != NULL;
        ppart = part, part = part->next, part_count++){

        if(part_count < start_part)
          continue;

        if(part->body.type == TYPEMULTIPART) {
            if(!nested_multipart) {
                q_status_message(1, 2, 4,
                                 "\007Can't include nested multipart sections. They will be omitted");
                nested_multipart = 1;
                
            }
            ppart->next = part->next;
            part = ppart;
            continue;
        }

        if(part->body.id == NULL)
          part->body.id = cpystr(generate_message_id(ps_global));
          
        sprintf(part_string, "%d", part_count - part_offset);
        if(part->body.type == TYPEMESSAGE) {
            part->body.contents.msg.env = NULL;
            part->body.contents.msg.body = NULL;
            if(part->body.subtype != NULL &&
		strucmp(part->body.subtype,"external-body") != 0) {
		/*
		 * the idea here is to fetch everything into storage objects
		 */
		part->body.contents.binary = (void *) so_get(CharStar, NULL,
							     EDIT_ACCESS);
		if(part->body.contents.binary == NULL
		    || (tp = mail_fetchbody(stream, msgno, part_string,
					&part->body.size.bytes)) == NULL){
		    *ep++ = part_count;
		}
		else {
		    so_puts((STORE_S *)part->body.contents.binary, tp);
		    got_one = 1;
		}
            } else {
                got_one = 1;
            }
        } else {
	    /*
	     * the idea here is to fetch everything into storage objects
	     * so, grab one, then fetch the body part
	     */
            part->body.contents.binary = (void *) so_get(CharStar, NULL,
							 EDIT_ACCESS);
            if(part->body.contents.binary == NULL
                || (tp=mail_fetchbody(stream, msgno, part_string,
                                  &part->body.size.bytes)) == NULL){
		*ep++ = part_count;
	    }
	    else {
		so_puts((STORE_S *)part->body.contents.binary, tp);
		got_one = 1;
	    }
        }
    }

    if(!got_one)  /* Didn't get any message parts, bomb out of this */
      return(1);

    if(ep != error_part) {
        /* Error fetching one or more parts */
        *ep = 0;
        sprintf(e_mess, "Error fetching message parts");
        e = e_mess + strlen(e_mess);
        for(ep = error_part; *ep != 0; ep++){
            sprintf(e, " %d,", *ep);
            e += strlen(e);
        }
        e--; /* Kill last comma */
        *e = '\0';
        
        q_status_message(1, 3, 5, e_mess);
    }
    return(0);
}



/*----------------------------------------------------------------------
    Copy the body structure

Args: new_body -- Pointer to already allocated body, or NULL, if none
      old_body -- The Body to copy


 This is called recursively traverses the body structure copying all the
elements. The new_body parameter can be NULL in which case a new body is
allocated. Alternatively it can point to an already allocated body
structure. This is used when copying body parts since a PART includes a 
BODY. The contents fields are *not* filled in.
  ----*/

BODY *
copy_body(new_body, old_body)
     BODY *old_body, *new_body;
{
    PART *new_part, *old_part;

    if(old_body == NULL)
      return(NULL);

    if(new_body == NULL)
      new_body = mail_newbody();

    *new_body = *old_body;
    if(old_body->id)
      new_body->id = cpystr(old_body->id);

    if(old_body->description)
      new_body->description = cpystr(old_body->description);

    if(old_body->subtype)
      new_body->subtype = cpystr(old_body->subtype);

    new_body->parameter = copy_parameters(old_body->parameter);

    new_part = NULL;
    if(new_body->type == TYPEMULTIPART) {
        for(old_part = new_body->contents.part; old_part != NULL;
            old_part = old_part->next){
            if(new_part == NULL) {
                new_part = mail_newbody_part();
                new_body->contents.part = new_part;
            } else {
                new_part->next = mail_newbody_part();
                new_part = new_part->next;
            }
            copy_body(&(new_part->body), &(old_part->body));
        }
    } else if(new_body->type == TYPETEXT) {
        new_body->contents.text = NULL;
    }
    return(new_body);
}



/*----------------------------------------------------------------------
    Copy the MIME parameter list
 
 Allocates storage for new part, and returns pointer to new paramter
list. If old_p is NULL, NULL is returned.
 ----*/

PARAMETER *
copy_parameters(old_p)
     PARAMETER *old_p;
{
    PARAMETER *new_p, *p1, *p2;

    if(old_p == NULL)
      return((PARAMETER *)NULL);

    new_p = p2 = NULL;
    for(p1 = old_p; p1 != NULL; p1 = p1->next) {
        if(new_p == NULL) {
            p2 = mail_newbody_parameter();
            new_p = p2;
        } else {
            p2->next = mail_newbody_parameter();
            p2 = p2->next;
        }
        p2->attribute = cpystr(p1->attribute);
        p2->value     = cpystr(p1->value);
    }
    return(new_p);
}
    
    

/*----------------------------------------------------------------------
    Make a complete copy of an envelope and all it's fields

Args:    e -- the envelope to copy

Result:  returns the new envelope, or NULL, if the given envelope was NULL

  ----*/

ENVELOPE *    
copy_envelope(e)
     register ENVELOPE *e;
{
    register ENVELOPE *e2;

    if(e == NULL)
      return(NULL);

    e2              = mail_newenvelope();
    e2->remail      = (e->remail      == NULL ? NULL : cpystr(e->remail));
    e2->return_path = (e->return_path == NULL ? NULL :
                                              rfc822_cpy_adr(e->return_path));
    e2->date        = (e->date        == NULL ? NULL : cpystr(e->date));
    e2->from        = (e->from        == NULL ? NULL :
                                              rfc822_cpy_adr(e->from));
    e2->sender      = (e->sender      == NULL ? NULL :
                                              rfc822_cpy_adr(e->sender));
    e2->reply_to    = (e->reply_to    == NULL ? NULL :
                                              rfc822_cpy_adr(e->reply_to));
    e2->subject     = (e->subject     == NULL ? NULL : cpystr(e->subject));
    e2->to          = (e->to          == NULL ? NULL : rfc822_cpy_adr(e->to));
    e2->cc          = (e->cc          == NULL ? NULL : rfc822_cpy_adr(e->cc));
    e2->bcc         = (e->bcc         == NULL ? NULL : rfc822_cpy_adr(e->bcc));
    e2->in_reply_to = (e->in_reply_to == NULL ? NULL : cpystr(e->in_reply_to));
    e2->message_id  = (e->message_id  == NULL ? NULL : cpystr(e->message_id));
    return(e2);
}






/*----------------------------------------------------------------------
     Generate the "In-reply-to" text from message header

  Args: message -- MESSAGECACHE entry

  Result: returns the string or NULL if there is a problem
 ----*/
char *
generate_reply_to(env)
     ENVELOPE *env;
{
    static char buffer[128];
    
    if (!env || !env->message_id)
        return(NULL);
    else
      sprintf(buffer, "%.100s",  env->message_id);

    return(buffer);
}



/*----------------------------------------------------------------------
        Generate a unique message id string.

   Args: ps -- The usual pine structure

  Result: Unique string is returned

Uniqueness is gaurenteed by using the host name, process id, date to the
second and a single unique character
*----------------------------------------------------------------------*/
char *
generate_message_id(ps)
     struct pine *ps;
{
    static char id[150], a = 'A';	/* room for a large pid/year */
    long        now;
    struct tm  *now_x;
    extern char version_buff[];

    now   = time(0);
    now_x = localtime(&now);

    sprintf(id,"<Pine.%.20s.%02d%02d%02d%02d%02d.%c%d@%.50s>",
            version_buff,now_x->tm_year, now_x->tm_mon + 1, now_x->tm_mday,
            now_x->tm_hour, now_x->tm_sec, a, getpid(), ps->hostname);

    if(a == 'Z')
      a = 'a';
    else if(a == 'z')
      a = 'A';
    else
      a++;

    return(id);
}



/*----------------------------------------------------------------------
  Return the first true address pointer (modulo group syntax allowance)

  Args: addr  -- Address list

 Result: First real address pointer, or NULL
  ----------------------------------------------------------------------*/
ADDRESS *
first_addr(addr)
    ADDRESS *addr;
{
    while(addr && !addr->host)
      addr = addr->next;

    return(addr);
}



/*----------------------------------------------------------------------
     Format an address field, wrapping lines nicely at commas

  Args: field_name  -- The name of the field we're formatting ("TO:", Cc:...)
        addr        -- ADDRESS structure to format
        line_prefix -- A prefix string for each line such as "> "

 Result: A formatted, malloced string is returned.

The resuling lines formatted are 80 columns wide.
  ----------------------------------------------------------------------*/
void
pretty_addr_string(field_name, addr, line_prefix, pc)
    ADDRESS *addr;
    char    *line_prefix, *field_name;
    gf_io_t   pc;
{
    char    *buf;
    size_t   need;
    int	     trailing = 0, llen, alen, plen;
    ADDRESS *atmp;

    if(!addr)
      return;

    gf_puts(line_prefix, pc);
    gf_puts(field_name, pc);
    plen = strlen(line_prefix);
    llen = plen + strlen(field_name);
    while(addr){
	atmp       = addr->next; 	/* remember what's next */
	addr->next = NULL;
	/*
	 * rfc822_write_address()/rfc822_cat() do unbounded strcat()s
	 * assuming the caller's buffer is "big enough" -- addr->mailbox/
	 * host/personal/adl are cpystr()'d with no length cap anywhere
	 * upstream (e.g. parsing a received message's From:/To: line),
	 * so a long address overflowed the fixed MAILTMPLEN buf this
	 * used to be. Confirmed under valgrind: viewing an ordinary
	 * received message with a 2000-byte From: mailbox smashed the
	 * stack (jumped to an address built from the injected bytes).
	 * Size dynamically per address instead, same technique as
	 * addrutil.c's addr_string().
	 */
	need = 2 * ((addr->personal ? strlen(addr->personal) : 0)
	            + (addr->mailbox ? strlen(addr->mailbox) : 0)
	            + (addr->host   ? strlen(addr->host)   : 0)
	            + (addr->adl    ? strlen(addr->adl)    : 0))
	       + 32;    /* "<>", "@", quotes, group ": ", ";" */
	if(need < MAILTMPLEN)
	  need = MAILTMPLEN;
	buf        = (char *) fs_get(need);
	buf[0]     = '\0';
	rfc822_write_address(buf, addr); /* write address into buf */
	alen = strlen(buf);
	if(!trailing){			/* first time thru, just address */
	    llen += alen;
	    trailing++;
	}
	else{				/* else preceding comma */
	    if(addr->host){		/* if not group syntax... */
		gf_puts(",", pc);
		llen++;
	    }

	    if(alen + llen + 1 > 76){
		gf_puts(NEWLINE, pc);
		gf_puts(line_prefix, pc);
		gf_puts("    ", pc);
		llen = alen + plen + 5;
	    }
	    else{
		gf_puts(" ", pc);
		llen += alen + 1;
	    }
	}

	if(alen && llen > 76){		/* handle long addresses */
	    register char *q, *p = &buf[alen-1];

	    while(p > buf){
		if(isspace(*p) && (llen - (alen - (int)(p - buf))) < 76){
		    for(q = buf; q < p; q++)
		      (*pc)(*q);	/* write character */

		    gf_puts(NEWLINE, pc);
		    gf_puts("    ", pc);
		    gf_puts(p, pc);
		    break;
		}
		else
		  p--;
	    }

	    if(p == buf)		/* no reasonable break point */
	      gf_puts(buf, pc);
	}
	else
	  gf_puts(buf, pc);

	fs_give((void **) &buf);
	addr->next = atmp;
	addr       = atmp;
    }

    gf_puts(NEWLINE, pc);
}



/*----------------------------------------------------------------------
  Acquire the pinerc defined signature file

  ----*/
char *
get_signature(void)
{
    char *sig = NULL, *tmp_sig, sig_path[MAXPATH+1];

    /*----- Get the signature if there is one to get -----*/
    if(ps_global->VAR_SIGNATURE_FILE != NULL &&
       ps_global->VAR_SIGNATURE_FILE[0] != '\0') {
	if(ps_global->VAR_SIGNATURE_FILE[0] == '/'){
	    strcpy(sig_path, ps_global->VAR_SIGNATURE_FILE);
	}
	else if(ps_global->VAR_SIGNATURE_FILE[0] == '~'){
	    strcpy(sig_path, ps_global->VAR_SIGNATURE_FILE);
	    fnexpand(sig_path, sizeof(sig_path));
	}
	else
	  build_path(sig_path, sizeof(sig_path), ps_global->home_dir,
		     ps_global->VAR_SIGNATURE_FILE);

	if(can_access(sig_path, ACCESS_EXISTS) == 0){
	    if(tmp_sig = read_file(sig_path)){
		sig = fs_get(strlen(tmp_sig) + 10);
		strcpy(sig, NEWLINE);
		strcat(sig, NEWLINE);
		strcat(sig, tmp_sig);
		fs_give((void **)&tmp_sig);
	    }
	    else
	      q_status_message2(1, 2, 4,
			    "\007Error \"%s\" reading signature file \"%s\"",
				error_description(errno), sig_path);
	}
    }

    return(sig ? sig : cpystr(""));
}
