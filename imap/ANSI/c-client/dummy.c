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

/*
 * dummy is no longer a registered DRIVER (see TODO-pdp11-done.md) --
 * these are now plain helper functions c-client/mail.c and bezerk.c
 * call directly: dummy_find()/dummy_find_all() do the real directory-
 * scanning wildcard folder search bezerk.c has no implementation of
 * its own, and dummy_open_failure()/dummy_append_failure() produce
 * the correct user-facing error text (including the "[TRYCREATE]"
 * hint) when mail_open()/mail_append() find no driver willing to
 * claim a mailbox name.
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

/* Dummy open failure message
 * Accepts: candidate mailbox name
 *
 * Called by mail_open() when no driver claims a mailbox name, to
 * produce the same specific error text this used to produce as a
 * driver's open() method (mail_open() itself only knows how to print
 * a generic "no such mailbox").
 */

void dummy_open_failure (char *name)
{
  int fd;
  char tmp[MAILTMPLEN];
  if (*name == '*')		/* is it a bboard? */
    sprintf (tmp,"No such bboard: %s",name+1);
				/* remote specification? */
  else if (*name == '{')
    sprintf (tmp,"Invalid remote specification: %s",name);
  else if ((fd = open (dummy_file (tmp,name),O_RDONLY,NIL)) < 0)
    sprintf (tmp,"%s: %s",strerror (errno),name);
  else {			/* must be bogus format file */
    sprintf (tmp,"%s is not a mailbox",name);
    close (fd);			/* toss out the fd */
  }
  mm_log (tmp,ERROR);
}

/* Dummy append failure message
 * Accepts: mail stream
 *	    destination mailbox
 * Returns: NIL, always
 *
 * Called by mail_append() when no driver claims the destination
 * mailbox, to produce the "[TRYCREATE]" hint (via mm_notify()) pine's
 * mm_notify() (pine/mailbox.c) depends on to prompt "Folder ... doesn't
 * exist. Create?" -- the same message this used to produce as a
 * driver's append() method.
 */

long dummy_append_failure (MAILSTREAM *stream,char *mailbox)
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
   * bezerk_valid(), nothing here checks for the mailbox actually
   * existing); every caller of dummy_file() passes a MAILTMPLEN-sized
   * dst, by convention across this whole file. Bounded well under
   * MAILTMPLEN (not just under it) to leave headroom for the
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
