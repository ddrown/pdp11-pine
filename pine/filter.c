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
     filter.c

     This code provides a generalized, flexible way to allow
     piping of data thru filters.  Each filter is passed a structure
     that it will use to hold its static data while it operates on 
     the stream of characters that are passed to it.  After processing
     it will either return or call the next filter in 
     the pipe with any character (or characters) it has ready to go. This
     means some terminal type of filter has to be the last in the 
     chain (i.e., one that writes the passed char someplace, but doesn't
     call another filter).

     See below for more details.

     The motivation is to handle MIME decoding, richtext conversion, 
     iso_code stripping and anything else that may come down the
     pike (e.g., PEM) in an elegant fashion.  mikes (920811)

   TODO:
       reasonable error handling

  ====*/


#include "headers.h"
#include "../pico/pico.h"

/* message repeated verbatim at two call sites below -- shared storage
 * instead of a separate copy at each site */
static char *bad_qp_hex = "Bad hex char in QP encoding";


/* INTERNAL PROTOTYPES */
int	gf_so_readc(unsigned char *);
int	gf_so_writec(int);
int	gf_sreadc(unsigned char *);
int	gf_swritec(int);
int	gf_freadc(unsigned char *);
int	gf_fwritec(int);
void	gf_terminal(FILTER_S *, int, int);
void    gf_error(char *);
void	gf_qp_default(FILTER_S *, int);
void	gf_8bit_default(FILTER_S *, int);
void	gf_8bit_put_quote(FILTER_S *, int);
void	gf_8bit_put(FILTER_S *, int);
int	so_reaquire(STORE_S *);



/*
 * GENERALIZED STORAGE FUNCTIONS.  Idea is to allow creation of
 * storage objects that can be written into and read from without
 * the caller knowing if the storage is core or in a file 
 * or whatever.
 */
/* Was an unconditional CHUNK-sized 8192/4096 (matching bezerk.c's
 * CHUNK-sized buffers) -- same class of oversized fixed buffer as the
 * CHUNK-vs-MAILTMPLEN bezerk_open() bug and send.c's RBUFSZ, both
 * fixed earlier (TODO-pdp11-done.md). so_writec()/so_puts() already
 * grow a CharStar object's storage on demand (the latter in a loop,
 * so multiple MSIZE_INC steps in one call are handled correctly), so
 * a smaller start size only changes resize frequency, not
 * correctness. This is the shared constant behind every CharStar
 * object in the tree (composer body, reply/forward body, fcc
 * storage, ...), so it's a much bigger lever than any one call site's
 * own buffer.
 *
 * Shrunk further from MAILTMPLEN (1024) after a confirmed real-hardware
 * crash: mail_view_screen()'s so_get(CharStar,...) -- the message-
 * display buffer -- hit "Out of free storage (1024 bytes)" right after
 * a Send, at a point where the process was already close enough to the
 * 64KB D-space ceiling that even this single upfront allocation didn't
 * fit (confirmed via adb: fs_get(02000) octal = 1024 decimal, called
 * directly from so_get()). 256 makes the *first* allocation for any
 * CharStar object small enough to have real headroom even under this
 * kind of memory pressure; so_puts()/so_writec()'s existing on-demand
 * growth (unaffected by this change) covers whatever the real content
 * size turns out to be, same as always -- see this same tree's
 * TmpFileStar/FileStar source types (already implemented, already used
 * elsewhere) as the real fix if 256 turns out not to be enough: the
 * message-display path doesn't support pointing scrolltool() at a file
 * source yet, so switching CharStar to TmpFileStar there specifically
 * would need real changes to format_scroll_text()'s line-array-building
 * logic, not just a source-type swap (see TODO-pdp11.md). */
#define	MSIZE_INIT	256
#define	MSIZE_INC	256

/*
 * allocate resources associated with the specified type of
 * storage.  If requesting a named file object, open it for 
 * appending, else just open a temp file.
 *
 * return the filled in storage object
 */
STORE_S *
so_get(source, name, rtype)
    SourceType  source;			/* requested storage type */
    char       *name;			/* file name 		  */
    int		rtype;			/* file access type	  */
{
    STORE_S *so;
    char    *type = (rtype&WRITE_ACCESS) ? "a+" : "r";

    so = (STORE_S *)fs_get(sizeof(STORE_S));
    memset(so, 0, sizeof(STORE_S));
    so->flags |= rtype;
    
    if(name)					/* stash the name */
      so->name = cpystr(name);
    else if(source == TmpFileStar)		/* make one up! */
      so->name = cpystr(temp_nam(NULL, "pine-tmp"));

    so->src = source;
    if(so->src == FileStar || so->src == TmpFileStar){
	/*
	 * The reason for both FileStar and TmpFileStar types is
	 * that, named or unnamed, TmpFileStar's are unlinked
	 * when the object is given back to the system.  This is
	 * useful for keeping us from running out of file pointers as
	 * the pointer associated with the object can be temporarily
	 * returned to the system without destroying the object.
	 * 
	 * The programmer is warned to be careful not to assign the
	 * TmpFileStar type to any files that are expected to remain
	 * after the dust has settled!
	 */
	if(so->name){
	    if((so->txt = (void *)fopen(so->name, type)) == NULL){
		fs_give((void **)&so->name);
		fs_give((void **)&so); 		/* so freed & set to NULL */
		q_status_message(1, 3, 5,"Can't create msg file");
	    }
	}
	else{
	    if((so->txt = (void *)create_tmpfile()) == NULL){
		q_status_message(1, 3, 5,"Can't create temp file");
		fs_give((void **)&so);		/* so freed & set to NULL */
	    }
	}
    }
    else{
	so->size = MSIZE_INIT;
	so->txt  = (void *)fs_get((size_t)so->size * sizeof(char));
	memset(so->txt, 0, (size_t)so->size * sizeof(char));
	so->txtp = (unsigned char *)so->txt;
    }

    return(so);
}


/* 
 * so_give - free resources associated with a storage object and then
 *           the object itself.
 */
void
so_give(so)
STORE_S **so;
{
    if(!so)
      return;

    if((*so)->src == FileStar || (*so)->src == TmpFileStar){
        if((*so)->txt)
	  fclose((FILE *)(*so)->txt);	/* disassociate from storage */

	if((*so)->name && (*so)->src == TmpFileStar)
	  unlink((*so)->name);		/* really disassociate! */
    }
    else if((*so)->txt)
      fs_give((void **)&((*so)->txt));

    if((*so)->name)
      fs_give((void **)&((*so)->name));	/* blast the name            */

    fs_give((void **)so);		/* release the object        */
}


/*
 * put a character into the specified storage object, 
 * expanding if neccessary 
 *
 * return 1 on success and 0 on failure
 */
int
so_writec(c, so)
    int      c;
    STORE_S *so;
{
    unsigned char ch = (unsigned char) c;

    if(so->src == CharStar){
	if((so->used + 2) >= so->size){
	    so->size += MSIZE_INC;		/* need to resize! */
	    fs_resize(&so->txt, (size_t)so->size * sizeof(char));
	    so->txtp = (unsigned char *)so->txt + so->used;
	    memset(so->txtp, 0, (size_t)(so->size - so->used) * sizeof(char));
	}

	*(so->txtp)++ = ch;
	so->used++;
	return(1);
    }
    else{
	if(!so->txt && !so_reaquire(so))
	  return(0);

	return(fwrite(&ch, sizeof(char), (size_t)1, (FILE *)so->txt));
    }
}


/*
 * get a character from the specified storage object.
 * 
 * return 1 on success and 0 on failure
 */
int
so_readc(c, so)
    unsigned char *c;
    STORE_S       *so;
{
    if(so->src == CharStar){
	/* NB: not written as a single "cond ? a = b, 1 : 0" return --
	 * 2.11BSD's cc rejects a comma expression in a conditional
	 * operator's true-branch ("Illegal conditional"/"Lvalue
	 * required"). */
	if((so->txtp - (unsigned char *)so->txt) < so->used){
	    *c = *(so->txtp)++;
	    return(1);
	}
	else
	  return(0);
    }
    else{		/* must be FileStar or TmpFileStar */
	if(!so->txt && !so_reaquire(so))
	  return(0);

	return(fread(c, sizeof(char), (size_t)1, (FILE *)so->txt));
    }
}


/* 
 * write a string into the specified storage object, 
 * expanding if necessary (and cheating if the object 
 * happens to be a file!)
 *
 * return 1 on success and 0 on failure
 */
int
so_puts(so, s)
    STORE_S *so;
    char    *s;
{
    if(so->src == CharStar){
	/* was `int l` -- a 16-bit-narrow type computed from strlen() on
	 * the real target, capable of silently wrapping (even negative)
	 * for a long enough string in a way this 32-bit dev build could
	 * never observe. Found while canary-hunting a real-hardware
	 * heap-corruption bug -- see TODO-pdp11-done.md. */
	long l = (long) strlen(s);

	if((so->used + l + 1) >= so->size){
	    while(so->size <= (so->used + l + 1))
	      so->size += MSIZE_INC;		/* need to resize! */

	    fs_resize(&so->txt, (size_t)so->size * sizeof(char));
	    so->txtp = (unsigned char *)so->txt + so->used;
	    memset(so->txtp, 0, (size_t)(so->size - so->used) * sizeof(char));
	}

	while(*s != '\0')
	  *(so->txtp)++ = *s++;

	so->used += l;
	return(1);
    }
    else{			/* must be FileStar or TmpFileStar */
	if(!so->txt && !so_reaquire(so))
	  return(0);

	/* fwrite not necessary, but it helps */
	return(fwrite(s, strlen(s)*sizeof(char), (size_t)1, (FILE *)so->txt));
    }
}


/*
 * Possition the storage object's pointer to the given offset
 * from the start of the object's data.
 */
int
so_seek(so, pos, orig)
STORE_S *so;
long     pos;
int      orig;
{
    if(so->src == CharStar){
	/* NB: not written as "cond ? a = b, 0 : -1" returns -- 2.11BSD's
	 * cc rejects a comma expression in a conditional operator's
	 * true-branch ("Illegal conditional"/"Lvalue required"). */
	switch(orig){
	    case 0 :				/* SEEK_SET */
	      if(pos < so->used){
		  so->txtp = (unsigned char *)so->txt + pos;
		  return(0);
	      }
	      else
		return(-1);
	    case 1 :				/* SEEK_CUR */
	      if(pos < (so->used - (so->txtp-(unsigned char *)so->txt))){
		  so->txtp += pos;
		  return(0);
	      }
	      else
		return(-1);
	    case 2 :				/* SEEK_END */
	      if(pos < so->used){
		  so->txtp = (unsigned char *)so->txt+(so->used-pos);
		  return(0);
	      }
	      else
		return(-1);
	    default :
	      return(-1);
	}
    }
    else{			/* FileStar or TmpFileStar */
	if(!so->txt && !so_reaquire(so))
	  return(0);

	return(fseek((FILE *)so->txt, pos, orig));
    }
}


/*
 * so_release - a rather misnamed function.  the idea is to release
 *              what system resources we can (e.g., open files).
 *              while maintaining a reference to it.
 *              it's up to the functions that deal with this object
 *              next to re-aquire those resources.
 */
int
so_release(so)
STORE_S *so;
{
    if(so->txt && so->name && (so->src == FileStar || so->src == TmpFileStar)){
	if(fget_pos((FILE *)so->txt, (fpos_t *)&(so->used)) == 0){
	    fclose((FILE *)so->txt);		/* free the handle! */
	    so->txt = NULL;
	}
    }

    return(1);
}


/*
 * so_reaquire - get any previously released system resources we
 *               may need for the given storage object.
 *       NOTE: at the moment, only FILE * types of objects are
 *             effected, so it only needs to be called before
 *             references to them.
 *                     
 */
int
so_reaquire(so)
STORE_S *so;
{
    int   rv = 1;
    char  *type = ((so->flags)&WRITE_ACCESS) ? "a+" : "r";

    if(!so->txt && (so->src == FileStar || so->src == TmpFileStar)){
	if((so->txt=(void *)fopen(so->name, type)) == NULL){
	    q_status_message2(1, 3, 5, "Can't reopen %s: %s", so->name,
				error_description(errno));
	    rv = 0;
	}
	else if(fset_pos((FILE *)so->txt, (fpos_t *)&(so->used))){
	    q_status_message2(1, 3, 5, "Seek error in %s: %s", 
				so->name, error_description(errno));
	    rv = 0;
	}
    }

    return(rv);
}


/*
 * so_text - return a pointer to the text the store object passed
 */
void *
so_text(so)
STORE_S *so;
{
    return((so) ? so->txt : NULL);
}


/*
 * END OF GENERALIZE STORAGE FUNCTIONS
 */


/*
 * Start of filters, pipes and various support functions
 */

/*
 * pointer to first function in a pipe, and pointer to last filter
 */
FILTER_S         *gf_master = NULL;
static	gf_io_t   last_filter;
static	char     *gf_error_string;
static	jmp_buf   gf_error_state;


/*
 * A list of states used by the various filters.  Reused in many filters.
 */
#define	DFL	0
#define	EQUAL	1
#define	HEX	2
#define	WSPACE	3
#define	CCR	4
#define	CESCP	5
#define	TOKEN	6


/*
 * Generalized getc and putc routines.  provided here so they don't
 * need to be re-done elsewhere to 
 */

/*
 * pointers to objects to be used by the generic getc and putc
 * functions
 */
static struct gf_io_struct {
    FILE          *file;
    char          *txtp;
    unsigned long  n;
} gf_in, gf_out;
static STORE_S *gf_so_in, *gf_so_out;


/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_readc(gc, txt, len, src)
    gf_io_t       *gc;
    void          *txt;
    unsigned long  len;
    SourceType     src;
{
    gf_in.n = len;
    if(src == FileStar){
	gf_in.file = (FILE *)txt;
	fseek(gf_in.file, 0L, 0);
	*gc = gf_freadc;
    }
    else{
	gf_in.txtp = (char *)txt;
	*gc = gf_sreadc;
    }
}


/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_so_readc(gc, so)
    gf_io_t *gc;
    STORE_S *so;
{
    gf_so_in = so;
    *gc      = gf_so_readc;
}


/*
 * setup to use and return a pointer to the generic
 * putc function
 */
void
gf_set_so_writec(pc, so)
    gf_io_t *pc;
    STORE_S *so;
{
    gf_so_out = so;
    *pc       = gf_so_writec;
}


/*
 * put the character to the object previously defined
 */
int
gf_so_writec(c)
int c;
{
    return(so_writec(c, gf_so_out));
}


/*
 * get a character from an object previously defined
 */
int
gf_so_readc(c)
unsigned char *c;
{
    return(so_readc(c, gf_so_in));
}


/* get a character from a file */
/* assumes gf_out struct is filled in */
int
gf_freadc(c)
unsigned char *c;
{
    return(fread(c, sizeof(unsigned char), (size_t)1, gf_in.file));
}


/* put a character to a file */
/* assumes gf_out struct is filled in */
int
gf_fwritec(c)
    int c;
{
    return(fwrite(&c, sizeof(unsigned char), (size_t)1, gf_out.file));
}


/* get a character from a string, return nonzero if things OK */
/* assumes gf_out struct is filled in */
int
gf_sreadc(c)
unsigned char *c;
{
    /* NB: not "cond ? a = b, n-- : 0" -- 2.11BSD's cc rejects a comma
     * expression in a conditional operator's true-branch ("Illegal
     * conditional"/"Lvalue required"). */
    if(gf_in.n){
	*c = *(gf_in.txtp)++;
	return(gf_in.n--);
    }
    else
      return(0);
}


/* put a character into a string, return nonzero if things OK */
/* assumes gf_out struct is filled in */
int
gf_swritec(c)
    int c;
{
    /* NB: not "cond ? a = b, n-- : 0" -- 2.11BSD's cc rejects a comma
     * expression in a conditional operator's true-branch ("Illegal
     * conditional"/"Lvalue required"). */
    if(gf_out.n){
	*(gf_out.txtp)++ = c;
	return(gf_out.n--);
    }
    else
      return(0);
}


/*
 * output the given string with the given function
 */
int
gf_puts(s, pc)
    register char *s;
    gf_io_t        pc;
{
    while(*s != '\0')
      if(!(*pc)(*s++))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * Start of generalized filter routines
 */

/* 
 * initializing function to make sure list of filters is empty.
 */
void
gf_filter_init()
{
    FILTER_S *flt, *fltn = gf_master;

    while((flt = fltn) != NULL){	/* free list of old filters */
	fltn = flt->next;
	fs_give((void **)&flt);
    }

    gf_master = NULL;
    gf_error_string = NULL;		/* clear previous errors */
}



/*
 * link the given filter into the filter chain
 */
int
gf_link_filter(filter_t f)
{
    char      c = 0;
    FILTER_S *new, *tail = gf_master;

    new = (FILTER_S *)fs_get(sizeof(FILTER_S));
    memset(new, 0, sizeof(FILTER_S));
    new->f = f;				/* set the function pointer     */
    (*f)(new, c, GF_RESET);		/* have it setup initial state  */

    if(tail == NULL){			/* attach new struct to list    */
	gf_master = new;		/* start a new list */
    }
    else{				/* or add it to end of existing  */
	while(tail->next != NULL)	/* list  */
	  tail = tail->next;

	tail->next = new;
    }

    return(0);
}


/*
 * terminal filter, doesn't call any other filters, typically just does
 * something with the output
 */
void
gf_terminal(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){			/* generic terminal filter */
	if((*last_filter)(c) <= 0)
	  gf_error("Pipe write error");
    }
}


/*
 * set some outside gf_io_t function to the terminal function 
 * for example: a function to write a char to a file or into a buffer
 */
void
gf_set_terminal(f)			/* function to set generic filter */
    gf_io_t f;
{
    last_filter = f;
}


/*
 * common function for filter's to make it known that an error
 * has occurred.  Jumps back to gf_pipe with error message.
 */
void
gf_error(s)
    char *s;
{
    /* let the user know the error passed in s */
    gf_error_string = s;
    longjmp(gf_error_state, 1);
}


/*
 * The routine that shoves each byte through the chain of
 * filters.  It sets up error handling, and the terminal function.
 * Then loops getting bytes with the given function, and passing
 * it on to the first filter in the chain.
 */
char *
gf_pipe(gc, pc)
    gf_io_t gc, pc;			/* how to get a character */
{
    unsigned char c;
    /*
     * set up for any errors a filter may encounter
     */
    if(setjmp(gf_error_state)){
	return(gf_error_string); 	/*  */
    }

    /*
     * set and link in the terminal filter
     */
    gf_set_terminal(pc);
    gf_link_filter(gf_terminal);

    /*
     * while there are chars to process, send them thru the pipe
     */
    while((*gc)(&c))
      (*gf_master->f)(gf_master, (c&0xff), GF_DATA);

    /*
     * toss an end-of-data marker down the pipe to give filters
     * that have any buffered data the opportunity to dump it
     */
    (*gf_master->f)(gf_master, 0, GF_EOD);

    return(NULL);			/* everything went OK */
}


/*
 * "Network output" terminal function -- relocated here from send.c
 * (was l_f/l_stream/l_out_buf/l_putc/l_flush_net). See TODO-pdp11-done.md
 * for the full diagnosis: gf_terminal() above calls through last_filter,
 * a gf_io_t set at runtime via gf_set_terminal(), which on this PDP-11
 * auto-overlay ("ld -i -X") link is only safe if the pointer's target
 * lives in the SAME overlay module as gf_terminal() itself. send.c's
 * old l_putc() lived in a *different* overlay (OV5) than filter.o
 * (OV1) -- calling gf_pipe() from send.c swapped OV5 out to load OV1,
 * invalidating a raw function pointer into l_putc() that was taken
 * *before* the swap, so gf_terminal()'s indirect call jumped into
 * whatever instruction bytes OV1 happened to have loaded at that
 * address instead. Symptoms ranged from a silently-missing message
 * body to a genuine infinite loop unresponsive to every signal but
 * SIGKILL to nonsensical adb backtraces, depending on what garbage
 * code got executed. Moving this whole mechanism into filter.c
 * guarantees gf_output_putc() is always resident whenever gf_terminal()
 * is (same module, same overlay), so the pointer can never go stale.
 */
/* unix_puts() (send.c, prototyped in pine.h) is called directly below.
 * gf_out_putc()/gf_flush_net() used to reach it through a
 * stored soutr_t pointer (gf_out_f) instead of calling it directly --
 * the exact same cross-overlay hazard as the l_putc()/last_filter bug
 * just described, one level further out: gf_set_output() (called from
 * send.c, so send.o's overlay is guaranteed loaded right then) stored
 * unix_puts's address, but gf_out_putc()/gf_flush_net() (this file,
 * a *different* overlay) called through that stored pointer much
 * later, after gf_pipe()'s filter chain had certainly swapped several
 * other overlays through the same region -- so unix_puts's overlay
 * was usually not the one resident by then. Confirmed via a real adb
 * crash: SIGSEGV/SIGILL inside gf_flush_net(), called from
 * pine_rfc822_output_body(), called from append_message2(), called
 * from call_sendmail() -- i.e. on every real Send. Unlike the
 * l_putc() fix, this one doesn't need relocating any code: unix_puts()
 * has exactly one real caller chain (grep confirms `f` is always
 * literally `unix_puts` at pine_rfc822_output_body()'s one non-
 * recursive call site in append_message2()), so gf_out_putc()/
 * gf_flush_net() now just call it directly -- an ordinary
 * statically-visible cross-file call, which the auto-overlay linker's
 * trampoline mechanism (unlike a raw runtime pointer) already handles
 * correctly regardless of which overlay unix_puts ends up in. */
static TCPSTREAM *gf_out_stream;
static STORE_S   *gf_out_fcc_so;
static unsigned   gf_out_c_in_buf = 0;

/*
 * gf_out_buf is a fixed, dedicated buffer -- unlike tmp_20k_buf, its
 * job is purely to batch bytes of the (already-formatted) outgoing
 * message body to cut down on write() calls, so its size is a
 * self-contained performance tradeoff, not a function of message
 * content, and it never needs to grow.
 */
#define GF_OUT_BUF_LEN 1024
static char       gf_out_buf[GF_OUT_BUF_LEN + 2];

/*
 * gf_set_output - register the real destination (stream, plus the
 *                 optional Fcc storage object) before calling gf_pipe()
 *                 with gf_output_putc_fn()'s function as the terminal.
 *                 Replaces send.c's old direct l_f/l_stream assignment.
 */
void
gf_set_output(TCPSTREAM *s, STORE_S *fcc_so)
{
    gf_out_stream = s;
    gf_out_fcc_so = fcc_so;
    gf_out_c_in_buf = 0;
}

/*
 * gf_out_putc - gf_io terminal function that calls unix_puts() to
 *               flush a full buffer. Static -- callers never invoke
 *               this directly, only indirectly via the pointer
 *               gf_output_putc_fn() returns (see gf_set_terminal()) --
 *               that indirection is fine since it never leaves this
 *               file/overlay, unlike the gf_out_f one removed above.
 */
static int
gf_out_putc(int c)
{
    gf_out_buf[gf_out_c_in_buf++] = (char) c;
    gf_out_buf[gf_out_c_in_buf]   = '\0';

    if(gf_out_c_in_buf > GF_OUT_BUF_LEN){
	if(gf_out_fcc_so)
	  so_puts(gf_out_fcc_so, gf_out_buf);

        gf_out_c_in_buf = 0;
	/* gf_out_stream is NULL on the Fcc-only/no-recipient path
	 * (pine_rfc822_output() called with f=NULL,s=NULL -- see
	 * call_mailer()) -- there's no real destination to flush to
	 * then, only the Fcc copy already written just above. */
        return(gf_out_stream ? (int) unix_puts(gf_out_stream, gf_out_buf) : 1);
    }
    else
	return(TRUE);
}

/*
 * gf_output_putc_fn - hand back gf_out_putc's address for use as a
 *                      gf_io_t terminal function (see gf_pipe()'s pc
 *                      parameter). Call gf_set_output() first.
 */
gf_io_t
gf_output_putc_fn(void)
{
    return((gf_io_t) gf_out_putc);
}

/*
 * gf_flush_net - empties gf_out_putc's buffer. Call once after gf_pipe()
 *                returns successfully.
 */
void
gf_flush_net(void)
{
    if(gf_out_c_in_buf){
	if(gf_out_stream)
	  unix_puts(gf_out_stream, gf_out_buf);

	if(gf_out_fcc_so)
	  so_puts(gf_out_fcc_so, gf_out_buf);
    }

    gf_out_c_in_buf = 0;
}



/*
 * END OF PIPE SUPPORT ROUTINES, BEGINNING OF FILTERS
 *
 * Filters MUST use the specified interface (pointer to filter
 * structure, the unsigned character to operate on, and a command
 * flag), and pass each resulting octet to the next filter in the
 * chain.  Only the terminal filter need not call another filter.
 * As a result, filters share a pretty general structure.
 * Typically three main conditionals separate initialization from
 * data from end-of-data command processing.
 * 
 * Lastly, being character-at-a-time, they're a little more complex
 * to write than filters operating on buffers because some state
 * must typically be kept between characters.  However, for a
 * little bit of complexity here, much convenience is gained later
 * as they can be arbitrarily chained together at run time and
 * consume few resources (especially memory or disk) as they work.
 *
 * A few notes about implementing filters:
 *
 *  - A generic filter template looks like:
 *    
 *    void
 *    gf_xxx_filter(f, c, flg)
 *        FILTER_S *f;
 *        int       c, flg;
 *    {
 *        if(flg == GF_DATA){
 *            // operate on c and pass it on here
 *            (*f->next->f)(f->next, c, GF_DATA);
 *        }
 *        else if(flg == GF_EOD){
 *            // process and buffered data here and shove it down
 *            (*f->next->f)(f->next, 0, GF_EOD);
 *        }
 *        else if(flg == GF_RESET){
 *            // initialize any data in the struct here
 *        }
 *    }
 *
 *  - Any free storage allocated during initialization (typically tied
 *    to the "line" pointer in FILTER_S) is the filter's responsibility
 *    to clean up when the GF_EOD command comes through.
 *
 *  - Filter's must pass GF_EOD they receive on to the next
 *    filter in the chain so it has the opportunity to flush
 *    any buffered data.
 *
 *  - All filters expect NVT end-of-lines.  The idea is to prepend
 *    or append either the gf_local_nvtnl or gf_nvtnl_local 
 *    os-dependant filters to the data on the appropriate end of the
 *    pipe for what needs to be done.
 *
 */



/*
 * BASE64 TO BINARY encoding and decoding routines below
 */

/*
 * array to help with base64 encoding
 */
static char *b64v="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";



/*
 * BINARY to BASE64 filter (encoding described in rfc1341)
 */
void
gf_binary_b64(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){
	/* NB: cast to (int) -- f->n is long, and 2.11BSD's cc rejects a
	 * non-int switch selector ("Illegal type of operand"). Safe: the
	 * value is always a small quantum position, never near LONG_MAX. */
	switch((int)(f->n++ % 3)){
	  case 0:
	    (*f->next->f)(f->next, b64v[c >> 2], GF_DATA);
					/* byte 1: high 6 bits (1) */
	    f->t = c << 4;		/* remember high 2 bits for next */
	    break;

	  case 1:
	    (*f->next->f)(f->next, b64v[((f->t)|(c>>4)) & 0x3f], GF_DATA);
	    f->t = c << 2;
	    break;

	  case 2:
	    (*f->next->f)(f->next, b64v[((f->t)|(c >> 6)) & 0x3f], GF_DATA);
	    (*f->next->f)(f->next, b64v[c & 0x3f], GF_DATA);
	    break;

	  default:
	    gf_error("Base64 encoder error");
	    return;			/* bummer! */
	}

	if((f->n % 45) == 0){		/* start a new line? */
	    (*f->next->f)(f->next, '\015', GF_DATA);
	    (*f->next->f)(f->next, '\012', GF_DATA);
	}
    }
    else if(flg == GF_EOD){		/* no more data */
	switch ((int)(f->n % 3)) {	/* handle trailing bytes */
	  case 0:			/* no trailing bytes */
	    break;

	  case 1:
	    (*f->next->f)(f->next, b64v[(f->t + (c >> 6)) & 0x3f], GF_DATA);
	    (*f->next->f)(f->next, '=', GF_DATA);	/* byte 3 */
	    (*f->next->f)(f->next, '=', GF_DATA);	/* byte 4 */
	    break;

	  case 2:
	    (*f->next->f)(f->next, b64v[(f->t + (c >> 6)) & 0x3f], GF_DATA);
	    (*f->next->f)(f->next, '=', GF_DATA);	/* byte 4 */
	    break;
	}

	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->n = 0L;
    }
}


/*
 * BASE64 to BINARY filter (encoding described in rfc1341)
 */
void
gf_b64_binary(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){
	if(f->f1){			/*  */
	    f->f1 = 0;
	    if (c != '=') {
		gf_error("Bad '=' in base64");
	        return;
	    }
	}

	if (isupper (c)) c -= 'A';
	else if (islower (c)) c -= 'a' - 26;
	else if (isdigit (c)) c -= '0' - 52;
	else if (c == '+') c = 62;
	else if (c == '/') c = 63;
	else if (c == '=') {		/* padding */
	    /* NB: cast to (int) -- f->n is long, and 2.11BSD's cc
	     * rejects a non-int switch selector ("Illegal type of
	     * operand"). Safe: value is always a small quantum position
	     * (0-3), reset well below LONG_MAX. */
	    switch ((int)((f->n)++)) {	/* check quantum position */
	      case 2:
		f->f1++;		/* expect an equal as next char */
		break;
	      case 3:
		f->n = 0L;		/* restart quantum */
		break;
	      default:			/* impossible quantum position */
		gf_error("Base64 decoder error");
		return;
	    }
	    return;
	}
	else return;			/* junk character */

	/* NB: cast to (int) -- f->n is long, and 2.11BSD's cc rejects a
	 * non-int switch selector ("Illegal type of operand"). Safe:
	 * value is always a small quantum position (0-3). */
	switch ((int)((f->n)++)) {	/* install based on quantum position */
	  case 0:
	    f->t = c << 2;		/* byte 1: high 6 bits */
	    break;

	  case 1:
	    /* byte 1: low 2 bits */
	    (*f->next->f)(f->next, ((f->t)|(c >> 4)), GF_DATA);
	    /* byte 2: high 4 bits */
	    f->t = c << 4;
	    break;

	  case 2:
	    /* byte 2: low 4 bits */
	    (*f->next->f)(f->next, ((f->t)|(c >> 2)), GF_DATA);
	    /* byte 3: high 2 bits */
	    f->t = c << 6;
	    break;

	  case 3:
	    (*f->next->f)(f->next, (f->t) | c, GF_DATA);
	    f->n = 0L;			/* reinitialize mechanism */
	    break;
	}
    }
    else if(flg == GF_EOD){
	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->n  = 0L;			/* quantum position */
	f->f1 = 0;			/* state holder: equal seen? */
    }

}




/*
 * QUOTED-PRINTABLE ENCODING AND DECODING filters below.
 * encoding described in rfc1341
 */

#define	GF_MAXLINE	80		/* good buffer size */


/*
 * QUOTED-PRINTABLE to 8BIT filter
 */
void
gf_qp_8bit(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){

	switch(f->f1){
	  case CCR    :			/* non-significant space */
	    f->f1 = DFL;
	    if(c == '\012')
	      return;

	    gf_qp_default(f, c);
	    break;

	  case EQUAL  :
	    if(c == '\015'){		/* "=\015" is a soft EOL */
		f->f1 = CCR;
		break;
	    }

	    if(c == '='){		/* compatibility clause for old guys */
		(*f->next->f)(f->next, '=', GF_DATA);
		f->f1 = DFL;
		break;
	    }

	    if(!isxdigit(c)){		/* must be hex! */
		fs_give((void **)&(f->line));
		gf_error(bad_qp_hex);
		return;
	    }

	    if (isdigit (c)) 
	      f->t = c - '0';
	    else 
	      f->t = c - (isupper (c) ? 'A' - 10 : 'a' - 10);

	    f->f1 = HEX;
	    break;

	  case HEX :
	    f->f1 = DFL;
	    if(!isxdigit(c)){		/* must be hex! */
		fs_give((void **)&(f->line));
		gf_error(bad_qp_hex);
		return;
	    }

	    if (isdigit(c)) 
	      c -= '0';
	    else 
	      c -= (isupper (c) ? 'A' - 10 : 'a' - 10);

	    (*f->next->f)(f->next, c + (f->t << 4), GF_DATA);
	    break;

	  case WSPACE :
	    if(c == ' '){		/* toss it in with other spaces */
		if(f->linep - f->line < GF_MAXLINE)
		  *(f->linep)++ = ' ';
		break;
	    }

	    f->f1 = DFL;
	    if(c == '\015'){		/* not our white space! */
		f->linep = f->line;	/* reset buffer */
		(*f->next->f)(f->next, '\015', GF_DATA);
		break;
	    }

	    /* the spaces are ours, write 'em */
	    f->n = f->linep - f->line;
	    while((f->n)--)
	      (*f->next->f)(f->next, ' ', GF_DATA);

	    /* fall thru and take care of c */
	  case DFL :
	  default:
	    gf_qp_default(f, c);
	    break;
	}
    }
    else if(flg == GF_EOD){
	fs_give((void **)&(f->line));
	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1 = DFL;
	f->linep = f->line = (char *)fs_get(GF_MAXLINE * sizeof(char));
    }

}


/*
 * default action for QUOTED-PRINTABLE to 8BIT decoder
 */
void
gf_qp_default(f, c)
    FILTER_S *f;
    int       c;
{
    if(c == ' '){
	f->f1 = WSPACE;
	f->linep = f->line;	/* reset white space! */
	*(f->linep)++ = ' ';
    }
    else if(c == '='){
	f->f1 = EQUAL;
    }
    else
      (*f->next->f)(f->next, c, GF_DATA);
}



/*
 * 8BIT to QUOTED-PRINTABLE filter
 */
void
gf_8bit_qp(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
#define	QP_MAXL	75			/* 76th place only for continuation */

    if(flg == GF_DATA){

	Find_Froms(f->t, f->f2, c);	/* keep track of "^JFrom " */

	switch(f->f1){
	  case CCR :			/* true line break? */
	    f->f1 = DFL;
	    if(c == '\012'){
		(*f->next->f)(f->next, '\015', GF_DATA);
		(*f->next->f)(f->next, '\012', GF_DATA);
		f->n = 0L;
	    }
	    else{
		gf_8bit_put_quote(f, '\015');	/* nope, quote the CR */
		gf_8bit_default(f, c);	/* don't forget about c! */
	    }
	    break;

	  case WSPACE:
	    f->f1 = DFL;
	    if(c == '\015' || f->t){	/* handle the space */
		gf_8bit_put_quote(f, ' ');
		f->t = 0;		/* reset From flag */
	    }
	    else
	      gf_8bit_put(f, ' ');

	    /* fall thru to take care of c! */

	  case DFL :
	    gf_8bit_default(f, c);	/* handle ordinary case */
	    break;
	}
    }
    else if(flg == GF_EOD){
	switch(f->f1){
	  case CCR :
	    gf_8bit_put_quote(f, '\015'); /* write the last cr */
	    break;
	  case WSPACE :
	    gf_8bit_put_quote(f, ' ');	/* write the last space */
	    break;
	}

	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1 = DFL;			/* state from last character        */
	f->f2 = 1;			/* state of "^NFrom " bitmap        */
	f->t  = 0;
	f->n  = 0L;			/* number of chars in current line  */
    }
}


/*
 * default action for 8bit to quoted printable encoder
 */
void
gf_8bit_default(f, c)
    FILTER_S *f;
    int       c;
{
    if(c == ' '){
	f->f1 = WSPACE;
    }
    else if(c == '\015'){
	f->f1 = CCR;
    }
    else if(iscntrl(c) || (c == 0x7f) || (c & 0x80) || (c == '=')){
	gf_8bit_put_quote(f, c);
    }
    else{
	gf_8bit_put(f, c);
    }
}



/*
 * write a quoted octet in QUOTED-PRINTABLE encoding, adding soft
 * line break if needed.
 */
void
gf_8bit_put_quote(f, c)
   FILTER_S *f;
   int       c;
{
    char *hex = "0123456789ABCDEF";

    if ((f->n += 3) > QP_MAXL) {	/* yes, would line overflow? */
	(*f->next->f)(f->next, '=', GF_DATA);
	(*f->next->f)(f->next, '\015', GF_DATA);
	(*f->next->f)(f->next, '\012', GF_DATA);
	f->n = 3;		/* set line count */
    }

    (*f->next->f)(f->next, '=', GF_DATA);
    (*f->next->f)(f->next, hex[c >> 4], GF_DATA); /* high order 4 bits */
    (*f->next->f)(f->next, hex[c & 0xf], GF_DATA); /* low order 4 bits */
}



/*
 * just write an ordinary octet in QUOTED-PRINTABLE, wrapping line
 * if needed.
 */
void
gf_8bit_put(f, c)
   FILTER_S *f;
   int       c;
{
    if((++(f->n)) > QP_MAXL){
	(*f->next->f)(f->next, '=', GF_DATA);
	(*f->next->f)(f->next, '\015', GF_DATA);
	(*f->next->f)(f->next, '\012', GF_DATA);
	f->n = 1;
    }

    (*f->next->f)(f->next, c, GF_DATA);
}


/*
 * RICHTEXT-TO-PLAINTEXT filter
 */

/*
 * option to be used by rich2plain (NOTE: if this filter is ever 
 * used more than once in a pipe, all instances will have the same
 * option value)
 */
static int gf_rich_plain = 0;


/*----------------------------------------------------------------------
      richtext to plaintext filter
    
 Args: gc, pc -- functions to read and write chars with
       width  --
       plain  -- 

  This basically removes all richtext formatting. A cute hack is used 
  to get bold and underlining to work.
  Further work could be done to handle things like centering and right
  and left flush, but then it could no longer be done in place. This
  operates on text *with* CRLF's.

  WARNING: does not wrap lines!
 ----*/
void
gf_rich2plain(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{

    if(flg == GF_DATA){
	switch(f->f1){
	  case TOKEN :			/* collect a richtext token */
	    if(c == '>'){		/* what should we do with it? */
		f->f1       = DFL;	/* return to default next time */
		*(f->linep) = '\0';	/* cap off token */
		if(f->line[0] == 'l' && f->line[1] == 't'){
		    (*f->next->f)(f->next, '<', GF_DATA); /* literal '<' */
		}
		else if(f->line[0] == 'n' && f->line[1] == 'l'){
		    (*f->next->f)(f->next, '\015', GF_DATA);/* newline! */
		    (*f->next->f)(f->next, '\012', GF_DATA);
		}
		else if(!strcmp("comment", f->line)){
		    (f->f2)++;
		}
		else if(!strcmp("/comment", f->line)){
		    f->f2 = 0;
		}
                else if(!strcmp("/paragraph", f->line)) {
		    (*f->next->f)(f->next, '\r', GF_DATA);
                    (*f->next->f)(f->next, '\n', GF_DATA);
                    (*f->next->f)(f->next, '\r', GF_DATA);
                    (*f->next->f)(f->next, '\n', GF_DATA);
                }
		else if(!gf_rich_plain){
		    /* Following are a cute hacks to get 
		       bold and underline on the screen. 
		       See Putline0n() where these codes are
		       interpreted */
		    if(!strcmp(f->line, "bold")) {
			(*f->next->f)(f->next, '\01', GF_DATA);
		    } else if(!strcmp(f->line, "/bold")) {
			(*f->next->f)(f->next, '\02', GF_DATA);
		    } else if(!strcmp(f->line, "italic")) {
			(*f->next->f)(f->next, '\03', GF_DATA);
		    } else if(!strcmp(f->line, "/italic")) {
			(*f->next->f)(f->next, '\04', GF_DATA);
		    } else if(!strcmp(f->line, "underline")) {
			(*f->next->f)(f->next,'\03', GF_DATA);
		    } else if(!strcmp(f->line, "/underline")) {
			(*f->next->f)(f->next, '\04', GF_DATA);
		    } 
		}
		/* else we just ignore the token! */

		f->linep = f->line;	/* reset token buffer */
	    }
	    else{			/* add char to token */
		if(f->linep - f->line > 40){
		    /* What? rfc1341 says 40 char tokens MAX! */
		    fs_give((void **)&(f->line));
		    gf_error("Richtext token too long");
		    return;
		}
		
		*(f->linep)++ = isupper(c) ? c - 'A' + 'a' : c;
	    }
	    break;

	  case CCR   :
	    f->f1 = DFL;		/* back to default next time */
	    if(c == '\012'){		/* treat as single space?    */
		(*f->next->f)(f->next, ' ', GF_DATA);
		break;
	    }
	    /* fall thru to process c */

	  case DFL   :
	  default:
	    if(c == '<')
	      f->f1 = TOKEN;
	    else if(c == '\015')
	      f->f1 = CCR;
	    else if(!f->f2)		/* not in comment! */
	      (*f->next->f)(f->next, c, GF_DATA);
	    
	    break;
	}
    }
    else if(flg == GF_EOD){
	if(f->f1 = (f->linep != f->line)){
	    /* incomplete token!! */
	    gf_error("Incomplete richtext token");
	}

	fs_give((void **)&(f->line));

	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1 = DFL;			/* state */
	f->f2 = 0;			/* set means we're in a comment */
	f->linep = f->line = (char *)fs_get(45 * sizeof(char));
    }

}


/*
 * function called from the outside to set
 * richtext filter's options
 */
void
gf_rich2plain_opt(plain)
    int plain;
{
    gf_rich_plain = plain;
}



/*
 * LINEWRAP FILTER - insert CRLF's at end of nearest whitespace before
 * specified line width
 */

/*
 * option to be used by gf_wrap_filter (NOTE: if this filter is ever 
 * used more than once in a pipe, all instances will have the same
 * option value)
 */
static int gf_wrap_width = 75;


/*
 * the simple filter, breaks lines at end of white space nearest
 * to global "gf_wrap_width" in length
 */
void
gf_wrap(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    register long i;
    register char *breakp, *tp;

    if(flg == GF_DATA){
	switch(f->f1){
	  case CCR:
	    f->f1 = DFL;
	    if(c == '\012'){
		for(tp = f->line; tp < f->linep; tp++)
		  (*f->next->f)(f->next, *tp, GF_DATA);

		(*f->next->f)(f->next, '\015', GF_DATA);
		(*f->next->f)(f->next, '\012', GF_DATA);
		f->n = 0L;
		f->linep = f->line;
		break;
	    }
	    else{
		*(f->linep)++ = '\015';	/* shouldn't happen often! */
		(f->n)++;
	    }
	    /* else fall thru to take care of c */

	  case DFL:
	    if(c == '\015'){		/* already has newline? */
		f->f1 = CCR;
		break;
	    }
	    else if(c == '\011'){	/* account for tabs too! */
		i = f->n;
		while((++i)&0x07)
		  ;

		i -= f->n;
	    }
	    else
	      i = 1;

	    if(f->n + i > (long)gf_wrap_width){ /* wrap? */
		for(breakp = &f->linep[-1]; breakp >= f->line; breakp--)
		  if(isspace(*breakp))
		    break;

		f->n = i = 0;
		for(tp = f->line; tp < f->linep; tp++){
		    if(breakp < f->line || tp <= breakp) /* write the line */
		      (*f->next->f)(f->next, *tp, GF_DATA);
		    else{			/* shift it back */
			i = tp - breakp - 1;
			if((f->line[i++] = *tp) == '\011')
			  while((++(f->n))&0x07);
			else
			  (f->n)++;
		    }
		}

		(*f->next->f)(f->next, '\015', GF_DATA);
		(*f->next->f)(f->next, '\012', GF_DATA);
		f->linep = &f->line[i];	/* reset f->linep */
	    }

	    if((*(f->linep)++ = c) == '\011')
	      while((++(f->n))&0x07);
	    else
	      (f->n)++;

	    break;
	}
    }
    else if(flg == GF_EOD){
	for(i = 0; i < f->n; i++)	/* flush the remaining line */
	  (*f->next->f)(f->next, f->line[i], GF_DATA);

	fs_give((void **)&(f->line));	/* free temp line buffer */
	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1    = DFL;
	f->n     = 0L;
	f->linep = f->line = (char *)fs_get((gf_wrap_width+10)*sizeof(char));
    }

}


/*
 * function called from the outside to set
 * wrap filter's width option
 */
void
gf_wrap_filter_opt(width)
    int width;
{
    gf_wrap_width = width;
}

/*
 * LINE PREFIX FILTER - insert given text at beginning of each
 * line
 */

/*
 * option to be used by gf_prefix (NOTE: if this filter is ever 
 * used more than once in a pipe, all instances will have the same
 * option value)
 */
static char *gf_prefix_prefix = NULL;


/*
 * the simple filter, prepends each line with the requested prefix.
 * if prefix is null, does nothing, and as with all filters, assumes
 * NVT end of lines.
 */
void
gf_prefix(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    register char *p;

    if(flg == GF_DATA){
	switch(f->f1){
	  case CCR:
	    f->f1 = DFL;
	    (*f->next->f)(f->next, '\015', GF_DATA);
	    if(c == '\012'){
		(*f->next->f)(f->next, '\012', GF_DATA);
		if((p = gf_prefix_prefix) != NULL){
		    while(*p != '\0')
		      (*f->next->f)(f->next, (int)*p++, GF_DATA);
		}

		break;
	    }
	    /* and fall thru to take care of c */
	  case DFL:
	    if(!(f->n)++){		/* write first prefix!! */
		if((p = gf_prefix_prefix) != NULL){
		    while(*p != '\0')
		      (*f->next->f)(f->next, (int)*p++, GF_DATA);
		}
	    }

	    if(c == '\015'){		/* already has newline? */
		f->f1 = CCR;
		break;
	    }
	    else
	      (*f->next->f)(f->next, c, GF_DATA);

	    break;
	}
    }
    else if(flg == GF_EOD){
	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1 = DFL;
	f->n  = 0L;
    }

}


/*
 * function called from the outside to set
 * prefix filter's prefix string
 */
void
gf_prefix_opt(prefix)
    char *prefix;
{
    gf_prefix_prefix = prefix;
}


/*
 * Network virtual terminal to local newline convention filter
 */
void
gf_nvtnl_local(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){
	switch(f->f1){
	  case CCR:
	    f->f1 = DFL;
	    if(c == '\012'){
		(*f->next->f)(f->next, '\012', GF_DATA);
		break;
	    }
	    else
	      (*f->next->f)(f->next, '\015', GF_DATA);
	    /* else fall thru to take care of c */

	  case DFL:
	    if(c == '\015')
	      f->f1 = CCR;
	    else
	      (*f->next->f)(f->next, c, GF_DATA);

	    break;
	}
    }
    else if(flg == GF_EOD){
	(*f->next->f)(f->next, 0, GF_EOD);
    }
    else if(flg == GF_RESET){
	f->f1 = DFL;
    }
}


/*
 * local to network newline convention filter
 */
void
gf_local_nvtnl(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
    if(flg == GF_DATA){
	if(c == '\012'){
	  (*f->next->f)(f->next, '\015', GF_DATA);
	  (*f->next->f)(f->next, '\012', GF_DATA);
	}
	else
	  (*f->next->f)(f->next, c, GF_DATA);
    }
    else if(flg == GF_EOD){
	(*f->next->f)(f->next, 0, GF_EOD);
    }
}

