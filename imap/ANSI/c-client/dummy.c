/*
 * Program:	Dummy routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	9 May 1991
 * Last Edited:	28 September 1993
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


#include <stdio.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "mail.h"
#include "osdep.h"
#include <pwd.h>
#include <sys/file.h>
#include "dummy.h"
#include "misc.h"

/* Dummy routines */


/* Driver dispatch used by MAIL */

/*
 * Slots commented "unreachable" below can never actually be invoked in
 * this build and are served by one of a handful of shared stub
 * functions (grouped by exact matching signature -- no unsafe casts):
 *   - dummy_open() unconditionally fails, so no stream can ever persist
 *     with dtb == &dummydriver, which means fetchfast/fetchflags/
 *     fetchheader/fetchtext/setflag/clearflag/search/check/expunge/
 *     copy/move/gc can never be dispatched to.
 *   - c-client/mail.c's mail_valid() explicitly excludes the driver
 *     named "dummy" whenever its nopen flag is set, which is how
 *     mail_create()/mail_delete()/mail_rename()/mail_subscribe()/
 *     mail_unsubscribe()/mail_subscribe_bboard()/
 *     mail_unsubscribe_bboard() all resolve their driver -- so
 *     create/delete/rename/subscribe/unsubscribe/subscribe_bboard/
 *     unsubscribe_bboard can never be dispatched to either.
 *   - pine's own bboard/newsgroup UI is gone (see TODO-pdp11-done.md),
 *     so nothing ever calls mail_find_bboards()/mail_find_all_bboards()
 *     -- find_bboards/find_all_bboards can never be dispatched to.
 */
DRIVER dummydriver = {
  "dummy",			/* driver name */
  (DRIVER *) NIL,		/* next driver */
  dummy_valid,			/* mailbox is valid for us */
  dummy_parameters,		/* manipulate parameters */
  dummy_find,			/* find mailboxes */
  dummy_dead_find,		/* find bboards -- unreachable */
  dummy_find_all,		/* find all mailboxes */
  dummy_dead_find,		/* find all bboards -- unreachable */
  dummy_dead_mbx,		/* subscribe to mailbox -- unreachable */
  dummy_dead_mbx,		/* unsubscribe from mailbox -- unreachable */
  dummy_dead_mbx,		/* subscribe to bboard -- unreachable */
  dummy_dead_mbx,		/* unsubscribe from bboard -- unreachable */
  dummy_dead_mbx,		/* create mailbox -- unreachable */
  dummy_dead_mbx,		/* delete mailbox -- unreachable */
  dummy_dead_mbx3,		/* rename mailbox -- unreachable */
  dummy_open,			/* open mailbox */
  dummy_close,			/* close mailbox */
  dummy_dead_find,		/* fetch message "fast" attributes -- unreachable */
  dummy_dead_find,		/* fetch message flags -- unreachable */
  dummy_fetchstructure,		/* fetch message structure */
  dummy_dead_fetch,		/* fetch message header only -- unreachable */
  dummy_dead_fetch,		/* fetch message body only -- unreachable */
  dummy_fetchbody,		/* fetch message body section */
  dummy_dead_setflag,		/* set message flag -- unreachable */
  dummy_dead_setflag,		/* clear message flag -- unreachable */
  dummy_dead_find,		/* search for message based on criteria -- unreachable */
  dummy_ping,			/* ping mailbox to see if still alive */
  dummy_dead_stream,		/* check for new messages -- unreachable */
  dummy_dead_stream,		/* expunge deleted messages -- unreachable */
  dummy_dead_mbx3,		/* copy messages to another mailbox -- unreachable */
  dummy_dead_mbx3,		/* move messages to another mailbox -- unreachable */
  dummy_append,			/* append string message to mailbox */
  dummy_gc			/* garbage collect stream */
};

/* Dummy validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *dummy_valid (char *name)
{
  return (name && *name) ? &dummydriver : NIL;
}


/* Dummy manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *dummy_parameters (long function,void *value)
{
  return NIL;
}

/* Dummy find list of subscribed mailboxes
 * Accepts: mail stream
 *	    pattern to search
 */

void dummy_find (MAILSTREAM *stream,char *pat)
{
  void *s = NIL;
  char *t;
  if (*pat == '{') return;	/* local only */
  if (t = sm_read (&s)) { 	/* if have subscription database */
    do if ((*t != '{') && (*t != '*') && strcmp (t,"INBOX") && pmatch (t,pat))
      mm_mailbox (t);
    while (t = sm_read (&s));	/* until no more subscriptions */
  }
  else dummy_find_all (stream,pat);
}


/* Dummy find list of all mailboxes
 * Accepts: mail stream
 *	    pattern to search
 */

void dummy_find_all (MAILSTREAM *stream,char *pat)
{
  DIR *dirp;
  struct direct *d;
  char tmp[MAILTMPLEN],file[MAILTMPLEN];
  char *s,*t;
  int i = 0;
  if (*pat == '{') return;	/* local only */
  if (s = strrchr (pat,'/')) {	/* directory specified in pattern? */
    strncpy (file,pat,i = (++s) - pat);
    file[i] = '\0';		/* tie off prefix */
    t = dummy_file (tmp,pat);	/* make fully-qualified file name */
				/* tie off directory name */
    if (s = strrchr (t,'/')) *s = '\0';
  }
  else t = myhomedir ();	/* use home directory to search */
  if (dirp = opendir (t)) {	/* now open that directory */
    while (d = readdir (dirp)) {/* for each directory entry */
      strcpy (file + i,d->d_name);
      if (pmatch (file,pat)) mm_mailbox (file);
    }
    closedir (dirp);		/* flush directory */
  }
				/* always an INBOX */
  if (pmatch ("INBOX",pat)) mm_mailbox ("INBOX");
}

/* Dummy stub for the unreachable subscribe/unsubscribe/create/delete
 * mailbox and bboard slots (see the comment above dummydriver's
 * initializer)
 */

long dummy_dead_mbx (MAILSTREAM *stream,char *mailbox)
{
  return NIL;
}

/* Dummy stub for the unreachable rename/copy/move slots (see the
 * comment above dummydriver's initializer)
 */

long dummy_dead_mbx3 (MAILSTREAM *stream,char *s1,char *s2)
{
  fatal ("Impossible dummy driver operation");
  return NIL;
}

/* Dummy open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *dummy_open (MAILSTREAM *stream)
{
  int fd;
  char tmp[MAILTMPLEN];
				/* OP_PROTOTYPE call or silence */
  if (!stream || stream->silent) return NIL;
  if (*stream->mailbox == '*')	/* is it a bboard? */
    sprintf (tmp,"No such bboard: %s",stream->mailbox+1);
				/* remote specification? */
  else if (*stream->mailbox == '{')
    sprintf (tmp,"Invalid remote specification: %s",stream->mailbox);
  else if ((fd = open (dummy_file (tmp,stream->mailbox),O_RDONLY,NIL)) < 0)
    sprintf (tmp,"%s: %s",strerror (errno),stream->mailbox);
  else {			/* must be bogus format file */
    sprintf (tmp,"%s is not a mailbox",stream->mailbox);
    close (fd);			/* toss out the fd */
  }
  mm_log (tmp,ERROR);
  return NIL;			/* always fails */
}


/* Dummy close
 * Accepts: MAIL stream
 */

void dummy_close (MAILSTREAM *stream)
{
  /* Exit quietly */
}

/* Dummy stub for the unreachable find_bboards/find_all_bboards/
 * fetchfast/fetchflags/search slots (see the comment above
 * dummydriver's initializer)
 */

void dummy_dead_find (MAILSTREAM *stream,char *pat)
{
  fatal ("Impossible dummy driver operation");
}


/* Dummy fetch envelope
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to return body
 * Returns: envelope of this message, body returned in body value
 */

ENVELOPE *dummy_fetchstructure (MAILSTREAM *stream,long msgno,BODY **body)
{
  fatal ("Impossible dummy_fetchstructure");
  return NIL;
}


/* Dummy stub for the unreachable fetchheader/fetchtext slots (see the
 * comment above dummydriver's initializer)
 */

char *dummy_dead_fetch (MAILSTREAM *stream,long msgno)
{
  fatal ("Impossible dummy driver operation");
  return NIL;
}


/* Berkeley fetch message body as a structure
 * Accepts: Mail stream
 *	    message # to fetch
 *	    section specifier
 * Returns: pointer to section of message body
 */

char *dummy_fetchbody (MAILSTREAM *stream,long m,char *sec,unsigned long *len)
{
  fatal ("Impossible dummy_fetchbody");
  return NIL;
}


/* Dummy stub for the unreachable setflag/clearflag slots (see the
 * comment above dummydriver's initializer)
 */

void dummy_dead_setflag (MAILSTREAM *stream,char *sequence,char *flag)
{
  fatal ("Impossible dummy driver operation");
}


/* Dummy ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

long dummy_ping (MAILSTREAM *stream)
{
  fatal ("Impossible dummy_ping");
  return NIL;
}


/* Dummy stub for the unreachable check/expunge slots (see the comment
 * above dummydriver's initializer)
 */

void dummy_dead_stream (MAILSTREAM *stream)
{
  fatal ("Impossible dummy driver operation");
}


/* Dummy append message string
 * Accepts: mail stream
 *	    destination mailbox
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long dummy_append (MAILSTREAM *stream,char *mailbox,STRING *message)
{
  int fd = -1,e;
  char tmp[MAILTMPLEN];
				/* see if such a file */
  if ((*mailbox != '*') && (*mailbox != '{') &&
      (fd = open (dummy_file (tmp,mailbox),O_RDONLY,NIL)) < 0) {
    if ((e = errno) == ENOENT)	/* failed, was it no such file? */
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
    sprintf (tmp,"%s: %s",strerror (e),mailbox);
  }
  else {			/* must be bogus format file */
    sprintf (tmp,"%s is not a valid mailbox",mailbox);
    close (fd);			/* toss out the fd */
  }
  mm_log (tmp,ERROR);		/* pass up error */
  return NIL;			/* always fails */
}


/* Dummy garbage collect stream
 * Accepts: mail stream
 *	    garbage collection flags
 */

void dummy_gc (MAILSTREAM *stream,long gcflags)
{
  fatal ("Impossible dummy_gc");
}

/* Dummy mail generate file string
 * Accepts: temporary buffer to write into
 *	    mailbox name string
 * Returns: local file string
 */

char *dummy_file (char *dst,char *name)
{
  struct passwd *pw;
  char *s,*t,tmp[MAILTMPLEN];
  /*
   * name is a caller-typed candidate mailbox name with no length cap
   * upstream (e.g. straight from the composer's Fcc field, and unlike
   * bezerk_valid(), dummy_valid() accepts any nonempty name with no
   * length check of its own); every caller of dummy_file() passes a
   * MAILTMPLEN-sized dst, by convention across this whole file. Bounded
   * well under MAILTMPLEN (not just under it) to leave headroom for the
   * "~"/default cases below, which prepend a home directory to (part of)
   * name into the same MAILTMPLEN dst.
   */
  if (strlen (name) >= MAILTMPLEN/2) name = "/NOSUCHNAME";
  switch (*name) {
  case '/':			/* absolute file path */
    strcpy (dst,name);		/* copy the mailbox name */
    break;
  case '~':			/* home directory */
    if (name[1] == '/') t = myhomedir ();
    else {
      /* name+1 is a caller-typed ~user prefix with no length cap upstream
	 -- truncate to fit tmp rather than overflowing it. A truncated
	 name will just fail getpwnam() and fall through to
	 "/NOSUCHUSER" below, same as any other nonexistent user. */
      strncpy (tmp,name + 1,MAILTMPLEN-1);
      tmp[MAILTMPLEN-1] = '\0';
      if (s = strchr (tmp,'/')) *s = '\0';
      t = ((pw = getpwnam (tmp)) && pw->pw_dir) ? pw->pw_dir : "/NOSUCHUSER";
    }
    sprintf (dst,"%s%s",t,(s = strchr (name,'/')) ? s : "");
    break;
  default:			/* any other name */
    sprintf (dst,"%s/%s",myhomedir (),name);
  }
  return dst;
}
