#!/bin/sh
#
# $Id: cmplhelp.sh,v 4.1 1993/04/09 22:11:38 mikes Exp $
#
#            T H E    P I N E    M A I L   S Y S T E M
#
#   Laurence Lundblade and Mike Seibel
#   Networks and Distributed Computing
#   Computing and Communications
#   University of Washington
#   Administration Building, AG-44
#   Seattle, Washington, 98195, USA
#   Internet: lgl@CAC.Washington.EDU
#             mikes@CAC.Washington.EDU
#
#   Please address all bugs and comments to "pine-bugs@cac.washington.edu"
#
#   Copyright 1989-1993  University of Washington
#
#    Permission to use, copy, modify, and distribute this software and its
#   documentation for any purpose and without fee to the University of
#   Washington is hereby granted, provided that the above copyright notice
#   appears in all copies and that both the above copyright notice and this
#   permission notice appear in supporting documentation, and that the name
#   of the University of Washington not be used in advertising or publicity
#   pertaining to distribution of the software without specific, written
#   prior permission.  This software is made available "as is", and
#   THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
#   WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
#   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
#   NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
#   INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
#   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
#   (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
#   WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#  
#   Pine and Pico are trademarks of the University of Washington.
#   No commercial use of these trademarks may be made without prior
#   written permission of the University of Washington.
#
#   Pine is in part based on The Elm Mail System:
#    ***********************************************************************
#    *  The Elm Mail System  -  Revision: 2.13                             *
#    *                                                                     *
#    * 			Copyright (c) 1986, 1987 Dave Taylor               *
#    * 			Copyright (c) 1988, 1989 USENET Community Trust    *
#    ***********************************************************************
# 
#


# 
# cmplhelp.sh -- This script take the pine.help file and turns it into
# a .c file defining lots of strings
#


awk 'BEGIN         {in_text = 0;
                    count = 0;
                    printf("#include <stdio.h>\n\n\n")
                   }


     /^====/       { if(in_text)
                         printf("NULL\n};\n\n\n");
                     printf ("char *%s[] = {\n ", $2 );
                     texts[count++] = $2;
                     in_text = 1;
                    }
     /^$/           { if(in_text)
                          printf("\" \",\n");
                    }
     /^[^====]/     { if(in_text)
                          printf("\"%s\",\n", $0);
                    }
     END            { if(in_text)
                          printf("NULL\n};\n\n");
                      printf("char **h_texts[] = {\n");
                      for(i = 0; i < count; i++)
                          printf("%s,\n",texts[i]);
                      printf("NULL\n};\n\n");
                    } '
