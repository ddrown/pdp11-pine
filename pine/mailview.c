#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailview.c,v 4.71 1993/12/06 20:09:20 mikes Exp $";
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
     mailview.c
     Implements the mailview screen
     Also includes scrolltool used to display help text
  ====*/


#include "headers.h"



#define TYPICAL_BIG_MESSAGE_LINES 200 

#ifdef ANSI
void       format_envelope(ENVELOPE *, gf_io_t);
void	   set_scroll_text(void *, int, int, int, SourceType);
void       describe_mime(BODY *, char *, int, int);
void       format_mime_size(char *, BODY *);
void       zero_atmts(struct attachment *);
void	   zero_scroll_text(void);
void       push_scroll_state();
void       pop_scroll_state();
void	   ScrollFile(int);
int	   make_file_index();
char      *show_multipart(MESSAGECACHE *, int);
MimeShow   mime_show(BODY *);
char      *part_desc(char *,BODY *, int);
char      *display_parameters(PARAMETER *);

#else
void       format_envelope();
void	   set_scroll_text();
void       scroll_scroll_text();
int        search_scroll_text();
void       describe_mime();
char      *type_desc();
void       format_mime_size();
void       zero_atmts();
void	   zero_scroll_text();
void       push_scroll_state();
void       pop_scroll_state();
void	   ScrollFile();
int	   make_file_index();
char      *show_multipart();
MimeShow   mime_show();
char      *part_desc();
char      *display_parameters();
#endif




/*----------------------------------------------------------------------
     Format a buffer with the text of the current message for browser

    Args: The pine_state structure
  
  Result: The scrolltool is called to display the message

  Loop here viewing mail until the folder changed or a command takes
us to another screen. Inside the loop the message text is fetched and
formatted into a buffer allocated for it.  These are passed to the
scrolltool(), that displays the message and executes commands. It
returns when it's times to display a different message when a we
change folders, or when it's time for a different screen. We also
break out fo the big loop when there is no more mail to read.
  ---*/

void
mail_view_screen(pine_state)
     struct pine *pine_state;
{
    char            last_was_full_header = 0;
    long            last_message_viewed;
    MESSAGECACHE   *mc;
    ENVELOPE       *env;
    BODY           *body;
    STORE_S        *store;
    gf_io_t         pc;
    SourceType	    src = CharStar;

    dprint(1, (debugfile, "\n\n  -----  MAIL VIEW  -----\n"));

    if(pine_state->max_msgno <= 0 || pine_state->mail_stream == NULL) {
        q_status_message(0, 1, 3, "No messages to read!");
        pine_state->next_screen = pine_state->prev_screen;
	return;
    }

    if(pine_state->current_sorted_msgno <= 0L)
      pine_state->current_sorted_msgno = 1L;
   
    last_message_viewed = -1;

    /*------ Loop viewing messages ------------------*/
    do {
        clear_index_cache_ent(pine_state->current_sorted_msgno);

        /* BUG, should check this return code */
	(void)mail_fetchstructure(pine_state->mail_stream,
			pine_state->sort[pine_state->current_sorted_msgno],
			NULL);

	mc = mail_elt(pine_state->mail_stream,
		      pine_state->sort[pine_state->current_sorted_msgno]);

	if(mc && !mc->seen)	/* count state change in check_point */
	  check_point_change();

#ifdef	DOS
	/* 
	 * Handle big text for DOS here
	 *
	 * this size could be adaptive if we don't find a good
	 * solution to the elt caching problem with mrc.
	 *
	 * judging from 1000+ message folders around here, it looks
	 * like 9X% of messages fall in the < 8k range, so it
	 * seems like this is as good a place to draw the line as any
	 *
	 * We ALSO need to divert all news articles we read to secondary
	 * storage as its possible we're using c-client's NNTP driver
	 * which returns BOGUS sizes UNTIL the whole thing is fetched.
	 * Note: this is more a deficiency in NNTP than in c-client.
	 */
	if(mc->rfc822_size > MAX_MSG_INCORE 
	   || strcmp(pine_state->mail_stream->dtb->name, "nntp") == 0){
	    src = FileStar;
	}
	else{
	    src = CharStar;
	}
#endif
	store = so_get(src, NULL, EDIT_ACCESS);
	gf_set_so_writec(&pc, store);

        if(!pine_state->full_header) {
	    body = NULL;

	    env = mail_fetchstructure(pine_state->mail_stream,
			pine_state->sort[pine_state->current_sorted_msgno],
			&body);


/* BUG: should check this return code!?! */
            (void) format_message(
		       pine_state->sort[pine_state->current_sorted_msgno],
		       env,
		       body,
		       (last_message_viewed != ps_global->current_sorted_msgno
			 || last_was_full_header == 1) ? FM_NEW_MESS : 0,
		       pc);

	} else {
#ifdef	DOS
	    /*
	     * a little cheating here.  Setup dos_gets to put
	     * what it receives in the file pointed to by
	     * the storage object we've already allocated.
	     * This'll save us having to copy files around.
	     */
	    if(src == FileStar){
	        append_file = (FILE *)so_text(store);
	        mailgets    = dos_gets;
	        (void)mail_fetchheader(pine_state->mail_stream,
		         pine_state->sort[pine_state->current_sorted_msgno]);

		(void)mail_fetchtext(pine_state->mail_stream,
			 pine_state->sort[pine_state->current_sorted_msgno]);

		append_file = NULL;
		mailgets    = NULL;
		mail_gc(pine_state->mail_stream, GC_TEXTS);
	    }
	    else
#endif
	    {
		so_puts(store, mail_fetchheader(pine_state->mail_stream,
			 pine_state->sort[pine_state->current_sorted_msgno]));

		so_puts(store, mail_fetchtext(pine_state->mail_stream,
			 pine_state->sort[pine_state->current_sorted_msgno]));
	    }
        }

        last_message_viewed  = ps_global->current_sorted_msgno;
        last_was_full_header = ps_global->full_header;

        pine_state->next_screen = SCREEN_FUN_NULL;

        scrolltool(so_text(store), "MESSAGE TEXT",
                   (int *)NULL, MessageText, src, (struct attachment *)NULL);

	so_give(&store);	/* free resources associated with store */

        if(pine_state->max_msgno == 0) {
            q_status_message(0, 2, 3, "\007No messages to read!");
            if(pine_state->next_screen == SCREEN_FUN_NULL)
              pine_state->next_screen = pine_state->prev_screen;
            pine_state->prev_screen = mail_view_screen;
            return;
        }
    }
    while(pine_state->next_screen == SCREEN_FUN_NULL);

    pine_state->prev_screen = mail_view_screen;
    return;
}

    


/*----------------------------------------------------------------------
    Add lines to the attachments structure
    
  Args: body   -- body of the part being described
        prefix -- The prefix for numbering the parts
        num    -- The number of this specific part
        should_show -- Flag indicating which of alternate parts should be shown

Result: The ps_global->attachments data structure is filled in. This
is called recursively to descend through all the parts of a message. 
The description strings filled in are malloced and should be freed.

  ----*/
void
describe_mime(body, prefix, num, should_show)
     BODY *body;
     char *prefix;
     int   num, should_show;
{
    PART              *part;
    char               numx[50], string[200];
    int                n, alt_to_show, has_name;
    struct attachment *a;
    MimeShow           alts[50]; /* BUG -- no bounds checking on this */
    PARAMETER         *param;

    if(body == NULL)
      return;

    if(body->type == TYPEMULTIPART) {
        if(strucmp(body->subtype, "alternative") == 0) {
            /*---- Figure out which alternative part to display ---*/
            for(part=body->contents.part,n=1; part!=NULL; part=part->next, n++)
              alts[n] = mime_show(&(part->body));
            n--;
            while(n > 1 && alts[n] == ShowNone)
              n--;
            alt_to_show = n;
        } else {
            /*--- Not alternative parts, display them all ----*/
            alt_to_show = 0;
        }
        n = 1;
        for(part = body->contents.part; part != NULL; part = part->next, n++) {
            sprintf(numx, "%s%d.", prefix, n);
            describe_mime(&(part->body),
                          part->body.type == TYPEMULTIPART ? numx : prefix,
                          n,
                          n == alt_to_show || alt_to_show == 0 ? 1 : 0);
        }
    } else {
        for(a = ps_global->atmts; a->description != NULL; a++);
        if(a - ps_global->atmts + 1 >= ps_global->atmts_allocated) {
            ps_global->atmts_allocated *=2;
            fs_resize((void **)&(ps_global->atmts),
                       ps_global->atmts_allocated * sizeof(struct attachment));
            a = &ps_global->atmts[ps_global->atmts_allocated/2 - 1];
        }

        format_mime_size(a->size, body);

        sprintf(string, "%s%s%.*s%s",
                type_desc(body->type, body->subtype, body->parameter, 0),
                body->description != NULL ? ", \"" : "",
                sizeof(string)-20, 
                body->description != NULL ? body->description : "",
                body->description != NULL ? "\"": "");

        a->description = cpystr(string);
        a->body        = body;
        a->can_display = mime_can_display(body->type, body->subtype,
                                          body->parameter);
	for(param = a->body->parameter; 
	    param != NULL && strucmp(param->attribute,"name") != 0;
	    param = param->next)
	      ;
	if (param != NULL && strucmp(param->attribute,"name") == 0)
	    has_name = 1;
	else
	    has_name = 0;
        a->shown = ((a->body->type==TYPETEXT && !has_name) ||
		a->body->type==TYPEMESSAGE) && a->can_display && should_show;
        sprintf(a->number, "%s%d",prefix, num);
        (a+1)->description = NULL;
        if(body->type == TYPEMESSAGE) {
	    if(body->subtype && strucmp(body->subtype, "rfc822") == 0){
		body = body->contents.msg.body;
		sprintf(numx, "%s%d.", prefix, num);
		describe_mime(body, numx, 1, should_show);
	    }
        }
    }
}



/*----------------------------------------------------------------------
   Zero out the attachments structure and free up storage
  ----*/
void
zero_atmts(atmts)
     struct attachment *atmts;
{
    struct attachment *a;
    for(a = atmts; a->description != NULL; a++)
      fs_give((void **)&(a->description)); 
    atmts->description = NULL;
}


char *
body_type_names(t)
     int t;
{
    static char *body_types[] = {"Text", "Multipart", "Message", 
                                 "Application", "Audio", "Image",
                                 "Video", "Other"};
    return(body_types[t]);
}

/*----------------------------------------------------------------------
  Return a nicely formatted discription of the type of the part
 ----*/

char *
type_desc(type, subtype, params, full)
     int type, full;
     char *subtype;
     PARAMETER *params;
{
    static char type_d[200];

    switch(type) {
      case TYPETEXT:
        while(params != NULL && strucmp(params->attribute,"charset") != 0)
          params = params->next;
        if(params != NULL) {
            if(strucmp(params->value, "us-ascii") == 0){
                strcpy(type_d, "Text");
            } else {
                strcpy(type_d, "Unknown text");
                if(full)
                  sprintf(type_d+strlen(type_d), " \"%.100s\"",params->value);
            }
        } else {
            strcpy(type_d, "Text");
        }
        break;

      case TYPEAPPLICATION:
        if(subtype != NULL && strucmp(subtype, "octet-stream") == 0){
            while(params != NULL && strucmp(params->attribute,"name") != 0)
              params = params->next;
            if(full)
              sprintf(type_d, "Attached file \"%s\"",
                      params != NULL && params->value != NULL ?
                                                    params->value : "");
            else
              sprintf(type_d, "File \"%s\"",
                      params != NULL && params->value != NULL ?
                                                    params->value : "");
        } else if(subtype != NULL && strucmp(subtype, "postscript") == 0)
          strcpy(type_d, "PostScript");
        else
          sprintf(type_d, "Application/%.100s", subtype != NULL ? subtype: "");
        break;

      default:
        if(full)
          sprintf(type_d, "%s/%.100s", body_type_names(type),
                 subtype != NULL ? subtype: "");
        else
          strcpy(type_d, body_type_names(type));
        break;
    }

    return(type_d);
}
     

void
format_mime_size(string, b)
     char *string;
     BODY *b;
{
    char tmp[10], *p;
    if(b->type == TYPETEXT) {
        sprintf(string, "%s lines", comatose(b->size.lines));
    } else {
        strcpy(string, byte_string(b->size.bytes));
        for(p = string; *p && (isdigit(*p) || ispunct(*p)); p++);
        sprintf(tmp, " %-5.5s",p);
        strcpy(p, tmp);
    }
}

        

/*----------------------------------------------------------------------
   Determine if we can show all, some or none of the parts of a body

Args: body --- The message body to check

Returns: ShowAll, ShowPart or ShowNone depending on how much of the body
    can be shown 
 ----*/     
MimeShow
mime_show(body)
     BODY *body;
{
    int   sp, sn, sa;
    PART *p;

    if(body == NULL)
      return(ShowNone);

    switch(body->type) {
      default:
        return(mime_can_display(body->type,body->subtype,body->parameter)== 1 ?
               ShowAll:
               ShowNone);

      case TYPEMESSAGE:
        return(mime_show(body->contents.msg.body) == ShowAll ? 
               ShowAll:
               ShowParts);

      case TYPEMULTIPART:
        sp = sn = sa = 0;
        for(p = body->contents.part; p != NULL; p = p->next) {
            switch(mime_show(&(p->body))) {
              case ShowAll:
                sa++;
                break;
              case ShowParts:
                sp++;
                break;
              case ShowNone:
                sn++;
                break;
            }
        }

        if(sa == 0)
          if(sp == 0)
            return(ShowNone);
          else
            return(ShowParts);
        else
          if(sn == 0)
            return(ShowAll);
          else
            return(ShowParts);
    }
}
        


/*----------------------------------------------------------------------
   Format a message message for viewing

 Args: msgno -- The number of the message to view
       env   -- pointer to the message's envelope
       body  -- pointer to the message's body
       src   -- where we can expect the text to come from
       dname -- name of destination if it's a file
       dest  -- where the formatted message is to be returned. valid
                 types are CharStar and FileStar
       flgs  -- possible flags:
                FM_NEW_MESS -- flag indicating a different message being
			       formatted than was formatted last time 
			       function was called
		FM_DO_PRINT -- Indicates formatted text is bound for
			       something other than display by pine
			       (printing, export, etc)

Result: Returns true if no problems encountered, else false.

First the envelope is formatted; next a list of all attachments is
formatted if there are more than one. Then all the body parts are
formatted, fetching them as needed. This includes headers of included
message. Richtext is also formatted. An entry is made in the text for
parts that are not displayed or can't be displayed.  source indicates 
how and where the caller would like to have the text formatted.

 ----*/    
int
format_message(msgno, env, body, flgs, pc)
    long         msgno;
    ENVELOPE    *env;
    BODY        *body;
    int          flgs;
    gf_io_t      pc;
{
    char              *decode_error;
    struct attachment *a;
    int                show_parts, error_found = 0;
    gf_io_t            gc;

    show_parts = !(flgs&FM_DO_PRINT);

    /*---- format and copy envelope ----*/
    format_envelope(env, pc);

    if(body == NULL) {
        /*--- Server is not an IMAP2bis, It can't parse MIME
              so we just show the text here. Hopefully the 
              message isn't a MIME message 
          ---*/
	void            *text2;
#ifdef	DOS
	/* for now, always fetch to disk.  This could be tuned to
	 * check for message size, then decide to deal with it on disk...
	 */
	mailgets    = dos_gets;
	append_file = NULL;		/* let dos_gets create tmp file */
#endif

        if(text2 = (void *)mail_fetchtext(ps_global->mail_stream, msgno)){
	    if(!gf_puts(NEWLINE, pc))
	      goto write_error;
#ifdef	DOS
	    gf_set_readc(&gc, append_file, 0L, FileStar);
#else
	    gf_set_readc(&gc, text2, (unsigned long)strlen(text2), CharStar);
#endif
	    gf_filter_init();
	    gf_link_filter(gf_nvtnl_local);
	    if(decode_error = gf_pipe(gc, pc)){
                sprintf(tmp_20k_buf, "%s%s    [Formatting error: %s]%s",
			NEWLINE, NEWLINE, decode_error, NEWLINE);
		if(!gf_puts(tmp_20k_buf, pc))
		  goto write_error;
	    }
#ifdef	DOS
	    if(append_file){
		fclose(append_file);
		append_file = NULL;
		mail_gc(ps_global->mail_stream, GC_TEXTS);
	    }
#endif
	}
	else{
	    if(!gf_puts(NEWLINE, pc)
	       || !gf_puts("    [ERROR fetching text of message]", pc)
	       || !gf_puts(NEWLINE, pc)
	       || !gf_puts(NEWLINE, pc))
	      goto write_error;
	}

#ifdef	DOS
	mailgets = NULL;		/* unset special switch */
#endif
	return(1);
    }
        
    if(flgs&FM_NEW_MESS) {
        zero_atmts(ps_global->atmts);
        describe_mime(body, "", 1, 1);
    }

    /*---- Calculate the approximate length of what we've got to show  ---*/

    /*=========== Format the header into the buffer =========*/
    /*----- First do the list of parts/attachments if needed ----*/
    if(show_parts && ps_global->atmts[1].description != NULL) {
	int max_num_l = 0, max_size_l = 0;

	if(!gf_puts("Parts/attachments:", pc) || !gf_puts(NEWLINE, pc))
	  goto write_error;

        /*----- Figure display lengths for nice display -----*/
        for(a = ps_global->atmts; a->description != NULL; a++) {
	    if(strlen(a->number) > max_num_l)
	      max_num_l = strlen(a->number);
	    if(strlen(a->size) > max_size_l)
	      max_size_l = strlen(a->size);
	}

        /*----- Format the list of attachments -----*/
        for(a = ps_global->atmts; a->description != NULL; a++) {
            sprintf(tmp_20k_buf, "   %-*.*s %s  %*.*s  %-*.*s%s",
                    max_num_l, max_num_l, a->number,
                    (a->shown ? "Shown" :
                       a->can_display ? "  OK " : "     "),
                    max_size_l, max_size_l, a->size,
                    ps_global->ttyo->screen_cols - max_num_l - max_size_l - 15,
                    ps_global->ttyo->screen_cols - max_num_l - max_size_l - 15,
                    a->description, NEWLINE);
	    if(!gf_puts(tmp_20k_buf, pc))
	      goto write_error;
        }

	if(!gf_puts("----------------------------------------", pc)
	   || !gf_puts(NEWLINE, pc))
	  goto write_error;
    }

    if(!gf_puts(NEWLINE, pc))
      goto write_error;

    show_parts = 0;

    /*======== Now loop through formatting all the parts =======*/
    for(a = ps_global->atmts; a->description != NULL; a++) {

        if(!a->shown) {
	    if(!gf_puts(part_desc(a->number, a->body,
				  (flgs&FM_DO_PRINT)?3:a->can_display ? 1:2),
			pc)
	       || ! gf_puts(NEWLINE, pc))
	      goto write_error;

            continue;
        } 

        switch(a->body->type){

          case TYPETEXT:
	    if(show_parts){
		sprintf(tmp_20k_buf, "%s  [ Part %s: \"%.55s\" ]%s",
			NEWLINE, a->number, 
			a->body->description ? a->body->description
					     : "Attached Text",
			NEWLINE);
		if(!gf_puts(tmp_20k_buf, pc))
		  goto write_error;
	    }

	    error_found += decode_text(a, msgno, pc,
				       (flgs&FM_DO_PRINT) ? QStatus : InLine,
				       !(flgs&FM_DO_PRINT));
            break;

          case TYPEMESSAGE:
            sprintf(tmp_20k_buf, "%s  [ Part %s: \"%.55s\" ]%s",
		    NEWLINE, a->number, 
                    a->body->description ? a->body->description
					 : "Included Message",
		    NEWLINE);
  	    if(!gf_puts(tmp_20k_buf, pc))
	      goto write_error;

	    if(a->body->subtype && strucmp(a->body->subtype, "rfc822") == 0){
		format_envelope(a->body->contents.msg.env, pc);
	    }
            else if(a->body->subtype 
		    && strucmp(a->body->subtype, "external-body") == 0) {
		if(!gf_puts("This part is not included and can be ", pc)
		   || !gf_puts("fetched as follows:", pc)
		   || !gf_puts(NEWLINE, pc)
		   || !gf_puts(display_parameters(a->body->parameter), pc))
		  goto write_error;
            }
	    else
	      error_found += decode_text(a, msgno, pc, 
					 (flgs&FM_DO_PRINT) ? QStatus : InLine,
					 !(flgs&FM_DO_PRINT));

	    if(!gf_puts(NEWLINE, pc))
	      goto write_error;

            break;

          default:
	    if(!gf_puts(part_desc(a->number,a->body,(flgs&FM_DO_PRINT) ? 3:1),
			pc))
	      goto write_error;
        }

	show_parts++;
    }

    return(!error_found);

  write_error:
    if(flgs & FM_DO_PRINT)
      q_status_message1(1, 2, 4, "Error writing message: %s", 
			error_description(errno));
    return(0);
}



/*----------------------------------------------------------------------
   Handle fetching and filtering a text message segment

Args: att   -- segment to fetch
      msgno -- message number segment is a part of
      pc    -- function to write characters from segment with
      style -- Indicates special handling for error messages
      display_bound -- Indicates special necessary filtering

Returns: 1 if errors encountered, 0 if everything went A-OK

 ----*/     
int
decode_text(att, msgno, pc, style, display_bound)
    struct attachment *att;
    long         msgno;
    gf_io_t      pc;
    DetachErrStyle style;
    int		 display_bound;
{
    int                match;
    PARAMETER         *param;
    filter_t	       aux_filter[4];
    int		       filtcnt = 0, error_found = 0;
    char	      *err;

    if(att->body->subtype != NULL &&
			strucmp(att->body->subtype, "richtext") == 0) {
	gf_rich2plain_opt(!display_bound);	/* maybe strip everything! */
	aux_filter[filtcnt++] = gf_rich2plain;
	if(!display_bound) {
	    gf_wrap_filter_opt(75);  /* width to use for file or printer */
	    aux_filter[filtcnt++] = gf_wrap;
	}
    }

    for(param = att->body->parameter; 
		param != NULL && strucmp(param->attribute,"charset") != 0;
		param = param->next)
	;

    match =  match_charset(param != NULL ? param->value : NULL);

    if(match == 1) {
	strcpy(tmp_20k_buf, "    [The following text is in the \"");
	strcat(tmp_20k_buf, param != NULL ? param->value : NULL);
	strcat(tmp_20k_buf, "\" character set]");
	strcat(tmp_20k_buf, NEWLINE);
	strcat(tmp_20k_buf, "    [Your display is set for the \"US-ASCII\" character set]");
	strcat(tmp_20k_buf, NEWLINE);
	if(!gf_puts(tmp_20k_buf, pc)
	   || !gf_puts("    [Some characters may be displayed incorrectly]",pc)
	   || !gf_puts(NEWLINE, pc)
	   || !gf_puts(NEWLINE, pc))
	  goto write_error;
    }

    aux_filter[filtcnt] = NULL;
    err = detach(msgno, att->body, att->number, (long *)NULL, pc, aux_filter);
    if(err != (char *)NULL) {
	error_found++;
	if(style == QStatus) {
	    q_status_message1(1, 2, 4, "%s", err);
	} else if(style == InLine) {
	    sprintf(tmp_20k_buf, "%s   [Error: %s]  \"%s\"%s%s", err,
		    NEWLINE,
	        att->body->description != NULL ? att->body->description : "",
		    NEWLINE, NEWLINE);
	    if(!gf_puts(tmp_20k_buf, pc))
	      goto write_error;
	}
    }

    if(att->body->subtype != NULL &&
       strucmp(att->body->subtype, "richtext") == 0 && !display_bound){
	if(!gf_puts(NEWLINE, pc) || !gf_puts(NEWLINE, pc))
	  goto write_error;
    }

    return(error_found);

  write_error:
    if(style == QStatus)
      q_status_message1(1, 2, 4, "Error writing message: %s", 
			error_description(errno));

    return(1);
}


/*----------------------------------------------------------------------
   
  Returns:  0 - can display all characters
            1 - can display most of the characters
            2 - can't display any characters or don't know
		(We never return 2 anymore.  This used to be much more
		 complicated, but now we just return 0 if the text is
		 ascii or if the text type and device type match
		 exactly.  Otherwise, we return 1 which means to print
		 a warning and then try to blat it out to see if it works.
		 The problem was that we were sometimes preventing people
		 from seeing text which they could at least read partially.)
  ----*/
match_charset(text_char_set)
     char *text_char_set;
{
    if(!text_char_set || strucmp(text_char_set, "us-ascii") == 0)
      return(0);

    return(1);
}



 
/*------------------------------------------------------------------
   This list of iso 2022 escapes is taken from the X11R5 source with only
  a remote understanding of what this all means
  ----*/
static char *iso2022_escapes[] = {
    "\033(B",   "\033(J",   "\033)I",   "\033-A",   "\033-B",
    "\033-C",   "\033-D",   "\033-F",   "\033-G",   "\033-H",
    "\033-L",   "\033-M",   "\033-$(A", "\033$)A",  "\033$(B",
    "\033$)B",  "\033$(C",  "\033$)C",  "\033$(D",  "\033$)D",
    "\033$B",   "\033$@",
    NULL};

match_iso2022(esc_seq)
     char *esc_seq;
{
    char **p;

    for(p = iso2022_escapes;
        *p != NULL && struncmp(esc_seq, *p, strlen(*p));
/* BUG: make sure this shouldn't be CASE SENSITIVE */
/* may be why some people see bugs once in a while!! */
        p++);
    if(*p == NULL)
      return(0);
    else
      return(strlen(*p));
}



/*

Returns: pointer to static buffer with formatted envelope
 ----*/
void
format_envelope(e, pc)
    ENVELOPE   *e;
    gf_io_t     pc;
{
    if(!e)
      return;

    if(e->date) {
	gf_puts("Date: ", pc);
	gf_puts(e->date, pc);
	gf_puts(NEWLINE, pc);
    }

    if(e->from)
	pretty_addr_string("From: ", e->from, "", pc);

    if(e->reply_to && (!e->from || 
		       strucmp(e->from->mailbox, e->reply_to->mailbox) ||
		       strucmp(e->from->host, e->reply_to->host)))
      pretty_addr_string("Reply to: ", e->reply_to, "", pc);

    if(e->to)
      pretty_addr_string("To: ", e->to, "", pc);

    if(e->cc)
      pretty_addr_string("Cc: ", e->cc, "", pc);


    if(e->bcc)
      pretty_addr_string("Bcc: ", e->bcc, "", pc);

    if(e->newsgroups) {
        gf_puts("Newsgroups: ", pc);
        gf_puts(e->newsgroups, pc);
	gf_puts(NEWLINE, pc);
    }

    if(e->subject && e->subject[0]){
	gf_puts("Subject: ", pc);
	gf_puts(e->subject, pc);
	gf_puts(NEWLINE, pc);
    }
}



/*----------------------------------------------------------------------
    Format a strings describing one unshown part of a Mime message

Args: number -- A string with the part number i.e. "3.2.1"
      body   -- The body part
      type   -- 1 - Not shown, but can be
                2 - Not shown, cannot be shown
                3 - Can't print


Result: pointer to formatted string in static buffer

Note that size of the strings are carefully calculated never to overflow 
the static buffer:
    number  < 20,  description limited to 100, type_desc < 200,
    size    < 20,  second line < 100           other stuff < 60
 ----*/
char *
part_desc(number, body, type)
     BODY *body;
     int type;
     char *number;
{
    char *t;

    sprintf(tmp_20k_buf, "%s  [Part %s, %s%.100s%s%s  %s%s]%s",
	    NEWLINE,
            number,
            body->description == NULL ? "" : "\"",
            body->description == NULL ? "" : body->description,
            body->description == NULL ? "" : "\"  ",
            type_desc(body->type, body->subtype, body->parameter, 1),
            body->type == TYPETEXT ? comatose(body->size.lines) :
                                     byte_string(body->size.bytes),
            body->type == TYPETEXT ? " lines" : "",
	    NEWLINE);

    t = &tmp_20k_buf[strlen(tmp_20k_buf)];

    switch(type) {
      case 1:
        sstrcpy(&t,
	    "  [Not Shown. Use the \"V\" command to view or save this part]");
	sstrcpy(&t, NEWLINE);
        break;

      case 2:
	sstrcpy(&t, "  [Can not ");
	if(body->type != TYPEAUDIO && body->type != TYPEVIDEO)
	  sstrcpy(&t, "dis");

	sstrcpy(&t, 
		"play this part. Use the \"V\" command to save in a file]");
	sstrcpy(&t, NEWLINE);
        break;

      case 3:
        sstrcpy(&t, "  [Unable to print this part]");
	sstrcpy(&t, NEWLINE);
        break;
    }

    return(tmp_20k_buf);
}


/*
 * This could be better.  For example, look at the first two.  The
 * thing that keeps us from using the same array is that the column number
 * is stored with each entry and it could be different for each.
 */
static struct key help_keys[] =
       {{"M","Main Menu",0},  {NULL,NULL,0},            {"E","Exit Help",0},
        {NULL,NULL,0},        {NULL,NULL,0},            {NULL,NULL,0},
        {"-","PrevPage",0},   {"Spc","NextPage",0},     {NULL,NULL,0},
        {NULL,NULL,0},        {NULL,NULL,0},            {"W", "WhereIs",0}};
static struct key_menu help_keymenu =
	{sizeof(help_keys)/(sizeof(help_keys[0])*12), 0, 0,0,0,0, help_keys};

static struct key composer_help_keys[] =
       {{NULL,NULL,0},        {NULL,NULL,0},            {"E","Exit Help",0},
        {NULL,NULL,0},        {NULL,NULL,0},            {NULL,NULL,0},
        {"-","PrevPage",0},   {"Spc","NextPage",0},     {NULL,NULL,0},
        {NULL,NULL,0},        {NULL,NULL,0},            {"W", "WhereIs",0}};
static struct key_menu composer_help_keymenu =
    {sizeof(composer_help_keys)/(sizeof(composer_help_keys[0])*12), 0, 0,0,0,0,
							composer_help_keys};

static struct key view_keys[] = 
       {{"?","Help",0},        {"O","OTHER CMDS",0},    {"M","Main Menu",0},
        {"V","ViewAttch",0},   {"P","PrevMsg",0},       {"N","NextMsg",0},
        {"-","PrevPage",0},    {"Spc","NextPage",0},    {"D","Delete",0},
        {"U","Undelete",0},    {"R","Reply",0},         {"F","Forward",0}, 

        {"?","Help",0},        {"O","OTHER CMDS",0},    {"Q","Quit",0},
        {"C","Compose",0},     {"L","ListFldrs",0},     {"G","GotoFldr",0},
        {"I","Index",0},       {"W","WhereIs",0},       {NULL,NULL,0},
        {NULL,NULL,0},         {"S","Save",0},          {"E","Export",0},

        {"?","Help",0},        {"O","OTHER CMDS",0},    {NULL,NULL,0},
        {NULL,NULL,0},         {NULL,NULL,0},           {NULL,NULL,0}, 
        {"J","Jump",0},        {"TAB","NextNew",0},     {"H","HdrMode",0},
        {"B","Bounce",0},      {"*","Flag",0},          {"|","Pipe",0}}; 
static struct key_menu view_keymenu =
	{sizeof(view_keys)/(sizeof(view_keys[0])*12), 0, 0,0,0,0, view_keys};
#define VIEW_FULL_HEADERS_KEY 32
#define BOUNCE_KEY 33
#define FLAG_KEY 34
#define VIEW_PIPE_KEY 35

static struct key text_att_view_keys[] =
       {{"?","Help",0},       {NULL,NULL,0},           {"E","Exit Viewer",0},
        {NULL,NULL,0},        {NULL,NULL,0},           {NULL,NULL,0},
        {"-","PrevPage",0},   {"Spc","NextPage",0},    {NULL,NULL,0},
        {NULL,NULL,0},        {"S","Save",0},          {"W", "WhereIs",0}};
static struct key_menu text_att_view_keymenu =
     {sizeof(text_att_view_keys)/(sizeof(text_att_view_keys[0])*12), 0, 0,0,0,0,
							text_att_view_keys};


/*----------------------------------------------------------------------
   routine for displaying help and message text on the screen.

  Args: text          buffer to display
        title         string with title of text being displayed
        pages         page numbering if text is to has page numbers
        style         whether we are display a message, help text ...
	source	      what's text: char **, char * or FILE * ???
 

   This displays in three different kinds of text. One is an array of
lines passed in in text_array. The other is a simple long string of
characters passed in in text. The simple string of characters may have
another string, the header which is prepended to the text with some
special processing. The two header.... args specify how format and
filter the header.

    It can scroll by line or by page. The pages array passed in is a
list of line numbers that are assumed to correspond to pages one
through n. When scrolling down it will go to the next page, whatever
line number that is.

  The style determines what some of the error messages will be, and
what commands are available as different things are appropriate for
help text than for message text etc.

 ---*/
#define LINES_ABOVE 2  /* Title Bar and blank line */
#define LINES_BELOW 3  /* Status line and key menu */
void
scrolltool(text, title, pages, style, source, att)
     void       *text;
     char       *title;
     int        *pages;
     TextType    style;		/* message, news, etc. */
     SourceType  source;	/* char **, char * or FILE * */
     struct attachment *att;	/* used only with style AttachText */
{
    register int     cur_top_line,  num_display_lines;
    int              result, done, ch, num_text_lines,
                     current_page, page_count, found_on,
                     orig_ch, count_by_lines, first_view, force;
    int level;
    struct key_menu *km;
    BITMAP           bitmap;
    OtherMenu        what;

    page_count        = 0; 
    ch                = 'x'; /* For display_messaage first time through */
    num_display_lines = ps_global->ttyo->screen_rows -LINES_ABOVE- LINES_BELOW;
    ps_global->mangled_header = 1;
    ps_global->mangled_footer = 1;
    ps_global->mangled_screen = 0;

    what           = FirstMenu;		/* which key menu to display */
    cur_top_line   = 0;
    done           = 0;
    current_page   = 0;
    found_on       = 0;
    first_view     = 1;
    force          = 0;
    count_by_lines = (pages == NULL);

    if(pages != NULL)
      for(page_count = 0; pages[page_count] != -1; page_count++);

    if(level = ps_global->scroll_text_level++)
      push_scroll_state();

    set_scroll_text(text, cur_top_line, LINES_ABOVE, 
		    LINES_ABOVE + LINES_BELOW, source);
    format_scroll_text();
    num_text_lines          = get_scroll_text_lines();
    ps_global->redrawer     = redraw_scroll_text;
    ps_global->mangled_body = 1;

    setbitmap(bitmap);
    if(style == AttachText) {
      km = &text_att_view_keymenu;
    }else if(style == MessageText) {
      km = &view_keymenu;
#ifndef DOS
      if(F_OFF(F_ENABLE_PIPE,ps_global))
#endif
        clrbitn(VIEW_PIPE_KEY, bitmap);  /* always clear for DOS */
      if(F_OFF(F_ENABLE_BOUNCE,ps_global))
	clrbitn(BOUNCE_KEY, bitmap);
      if(F_OFF(F_ENABLE_FLAG,ps_global))
	clrbitn(FLAG_KEY, bitmap);
      if(F_OFF(F_ENABLE_FULL_HDR,ps_global))
	clrbitn(VIEW_FULL_HEADERS_KEY, bitmap);
    }else if(style == ComposerHelpText) {
      km = &composer_help_keymenu;
    }else {
      km = &help_keymenu;
    }

    while(!done) {
	if(ps_global->mangled_screen) {
	    ps_global->mangled_header = 1;
	    ps_global->mangled_footer = 1;
            ps_global->mangled_body   = 1;
            ps_global->mangled_screen = 0;
	}

        if(streams_died())
          ps_global->mangled_header = 1;

        dprint(9, (debugfile, "@@@@ new: %ld  current:%ld\n",
                   ps_global->new_current_sorted_msgno,
                   ps_global->current_sorted_msgno));


        /*==================== All Screen painting ====================*/
        /*-------- The title bar ---------------*/
	if(count_by_lines) {
	    if(ps_global->mangled_header) {
		set_titlebar(title, 1, TextPercent,
                             ps_global->current_sorted_msgno,
		             cur_top_line +num_display_lines > num_text_lines ?
		  	     num_text_lines :
                             cur_top_line + num_display_lines,
		             num_text_lines);

		ps_global->mangled_header = 0;
	    }else {
		update_titlebar_percent(cur_top_line + num_display_lines >
				      num_text_lines ?
			num_text_lines : cur_top_line + num_display_lines);
	    } 
	}else {
	    if(ps_global->mangled_header) {
	        set_titlebar(title,1, PageNumber,0L,current_page+1,page_count);
	        ps_global->mangled_header = 0;
	    }else {
		update_titlebar_page(current_page + 1);
	    }

	}


        /*---- Scroll or update the body of the text on the screen -------*/
        scroll_scroll_text(cur_top_line, ps_global->mangled_body);
        ps_global->mangled_body = 0;
        num_text_lines          = get_scroll_text_lines();


        /*------------- The key menu footer --------------------*/
	if(ps_global->mangled_footer) {
               
            draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
							    -2, 0, what, 0);
	    what = SameTwelve;
	    ps_global->mangled_footer = 0;
	}

	/*=================  Check for New Mail and CheckPoint ===*/
        if(new_mail(NULL, force, (ch == NO_OP_IDLE || first_view) ? 0 :
				  ch == NO_OP_COMMAND ? 1 : 2) >= 0){
            if(ps_global->new_current_sorted_msgno > 0L) {
                ps_global->current_sorted_msgno =
                  ps_global->new_current_sorted_msgno;
                ps_global->new_current_sorted_msgno = -1L;
            }

	    if(count_by_lines) {
		set_titlebar(title, 1, TextPercent,
                             ps_global->current_sorted_msgno,
		             cur_top_line +num_display_lines > num_text_lines ?
		  	     num_text_lines :
                             cur_top_line + num_display_lines,
		             num_text_lines);

	    }else {
	        set_titlebar(title,1, PageNumber,0L,current_page+1,page_count);
	    }
        }
	force      = 0;		/* may not need to next time around */
	first_view = 0;		/* check_point a priority any more? */

	/*==================== Output the status message ==============*/
        display_message(ch);
	MoveCursor(min(LINES_ABOVE + num_display_lines,
                       ps_global->ttyo->screen_rows), 0);


	/*================ Get command and validate =====================*/
        ch = read_command();
        orig_ch = ch;

        if(ch < 0x0100)
	    if(isupper(ch))
	       ch = tolower(ch);

        if(km->which == 1)
          if(ch >= PF1 && ch <= PF12)
            ch = PF2OPF(ch);
        else if(km->which == 2)
          if(ch >= PF1 && ch <= PF12)
            ch = PF2OOPF(ch);

	ch = validatekeys(ch);


	/*============= Execute command =======================*/
        switch(ch){

            /* ------ Help -------*/
          case PF1:
          case OPF1:
          case OOPF1:
          case '?':
          case ctrl('G'):
            if(ch == PF1) {
	        if(style == HelpText)
                  goto df;
		if(style == ComposerHelpText)
                  goto unknown;
	    }
            if(style == HelpText || style == ComposerHelpText) {
                q_status_message(1, 3, 5, "Already in Help");
		break;
	    }
            if(style == AttachText){
                helper(h_mail_text_att_view,
				"HELP FOR ATTACHMENT TEXT VIEW", 0);
	    }
	    else{
                helper(h_mail_view, "HELP FOR MESSAGE TEXT VIEW", 0);
	    }
	    if(ps_global->next_screen == main_menu_screen) {
		done = 1;
	    }else {
		/* Have to reset because helper uses scroll_text */
		set_scroll_text(text, cur_top_line, LINES_ABOVE,
					    LINES_ABOVE + LINES_BELOW, source);
		redraw_scroll_text();
		num_text_lines = get_scroll_text_lines();
		ps_global->mangled_header = 1;
		ps_global->mangled_footer = 1;
	    }
            break; 


            /*---------- Roll keymenu ------*/
          case PF2:
          case OPF2:
          case OOPF2:
	  case 'o':
	    if(km->how_many == 1)
	      goto unknown;
            if (ch == 'o')
	      warn_other_cmds();
	    what = NextTwelve;
	    ps_global->mangled_footer = 1;
	    break;
            

            /* -------- Scroll back one page -----------*/
          case PF7:
          case '-':   
          case ctrl('Y'): 
          case KEY_PGUP:
	    if(cur_top_line == 0) {
	        q_status_message(0, 0, 1,
                      style == MessageText   ? "Already at start of message" :
	              style == HelpText         ? "Already at start of help" :
	              style == ComposerHelpText ? "Already at start of help" :
                                                  "Already at start of text");

	    }else {
	        if(pages == NULL) {
                    cur_top_line -= (num_display_lines-OVERLAP);
	            if(cur_top_line < 0)
	              cur_top_line = 0;
	        }else {
	    	current_page--;
	    	cur_top_line = pages[current_page];
	        }
            }
            break;


            /*---- Scroll down one page -------*/
          case PF8:
          case '+':     
          case ctrl('V'): 
          case KEY_PGDN:
          case ' ':
            if(cur_top_line + num_display_lines < num_text_lines){
	        if(pages == NULL) {
                    cur_top_line += (num_display_lines - OVERLAP);
	        }else {
                    if(current_page + 1 <  page_count) {
                            current_page++;
                        cur_top_line = pages[current_page];
                    }else {
                        q_status_message(0, 0, 1,
                         style == MessageText ? "Already at end of message" :
                         style == ComposerHelpText ? "Already at end of help" :
                         style == HelpText ?    "Already at end of help" :
                                                "Already at end of text");
                    }   
	        }
            }else {
	        q_status_message(0, 0, 1,
                  style == MessageText ? "Already at end of message" :
	    	  style == HelpText ?         "Already at end of help" :
	    	  style == ComposerHelpText ? "Already at end of help" :
                                              "Already at end of text");
	    }
            break;


	  case ctrl('M'):
	  case ctrl('J'):
	    q_status_message1(0, 0, 2,
			"\007No default command in %s",
                  style == MessageText ? "Message View Screen" :
	    	  (style == HelpText || style == ComposerHelpText) ?
					              "Help Screen" :
                                              "Attachment Text Screen");
	    break;


            /*------ Scroll down one line -----*/
	  case KEY_DOWN:
	  case ctrl('N'):
            if(cur_top_line + num_display_lines > num_text_lines)
	        q_status_message(0, 0, 1,
                 style == MessageText ? "Already at end of message" :
	    	 style == HelpText ?         "Already at end of help" :
	    	 style == ComposerHelpText ? "Already at end of help" :
                                             "Already at end of text");
	    else {
	        cur_top_line++;
	        if(pages != NULL&& cur_top_line >= pages[current_page+1] &&
	           pages[current_page+1] != -1)
	          current_page++;
	    }
	    break;


            /* ------ Scroll back up one line -------*/
          case KEY_UP:
	  case ctrl('P'):
	    if(cur_top_line == 0) {
	       q_status_message(0, 0, 1,
                      style == MessageText ? "Already at start of message" :
                      style == HelpText ?         "Already at start of help":
                      style == ComposerHelpText ? "Already at start of help":
                                                  "Already at start of text");
	    }else {
	        cur_top_line--;
	        if(pages != NULL && cur_top_line < pages[current_page])
	          current_page--;
	    }
	    break;


            /*---------- Search text (where is) ----------*/
          case PF12:
          case 'w':
	    /* PF12 is not whereis in this case */
	    if(style == MessageText && ch == PF12)
	      goto df;

whereis:
            ps_global->mangled_footer = 1;
            found_on = search_text( -3,
                    found_on <= 0 || found_on < cur_top_line ||
                      found_on >= cur_top_line + num_display_lines
                             ? cur_top_line : found_on + 1);
            if(found_on >= 0) {
		result = found_on <= cur_top_line;
		if(ps_global->low_speed){
		    if(found_on > cur_top_line + num_display_lines ||
		       found_on < cur_top_line)  {
			cur_top_line = (found_on/(num_display_lines-OVERLAP))
			  *(num_display_lines -OVERLAP);
		    }
		}
		else
		  cur_top_line = found_on - ((found_on > 0) ? 1 : 0);
                if(pages != NULL) {
                    int *pg;
                    for(pg = pages; pg < &pages[page_count]; pg++)
                      if(cur_top_line <= *pg)
                        break;
                    if(pg != pages)
                      pg--;
                    current_page = pg - pages;
                }
                
                q_status_message2(0, 1, 3, "%sFound on line %s on screen",
                                  result ? "Search wrapped to start. " : "",
                                  int2string(found_on - cur_top_line + 1));
            }else if(found_on == -1) {
                q_status_message(0, 0, 2, "\007Search cancelled");
            }else {
                q_status_message(0, 1, 3, "\007Word not found");
            }
            break; 


            /*---------- Suspend Pine ( ^Z ) ----------*/
          case ctrl('Z'):
            if(!have_job_control())
              goto df;
            if(F_OFF(F_CAN_SUSPEND,ps_global)) {
                q_status_message(1, 1, 3,
                        "\007Pine suspension not enabled - see help text");
                break;
            }else {
                do_suspend(ps_global);
            }
            /*-- Fall through to redraw --*/

            /*-------------- refresh -------------*/
          case KEY_RESIZE:
          case ctrl('L'):
	    ClearScreen();
            num_display_lines = ps_global->ttyo->screen_rows - LINES_ABOVE -
                         LINES_BELOW;
            ps_global->mangled_screen = 1;
	    force                     = 1;
	    if(ch == KEY_RESIZE)
	      clear_index_cache();

            break;


            /*------- no op timeout to check for new mail ------*/
          case NO_OP_IDLE:
          case NO_OP_COMMAND:
            break;


	    /*------- Other commands of error ------*/
          default:
	  df:
	    if(style == AttachText && (ch == 's' || ch == PF11)) {
		int qline = -3;
	        /*
	         * This section is an expedient hack to get this working.
	         * We'd probably like to pass a "process_cmd()" in as an
	         * argument or something like that.  Also, save_attachment()
	         * opens and reads the attachment again even though we've
	         * already opened and read it before in this case.
	         */
		save_attachment(qline, ps_global->current_sorted_msgno, att);
                ps_global->mangled_footer = 1;

	    }else if(style == MessageText){
	        result = process_cmd(ch, 0, orig_ch, &force); 
	        dprint(7, (debugfile, "PROCESS_CMD return: %d\n", result));

                if(ps_global->next_screen != SCREEN_FUN_NULL || result == 1){
		    ps_global->scroll_text_level--;
		    if(level)
		      pop_scroll_state();
		    else
		      zero_scroll_text();

		    return; /* New screen, new message or new attachments */
		}

            }else {
		if((ch == 'm' || ch == PF1) && style == HelpText) {
    	            /*---------- Main menu -----------*/
                    ps_global->next_screen = main_menu_screen;
                    done = 1;
    	        }else if(ch == PF3 || ch == 'e') {
    	            /*----------- Done -----------*/
                    done = 1;
                }else {
unknown:
    	            /*----------- Unknown command -------*/
                    q_status_message2(0, 0, 2,
              "\007Command \"%s\" not defined for this screen. Use %s for help",
                          (void *)pretty_command(orig_ch),
                          F_ON(F_USE_FK,ps_global) ? "F1" : "?");
    	        }
            }
            break;

        } /* End of switch() */

    } /* End of while() -- loop executing commands */

    ps_global->scroll_text_level--;
    if(level)
      pop_scroll_state();	/* pop the old state handles zeroing text */
    else
      zero_scroll_text();	/* very important to zero out on return!!! */

    return;
}


/*----------------------------------------------------------------------
   Search text being viewed (help or message)

      Args: q_line      -- The screen line to prompt for search string on
            start_line  -- Line number in text to begin search on

    Result: returns line number string was found on
            -1 for cancel
            -2 if not found

 ---*/

search_text(q_line, start_line)
     int            q_line, start_line;
{
    static char search_string[MAX_SEARCH+1] = { '\0' };
    char        prompt[MAX_SEARCH+50], nsearch_string[MAX_SEARCH+1];
    HelpType	help;
    int         rc;

    sprintf(prompt, "Word to search for [%s] : ", search_string);
    help = NO_HELP;
    nsearch_string[0] = '\0';

    while(1) {
        rc = optionally_enter(nsearch_string, q_line, 0, MAX_SEARCH, 1, 0,
                              prompt, NULL, help, 0);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_searchview : NO_HELP;
            continue;
        }
        if(rc != 4)
          break;
    }

    if(rc == 1 || (search_string[0] == '\0' && nsearch_string[0] == '\0'))
      return(-1);

    if(nsearch_string[0] != '\0')
      strcpy(search_string, nsearch_string);


    rc = search_scroll_text(start_line, search_string);
    return(rc);
}



/*----------------------------------------------------------------------
    Saved state for scrolling text 
 ----*/
static struct scroll_text {
    void *text;          /* Original text */
    char **text_lines;   /* Lines to display. Malloced once for life of Pine */
    FILE  *findex;	 /* file containing line offsets in another file */
    char  *fname;	 /* name of file containing line offsets */
    int top_text_line;   /* index into text array of line on top of screen */
    int num_lines;       /* Calculated number lines of text to display */
    int lines_allocated; /* size of array text_lines */
    int screen_width;    /* screen width of current formatting */
    int screen_start_line; /* First line on screen that we scroll text on */
    int screen_other_lines;/* Line ons screen not used for scroll text */
    short *line_lengths;   /* Lengths of lines for display, not \0 terminatd*/
    SourceType source;	/* How to interpret "text" field */
} st = {NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, NULL, CharStar},
  scroll_stack;


#define	PGSIZE (ps_global->ttyo->screen_rows - st.screen_other_lines)



void
push_scroll_state()
{
    scroll_stack    = st;		/* save old state */
    st.text         = NULL;		/* prepare st struct for new data */
    st.text_lines   = NULL;
    st.findex       = NULL;
    st.fname        = NULL;
    st.top_text_line = st.num_lines = st.lines_allocated = st.screen_width = 0;
    st.screen_start_line = st.screen_other_lines = 0;
    st.line_lengths = NULL;
    st.source       = CharStar;
}


void
pop_scroll_state()
{
    zero_scroll_text();			/* clean up each line */
    end_scroll_text();			/* clean up array of pointers */
    st = scroll_stack;			/* restore old state */
}

/*----------------------------------------------------------------------
      Save all the data for scrolling text and paint the screen


  ----*/
void
set_scroll_text(text, current_line, screen_top, screen_other, source)
     void       *text;
     int         current_line, screen_top, screen_other;
     SourceType  source;
{
    /* save all the stuff for possible asynchronous redraws */
    st.text               = text;
    st.top_text_line      = current_line;
    st.screen_start_line  = screen_top;
    st.screen_other_lines = screen_other;
    st.source             = source;
    
    st.screen_width = -1; /* Force text formatting calculation */

}



/*----------------------------------------------------------------------
     Redraw the text on the screen, possibly reformatting if necessary

   Args None

 ----*/
void
redraw_scroll_text()
{
    int i, len, offset;

    format_scroll_text();

    offset = (st.source == FileStar) ? 0 : st.top_text_line;

    /*---- Actually display the text on the screen ------*/
    for(i = 0; i < ps_global->ttyo->screen_rows - st.screen_other_lines; i++){
        MoveCursor((i + st.screen_start_line), 0);
        CleartoEOLN();
        if((offset + i) < st.num_lines) {
            len = min(st.line_lengths[offset + i], st.screen_width);
            PutLine0n8b(i + st.screen_start_line, 0,
                      st.text_lines[offset + i], len);
        }
    }
    fflush(stdout);
}




/*----------------------------------------------------------------------
  Free memory used as scrolling buffers for text on disk.  Also mark
  text_lines as available
  ----*/
void
zero_scroll_text()
{
    register int i;

    for(i = 0; i < st.lines_allocated; i++)
      if(st.source == FileStar && st.text_lines[i])
	fs_give((void **)&st.text_lines[i]);
      else
	st.text_lines[i] = NULL;

    if(st.source == FileStar && st.findex != NULL){
	fclose(st.findex);
	st.findex = NULL;
	if(st.fname){
	    unlink(st.fname);
	    fs_give((void **)&st.fname);
	}
    }

#ifdef	DOS
    st.lines_allocated = 0;
    if(st.text_lines)
      fs_give((void **)&st.text_lines);

    if(st.line_lengths)
      fs_give((void **)&st.line_lengths);
#endif
}



/*----------------------------------------------------------------------
  Free residual memory used by scroll text functions
  ----*/
void
end_scroll_text()
{
    if(st.lines_allocated != 0) {
        fs_give((void **)&st.text_lines);
        fs_give((void **)&st.line_lengths);
    }
}
    



/*----------------------------------------------------------------------

Always format at least 20 chars wide. Wrapping lines would be crazy for
screen widths of 1-20 characters 
  ----*/
void
format_scroll_text()
{
    int             i, line_len;
    char           *p, **pp;
#ifdef	X_NEW
    char           *max_line;
#endif
    register short  *ll;
    register char  **tl, **tl_end, *last_space;

    if(st.screen_width == ps_global->ttyo->screen_cols)
        return;

    st.screen_width = max(20, ps_global->ttyo->screen_cols);

    if(st.lines_allocated == 0) {
        st.lines_allocated = TYPICAL_BIG_MESSAGE_LINES;
        st.text_lines   = (char **)fs_get(st.lines_allocated *sizeof(char *));
	memset(st.text_lines, 0, st.lines_allocated * sizeof(char *));
        st.line_lengths = (short *)fs_get(st.lines_allocated *sizeof(short));
    }

    tl     = st.text_lines;
    ll     = st.line_lengths;
    tl_end = &st.text_lines[st.lines_allocated];

    if(st.source == CharStarStar) {
        /*---- original text is already list of lines -----*/
        /*   The text could be wrapped nicely for narrow screens; for now
             it will get truncated as it is displayed */
        for(pp = (char **)st.text; *pp != NULL;) {
            *tl++ = *pp++;
            *ll++ = st.screen_width;
            if(tl >= tl_end) {
                int index = tl - st.text_lines;
                st.lines_allocated *= 2;
                fs_resize((void **)&st.text_lines,
                          st.lines_allocated * sizeof(char *));
                fs_resize((void **)&st.line_lengths,
                          st.lines_allocated*sizeof(short));
                tl     = &st.text_lines[index];
                ll     = &st.line_lengths[index];
                tl_end = &st.text_lines[st.lines_allocated];
            }
        }

	st.num_lines = tl - st.text_lines;
    } else if (st.source == CharStar) {
        /*------ Format the plain text ------*/
        for(p = (char *)st.text; *p; ) {
            *tl = p;
#ifdef X_NEW
            max_line = p + st.screen_width;
            while(*p != '\r' && *p != '\n' && p < max_line && *p) p++;
#else
            line_len = 0;            
            last_space = NULL;

            while(*p) {
                if(*p == '\r' || *p == '\n') {
                    break;
                } else if(*p == '\033') {
                    int x;
                    if(x = match_iso2022(p))  {
                        /* Don't count 2022 escape in length */
                        while(x--)
                          p++;
                    } else {
                        /* A plain escape, not sure what it means in the 
                           text, but count it's length anyway to be safe 
                         */
                        p++;
                        line_len++;
                    }
                } else if(*p == '\t') {
		    while(line_len < st.screen_width - 1 
			  && ((++line_len)&0x07) != 0) /* add tab's spaces */
		      ;

                    p++;
                    last_space = p;
                } else if(*p >= '\01' && *p  <= '\04') {
                    /* Don't count the bold and underline hacks */
                    p++;
                } else if(*p == ' ') {
                    p++;
                    last_space = p;
                    line_len++;
                } else {
                    p++;
                    line_len++;
                }
                if(line_len >= st.screen_width) {
                    if(last_space == NULL) {
                        break;
                    } else {
                        p = last_space;
                        break;
                    }
                }
            }
        
#endif
            *ll = p - *tl;
            ll++; tl++;
            if(tl >= tl_end) {
                int index = tl - st.text_lines;
                st.lines_allocated *= 2;
                fs_resize((void **)&st.text_lines,
                          st.lines_allocated * sizeof(char *));
                fs_resize((void **)&st.line_lengths,
                         st.lines_allocated*sizeof(short));
                tl     = &st.text_lines[index];
                ll     = &st.line_lengths[index];
                tl_end = &st.text_lines[st.lines_allocated];
            }      
            if(*p == '\r' && *(p+1) == '\n') 
              p += 2;
            else if(*p == '\n' || *p == '\r')
              p++;
             continue;
        }

	st.num_lines = tl - st.text_lines;
    }
    else {
	/*------ Display text is in a file --------*/

	/*
	 * This is pretty much only useful under DOS where we can't fit
	 * all of big messages in core at once.  This scheme makes
	 * some simplifying assumptions:
	 *  1. Lines are on disk just the way we'll display them.  That
	 *     is, line breaks and such are left to the function that
	 *     writes the disk file to catch and fix.
	 *  2. We get away with this mainly because the DOS display isn't
	 *     going to be resized out from under us.
	 *
	 * The idea is to use the already alloc'd array of char * as a 
	 * buffer for sections of what's on disk.  We'll set up the first
	 * few lines here, and read new ones in as needed in 
	 * scroll_scroll_text().
	 *  
	 * but first, make sure there are enough buffer lines allocated
	 * to serve as a place to hold lines from the file.
	 */
	if(st.lines_allocated < (2 * PGSIZE) + 1){
	    /*
	     * clear lines already alloc'd 
	     */
	    for(i = 0; i < st.lines_allocated; i++)
	      if(st.text_lines[i]) 		/* clear alloc'd lines */
		fs_give((void **)&st.text_lines[i]);

	    st.lines_allocated = (2 * PGSIZE) + 1; /* resize */

	    fs_resize((void **)&st.text_lines,
		      st.lines_allocated * sizeof(char *));
	    memset(st.text_lines, 0, st.lines_allocated * sizeof(char *));
	    fs_resize((void **)&st.line_lengths,
		      st.lines_allocated*sizeof(short));
	}

	for(i = 0; i <= PGSIZE; i++)
	  if(st.text_lines[i] == NULL)
	    st.text_lines[i] = (char *)fs_get(st.screen_width*sizeof(char));

	tl = &st.text_lines[i];

	st.num_lines = make_file_index();

	ScrollFile(st.top_text_line);		/* then load them up */

    }

    *tl = NULL;
}




/*
 * ScrollFile - scroll text into the st struct file making sure 'line'
 *              of the file is the one first in the text_lines buffer.
 *
 *   NOTE: talk about massive potential for tuning...
 *         Goes without saying this is still under constuction
 */
void
ScrollFile(line)
int line;
{
    register int  i;
             long x;

    if(line == 0){		/* reset and load first couple of pages */
	fseek((FILE *) st.text, 0L, 0);
    }
    else{
	/*** do stuff to get the file pointer into the right place ***/
	/*
	 * BOGUS: this is painfully crude right now, but I just want to get
	 * it going. 
	 *
	 * possibly in the near furture, an array of indexes into the 
	 * file that are the offset for the beginning of each line will
	 * speed things up.  Of course, this
	 * will have limits, so maybe a disk file that is an array
	 * of indexes is the answer.
	 */
	fseek((FILE *) st.findex, (size_t)(line-1) * sizeof(long), 0);
	if(fread(&x, sizeof(long), (size_t)1, (FILE *)st.findex) != 1){
	    return;
	}

	fseek((FILE *) st.text, x, 0);
    }

    for(i = 0; i < PGSIZE; i++){
	if(!st.text_lines || !st.text_lines[i]
	   || fgets(st.text_lines[i],st.screen_width,(FILE *)st.text) == NULL)
	  break;

	st.line_lengths[i] = strlen(st.text_lines[i]);
    }

    for(; i < PGSIZE; i++)
      if(st.text_lines && st.text_lines[i]) /* blank out any unused lines */
	*st.text_lines[i] = '\0';
}


/*
 * make_file_index - do a single pass over the file containing the text
 *                   to display, recording line lengths and offsets.
 *    NOTE: This is never really to be used on a real OS with virtual
 *          memory.  This is the whole reason st.findex exists.  Don't
 *          want to waste precious memory on a stupid array that could 
 *          be very large.
 */
int
make_file_index()
{
    register int l = 0;
    long i = 0;

    if(!st.findex){
	if(!st.fname)
	  st.fname = (char *)temp_nam(NULL, "pi");

	if((st.findex = (void *)fopen(st.fname,"w+b")) == NULL)
	  return(0);
    }
    else
      fseek(st.findex, 0L, 0);

    fseek((FILE *)st.text, 0L, 0);

    fwrite((void *)&i, sizeof(long), (size_t)1, st.findex);
    while(fgets(tmp_20k_buf, st.screen_width, (FILE *)st.text) != NULL){
	i = ftell((FILE *)st.text);
	fwrite((void *)&i, sizeof(long), (size_t)1, st.findex);
	l++;
    }

    fseek((FILE *)st.text, 0L, 0);

    return(l);
}



/*----------------------------------------------------------------------
     Scroll the text on the screen

   Args:  new_top_line -- The line to be displayed on top of the screen
          redraw -- Flag to force a redraw even in nothing changed 

 ----*/
void
scroll_scroll_text(new_top_line, redraw)
     int new_top_line, redraw;
{
    int num_display_lines, len, l;
    num_display_lines = ps_global->ttyo->screen_rows - st.screen_other_lines;

    if(st.top_text_line == new_top_line && !redraw)
      return;

    format_scroll_text();

    if(st.source == FileStar)
      ScrollFile(new_top_line);		/* set up new st.text_lines */

    /* --- 
       Check out the scrolling situation. If we want to scroll, but BeginScroll
       says we can't then repaint,  + 10 is so we repaint most of the time.
      ----*/
    if(redraw ||
       (st.top_text_line - new_top_line + 10 >= num_display_lines ||
        new_top_line - st.top_text_line + 10 >= num_display_lines) ||
	BeginScroll(st.screen_start_line,
                    st.screen_start_line + num_display_lines - 1) != 0) {
        /* Too much text to scroll, or can't scroll -- just repaint */
        st.top_text_line = new_top_line;
        redraw_scroll_text();
        return;
    }

    if(new_top_line > st.top_text_line  ) {
        /*------ scroll down ------*/
        while(new_top_line > st.top_text_line) {
            ScrollRegion(1);
	    if(st.source == FileStar)
	      l = num_display_lines - (new_top_line - st.top_text_line);
	    else
              l = st.top_text_line + num_display_lines;
            if(l < st.num_lines) {
                len = min(st.line_lengths[l], st.screen_width);
                PutLine0n8b(st.screen_start_line + num_display_lines - 1, 0,
                          st.text_lines[l], len);
            }
            st.top_text_line++;
        }
    } else {
        /*------ scroll up -----*/
        while(new_top_line < st.top_text_line) {
            ScrollRegion(-1);
            st.top_text_line--;
	    if(st.source == FileStar)
	      l = st.top_text_line - new_top_line;
	    else
	      l = st.top_text_line;
	    len = min(st.line_lengths[l], st.screen_width);
	    PutLine0n8b(st.screen_start_line, 0, st.text_lines[l], len);
        }
    }
    EndScroll();
    fflush(stdout);
}



/*----------------------------------------------------------------------
     Return the number of lines of text with the current formatting
  ----*/
int
get_scroll_text_lines()
{
    return(st.num_lines);
}



/*----------------------------------------------------------------------
      Search the set scrolling text

   Args:   start_line -- line to start searching on
           word       -- string to search for

   Returns: the line the word was found on or -2 if it wasn't found

 ----*/
int
search_scroll_text(start_line, word)
     char *word;
     int   start_line;
{
    char tmp[MAX_SCREEN_COLS+1];
    int  l, offset, dlines;

    dlines = ps_global->ttyo->screen_rows - st.screen_other_lines;
    offset = (st.source == FileStar) ? st.top_text_line + 1 : 0;

    for(l = start_line+1; l < st.num_lines; l++) {
	if(st.source == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	strncpy(tmp, st.text_lines[l-offset], 
		min(st.line_lengths[l-offset], MAX_SCREEN_COLS));
	tmp[min(st.line_lengths[l-offset], MAX_SCREEN_COLS) + 1] = '\0';
        if(srchstr(tmp, word)!= NULL)
          break;
    }

    if(l < st.num_lines) {
        return(l);
    }

    if(st.source == FileStar)		/* wrap offset */
      ScrollFile(offset = 0);

    for(l = 0; l < start_line; l++) {
	if(st.source == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	strncpy(tmp, st.text_lines[l-offset], 
		min(st.line_lengths[l-offset], MAX_SCREEN_COLS));
	tmp[min(st.line_lengths[l-offset], MAX_SCREEN_COLS) + 1] = '\0';
        if(srchstr(tmp, word)!= NULL)
          break;
    }

    return(l == start_line ? -2 : l);
}
     

char *
display_parameters(parameter_list)
     PARAMETER *parameter_list;
{
    int        longest_attribute;
    PARAMETER *p;
    char      *d;
    size_t     need;

    if(parameter_list == NULL) {
        tmp_20k_buf[0] = '\0';

    } else {
        longest_attribute = 0;
        need = 0;

        for(p = parameter_list; p != NULL; p = p->next){
          longest_attribute = max(longest_attribute, (p->attribute == NULL ?
                                                      0 :
                                                      strlen(p->attribute)));
          /*
           * This is remote-message-controlled content (MIME Content-Type
           * parameters, e.g. a filename= value) formatted into
           * tmp_20k_buf one parameter at a time with no bound of its
           * own, so size the buffer for the actual worst case instead
           * of assuming the baseline allocation is always big enough.
           */
          need += (p->attribute ? strlen(p->attribute) : 0)
                  + (p->value ? strlen(p->value) : 0) + 16;
        }

        longest_attribute = min(longest_attribute, 11);
        grow_tmp_20k_buf(need + 16);

        d = tmp_20k_buf;
        for(p = parameter_list; p != NULL; p = p->next) {
            sprintf(d, "%-*s: %s\n", longest_attribute,
                    p->attribute != NULL ? p->attribute : "",
                    p->value     != NULL ? p->value     : "");
            d += strlen(d);
        }
    }
    return(tmp_20k_buf);
}
