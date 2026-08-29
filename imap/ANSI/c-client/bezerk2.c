/*
 * Program:	Berkeley mail routines -- message-level operations
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 December 1989
 * Last Edited:	18 November 1993
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
 * Split out of bezerk.c (see TODO-pdp11-done.md's "Split bezerk.c into
 * two object files" entry): the PDP-11 overlay linker (ld -Z/-Y, driven
 * by mkovmake) can't place part of one .o in one overlay and part in
 * another -- a single object file is the smallest unit it can move
 * around. Once bezerk.c's message-cache rewrite grew bezerk.o past the
 * largest size any single overlay could be (given this program's other,
 * non-negotiable resident-base requirements), no amount of compiler-flag
 * or overlay-count tuning could fit it as one file, only splitting it
 * could. This file holds the per-message operations (fetch/flags/
 * expunge/copy/append); bezerk.c keeps mailbox lifecycle (open/close),
 * parsing, and writing back. The split follows existing seams exactly --
 * every function referenced from the other file was already prototyped
 * in bezerk.h (LOCAL->msgs[]/FILECACHE and friends are shared state, not
 * per-file), so this is a pure move, no interface changes.
 */

#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <signal.h>
#include <sys/time.h>		/* must be before osdep.h */
#include "mail.h"
#include "osdep.h"
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "bezerk.h"
#include "rfc822.h"
#include "misc.h"
#include "dummy.h"
#include <time.h>
#include <sys/uio.h>

/* Berkeley mail fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 */

void bezerk_fetchfast (MAILSTREAM *stream,char *sequence)
{
  return;			/* no-op for local mail */
}


/* Berkeley mail fetch flags
 * Accepts: MAIL stream
 *	    sequence
 */

void bezerk_fetchflags (MAILSTREAM *stream,char *sequence)
{
  return;			/* no-op for local mail */
}

/* Berkeley mail fetch structure
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to return body
 * Returns: envelope of this message, body returned in body value
 *
 * Fetches the "fast" information as well
 */

ENVELOPE *bezerk_fetchstructure (MAILSTREAM *stream,long msgno,BODY **body)
{
  ENVELOPE **env;
  BODY **b;
  STRING bs;
  LONGCACHE *lelt;
  FILECACHE *m = LOCAL->msgs[msgno - 1];
				/* transient copies of this message's header
				   and body, read from the cache file -- kept
				   separate from LOCAL->buf, which
				   rfc822_parse_msg() below uses as its own
				   scratch space (aliasing the two would let
				   a grow-in-place there invalidate the very
				   header/body bytes it's still reading) */
  char *hdr = NIL,*bdy = NIL;
  unsigned long hdrlen = 0,bdylen = 0;
  if (stream->scache) {		/* short cache */
    if (msgno != stream->msgno){/* flush old poop if a different message */
      mail_free_envelope (&stream->env);
      mail_free_body (&stream->body);
    }
    stream->msgno = msgno;
    env = &stream->env;		/* get pointers to envelope and body */
    b = &stream->body;
  }
  else {			/* long cache */
    lelt = mail_lelt (stream,msgno);
    env = &lelt->env;		/* get pointers to envelope and body */
    b = &lelt->body;
  }
  if ((body && !*b) || !*env) {	/* have the poop we need? */
    mail_free_envelope (env);	/* flush old envelope and body */
    mail_free_body (b);
    if (max (m->headersize,m->bodysize) > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);	/* make sure enough buffer space */
      LOCAL->buf = (char *)
	fs_get ((LOCAL->buflen = max (m->headersize,m->bodysize)) + 1);
    }
    bezerk_readcache (stream,m->hoff + m->fromlen,m->headersize,
		      &hdr,&hdrlen);
    bezerk_readcache (stream,m->hoff + m->fromlen + m->headersize,
		      m->bodysize,&bdy,&bdylen);
    INIT (&bs,mail_string,(void *) bdy,m->bodysize);
				/* parse envelope and body */
    rfc822_parse_msg (env,body ? b : NIL,hdr,m->headersize,&bs,lhostn,
		      LOCAL->buf);
    fs_give ((void **) &hdr);
    fs_give ((void **) &bdy);
  }
  if (body) *body = *b;		/* return the body */
  return *env;			/* return the envelope */
}

/* Berkeley mail snarf message, only for Tenex driver
 * Accepts: MAIL stream
 *	    message # to snarf
 *	    pointer to size to return
 * Returns: message text in RFC822 format
 */

char *bezerk_snarf (MAILSTREAM *stream,long msgno,long *size)
{
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  FILECACHE *m = LOCAL->msgs[msgno - 1];
  if (((*size = m->headersize + m->bodysize) > LOCAL->buflen) ||
      LOCAL->buflen > CHUNK) {	/* make sure stream can hold the text */
				/* fs_resize would do an unnecessary copy */
    fs_give ((void **) &LOCAL->buf);
    LOCAL->buf = (char *) fs_get((LOCAL->buflen = max (*size,(long) CHUNK))+1);
  }
				/* read the text straight into place --
				   header and body sit contiguously in the
				   cache file, so this is one seek+read */
  if ((lseek (LOCAL->cf,m->hoff + m->fromlen,L_SET) < 0) ||
      ((m->headersize + m->bodysize) &&
       read (LOCAL->cf,LOCAL->buf,(unsigned) (m->headersize + m->bodysize))
       != (m->headersize + m->bodysize)))
    fatal ("Message cache file read failed");
  LOCAL->buf[*size] = '\0';	/* tie off string */
  return LOCAL->buf;
}

/* Berkeley mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 * Returns: message header in RFC822 format
 */

char *bezerk_fetchheader (MAILSTREAM *stream,long msgno)
{
  FILECACHE *m = LOCAL->msgs[msgno - 1];
				/* raw header bytes, read from the cache
				   file -- must be a buffer distinct from
				   LOCAL->buf, since strcrlfcpy() below reads
				   src while (re)sizing/writing dst and the
				   two can't safely be the same buffer */
  char *hdr = NIL;
  unsigned long hdrlen = 0;
  bezerk_readcache (stream,m->hoff + m->fromlen,m->headersize,&hdr,&hdrlen);
				/* copy the string */
  strcrlfcpy (&LOCAL->buf,&LOCAL->buflen,hdr,m->headersize);
  fs_give ((void **) &hdr);
  return LOCAL->buf;
}


/* Berkeley mail fetch message text (body only)
 * Accepts: MAIL stream
 *	    message # to fetch
 * Returns: message text in RFC822 format
 */

char *bezerk_fetchtext (MAILSTREAM *stream,long msgno)
{
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  FILECACHE *m = LOCAL->msgs[msgno - 1];
  if (!elt->seen) {		/* if message not seen */
    elt->seen = T;		/* mark message as seen */
				/* recalculate Status/X-Status lines */
    bezerk_update_status (m->status,elt);
    LOCAL->dirty = T;		/* note stream is now dirty */
  }
				/* raw body bytes, read from the cache file
				   -- see bezerk_fetchheader()'s comment on
				   why this can't just be LOCAL->buf */
  {
    char *bdy = NIL;
    unsigned long bdylen = 0;
    bezerk_readcache (stream,m->hoff + m->fromlen + m->headersize,
		      m->bodysize,&bdy,&bdylen);
    strcrlfcpy (&LOCAL->buf,&LOCAL->buflen,bdy,m->bodysize);
    fs_give ((void **) &bdy);
  }
  return LOCAL->buf;
}

/* Berkeley fetch message body as a structure
 * Accepts: Mail stream
 *	    message # to fetch
 *	    section specifier
 *	    pointer to length
 * Returns: pointer to section of message body
 */

char *bezerk_fetchbody (MAILSTREAM *stream,long m,char *s,unsigned long *len)
{
  BODY *b;
  PART *pt;
  unsigned long i;
  FILECACHE *fc = LOCAL->msgs[m - 1];
  char *base = NIL;		/* filled in below, once we know we need it */
  unsigned long baselen = 0;
  unsigned long offset = 0;
  MESSAGECACHE *elt = mail_elt (stream,m);
				/* make sure have a body */
  if (!(bezerk_fetchstructure (stream,m,&b) && b && s && *s &&
	((i = strtol (s,&s,10)) > 0))) return NIL;
  do {				/* until find desired body part */
				/* multipart content? */
    if (b->type == TYPEMULTIPART) {
      pt = b->contents.part;	/* yes, find desired part */
      while (--i && (pt = pt->next));
      if (!pt) return NIL;	/* bad specifier */
				/* note new body, check valid nesting */
      if (((b = &pt->body)->type == TYPEMULTIPART) && !*s) return NIL;
      offset = pt->offset;	/* get new offset */
    }
    else if (i != 1) return NIL;/* otherwise must be section 1 */
				/* need to go down further? */
    if (i = *s) switch (b->type) {
    case TYPEMESSAGE:		/* embedded message */
      offset = b->contents.msg.offset;
      b = b->contents.msg.body;	/* get its body, drop into multipart case */
    case TYPEMULTIPART:		/* multipart, get next section */
      if ((*s++ == '.') && (i = strtol (s,&s,10)) > 0) break;
    default:			/* bogus subpart specification */
      return NIL;
    }
  } while (i);
				/* lose if body bogus */
  if ((!b) || b->type == TYPEMULTIPART) return NIL;
  if (!elt->seen) {		/* if message not seen */
    elt->seen = T;		/* mark message as seen */
				/* recalculate Status/X-Status lines */
    bezerk_update_status (fc->status,elt);
    LOCAL->dirty = T;		/* note stream is now dirty */
  }
				/* raw body bytes, read from the cache file
				   -- separate from LOCAL->buf, which
				   rfc822_contents() below uses as its own
				   output buffer, and always copies out of
				   (never returns) its source */
  bezerk_readcache (stream,fc->hoff + fc->fromlen + fc->headersize,
		    fc->bodysize,&base,&baselen);
  {
    char *ret = rfc822_contents (&LOCAL->buf,&LOCAL->buflen,len,base + offset,
				 b->size.ibytes,b->encoding);
    fs_give ((void **) &base);
    return ret;
  }
}

/* Berkeley mail set flag
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 */

void bezerk_setflag (MAILSTREAM *stream,char *sequence,char *flag)
{
  MESSAGECACHE *elt;
  long i;
  short f = bezerk_getflags (stream,flag);
  if (!f) return;		/* no-op if no flags to modify */
				/* get sequence and loop on it */
  if (mail_sequence (stream,sequence)) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
				/* set all requested flags */
      if (f&fSEEN) elt->seen = T;
      if (f&fDELETED) elt->deleted = T;
      if (f&fFLAGGED) elt->flagged = T;
      if (f&fANSWERED) elt->answered = T;
				/* recalculate Status/X-Status lines */
      bezerk_update_status (LOCAL->msgs[i - 1]->status,elt);
      LOCAL->dirty = T;		/* note stream is now dirty */
    }
}


/* Berkeley mail clear flag
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 */

void bezerk_clearflag (MAILSTREAM *stream,char *sequence,char *flag)
{
  MESSAGECACHE *elt;
  long i;
  short f = bezerk_getflags (stream,flag);
  if (!f) return;		/* no-op if no flags to modify */
				/* get sequence and loop on it */
  if (mail_sequence (stream,sequence)) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
				/* clear all requested flags */
      if (f&fSEEN) elt->seen = NIL;
      if (f&fDELETED) elt->deleted = NIL;
      if (f&fFLAGGED) elt->flagged = NIL;
      if (f&fANSWERED) elt->answered = NIL;
				/* recalculate Status/X-Status lines */
      bezerk_update_status (LOCAL->msgs[i - 1]->status,elt);
      LOCAL->dirty = T;		/* note stream is now dirty */
    }
}

/* Berkeley mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

long bezerk_ping (MAILSTREAM *stream)
{
  char lock[MAILTMPLEN];
  struct stat sbuf;
  int fd;
				/* does he want to give up readwrite? */
  if (stream->readonly && LOCAL->ld) {
    flock (LOCAL->ld,LOCK_UN);	/* yes, release the lock */
    close (LOCAL->ld);		/* close the lock file */
    LOCAL->ld = NIL;		/* no more lock fd */
    unlink (LOCAL->lname);	/* delete it */
    fs_give ((void **) &LOCAL->lname);
  }
				/* make sure it is alright to do this at all */
  if (LOCAL && LOCAL->ld && !stream->lock) {
				/* get current mailbox size */
				/* parse if mailbox changed */
    if (!stat (LOCAL->name,&sbuf) && (sbuf.st_size != LOCAL->filesize) &&
	((fd = bezerk_parse (stream,lock,LOCK_SH)) >= 0)) {
				/* unlock mailbox */
      bezerk_unlock (fd,stream,lock);
      mail_unlock (stream);	/* and stream */
    }
  }
  return LOCAL ? T : NIL;	/* return if still alive */
}

/* Berkeley mail check mailbox
 * Accepts: MAIL stream
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

void bezerk_check (MAILSTREAM *stream)
{
  char lock[MAILTMPLEN];
  int fd;
				/* parse and lock mailbox */
  if (LOCAL && LOCAL->ld && ((fd = bezerk_parse (stream,lock,LOCK_EX)) >= 0)) {
    if (LOCAL->dirty && bezerk_extend (stream,fd,"Unable to update mailbox"))
      bezerk_save (stream,fd);	/* dump checkpoint if needed */
				/* flush locks */
    bezerk_unlock (fd,stream,lock);
    mail_unlock (stream);
  }
  if (LOCAL && LOCAL->ld) mm_log ("Check completed",NIL);
}

/* Berkeley mail expunge mailbox
 * Accepts: MAIL stream
 */

void bezerk_expunge (MAILSTREAM *stream)
{
  int fd,j;
  long i = 1;
  long n = 0;
  unsigned long recent;
  MESSAGECACHE *elt;
  char *r = "No messages deleted, so no update needed";
  char lock[MAILTMPLEN];
  if (LOCAL && LOCAL->ld) {	/* parse and lock mailbox */
    if ((fd = bezerk_parse (stream,lock,LOCK_EX)) >= 0) {
      recent = stream->recent;	/* get recent now that new ones parsed */
      while ((j = (i<=stream->nmsgs)) && !(elt = mail_elt (stream,i))->deleted)
	i++;			/* find first deleted message */
      if (j) {			/* found one? */
				/* make sure we can do the worst case thing */
	if (bezerk_extend (stream,fd,"Unable to expunge mailbox")) {
	  do {			/* flush deleted messages */
	    if ((elt = mail_elt (stream,i))->deleted) {
				/* if recent, note one less recent message */
	      if (elt->recent) --recent;
				/* flush local cache entry */
	      fs_give ((void **) &LOCAL->msgs[i - 1]);
	      for (j = i; j < stream->nmsgs; j++)
		LOCAL->msgs[j - 1] = LOCAL->msgs[j];
	      LOCAL->msgs[stream->nmsgs - 1] = NIL;
				/* notify upper levels */
	      mail_expunged (stream,i);
	      n++;		/* count another expunged message */
	    }
	    else i++;		/* otherwise try next message */
	  } while (i <= stream->nmsgs);
				/* dump checkpoint of the results */
	  bezerk_save (stream,fd);
	  sprintf ((r = LOCAL->buf),"Expunged %ld messages",n);
	}
      }
				/* notify upper level, free locks */
      mail_exists (stream,stream->nmsgs);
      mail_recent (stream,recent);
      bezerk_unlock (fd,stream,lock);
      mail_unlock (stream);
    }
  }
  else r = "Expunge ignored on readonly mailbox";
  if (LOCAL && !stream->silent) mm_log (r,NIL);
}

/* Berkeley mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 * Returns: T if copy successful, else NIL
 */

long bezerk_copy (MAILSTREAM *stream,char *sequence,char *mailbox)
{
				/* copy the messages */
  return (mail_sequence (stream,sequence)) ?
    bezerk_copy_messages (stream,mailbox) : NIL;
}


/* Berkeley mail move message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 * Returns: T if move successful, else NIL
 */

long bezerk_move (MAILSTREAM *stream,char *sequence,char *mailbox)
{
  long i;
  MESSAGECACHE *elt;
  if (!(mail_sequence (stream,sequence) &&
	bezerk_copy_messages (stream,mailbox))) return NIL;
				/* delete all requested messages */
  for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      elt->deleted = T;		/* mark message deleted */
				/* recalculate Status/X-Status lines */
      bezerk_update_status (LOCAL->msgs[i - 1]->status,elt);
      LOCAL->dirty = T;		/* note stream is now dirty */
    }
  return T;
}

/* Berkeley mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    stringstruct of messages to append
 * Returns: T if append successful, else NIL
 */

/* Was 8*MAILTMPLEN (8192) -- a stack-allocated write-batching buffer
 * this size is the same class of oversized fixed buffer as this
 * file's own CHUNK-vs-MAILTMPLEN bezerk_open() bug and pine's
 * MSIZE_INIT/RBUFSZ, all fixed elsewhere this session (see
 * TODO-pdp11-done.md) -- confirmed via a real-hardware adb crash
 * ("Illegal instruction") inside bezerk_append_putc(), called from
 * bezerk_append(), right after a Send: this function's ~9KB combined
 * stack frame (this buffer plus lock[MAILTMPLEN] just below) landing
 * on top of an already near-full 64KB D-space very plausibly collided
 * the stack into the heap. BUFLEN only controls how often
 * bezerk_append_putc() flushes to write(2) -- smaller just means more
 * write() calls for a large message, not a correctness change. */
#define BUFLEN (MAILTMPLEN/2)

long bezerk_append (MAILSTREAM *stream,char *mailbox,STRING *message)
{
  struct stat sbuf;
  int fd,i;
  char buf[BUFLEN],lock[MAILTMPLEN];
  char c = '\n';
  long ok = T;
  unsigned long t = time (0);
  long size = SIZE (message);
  if ((fd = bezerk_lock (bezerk_file (buf,mailbox),O_WRONLY|O_APPEND|O_CREAT,
			 S_IREAD|S_IWRITE,lock,LOCK_EX)) < 0) {
    sprintf (buf,"Can't open append mailbox: %s",strerror (errno));
    mm_log (buf,ERROR);
    return NIL;
  }
  mm_critical (stream);		/* go critical */
  fstat (fd,&sbuf);		/* get current file size */
  sprintf (buf,"From %s@%s %s",myusername (),lhostn,ctime (&t));
  i = strlen (buf);		/* initial buffer space used */
  while (ok && size--) {	/* copy text, tossing out CR's */
    if (c == '\n') {		/* if at start of line */
      t = GETPOS (message);	/* prepend a broket if needed */
      if ((SNX (message) == 'F') && (SNX (message) == 'r') &&
	  (SNX (message) == 'o') && (SNX (message) == 'm') &&
	  (SNX (message) == ' ')) ok = bezerk_append_putc (fd,buf,&i,'>');
      SETPOS (message,t);	/* restore position as needed */
    }
				/* copy another character */
    if ((c = SNX (message)) != '\015') ok = bezerk_append_putc (fd,buf,&i,c);
  }
				/* write trailing newline */
  if (ok) ok = bezerk_append_putc (fd,buf,&i,'\n');
  if (!(ok && (ok = (write (fd,buf,i) >= 0)))) {
    sprintf (buf,"Message append failed: %s",strerror (errno));
    mm_log (buf,ERROR);
    ftruncate (fd,sbuf.st_size);
  }
  fsync (fd);			/* force out the update */
  bezerk_unlock (fd,NIL,lock);	/* unlock and close mailbox */
  mm_nocritical (stream);	/* release critical */
  return ok;			/* return success */
}

/* Berkeley mail append character
 * Accepts: file descriptor
 *	    output buffer
 *	    pointer to current size of output buffer
 *	    character to append
 * Returns: T if append successful, else NIL
 */

long bezerk_append_putc (int fd,char *s,int *i,char c)
{
  s[(*i)++] = c;
  if (*i == BUFLEN) {		/* dump if buffer filled */
    if (write (fd,s,*i) < 0) return NIL;
    *i = 0;			/* reset */
  }
  return T;
}

/* Berkeley garbage collect stream
 * Accepts: Mail stream
 *	    garbage collection flags
 */

void bezerk_gc (MAILSTREAM *stream,long gcflags)
{
  /* nothing here for now */
}

/* Parse flag list
 * Accepts: MAIL stream
 *	    flag list as a character string
 * Returns: flag command list
 */


short bezerk_getflags (MAILSTREAM *stream,char *flag)
{
  char *t;
  short f = 0;
  short i,j;
  if (flag && *flag) {		/* no-op if no flag string */
				/* check if a list and make sure valid */
    if ((i = (*flag == '(')) ^ (flag[strlen (flag)-1] == ')')) {
      mm_log ("Bad flag list",ERROR);
      return NIL;
    }
				/* copy the flag string w/o list construct */
    strncpy (LOCAL->buf,flag+i,(j = strlen (flag) - (2*i)));
    LOCAL->buf[j] = '\0';
    t = ucase (LOCAL->buf);	/* uppercase only from now on */

    while (*t) {		/* parse the flags */
      if (*t == '\\') {		/* system flag? */
	switch (*++t) {		/* dispatch based on first character */
	case 'S':		/* possible \Seen flag */
	  if (t[1] == 'E' && t[2] == 'E' && t[3] == 'N') i = fSEEN;
	  t += 4;		/* skip past flag name */
	  break;
	case 'D':		/* possible \Deleted flag */
	  if (t[1] == 'E' && t[2] == 'L' && t[3] == 'E' && t[4] == 'T' &&
	      t[5] == 'E' && t[6] == 'D') i = fDELETED;
	  t += 7;		/* skip past flag name */
	  break;
	case 'F':		/* possible \Flagged flag */
	  if (t[1] == 'L' && t[2] == 'A' && t[3] == 'G' && t[4] == 'G' &&
	      t[5] == 'E' && t[6] == 'D') i = fFLAGGED;
	  t += 7;		/* skip past flag name */
	  break;
	case 'A':		/* possible \Answered flag */
	  if (t[1] == 'N' && t[2] == 'S' && t[3] == 'W' && t[4] == 'E' &&
	      t[5] == 'R' && t[6] == 'E' && t[7] == 'D') i = fANSWERED;
	  t += 8;		/* skip past flag name */
	  break;
	default:		/* unknown */
	  i = 0;
	  break;
	}
				/* add flag to flags list */
	if (i && ((*t == '\0') || (*t++ == ' '))) f |= i;
	else {			/* bitch about bogus flag */
	  mm_log ("Unknown system flag",ERROR);
	  return NIL;
	}
      }
      else {			/* no user flags yet */
	mm_log ("Unknown flag",ERROR);
	return NIL;
      }
    }
  }
  return f;
}
