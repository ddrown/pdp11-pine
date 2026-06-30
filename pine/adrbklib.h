/*----------------------------------------------------------------------
  $Id: adrbklib.h,v 4.3 1993/07/29 23:19:12 hubert Exp $

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

#ifndef _ADRBKLIB_INCLUDED
#define _ADRBKLIB_INCLUDED

typedef enum {Atom, List} Tag;

typedef struct adrbk_entry {
    char *nickname;
    char *fullname;
    union addr {
        char *addr;
        char **list;
    } addr;
    char *extra;
    char  referenced;    /* Tag for detecting loops during lookup */
    Tag   tag;
} AdrBk_Entry;


typedef struct adrbk {
    char         *filename;
    char         *temp_filename;    
    char         *storage;
    char         *storage_end;
    AdrBk_Entry **book;
    int           book_used;
    int           book_allocated;
} AdrBk;


/* There are no restrictions on the length on any of the fields. This current
   code restricts the number of addresses in a list to 1000.
 */


#ifdef ANSI
AdrBk          *adrbk_open(char *, char *);
unsigned int    adrbk_count(AdrBk *);
AdrBk_Entry    *adrbk_get(AdrBk *, unsigned int);
AdrBk_Entry    *adrbk_lookup(AdrBk *, char *);
char           *adrbk_formatname(char *);
void            adrbk_clearrefs(AdrBk *);
AdrBk_Entry    *adrbk_newentry();
int             adrbk_add(AdrBk *, AdrBk_Entry **, char *, char *,
                             char *,Tag , int((*)(AdrBk_Entry *)));
int             adrbk_delete(AdrBk *, AdrBk_Entry *);
int             adrbk_listdel(AdrBk *, AdrBk_Entry *, char *);
int             adrbk_listadd(AdrBk *, AdrBk_Entry *, char *,char **);
void            adrbk_close(AdrBk *);
#else

AdrBk          *adrbk_open();
unsigned int    adrbk_count();
AdrBk_Entry    *adrbk_get();
AdrBk_Entry    *adrbk_lookup();
char           *adrbk_formatname();
void            adrbk_clearrefs();
AdrBk_Entry    *adrbk_newentry();
int             adrbk_add();
int             adrbk_delete();
int             adrbk_listdel();
int             adrbk_listadd();
void            adrbk_close();
#endif

/* Address book numbers used by adrbk_get start at 0 */

#endif /* _ADRBKLIB_INCLUDED */
