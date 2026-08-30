/*
 * Program:	Berkeley mail routines
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


/* Dedication:
 *  This file is dedicated with affection to those Merry Marvels of Musical
 * Madness . . .
 *  ->  The Incomparable Leland Stanford Junior University Marching Band  <-
 * who entertain, awaken, and outrage Stanford fans in the fact of repeated
 * losing seasons and shattered Rose Bowl dreams [Cardinal just don't have
 * HUSKY FEVER!!!].
 *
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

/* Counter for building unique names for bezerk_open()'s per-stream
 * message cache file (see LOCAL->cf) -- getpid() alone isn't enough
 * since one process can have more than one bezerk stream open at once. */
static int bezcache_ctr = 0;

/* Berkeley mail routines */


				/* prototype stream: zero-initialized (local,
				   mailbox NIL; cache union, msgno, etc. all
				   0) except .open, which must be T -- can't
				   set that here via an aggregate initializer
				   since 2.11BSD's cc can't initialize a
				   bit-field member that way ("No field
				   initialization"/"union initialization
				   non-portable" at this line). Set instead
				   in pine.c's main(), once, before any
				   mail_open()/mail_create() call can reach
				   it via default_driver()/bezerk_open(NIL) */
MAILSTREAM bezerkproto;
				/* standard driver's prototype */
MAILSTREAM *mailstd_proto = &bezerkproto;

/* Berkeley mail validate mailbox
 * Accepts: mailbox name
 * Returns: T if name is valid, NIL otherwise
 */

long bezerk_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return bezerk_isvalid (name,tmp);
}


/* Berkeley mail test for valid mailbox
 * Accepts: mailbox name
 * Returns: T if valid, NIL otherwise
 */

int bezerk_isvalid (char *name,char *tmp)
{
  int i,fd,ti,zn;
  int ret = NIL;
  char *s = tmp,*t;
  struct stat sbuf;
  if (!lhostn) {		/* have local host yet? */
    /* No gethostbyname()/DNS lookup here -- this project's PDP-11
     * target has no TCP/IP (see README.md/CLAUDE.md), so a resolver
     * call could never do anything useful, while its mere presence
     * drags the entire BSD resolver library into every link (found
     * via a real 2.11BSD `ld -M` load map; see TODO-pdp11-done.md). */
    gethostname(tmp,MAILTMPLEN);/* get local host name */
    lhostn = cpystr (tmp);
  }
				/* name is a caller-typed candidate mailbox
				   name with no length cap upstream; tmp is
				   MAILTMPLEN. Nothing this long could be a
				   real local mailbox path, so reject rather
				   than overflow. */
  if (strlen (name) >= MAILTMPLEN) return NIL;
				/* INBOX is always accepted */
  if (!strcmp (ucase (strcpy (tmp,name)),"INBOX")) return T;
				/* if file, get its status */
  if ((*name != '{') && !((*name == '*') && (name[1] == '{')) &&
      (stat (bezerk_file (tmp,name),&sbuf) == 0) &&
      ((fd = open (tmp,O_RDONLY,NIL)) >= 0)) {
    if (sbuf.st_size == 0) { 	/* allow empty file if not .TxT */
      if ((i = strlen (tmp)) < 4 || strcmp (tmp + i - 4 ,".TxT"))
	return LONGT;
    }
    else if ((read (fd,tmp,MAILTMPLEN-1) >= 0) &&
	     (tmp[0] == 'F') && VALID (s,t,ti,zn)) ret = T;
    close (fd);			/* close the file */
  }
  return ret;			/* return what we should */
}

/* Berkeley manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *bezerk_parameters (long function,void *value)
{
  fatal ("Invalid bezerk_parameters function");
  return NIL;
}

/* Berkeley mail find list of mailboxes
 * Accepts: mail stream
 *	    pattern to search
 */

void bezerk_find (MAILSTREAM *stream,char *pat)
{
  if (stream) dummy_find_all (NIL,pat);
}


/* Berkeley mail find list of all mailboxes
 * Accepts: mail stream
 *	    pattern to search
 */

void bezerk_find_all (MAILSTREAM *stream,char *pat)
{
  if (stream) dummy_find_all (NIL,pat);
}


/* Berkeley mail create mailbox
 * Accepts: MAIL stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long bezerk_create (MAILSTREAM *stream,char *mailbox)
{
  char tmp[MAILTMPLEN];
  int i,fd;
				/* must not be a ".TxT" file */
  if ((i = strlen (mailbox)) > 4 && !strcmp (mailbox + i - 4 ,".TxT")) {
    mm_log ("Can't create mailbox: name must not end with .TxT",ERROR);
    return NIL;
  }
  if ((fd = open (bezerk_file (tmp,mailbox),O_WRONLY|O_CREAT|O_EXCL,0600))<0) {
    sprintf (tmp,"Can't create mailbox %s: %s",mailbox,strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  close (fd);			/* close the file */
  return T;			/* return success */
}


/* Berkeley mail delete mailbox
 * Accepts: MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long bezerk_delete (MAILSTREAM *stream,char *mailbox)
{
  return bezerk_rename (stream,mailbox,NIL);
}

/* Berkeley mail rename mailbox
 * Accepts: MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long bezerk_rename (MAILSTREAM *stream,char *old,char *new)
{
  long ret = T;
  char tmp[MAILTMPLEN],file[MAILTMPLEN],lock[MAILTMPLEN],lockx[MAILTMPLEN];
  int fd,ld;
				/* get the c-client lock */
  if ((ld = open (lockname (lock,bezerk_file (file,old)),O_RDWR|O_CREAT,0666))
      < 0) {
    sprintf (tmp,"Can't get lock for mailbox %s: %s",old,strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* lock out other c-clients */
  if (flock (ld,LOCK_EX|LOCK_NB)) {
    close (ld);			/* couldn't lock, give up on it then */
    sprintf (tmp,"Mailbox %s is in use by another process",old);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* lock out non c-client applications */
  if ((fd = bezerk_lock (file,O_RDONLY,S_IREAD|S_IWRITE,lockx,LOCK_EX)) < 0) {
    sprintf (tmp,"Can't lock mailbox %s: %s",old,strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* do the rename or delete operation */
  if (new ? rename (file,bezerk_file (tmp,new)) : unlink (file)) {
    sprintf (tmp,"Can't %s mailbox %s: %s",new ? "rename" : "delete",old,
	     strerror (errno));
    mm_log (tmp,ERROR);
    ret = NIL;			/* set failure */
  }
  bezerk_unlock (fd,NIL,lockx);	/* unlock and close mailbox */
  flock (ld,LOCK_UN);		/* release c-client lock lock */
  close (ld);			/* close c-client lock */
  unlink (lock);		/* and delete it */
  return ret;			/* return success */
}

/* Berkeley mail open
 * Accepts: Stream to open
 * Returns: Stream on success, NIL on failure
 */

MAILSTREAM *bezerk_open (MAILSTREAM *stream)
{
  long i;
  int fd;
  char tmp[MAILTMPLEN];
  struct stat sbuf;
  long retry;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &bezerkproto;
  retry = stream->silent ? 1 : KODRETRY;
  if (LOCAL) {			/* close old file if stream being recycled */
    bezerk_close (stream);	/* dump and save the changes */
    stream->open = T;		/* reattach this driver */
    mail_free_cache (stream);	/* clean up cache */
  }
  stream->local = fs_get (sizeof (BEZERKLOCAL));
				/* canonicalize the stream mailbox name */
  bezerk_file (tmp,stream->mailbox);
				/* force readonly if bboard */
  if (*stream->mailbox == '*') stream->readonly = T;
  else {			/* canonicalize name */
    fs_give ((void **) &stream->mailbox);
    stream->mailbox = cpystr (tmp);
  }
  /* You may wonder why LOCAL->name is needed.  It isn't at all obvious from
   * the code.  The problem is that when a stream is recycled with another
   * mailbox of the same type, the driver's close method isn't called because
   * it could be IMAP and closing then would defeat the entire point of
   * recycling.  Hence there is code in the file drivers to call the close
   * method such as what appears above.  The problem is, by this point,
   * mail_open() has already changed the stream->mailbox name to point to the
   * new name, and bezerk_close() needs the old name.
   */
  LOCAL->name = cpystr (tmp);	/* local copy for recycle case */
				/* build name of our lock file */
  LOCAL->lname = cpystr (lockname (tmp,LOCAL->name));
  LOCAL->ld = NIL;		/* no state locking yet */
  LOCAL->filesize = 0;		/* initialize file information */
  LOCAL->filetime = 0;
  LOCAL->msgs = NIL;		/* no cache yet */
  LOCAL->cachesize = 0;
  /* Start small (MAILTMPLEN, big enough for every sprintf()/date-parse
   * use of LOCAL->buf in this file) rather than the full CHUNK (8192)
   * -- this runs on every open, before we even know the mailbox file
   * exists, and bezerk_parse()'s real-data-reading path already grows
   * this buffer (fs_resize()) to whatever size it actually needs. On
   * a PDP-11 with data+bss+heap+stack sharing one 64KB space, wasting
   * 8KB here unconditionally starved the empty/nonexistent-INBOX case
   * (the common one for a brand new user) of enough room to open at
   * all -- see TODO-pdp11-done.md. */
  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = MAILTMPLEN) + 1);
  /* Per-stream scratch file backing every message's compacted raw text
   * (see bezerk_readcache()) -- replaces a permanent per-message RAM
   * copy that made memory scale with total mailbox size instead of
   * message count. open()+unlink() (rather than tmpfile(), an ANSI C
   * addition this target's stdio.h doesn't declare) needs no further
   * cleanup bookkeeping -- the fd stays valid and readable/writable
   * after the name is removed from the directory, so nothing is left
   * behind even if the process dies without a clean bezerk_abort() --
   * and works the same whether this stream ends up read-only or
   * read-write, unlike normalizing the real mailbox file in place.
   * tmp is free to reuse here; its only other uses in this function
   * are already done with it by this point. */
  sprintf (tmp,"/tmp/.bezcache%d.%d",getpid (),bezcache_ctr++);
  if ((LOCAL->cf = open (tmp,O_RDWR|O_CREAT|O_EXCL,0600)) < 0) {
    mm_log ("Can't create message cache file",ERROR);
    bezerk_abort (stream);
    return NIL;
  }
  unlink (tmp);
  LOCAL->cfsize = 0;
  stream->sequence++;		/* bump sequence number */

  LOCAL->dirty = NIL;		/* no update yet */
				/* make lock for read/write access */
  if (!stream->readonly) while (retry) {
				/* get a new file handle each time */
    if ((fd = open (LOCAL->lname,O_RDWR|O_CREAT,0666)) < 0)
      mm_log ("Can't open mailbox lock, access is readonly",WARN);
				/* can get the lock? */
    if (flock (fd,LOCK_EX|LOCK_NB)) {
      if (retry-- == KODRETRY) {/* no, first time through? */
				/* yes, get other process' PID */
	if (!fstat (fd,&sbuf) && (i = min (sbuf.st_size,MAILTMPLEN)) &&
	    (read (fd,tmp,(unsigned)i) == i) && (i = atol (tmp))) {
	  kill (i,SIGUSR2);	/* send the Kiss Of Death */
	  sprintf (tmp,"Trying to get mailbox lock from process %ld",i);
	  mm_log (tmp,WARN);
	}
	else retry = 0;		/* give up */
      }
      close (fd);		/* get a new handle next time around */
      if (!stream->silent) {	/* nothing if silent stream */
	if (retry) sleep (1);	/* wait a second before trying again */
	else mm_log ("Mailbox is open by another process, access is readonly",
		     WARN);
      }
    }
    else {			/* got the lock, nobody else can alter state */
      LOCAL->ld = fd;		/* note lock's fd */
      chmod (LOCAL->lname,0666);/* make sure mode OK (don't use fchmod()) */
      if (stream->silent) i = 0;/* silent streams won't accept KOD */
      else {			/* note our PID in the lock */
	sprintf (tmp,"%d",getpid ());
	write (fd,tmp,(unsigned)((i = strlen (tmp))+1));
      }
      ftruncate (fd,i);		/* make sure tied off */
      fsync (fd);		/* make sure it's available */
      retry = 0;		/* no more need to try */
    }
  }

				/* parse mailbox */
  stream->nmsgs = stream->recent = 0;
				/* will we be able to get write access? */
  if (LOCAL->ld && access (LOCAL->name,W_OK) && (errno == EACCES)) {
    mm_log ("Can't get write access to mailbox, access is readonly",WARN);
    flock (LOCAL->ld,LOCK_UN);	/* release the lock */
    close (LOCAL->ld);		/* close the lock file */
    LOCAL->ld = NIL;		/* no more lock fd */
    unlink (LOCAL->lname);	/* delete it */
    fs_give ((void **) &LOCAL->lname);
  }
				/* abort if can't get RW silent stream */
  if (stream->silent && !stream->readonly && !LOCAL->ld) bezerk_abort (stream);
				/* parse mailbox */
  else if ((fd = bezerk_parse (stream,tmp,LOCK_SH)) >= 0) {
    bezerk_unlock (fd,stream,tmp);
    mail_unlock (stream);
  }
  if (!LOCAL) return NIL;	/* failure if stream died */
  stream->readonly = !LOCAL->ld;/* make sure upper level knows readonly */
				/* notify about empty mailbox */
  if (!(stream->nmsgs || stream->silent)) mm_log ("Mailbox is empty",NIL);
  return stream;		/* return stream alive to caller */
}

/* Berkeley mail close
 * Accepts: MAIL stream
 */

void bezerk_close (MAILSTREAM *stream)
{
  int fd;
  int silent = stream->silent;
  char lock[MAILTMPLEN];
  if (LOCAL && LOCAL->ld) {	/* is stream alive? */
    stream->silent = T;		/* note this stream is dying */
				/* lock mailbox and parse new messages */
    if (LOCAL->dirty && (fd = bezerk_parse (stream,lock,LOCK_EX)) >= 0) {
				/* dump any changes not saved yet */
      if (bezerk_extend	(stream,fd,"Close failed to update mailbox"))
	bezerk_save (stream,fd);
				/* flush locks */
      bezerk_unlock (fd,stream,lock);
      mail_unlock (stream);
    }
    stream->silent = silent;	/* restore previous status */
  }
  bezerk_abort (stream);	/* now punt the file and local data */
}


/* Internal routines */


/* Berkeley mail abort stream
 * Accepts: MAIL stream
 */

void bezerk_abort (MAILSTREAM *stream)
{
  long i;
  if (LOCAL) {			/* only if a file is open */
    if (LOCAL->name) fs_give ((void **) &LOCAL->name);
    if (LOCAL->ld) {		/* have a mailbox lock? */
      flock (LOCAL->ld,LOCK_UN);/* yes, release the lock */
      close (LOCAL->ld);	/* close the lock file */
      unlink (LOCAL->lname);	/* and delete it */
    }
    fs_give ((void **) &LOCAL->lname);
    if (LOCAL->msgs) {		/* free local cache */
      for (i = 0; i < stream->nmsgs; ++i) fs_give ((void **) &LOCAL->msgs[i]);
      fs_give ((void **) &LOCAL->msgs);
    }
				/* free local text buffers */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
				/* close and delete the message cache file */
    if (LOCAL->cf) close (LOCAL->cf);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->open = NIL;		/* mark as no longer open */
  }
}

/* Berkeley mail generate file string
 * Accepts: temporary buffer to write into
 *	    mailbox name string
 * Returns: local file string
 */


char *bezerk_file (char *dst,char *name)
{
  struct passwd *pw;
  char *s,*t,tmp[MAILTMPLEN];
  /*
   * name is a caller-typed candidate mailbox name with no length cap
   * upstream (e.g. straight from the composer's Fcc field); every caller
   * of bezerk_file() passes a MAILTMPLEN-sized dst, by convention across
   * this whole file. Nothing this long could be a real local mailbox
   * name, so just tie off a truncated dst instead of overflowing it.
   * Bounded well under MAILTMPLEN (not just under it) to leave headroom
   * for the "~" case below, which prepends a home directory to (part of)
   * name into the same MAILTMPLEN dst.
   */
  if (strlen (name) >= MAILTMPLEN/2) name = "/NOSUCHNAME";
  switch (*name) {
  case '*':			/* bboard? */
    sprintf (tmp,"~ftp/%.1000s",(name[1] == '/') ? name+2 : name+1);
    dst = bezerk_file (dst,tmp);/* recurse to get result */
    break;
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
  default:			/* other name */
    if (strcmp (ucase (strcpy (dst,name)),"INBOX"))
      sprintf (dst,"%s/%s",myhomedir (),name);
				/* INBOX becomes mail spool directory file */
    else sprintf (dst,MAILFILE,myusername ());
  }
  return dst;
}

/* Berkeley open and lock mailbox
 * Accepts: file name to open/lock
 *	    file open mode
 *	    destination buffer for lock file name
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 */

int bezerk_lock (char *file,int flags,int mode,char *lock,int op)
{
  int fd,ld,j;
  int i = LOCKTIMEOUT * 60 - 1;
  char tmp[MAILTMPLEN];
  struct timeval tp;
  struct stat sb;
				/* build lock filename */
  strcat (bezerk_file (lock,file),".lock");
  do {				/* until OK or out of tries */
    gettimeofday (&tp,NIL);	/* get the time now */
  /* This works on modern Unix systems which are not afflicted with NFS mail.
   * "Modern" means that O_EXCL works.  I think that NFS mail is a terrible
   * idea -- that's what IMAP is for -- but some people insist upon losing...
   */
				/* try to get the lock */
    if ((ld = open (lock,O_WRONLY|O_CREAT|O_EXCL,0666)) < 0) switch (errno) {
    case EEXIST:		/* if extant and old, grab it for ourselves */
      if ((!stat (lock,&sb)) && tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60)
	ld = open (lock,O_WRONLY|O_CREAT,0666);
      break;
    case EACCES:		/* protection fail, ignore if non-ex or old */
      if (stat (lock,&sb) || (tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60))
	*lock = '\0';		/* assume no world write mail spool dir */
      break;
    default:			/* some other failure */
      sprintf (tmp,"Error creating %s: %s",lock,strerror (errno));
      mm_log (tmp,WARN);	/* this is probably not good */
      *lock = '\0';		/* don't use lock files */
      break;
    }
    if (ld >= 0) {		/* if made a lock file */
      chmod (lock,0666);	/* make sure others can break the lock */
      close (ld);		/* close the lock file */
    }
    if ((ld < 0) && *lock) {	/* if failed to make lock file and retry OK */
      if (!(i%15)) {
	sprintf (tmp,"Mailbox %s is locked, will override in %d seconds...",
		 file,i);
	mm_log (tmp,WARN);
      }
      sleep (1);		/* wait 1 second before next try */
    }
  } while (i-- && ld < 0 && *lock);
				/* open file */
  if ((fd = open (file,flags,mode)) >= 0) flock (fd,op);
  else {			/* open failed */
    j = errno;			/* preserve error code */
    if (*lock) unlink (lock);	/* flush the lock file if any */
    errno = j;			/* restore error code */
  }
  return fd;
}

/* Berkeley unlock and close mailbox
 * Accepts: file descriptor
 *	    (optional) mailbox stream to check atime/mtime
 *	    (optional) lock file name
 */

void bezerk_unlock (int fd,MAILSTREAM *stream,char *lock)
{
  struct stat sbuf;
  struct timeval tp[2];
  fstat (fd,&sbuf);		/* get file times */
				/* if stream and csh would think new mail */
  if (stream && (sbuf.st_atime <= sbuf.st_mtime)) {
    gettimeofday (&tp[0],NIL);	/* set atime to now */
				/* set mtime to (now - 1) if necessary */
    tp[1].tv_sec = tp[0].tv_sec > sbuf.st_mtime ? sbuf.st_mtime :
      tp[0].tv_sec - 1;
    tp[1].tv_usec = 0;		/* oh well */
				/* set the times, note change */
    if (!utimes (LOCAL->name,tp)) LOCAL->filetime = tp[1].tv_sec;
  }
  flock (fd,LOCK_UN);		/* release flock'ers */
  close (fd);			/* close the file */
				/* flush the lock file if any */
  if (lock && *lock) unlink (lock);
}

/* Berkeley mail parse and lock mailbox
 * Accepts: MAIL stream
 *	    space to write lock file name
 *	    type of locking operation
 * Returns: file descriptor if parse OK, mailbox is locked shared
 *	    -1 if failure, stream aborted
 */

int bezerk_parse (MAILSTREAM *stream,char *lock,int op)
{
  int fd;
  long delta,i,j,is,is1,msglen;
  char *s,*s1,*t = NIL,*e,*sdel;
  char *msgstart,*hdrstart,*bodystart,*msgend;
  char savedch[2];
  unsigned long hdrsize,bodysize;
  char msgstatus[sizeof (STATUS)];
  char datebuf[27];
  int ti = 0,zn = 0;
  int first = T;
  long nmsgs = stream->nmsgs;
  long recent = stream->recent;
  struct stat sbuf;
  MESSAGECACHE *elt;
  mail_lock (stream);		/* guard against recursion or pingers */
				/* open and lock mailbox (shared OK) */
  if ((fd = bezerk_lock (LOCAL->name,LOCAL->ld ? O_RDWR : O_RDONLY,NIL,
			 lock,op)) < 0) {
				/* failed, OK for non-ex file? */
    if ((errno != ENOENT) || LOCAL->filesize) {
      sprintf (LOCAL->buf,"Mailbox open failed, aborted: %s",strerror (errno));
      mm_log (LOCAL->buf,ERROR);
      bezerk_abort (stream);
    }
    else {			/* this is to allow for non-ex INBOX */
      mail_exists (stream,0);	/* make sure upper level sees this as empty */
      mail_recent (stream,0);
    }
    mail_unlock (stream);
    return -1;
  }
  fstat (fd,&sbuf);		/* get status */
				/* calculate change in size */
  if ((delta = sbuf.st_size - LOCAL->filesize) < 0) {
    sprintf (LOCAL->buf,"Mailbox shrank from %ld to %ld bytes, aborted",
	     LOCAL->filesize,sbuf.st_size);
    mm_log (LOCAL->buf,ERROR);	/* this is pretty bad */
    bezerk_unlock (fd,stream,lock);
    bezerk_abort (stream);
    mail_unlock (stream);
    return -1;
  }

  else if (delta) {		/* get to that position in the file */
    lseek (fd,LOCAL->filesize,L_SET);
    s = s1 = LOCAL->buf;	/* initial read-in location */
    i = 0;			/* initial unparsed read-in count */
    do {
      i = min (CHUNK,delta);	/* calculate read-in size */
				/* increase the read-in buffer if necessary */
      if ((j = i + (s1 - s)) >= LOCAL->buflen) {
	is = s - LOCAL->buf;	/* note former start of message position */
	is1 = s1 - LOCAL->buf;	/* and start of new data position */
	if (s1 - s) fs_resize ((void **) &LOCAL->buf,(LOCAL->buflen = j) + 1);
	else {			/* fs_resize would do an unnecessary copy */
	  fs_give ((void **) &LOCAL->buf);
	  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = j) + 1);
	}
	s = LOCAL->buf + is;	/* new start of message */
	s1 = LOCAL->buf + is1;	/* new start of new data */
      }
      s1[i] = '\0';		/* tie off chunk */
      /*
       * Was read(fd,s1,i) -- i is long, but read() has no prototype in
       * scope on this target (unistd.h declares it "ssize_t read();",
       * an empty-parens K&R-style declaration with no argument
       * checking), and the real syscall stub's count parameter is a
       * single-word unsigned. Passing a long there pushed an extra
       * word the callee never expected, silently corrupting the
       * count it actually saw (observed on real hardware: a request
       * for 559 bytes came back as 0 bytes read, no error) --
       * explaining the "Mailbox format invalidated" false failure
       * below on an otherwise perfectly valid mailbox. See
       * TODO-pdp11-done.md.
       */
      if (read (fd,s1,(unsigned)i) < 0) {	/* read a chunk of new text */
	sprintf (LOCAL->buf,"Error reading mail file: %s",strerror (errno));
	mm_log (LOCAL->buf,ERROR);
	bezerk_unlock (fd,stream,lock);
	bezerk_abort (stream);
	mail_unlock (stream);
	return -1;
      }
      delta -= i;		/* account for data read in */
				/* validate newly-appended data */
      if (first) {		/* only do this first time! */
	if (!((*s =='F') && VALID (s,t,ti,zn))) {
	  mm_log ("Mailbox format invalidated (consult an expert), aborted",
		  ERROR);
	  bezerk_unlock (fd,stream,lock);
	  bezerk_abort (stream);
	  mail_unlock (stream);
	  return -1;
	}
	first = NIL;		/* don't do this again */
      }

				/* found end of message or end of data? */
      while ((e = bezerk_eom (s,s1,i)) || !delta) {
	nmsgs++;		/* yes, have a new message */
				/* calculate message length */
	msglen = ((e ? e : s1 + i) - s) - 1;
	msgstart = s;
	/*
	 * Process this message directly against its span in LOCAL->buf
	 * instead of memcpy()'ing it out to a private scratch block
	 * first (that used to be what the RAWCACHE struct/list below
	 * this comment's old home were for) -- see TODO-pdp11-done.md's
	 * writeup of the "Out of free storage" crash this caused when a
	 * single message's bytes were transiently resident twice (once
	 * here, once in its own copy) at the same time.
	 *
	 * msgend/savedch temporarily NUL-terminate the message in place
	 * so the same strchr()/VALID()-based scanning logic below (kept
	 * byte-for-byte) can run on it. Two bytes, not one, are saved
	 * and restored: the empty-message bugtrap a few lines down can
	 * strcat() a synthetic "\n" onto a message with no newlines at
	 * all, which (like the original RAWCACHE allocation's own +1
	 * spare byte) needs one byte of headroom past the terminator
	 * for its own new terminator. msgend[1] is always a real,
	 * already-valid buffer byte to borrow: either the following
	 * message's first character (when e is set), or the read loop's
	 * own chunk-tie-off NUL (when this is the last message and e is
	 * NIL) -- save/restore leaves either case exactly as found.
	 */
	msgend = s + msglen;
	savedch[0] = msgend[0]; savedch[1] = msgend[1];
	msgend[0] = '\0';

	/* This is a bugtrap for bogons in the mailbox text, which may
	 * happen if memory is corrupted.  Note that in the case of a
	 * totally empty message, a newline is appended and counts
	 * adjusted.
	 */
	if (!((s = msgstart) && VALID (s,t,ti,zn)))
	  if (!(s && !strchr (s,'\n') && strcat (s,"\n") && VALID (s,t,ti,zn) &&
		msglen++))
	    fatal ("Bogus entry in mailbox text");
				/* pointer to message header */
	hdrstart = s = strchr (t++,'\n') + 1;
	hdrsize = msglen - (hdrstart - msgstart);
	bodystart = NIL;	/* assume no body as yet */
	bodysize = 0;
	recent++;		/* assume recent by default */
	(elt = mail_elt (stream,nmsgs))->recent = T;
				/* calculate initial Status/X-Status lines */
	bezerk_update_status (msgstatus,elt);
				/* generate plausable IMAPish date string */
	datebuf[2] = datebuf[6] = datebuf[20] = '-';
	datebuf[11] = ' ';
	datebuf[14] = datebuf[17] = ':';
				/* dd */
	datebuf[0] = t[ti - 3]; datebuf[1] = t[ti - 2];
				/* mmm */
	datebuf[3] = t[ti - 7]; datebuf[4] = t[ti - 6];
	datebuf[5] = t[ti - 5];
				/* hh */
	datebuf[12] = t[ti]; datebuf[13] = t[ti + 1];
				/* mm */
	datebuf[15] = t[ti + 3]; datebuf[16] = t[ti + 4];
	if (t[ti += 5] == ':') {	/* ss if present */
	  datebuf[18] = t[++ti];
	  datebuf[19] = t[++ti];
	  ti++;			/* move to space */
	}
	else datebuf[18] = datebuf[19] = '0';
				/* yy -- advance over timezone if necessary */
	if (zn == ++ti) ti += (((t[zn] == '+') || (t[zn] == '-')) ? 6 : 4);
	datebuf[7] = t[ti]; datebuf[8] = t[ti + 1];
	datebuf[9] = t[ti + 2]; datebuf[10] = t[ti + 3];
				/* zzz */
	t = zn ? (t + zn) : "LCL";
	datebuf[21] = *t++; datebuf[22] = *t++; datebuf[23] = *t++;
	if ((datebuf[21] != '+') && (datebuf[21] != '-'))
	  datebuf[24] = '\0';
	else {			/* numeric time zone */
	  datebuf[24] = *t++; datebuf[25] = *t++;
	  datebuf[26] = '\0'; datebuf[20] = ' ';
	}
				/* set internal date */
	if (!mail_parse_date (elt,datebuf)) mm_log ("Unparsable date",WARN);

	is = 0;			/* initialize newline count */
	sdel = NIL;		/* no status stuff yet */
	do switch (*(t = s)) {	/* look at header lines */
	case 'X':		/* possible X-Status: line */
	  if (s[1] == '-' && s[2] == 'S') s += 2;
	  else {
	    is++;		/* count another newline */
	    break;		/* this is uninteresting after all */
	  }
	case 'S':		/* possible Status: line */
	  if (s[1] == 't' && s[2] == 'a' && s[3] == 't' && s[4] == 'u' &&
	      s[5] == 's' && s[6] == ':') {
	    if (!sdel) sdel = t;	/* note deletion point */
	    s += 6;			/* advance to status flags */
	    do switch (*s++) {		/* parse flags */
	    case 'R':		/* message read */
	      elt->seen = T;
	      break;
	    case 'O':		/* message old */
	      if (elt->recent) {	/* don't do this more than once! */
		elt->recent = NIL;
		recent--;		/* not recent any longer... */
	      }
	      break;
	    case 'D':		/* message deleted */
	      elt->deleted = T;
	      break;
	    case 'F':		/* message flagged */
	      elt ->flagged = T;
	      break;
	    case 'A':		/* message answered */
	      elt ->answered = T;
	      break;
	    default:		/* some other crap */
	      break;
	    } while (*s && *s != '\n');
				/* recalculate Status/X-Status lines */
	    bezerk_update_status (msgstatus,elt);
	  }
	  else is++;		/* otherwise random line */
	  break;

	case '\n':		/* end of header */
	  bodystart = ++s;	/* start of body is here */
	  j = bodystart - hdrstart;	/* new header size */
				/* calculate body size */
	  bodysize = hdrsize - j;
	  if (sdel) {		/* saw status poop? */
	    *sdel++ = '\n';	/* patch in trailing newline */
	    hdrsize = sdel - hdrstart;
	  }
	  else hdrsize = j;	/* set header size */
	  s = NIL;		/* don't scan any further */
	  is++;			/* count a newline */
	  break;
	case '\0':		/* end of message */
	  if (sdel) {		/* saw status poop? */
	    *sdel++ = '\n';	/* patch in trailing newline */
	    hdrsize = sdel - hdrstart;
	  }
	  is++;			/* count an extra newline here */
	  break;
	default:		/* anything else is uninteresting */
	  if (sdel) {		/* have status stuff to worry about? */
	    j = s - sdel;	/* yuck!!  calculate size of delete area */
				/* blat remaining number of bytes down */
	    memmove (sdel,s,hdrstart + hdrsize - s);
	    hdrsize -= j;	/* update for new size */
	    sdel = NIL;		/* no more delete area */
				/* tie off old cruft */
	    *(hdrstart + hdrsize) = '\0';
	  }
	  is++;
	  break;
	} while (s && (s = strchr (s,'\n')) && s++);
				/* count newlines in body */
	if (s = bodystart) while (*s) if (*s++ == '\n') is++;
	elt->rfc822_size = hdrsize + bodysize + is;
	{			/* flush this message's now-compacted text to
				   the cache file and record a small
				   permanent FILECACHE entry -- see
				   bezerk.h's FILECACHE comment.

				   [From line][header] (msgstart through
				   hdrstart+hdrsize) is always genuinely
				   contiguous -- but bodystart is NOT always
				   exactly hdrstart+hdrsize: when the header's
				   last line(s) before the blank line were
				   Status:/X-Status: lines being deleted, the
				   '\n'/'\0' cases above just patch a synthetic
				   newline in and shrink hdrsize, without
				   physically memmove()'ing the now-logically-
				   deleted bytes out (unlike the default case,
				   which does memmove() when a normal line
				   follows a deletion run) -- so there can be a
				   few bytes of stale gap between the logical
				   header end and the real bodystart pointer.
				   This cache file is laid out as
				   [header][body] with NO gap, so header and
				   body are written separately here rather
				   than as one assumed-contiguous span.
				   bodystart may be NIL (a message with no
				   blank line before end of text), but only
				   when bodysize == 0, so the second write
				   below is skipped in that case. */
	  FILECACHE *fc = (FILECACHE *) fs_get (sizeof (FILECACHE));
	  unsigned long fromlen = hdrstart - msgstart;
	  unsigned long hlen = fromlen + hdrsize;
	  if ((lseek (LOCAL->cf,LOCAL->cfsize,L_SET) < 0) ||
	      (hlen && write (LOCAL->cf,msgstart,(unsigned) hlen) != hlen) ||
	      (bodysize &&
	       write (LOCAL->cf,bodystart,(unsigned) bodysize) != bodysize))
	    fatal ("Message cache file write failed");
	  fc->hoff = LOCAL->cfsize;
	  fc->fromlen = fromlen;
	  fc->headersize = hdrsize;
	  fc->bodysize = bodysize;
	  memcpy (fc->status,msgstatus,sizeof (STATUS));
	  LOCAL->cfsize += hlen + bodysize;
				/* expand the message-info cache if needed */
	  if (LOCAL->msgs) {
	    if (nmsgs >= LOCAL->cachesize)
	      fs_resize ((void **) &LOCAL->msgs,
			 (LOCAL->cachesize += CACHEINCREMENT) * sizeof (FILECACHE *));
	  }
	  else LOCAL->msgs = (FILECACHE **)
	    fs_get ((LOCAL->cachesize = nmsgs + CACHEINCREMENT) *sizeof (FILECACHE *));
	  LOCAL->msgs[nmsgs - 1] = fc;
	}

				/* restore the bytes the fence replaced */
	msgend[0] = savedch[0]; msgend[1] = savedch[1];
	if (e) {		/* saw end of message? */
	  i -= e - s1;		/* new unparsed data count */
	  s = s1 = e;		/* advance to new message */
	}
	else break;		/* else punt this loop */
      }
      if (delta) {		/* end of message not found? */
	s1 += i;		/* end of unparsed data */
	if (s != LOCAL->buf){	/* message doesn't begin at buffer? */
	  i = s1 - s;		/* length of message so far */
	  memmove (LOCAL->buf,s,(unsigned)i);
	  s = LOCAL->buf;	/* message now starts at buffer origin */
	  s1 = s + i;		/* calculate new end of unparsed data */
	}
      }
    } while (delta);		/* until nothing more new to read */
  }
  else {			/* no change, don't babble if never got time */
    if (LOCAL->filetime && LOCAL->filetime != sbuf.st_mtime)
      mm_log ("New mailbox modification time but apparently no changes",WARN);
  }
				/* maybe move where the buffer is in memory --
				   safe now that every message has already
				   been flushed to LOCAL->cf above, so
				   nothing still needs LOCAL->buf's content */
  if (LOCAL->buflen > CHUNK) {
    fs_give ((void **) &LOCAL->buf);
    LOCAL->buf = (char *) fs_get ((LOCAL->buflen = CHUNK) + 1);
  }
  i = nmsgs;
  while (i < LOCAL->cachesize) LOCAL->msgs[i++] = NIL;
				/* update parsed file size and time */
  LOCAL->filesize = sbuf.st_size;
  LOCAL->filetime = sbuf.st_mtime;
  mail_exists (stream,nmsgs);	/* notify upper level of new mailbox size */
  mail_recent (stream,recent);	/* and of change in recent messages */
  if (recent) LOCAL->dirty = T;	/* mark dirty so O flags are set */
  return fd;			/* return the winnage */
}

/* Berkeley search for end of message
 * Accepts: start of message
 *	    start of new data
 *	    size of new data
 * Returns: pointer to start of new message if one found
 */

#define Word unsigned long

char *bezerk_eom (char *som,char *sod,long i)
{
  char *s = (sod > som) ? sod - 1 : sod;
  char *t;
  int ti,zn;
  union {
    unsigned long wd;
    char ch[9];
  } wdtest;
  strcpy (wdtest.ch,"AAAA1234");/* constant for word testing */
  while ((s > som) && *s-- != '\n');
  if (wdtest.wd != 0x41414141) {/* not a 32-bit word machine? */
    while (s = strstr (s,"\nFrom ")) if (s++ && VALID (s,t,ti,zn)) return s;
  }
  else {			/* can do it faster this way */
    register Word m = 0x0a0a0a0a;
    while ((long) s & 3)	/* any characters before word boundary? */
      if ((*s++ == '\n') && s[0] == 'F' && VALID (s,t,ti,zn)) return s;
    i = (sod + i) - s;		/* total number of tries */
    do {			/* fast search for newline */
      if (0x80808080 & (0x01010101 + (0x7f7f7f7f & ~(m ^ *(Word *) s)))) {
				/* interesting word, check it closer */
	if (*s++ == '\n' && s[0] == 'F' && VALID (s,t,ti,zn)) return s;
	else if (*s++ == '\n' && s[0] == 'F' && VALID (s,t,ti,zn)) return s;
	else if (*s++ == '\n' && s[0] == 'F' && VALID (s,t,ti,zn)) return s;
	else if (*s++ == '\n' && s[0] == 'F' && VALID (s,t,ti,zn)) return s;
      }
      else s += 4;		/* try next word */
      i -= 4;			/* count a word checked */
    } while (i > 24);		/* continue until end of plausible string */
  }
  return NIL;
}

/* Berkeley extend mailbox to reserve worst-case space for expansion
 * Accepts: MAIL stream
 *	    file descriptor
 *	    error string
 * Returns: T if extend OK and have gone critical, NIL if should abort
 */

int bezerk_extend (MAILSTREAM *stream,int fd,char *error)
{
  struct stat sbuf;
  FILECACHE *m;
  char tmp[MAILTMPLEN];
  int i,ok;
  long f;
  char *s;
  int retry;
				/* calculate estimated size of mailbox */
  for (i = 0,f = 0; i < stream->nmsgs; i++) {
    m = LOCAL->msgs[i];		/* get cache pointer */
    f += m->fromlen + m->headersize + sizeof (STATUS) +
      m->bodysize + 1;		/* update guesstimate */
    }
  mm_critical (stream);		/* go critical */
				/* return now if file large enough */
  if (f <= LOCAL->filesize) return T;
  s = (char *) fs_get (f -= LOCAL->filesize);
  memset (s,0,(unsigned)f);	/* get a block of nulls */
				/* get to end of file */
  lseek (fd,LOCAL->filesize,L_SET);
  do {
    retry = NIL;		/* no retry yet */
    if (!(ok = (write (fd,s,(unsigned)f) >= 0))) {
      i = errno;		/* note error before doing ftruncate */
				/* restore prior file size */
      ftruncate (fd,LOCAL->filesize);
      fsync (fd);		/* is this necessary? */
      fstat (fd,&sbuf);		/* now get updated file time */
      LOCAL->filetime = sbuf.st_mtime;
				/* punt if that's what main program wants */
      if (mm_diskerror (stream,i,NIL)) {
	mm_nocritical (stream);	/* exit critical */
	sprintf (tmp,"%s: %s",error,strerror (i));
	mm_notify (stream,tmp,WARN);
      }
      else retry = T;		/* set to retry */
    }
  } while (retry);		/* repeat if need to try again */
  fs_give ((void **) &s);	/* flush buffer of nulls */
  return ok;			/* return status */
}

/* Berkeley save mailbox
 * Accepts: MAIL stream
 *	    mailbox file descriptor
 *
 * Mailbox must be readwrite and locked for exclusive access.
 */

void bezerk_save (MAILSTREAM *stream,int fd)
{
  struct stat sbuf;
  long i,e;
  int err;
  int retry;
				/* scratch buffer for bezerk_write_message(),
				   grown as needed, shared/reused across every
				   message instead of a per-message alloc */
  char *buf = NIL;
  unsigned long buflen = 0;
  do {				/* restart point if failure */
    retry = NIL;		/* no need to retry yet */
				/* start at beginning of file */
    lseek (fd,LOCAL->filesize = 0,L_SET);
				/* loop through all messages, one writev()
				   per message -- was batched up to 16
				   messages per writev() when the source
				   bytes were RAM-resident; now that they're
				   read from LOCAL->cf into a single reused
				   scratch buffer, batching would mean the
				   next message's read clobbers a still-
				   unflushed earlier one, so this trades a
				   few more write(2) calls (a cold path) for
				   not needing several buffers alive at once */
    for (i = 1; i <= stream->nmsgs; i++) {
      if ((e = bezerk_write_message (stream,fd,LOCAL->msgs[i-1],
				      &buf,&buflen)) < 0) {
	err = errno;
	sprintf (LOCAL->buf,"Unable to rewrite mailbox: %s",strerror (err));
	mm_log (LOCAL->buf,WARN);
	mm_diskerror (stream,err,T);
	retry = T;		/* must retry */
	break;		/* abort this particular try */
      }
      LOCAL->filesize += e;	/* count these bytes in data */
    }
  } while (retry);		/* repeat if need to try again */
  if (buf) fs_give ((void **) &buf);
  fsync (fd);			/* make sure the disk has the update */
				/* nuke any cruft after that */
  ftruncate (fd,LOCAL->filesize);
  fsync (fd);			/* is this necessary? */
  fstat (fd,&sbuf);		/* now get updated file time */
  LOCAL->filetime = sbuf.st_mtime;
  LOCAL->dirty = NIL;		/* stream no longer dirty */
  mm_nocritical (stream);	/* exit critical */
}

/* Berkeley copy messages
 * Accepts: MAIL stream
 *	    mailbox name
 * Returns: T if copy successful else NIL
 */

int bezerk_copy_messages (MAILSTREAM *stream,char *mailbox)
{
  char file[MAILTMPLEN];
  char lock[MAILTMPLEN];
  int fd;
  struct stat sbuf;
  long i;
  int ok = T;
				/* scratch buffer for bezerk_write_message(),
				   see bezerk_save()'s comment on why this is
				   one-message-at-a-time now */
  char *buf = NIL;
  unsigned long buflen = 0;
				/* make sure valid mailbox */
  if (!bezerk_isvalid (mailbox,file)) {
    if (errno == ENOENT)	/* failed, was it no such file? */
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
    else {
      sprintf (LOCAL->buf,"Not a Berkeley-format mailbox: %s",mailbox);
      mm_log (LOCAL->buf,ERROR);
    }
    return NIL;
  }
  if ((fd = bezerk_lock (bezerk_file (file,mailbox),O_WRONLY|O_APPEND|O_CREAT,
			 S_IREAD|S_IWRITE,lock,LOCK_EX)) < 0) {
    sprintf (LOCAL->buf,"Can't open destination mailbox: %s",strerror (errno));
    mm_log (LOCAL->buf,ERROR);
    return NIL;
  }
  mm_critical (stream);		/* go critical */
  fstat (fd,&sbuf);		/* get current file size */
				/* write all requested messages to mailbox */
  for (i = 1; ok && i <= stream->nmsgs; i++)
				/* write it if message selected */
    if (mail_elt (stream,i)->sequence &&
	bezerk_write_message (stream,fd,LOCAL->msgs[i - 1],&buf,&buflen) < 0) {
      sprintf (LOCAL->buf,"Message copy failed: %s",strerror (errno));
      mm_log (LOCAL->buf,ERROR);
      ftruncate (fd,sbuf.st_size);
      ok = NIL;
    }
  if (buf) fs_give ((void **) &buf);
  fsync (fd);			/* force out the update */
  bezerk_unlock (fd,NIL,lock);	/* unlock and close mailbox */
  mm_nocritical (stream);	/* release critical */
  return ok;			/* return whether or not succeeded */
}

/* Berkeley write message to mailbox
 * Accepts: MAIL stream
 *	    destination file descriptor
 *	    local cache for this message
 *	    pointer to scratch buffer pointer (grown as needed)
 *	    pointer to scratch buffer's current size
 * Returns: number of bytes written, or -1 on error (errno set, same as
 *	    writev())
 *
 * This routine is the reason why the local cache has a copy of the status.
 * We can be called to dump out the mailbox as part of a stream recycle, since
 * we don't write out the mailbox when flags change and hence an update may be
 * needed.  However, at this point the elt has already become history, so we
 * can't use any information other than what is local to us.
 *
 * Used to just build iovecs against RAM the caller could batch across many
 * messages into one writev(); now that the source bytes live in LOCAL->cf
 * (see bezerk.h's FILECACHE comment), this reads one message's text into
 * *buf itself and writes it immediately -- see bezerk_save()'s comment for
 * why batching across messages doesn't work with a shared scratch buffer.
 */

long bezerk_write_message (MAILSTREAM *stream,int fd,FILECACHE *m,
			   char **buf,unsigned long *buflen)
{
  struct iovec iov[4];
  int i = 0;
				/* From line + real header, contiguous */
  unsigned long hlen = m->fromlen + m->headersize;
				/* one read covers header and body, since
				   they sit contiguously in the cache file */
  bezerk_readcache (stream,m->hoff,hlen + m->bodysize,buf,buflen);
  iov[i].iov_base = *buf;	/* pointer/counter to headers */
				/* suppress extra newline if present */
  iov[i++].iov_len = ((*buf)[hlen - 2] == '\n') ? hlen - 1 : hlen;
  iov[i].iov_base = m->status;	/* pointer/counter to status */
  iov[i++].iov_len = strlen (m->status);
  iov[i].iov_base = *buf + hlen;	/* pointer/counter to text body */
  iov[i++].iov_len = m->bodysize;
  iov[i].iov_base = "\n";	/* pointer/counter to extra newline */
  iov[i++].iov_len = 1;
  return writev (fd,iov,i);
}

/* Berkeley update status string
 * Accepts: destination string to write
 *	    message cache entry
 */

void bezerk_update_status (char *status,MESSAGECACHE *elt)
{
  /* This used to be an sprintf(), but thanks to certain cretinous C libraries
     with horribly slow implementations of sprintf() I had to change it to this
     mess.  At least it should be fast. */
  char *t = status + 8;
  status[0] = 'S'; status[1] = 't'; status[2] = 'a'; status[3] = 't';
  status[4] = 'u'; status[5] = 's'; status[6] = ':';  status[7] = ' ';
  if (elt->seen) *t++ = 'R'; *t++ = 'O'; *t++ = '\n';
  *t++ = 'X'; *t++ = '-'; *t++ = 'S'; *t++ = 't'; *t++ = 'a'; *t++ = 't';
  *t++ = 'u'; *t++ = 's'; *t++ = ':'; *t++ = ' ';
  if (elt->deleted) *t++ = 'D'; if (elt->flagged) *t++ = 'F';
  if (elt->answered) *t++ = 'A';
  *t++ = '\n'; *t++ = '\n'; *t++ = '\0';
}

/* Berkeley read message text out of the per-stream cache file
 * Accepts: MAIL stream
 *	    cache file offset
 *	    number of bytes to read
 *	    pointer to destination buffer pointer
 *	    pointer to destination buffer's current size
 * Returns: destination buffer, filled with size bytes plus a trailing NUL
 *
 * Every FILECACHE consumer goes through here instead of dereferencing a
 * header/body RAM pointer directly -- see bezerk.h's FILECACHE comment.
 * The destination buffer and its size follow the same grow-as-needed
 * convention LOCAL->buf itself uses throughout this file.
 */

char *bezerk_readcache (MAILSTREAM *stream,off_t off,unsigned long size,
			char **buf,unsigned long *buflen)
{
  /*
   * "size > *buflen" alone left *buf NULL forever on a stream whose
   * very first call here asks for 0 bytes (a message with an empty
   * body, size==0, *buflen starts at 0 too) -- the tie-off write below
   * then dereferenced that NULL. See TODO-pdp11-done.md.
   */
  if (!*buf || size > *buflen) {	/* make sure enough buffer space */
    if (*buf) fs_give ((void **) buf);
    *buf = (char *) fs_get ((*buflen = size) + 1);
  }
  if ((lseek (LOCAL->cf,off,L_SET) < 0) ||
      (size && read (LOCAL->cf,*buf,(unsigned) size) != size))
    fatal ("Message cache file read failed");
  (*buf)[size] = '\0';		/* tie off string */
  return *buf;
}

