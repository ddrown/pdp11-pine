#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: aetest.c,v 4.3 1993/04/14 21:44:13 hubert Exp $";
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

#include <stdio.h>
#include "../c-client/mail.h"
#include "adrbklib.h"

#define SLEN 512
#define BERKNAMES

ADDRESS *mtp_parse_address(), *expand_address();
char *local_name_lookup(), *cpystr(), *gcos_name(), *fs_get();

main(argc, argv)
     char **argv;
{
    char ibuf[200], obuf[3000];
    char *domain = "cac.washington.edu";
    AdrBk *addressbook;
    ADDRESS *a1;

    addressbook = adrbk_open(argv[1] == NULL ? "addressbook": argv[1],
                                                                       NULL);
    if(addressbook == NULL) {
        fprintf(stderr, "can't open address book %s\n",
                argv[1] == NULL ? "addressbook": argv[1]);
        exit(-1);
    }

    printf("Addr: "); fflush(stdout);
    while(gets(ibuf) != NULL) {
        a1 = expand_address(ibuf, domain, addressbook);
        obuf[0] = '\0';
         rfc822_write_address(obuf,a1); 
        printf("%s\n", obuf); 
        printf("Addr: "); fflush(stdout);
    }
    printf("\n");
}

    

#include <pwd.h>    
    
/*----------------------------------------------------------------------
      Look up a userid on the local system and return rfc822 addres

 Input: possible local user login

 Result: returns NULL or pointer to static string rfc822 address.
  ----------------------------------------------------------------------*/
char *
local_name_lookup(name)
     char *name;
{
/*   return("Foooo"); */

    struct passwd *pw;

    pw = getpwnam(name);
    if(pw == NULL)
      return(NULL);

    return(gcos_name(pw->pw_gecos, pw->pw_name));
}



void mm_log(string)
     char *string;
{
    printf("%s\n", string);
}

void mm_dlog(string)
     char *string;
{
    printf("%s\n", string);
}

void mm_searched () {}
void mm_exists() {}
void mm_expunged() {}




/*----------------------------------------------------------------------
      Pull the name out of the gcos field if we have that sort of /etc/passwd

 Input:  field from passwd
         login name

 Result: returns pointer to buffer with name
  ----------------------------------------------------------------------*/
char *
gcos_name(gcos_field, logname)
char *logname, *gcos_field;
{
    /** Return the full name found in a passwd file gcos field **/

#ifdef BERKNAMES

    static char fullname[SLEN];
    register char *fncp, *gcoscp, *lncp, *end;


    /* full name is all chars up to first ',' (or whole gcos, if no ',') */
    /* replace any & with logname in upper case */

    for(fncp = fullname, gcoscp= gcos_field, end = fullname + SLEN - 1;
        (*gcoscp != ',' && *gcoscp != '\0' && fncp != end);
	gcoscp++) {

	if(*gcoscp == '&') {
	    for(lncp = logname; *lncp; fncp++, lncp++)
		*fncp = toupper(*lncp);
	} else {
	    *fncp++ = *gcoscp;
	}
    }
    
    *fncp = '\0';
    return(fullname);
#else
#ifdef USGNAMES

    char *firstcp, *lastcp;

    /* The last character of the full name is the one preceding the first
     * '('. If there is no '(', then the full name ends at the end of the
     * gcos field.
     */
    if(lastcp = strchr(gcos_field, '('))
	*lastcp = '\0';

    /* The first character of the full name is the one following the 
     * last '-' before that ending character. NOTE: that's why we
     * establish the ending character first!
     * If there is no '-' before the ending character, then the fullname
     * begins at the beginning of the gcos field.
     */
    if(firstcp = strrchr(gcos_field, '-'))
	firstcp++;
    else
	firstcp = gcos_field;

    return(firstcp);

#else
    /* use full gcos field */
    return(gcos_field);
#endif
#endif
}



p_list(a)
     ADDRESS *a;
{
    while(a != NULL) {
        printf("\"%s\" ", a->mailbox);
        a = a->next;
    }
}
