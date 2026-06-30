#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: helpindx.c,v 4.5 1993/07/12 05:47:30 mikes Exp $";
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

/*
 * very short, very specialized
 *
 *
 *
 */
#include <stdio.h>
#include <ctype.h>

struct hindx {
    char  key[32];			/* name of help section */
    long  offset;			/* where help text starts */
    short lines;			/* how many lines there are */
};

main(argc, argv)
int  argc;
char **argv;
{
    char *p, s[256], key[32];
    long index;
    int  section, 
	 len, 
	 line,
	 i; 
    FILE *hp,
	 *hip,					/* help index ptr */
    	 *hhp;					/* help header ptr */
    struct hindx irec;

    if(argc < 4){
	fprintf(stderr,"usage: helpindx <help_file> <index_file> <header_file>\n");
	exit(-1);
    }

    if((hp = fopen(argv[1], "rb")) == NULL){	/* problems */
	perror(argv[1]);
        exit(-1);
    }

    if((hip = fopen(argv[2], "wb")) == NULL){	/* problems */
	perror(argv[2]);
        exit(-1);
    }

    if((hhp = fopen(argv[3], "w")) == NULL){	/* problems */
	perror(argv[3]);
        exit(-1);
    }

    fprintf(hhp,"/*\n * Pine Help text header file\n */\n");
    fprintf(hhp,"struct hindx {\n");
    fprintf(hhp,"    char  key[32];\t\t\t/* name of help section */\n");
    fprintf(hhp,"    long  offset;\t\t\t/* where help text starts */\n");
    fprintf(hhp,"    short lines;\t\t\t/* how many lines there are */\n");
    fprintf(hhp,"};\n\n\n/*\n * defs for help section titles\n */\n");

    index = 0L;
    line  = section = 0;

    while(fgets(s, 255, hp) != NULL){
	line++;
	len = strlen(s);
	if(s[0] == '='){			/* new section? */
	    i = 0;
	    while((s[i] == '=' || isspace(s[i])) && i < len)
		i++;

	    if(section)
	        fwrite(&irec, sizeof(irec), 1, hip);

	    irec.offset = index + (long)i;	/* save where name starts */
	    irec.lines = 0;
	    p = &irec.key[0];			/* save name field */
	    while(!isspace(s[i]) && i < len)
		*p++ = s[i++];
	    *p = '\0';
	
	    if(irec.key[0] == '\0'){
		fprintf(stderr,"Invalid help line %d: %s", line, s);
		exit(-1);
	    }
	    else
	      fprintf(hhp, "#define\t%s\t%d\n", irec.key, section++);

	}
	else if(s[0] == '#' && section){
	    fprintf(stderr,"Comments not allowed in help text: line %d", line);
	    exit(-1);
	}
	else{
	    irec.lines++;
	}
	index += len;
    }

    if(section)				/* write last entry */
      fwrite(&irec, sizeof(irec), 1, hip);

    fprintf(hhp, "#define\tLASTHELP\t%d\n", section);

    fclose(hp);
    fclose(hip);
    fclose(hhp);
}
