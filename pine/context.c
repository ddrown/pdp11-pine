#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: context.c,v 4.25 1993/12/01 02:56:46 mikes Exp $";
#endif
/*
 * Program:	Mailbox Context Management
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	27 February 1993
 * Last Edited:	27 February 1993
 *
 * Copyright 1993 by the University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appears in all copies and that both the
 * above copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the University of Washington not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  This software is made
 * available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "../c-client/misc.h"
#include "headers.h"

char *current_context = NIL;	/* current context for FIND */

/* Mailbox found
 * Accepts: Mailbox name as FQN
 */

void mm_mailbox (string)
char *string;
{
  context_fqn_to_an (context_mailbox,current_context,string);
}


/* BBoard found
 * Accepts: BBoard name as FQN
 */

void mm_bboard (string)
char *string;
{
  context_fqn_to_an (context_bboard,current_context,string);
}

/* Convert fully-qualified name to ambiguous name and send to function
 * Accepts: find return function
 *	    current context
 *	    fully-qualified name
 */

void context_fqn_to_an (f, context_string, fqn)
find_return f;
char *context_string,*fqn;
{
  char tmp[MAILTMPLEN];
  char *ret = fqn;
  char *delimiter,*prefix,*suffix;
  int pfxlen = 0;
  int sfxlen = 0;
  int fqnlen = strlen (fqn);
  int anlen;
  char context[MAILTMPLEN];

  if (context_string && *context_string && /* have a valid context? */
      context_digest(context_string, context, NULL, NULL) == NULL){
    delimiter = strstr(context, "%s");
				/* determine prefix */
    if (prefix = (pfxlen = delimiter - context) ? strncpy (tmp,context,pfxlen)
	                                          : NIL)
	prefix[pfxlen] = '\0';
				/* determine suffix */
    if (suffix = delimiter[2] ? delimiter+2 : NIL) sfxlen = strlen (suffix);

    /*
     * The fqn prefix may not match the context prefix.
     * Two cases so far:
     *   1) mail_find on open stream returns {host}context
     *   2) mail_find_all returns full paths though find string
     *      relative
     */

    if((ret=((!prefix) ? fqn : strstr(fqn, prefix))) != NULL){
	fqn = ret;
	fqnlen = strlen(fqn);
    }

    /* Determine if FQN is proper match for given prefix and suffix.  This
     * should normally always happen, but if it doesn't, it probably means
     * that this test isn't hairy enough and needs more work.
     */
    if (((anlen = fqnlen - (pfxlen + sfxlen)) < 1) ||
	(pfxlen && strncmp (prefix,fqn,pfxlen) != 0) ||
	(sfxlen && strcmp (suffix,fqn + fqnlen - sfxlen) != 0)) {
      sprintf (tmp,"Found mailbox outside context: %s",fqn);
      mm_log (tmp,(long) NIL);
      pfxlen = 0;
      anlen  = fqnlen;
    }
				/* copy the an */
    strcpy (ret = tmp,fqn+pfxlen);
    ret[anlen] = '\0';		/* tie off string */
  }
  (*f) (ret);			/* in case no context? */
}


/* Context Manager context format digester
 * Accepts: context string and buffers for sprintf-suitable context,
 *          remote context (for display), and view string
 * Returns: NULL if successful, else error string
 *
 * Comments: OK, here's what we expect a fully qualified context to
 *           look like:
 *
 * [<cntxt>] "[" [<view>] "]" [<cntxt>]
 *
 *         2) It's understood that double "[" or "]" are used to
 *            quote a literal '[' or ']' in a context name.
 *
 *         3) an empty view in context implies a view of '*', so that's
 *            what get's put in the view string
 *
 */
char *
context_digest(char *context, char *scontext, char *rcontext, char *view)
{
    char *p, *viewp = view, tmp[MAILTMPLEN];
    int   i = 0;

    if((p = context) == NULL || *p == '\0'){
	if(scontext)			/* so the caller can apply */
	  strcpy(scontext, "%s");	/* folder names as is.     */

	return(NULL);			/* no error, just empty context */
    }

    if(!rcontext && scontext)		/* place to collect rcontext ? */
      rcontext = tmp;

    for(; *p ; p++){			/* get thru context */
	if(rcontext){
	    /*
	     * rcontext defaults to this function's own local tmp[MAILTMPLEN]
	     * (above) when the caller doesn't supply its own -- context
	     * comes from a .pinerc folder-collection string with no length
	     * cap upstream, so bound the copy rather than overflow tmp.
	     */
	    if(i >= MAILTMPLEN - 1)
	      return("Context too long");
	    rcontext[i++] = *p;		/* copy if requested */
	}

	if(*p == '['){			/* done? */
	    if(*++p == '\0')		/* look ahead */
	      return("Unbalanced '['");

	    if(*p != '[')		/* not quoted: "[[" */
	      break;
	}
    }

    if(*p == '\0')
      return("No '[' in context");

    for(; *p ; p++){			/* possibly in view portion ? */
	if(*p == ']'){
	    if(*(p+1) == ']')		/* is quoted */
	      p++;
	    else
	      break;
	}

	if(viewp)
	  *viewp++ = *p;
    }

    if(*p != ']')
      return("No ']' in context");

    for(; *p ; p++){			/* trailing context ? */
	if(rcontext){
	    if(i >= MAILTMPLEN - 1)
	      return("Context too long");
	    rcontext[i++] = *p;
	}
    }

    if(rcontext) rcontext[i] = '\0';
    if(viewp) {
	if(viewp == view)
	  *viewp++ = '*';

	*viewp = '\0';
    }

    if(scontext){			/* sprint'able context request ? */
	if(*context == '*')
	  *scontext++ = *context++;

	for(p = rcontext; *p ; p++){
	    if(*p == '[' && *(p+1) == ']'){
		*scontext++ = '%';	/* replace "[]" with "%s" */
		*scontext++ = 's';
		p++;			/* skip ']' */
	    }
	    else
	      *scontext++ = *p;
	}

	*scontext = '\0';
    }

    return(NULL);			/* no problems to report... */
}


/* Context Manager apply name to context
 * Accepts: buffer to write, context to apply, ambiguous folder name
 * Returns: buffer filled with fully qualified name in context
 *          No context applied if error
 */
void
context_apply(b, c, n)
char *b, *c, *n;
{
    char tmp[MAILTMPLEN];

    if(context_digest(c, tmp, NULL, NULL) == NULL){
      /* tmp is context_digest()'s own "[]"->"%s" template, not
       * external input -- always exactly one %s. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
      sprintf(b, tmp, n);		/* everythings ok,  */
#pragma GCC diagnostic pop
    }
    else
      strcpy(b, n);			/* don't apply bogus context */
}


/* Context Manager check if name is ambiguous
 * Accepts: candidate string
 * Returns: T if ambiguous, NIL if fully-qualified
 */

short context_isambig (s)
char *s;
{
				/* first character after possible bboard * */
  char c = (*s == '*') ? s[1] : s[0];
  if(c == '\0' && *s == '*')	/* special case! */
    return(1);
  else
    /*
     * return ambiguous if alphanumeric OR something that starts
     * with "dot".  Now, if this seems to indicate that the client
     * assumes knowledge of the server's naming syntax, well, it does.
     * THIS MUST BE REMOVED!!!!
     */
    return (short) (c == '.' || isalnum (c));
}

/* Context Manager find list of all mailboxes
 * Accepts: context
 *	    mail stream
 *	    pattern to search
 */

void context_find_all (context, stream, pat)
char *context;MAILSTREAM *stream;char *pat;
{
  char tmp[MAILTMPLEN];
				/* must be within context */
  if (!context_isambig (pat)) {
    /*
     * pat is a caller-typed folder/Fcc name with no length cap upstream
     * -- bound both the log message and the copy below rather than
     * overflow tmp.
     */
    sprintf (tmp,"Find of mailbox outside context: %.900s",pat);
    mm_log (tmp,(long) NIL);
    strncpy(tmp, pat, MAILTMPLEN-1);	/* allow find for now */
    tmp[MAILTMPLEN-1] = '\0';
  }
  else{
    current_context = context;	/* note current context */
    context_apply (tmp,context,pat); /* build fully-qualified name */
  }
				/* try to do the find */
  mail_find_all (stream,tmp);
}


/* Context Manager create mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long context_create (context, stream, mailbox)
char *context;MAILSTREAM *stream;char *mailbox;
{
  char tmp[MAILTMPLEN];		/* must be within context */

  if (!context_isambig (mailbox)) {
    char *s, errbuf[MAILTMPLEN];
    if(mailbox[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], mailbox);
    }
    else strcpy(tmp, mailbox);	/* allow create for now */

    sprintf (errbuf,"Create of mailbox outside context: %.988s", tmp);
    mm_log (errbuf,(long) NIL);
  }
  else
    context_apply (tmp,context,mailbox);/* build fully-qualified name */
				/* try to do the create */
  if(!stream){			/* stream not specified? */
     /* This code is called only when the target of a create is a
	remote mailbox.  We depend upon this, because mail_open
	requires a local mailbox to exist even if all you want is
	a prototype back.  */
     if (!(stream = mail_open (NIL,tmp,OP_PROTOTYPE))){
	 sprintf (tmp,"Unable to infer network server from context: %s",
		  mailbox);
	 mm_log (tmp,ERROR);
	 return NIL;
     }
  }

  return mail_create (stream,tmp);
}


/* Context Manager open
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: stream to use on success, NIL on failure
 */

MAILSTREAM *context_open (context, old, name, opt)
char *context;MAILSTREAM *old;char *name;long opt;
{
  char *s,tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
  char host[MAILTMPLEN];

  if (!context) context = "[]";	/* non spec'd, pass name */
  if(context_isambig (name)) context_apply(s = tmp, context, name);
  else if(name[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], name);
      s = tmp;
  }
  else s = name;		/* fully-qualified name */
  return mail_open (old,s,opt);	/* do the normal c-client action */
}


/* Context Manager rename
 * Accepts: context
 *	    mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long context_rename (context, stream, old, new)
char *context;MAILSTREAM *stream;char *old, *new;
{
  char *s,*t,tmp[MAILTMPLEN],tmp2[MAILTMPLEN];
	/* build FQN from ambiguous name */
  if (context_isambig (old)) context_apply (s = tmp,context,old);
  else if(old[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], old);
      s = tmp;
  }
  else s = old;		/* fully-qualified name */
  if (context_isambig (new)) context_apply (t = tmp2,context,new);
  else if(new[0] != '{' && context[0] == '{' && (t = strindex(context, '}'))){
      strncpy(tmp, context, (t - context) + 1);
      strcpy(&tmp[(t - context)+1], new);
      t = tmp;
  }
  else t = new;		/* fully-qualified name */
  return mail_rename (stream,s,t);
}


/* Context Manager delete mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long context_delete (context, stream, name)
char *context;MAILSTREAM *stream;char *name;
{
  char *s,tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
  if (context_isambig (name)) context_apply (s = tmp,context,name);
  else if(name[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], name);
      s = tmp;
  }
  else s = name;		/* fully-qualified name */
  return mail_delete (stream,s);
}


/* Context Manager append message string
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long context_append (context, stream, name, msg)
char *context;MAILSTREAM *stream;char *name;STRING *msg;
{
  char *s,tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
  if (context_isambig (name)) context_apply (s = tmp,context,name);
  else if(name[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], name);
      s = tmp;
  }
  else s = name;		/* fully-qualified name */
  return mail_append (stream,s,msg);
}



/*
 * Context manager stream usefulness test
 * Accepts: context
 *	    mail name
 *	    mailbox name
 *	    mail stream to test against mailbox name
 * Returns: stream if useful, else NIL
 */
MAILSTREAM *
context_same_stream(context, name, stream)
char *context;MAILSTREAM *stream; char *name;
{
  extern MAILSTREAM *same_stream();
  char *s,tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
  if (context_isambig (name)) context_apply (s = tmp,context,name);
  else if(name[0] != '{' && context[0] == '{' && (s = strindex(context, '}'))){
      strncpy(tmp, context, (s - context) + 1);
      strcpy(&tmp[(s - context)+1], name);
      s = tmp;
  }
  else s = name;		/* fully-qualified name */
  return same_stream (s,stream);
}
