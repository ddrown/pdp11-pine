/*
 *
 * $Id: context.h,v 4.10 1993/07/29 23:19:12 hubert Exp $
 *
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

#ifndef _CONTEXT_INCLUDED
#define _CONTEXT_INCLUDED

extern char *current_context;
typedef void (*find_return) (char *);
void context_mailbox(char *);
void context_bboard(char *);
void context_fqn_to_an(find_return, char *, char *);

short context_isambig(char *);
char *context_digest(char *, char *, char *, char *);
void context_apply(char *, char *, char *);
void context_find_all(char *, MAILSTREAM *, char *);
long context_create(char *, MAILSTREAM *, char *);
MAILSTREAM *context_open(char *, MAILSTREAM *, char *, long);
long context_rename(char *, MAILSTREAM *, char *, char *);
long context_delete(char *, MAILSTREAM *, char *);
long context_append(char *, MAILSTREAM *, char *, STRING *);

#endif /* _CONTEXT_INCLUDED */
