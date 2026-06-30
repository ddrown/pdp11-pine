#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: abtest.c,v 4.3 1993/04/14 21:44:13 hubert Exp $";
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
#include "adrbklib.h"
#include <errno.h>

main()
{
    AdrBk *ab;
    AdrBk_Entry *ae;
    char cmd[100], nick[100],full[100],addr[100];
    
    
    ab = adrbk_open("addressbook", NULL);

    if(ab == NULL) {
        perror("addressbook");
        exit(-1);
    }

    printf("%d entries read\n", adrbk_count(ab));

    ae = NULL;

    printf("\nCmd (A)dd, (D)elete, (L)ookup, li(S)t: ");
    fflush(stdout);
    while(fgets(cmd, sizeof(cmd),stdin) != NULL) {
        switch(cmd[0]) {
          case 'a':
            printf("Nickname: "); fflush(stdout);
            gets(nick);
            printf("Fullname: "); fflush(stdout);
            gets(full);
            printf("Address:  "); fflush(stdout);
            gets(addr);
            printf("adrbk_add() returns %d\n",
              adrbk_add(ab, NULL, nick, full, addr, Atom, NULL));
            break;

          case 'd':
            if(ae == NULL) {
                printf("Use (L)ookup command to select item for deletion\n");
            } else {
                printf("Delete %s? (y/n)", ae->nickname); fflush(stdout);
                fgets(cmd, sizeof(cmd), stdin);
                if(cmd[0] == 'y') {
                    printf("delete returned: %d  %d\n",
                       adrbk_delete(ab, ae),  errno);
                    printf("%d entries in adrbk\n", adrbk_count(ab));
                } else
                    printf("Not deleted\n");
            }
            break;

          case 'l':
            printf("Lookup: "); fflush(stdout);
            fgets(cmd, sizeof(cmd), stdin);
            cmd[strlen(cmd) - 1] = '\0';
            ae = adrbk_lookup(ab, cmd);
            if(ae == NULL) {
                printf("Not found\n");
            } else {
                print_ae(ae);
            }
            break;

          case 's':
            adrbk_splat(ab);
            break;

          case 'e':
            if(ae == NULL) {
                printf("Use l to look what to edit first\n");
                break;
            }
            printf("Editing fullname \"%s\"\n", ae->fullname);
            printf("Fullname: "); fflush(stdout);
            gets(full);
            printf("adrbk_add() returns %d\n",
              adrbk_add(ab, NULL, ae->nickname, full, ae->address, Atom, NULL));
            break;
            
        }
        printf("\nCmd (A)dd, (D)elete, (L)ookup, li(S)t (E)dit: ");
        fflush(stdout);
    }

    printf("\n");

    adrbk_close(ab);
        
        
    
}


    
print_ae(ae)
     AdrBk_Entry *ae;
{
    printf("[%s]\n[%s]\n[%s]\n[%s]\n\n", ae->nickname,
           ae->fullname == NULL ? "NULL" : ae->fullname,
           ae->address  == NULL ? "NULL" : ae->address,
           ae->extra    == NULL ? "NULL" : ae->extra);
}

    
