/*----------------------------------------------------------------------
  $Id: headers.h,v 4.9 1993/10/06 01:36:39 mikes Exp $

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
       headers.h

   The include file to always include that includes a few other things
     -  includes the most general system files and other pine include files
     -  declares the global variables
       
 ====*/
         

#ifndef _HEADERS_INCLUDED
#define _HEADERS_INCLUDED

/*----------------------------------------------------------------------
           Include files
 
 System specific includes and defines are in os.h, the source for which
is os-xxx.h. (Don't edit osdep.h; edit os-xxx.h instead.)
 ----*/
#include <stdio.h>

#include "os.h"

#include "../c-client/mail.h"
#include "../c-client/osdep.h"
#include "../c-client/rfc822.h"

/* These includes are all ANSI, and OK with all other compilers (so far) */
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>

#include "pine.h"

#include "context.h"

#include "helptext.h"




/*----------------------------------------------------------------------
    The few global variables we use in Pine
  ----*/

extern struct pine *ps_global;

extern int          timeout;  /* referenced by weemacs. Set in pine.c. */

extern char        *tmp_20k_buf;
extern void         grow_tmp_20k_buf(size_t);

#ifdef DEBUG
extern FILE        *debugfile;	     /* file for debut output    */
extern int          debug;	     /* flag: debugging mode on? */
#endif

#endif /* _HEADERS_INCLUDEDS */
