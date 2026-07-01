#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: help.c,v 4.22 1993/10/05 00:36:26 hubert Exp $";
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

/*======================================================================
     help.c
     Functions to support the pine help screens
 ====*/


#include "headers.h"

#ifdef	DOS
static struct hindx *helpindex = NULL;
static FILE *helpfile = NULL;


/*
 * get_help_text - return the help text associated with index
 *                 in an array of pointers to each line of text.
 */
char **
get_help_text(index, pages)
short index;
int   *pages;
{
    int  i, current_page, len;
    char buf[256], **htext;
    void copy_fix_keys();
    extern long coreleft();
    extern char *read_binary_file();

    if(index < 0 || index >= LASTHELP)
	return(NULL);

    /* make sure index file is available */
    if(!helpindex){
	*buf = '\0';

	if(getenv("PINEHOME")){		/* explicitly telling us? */
	    build_path(buf, getenv("PINEHOME"), HELPINDEX);
	    if(can_access(buf, 4) < 0)
	      *buf = '\0';
	}

	if(*buf == '\0'){		/* no? try same dir as pinerc */
	    int l = last_cmpnt(ps_global->pinerc) - ps_global->pinerc;

	    strncpy(buf, ps_global->pinerc, l);
	    buf[l] = '\0';
	    strcat(buf, HELPINDEX);
	    if(can_access(buf, 4) < 0)
	      *buf = '\0';
	}

	if(*buf == '\0'){		/* else, try same dir as PINE.EXE */
	    int l = last_cmpnt(ps_global->pine_exe) - ps_global->pine_exe;

	    strncpy(buf, ps_global->pine_exe, l);
	    buf[l] = '\0';
	    strcat(buf, HELPINDEX);
	}

	if((helpindex = (struct hindx *)read_binary_file(buf)) == NULL){
            q_status_message1(1,3,5,"No Help!  Index \"%s\" not found.", buf);
	    return(NULL);
	}
    }

    /* make sure help file is open */
    if(!helpfile){
	*buf = '\0';

	if(getenv("PINEHOME")){		/* explicitly telling us? */
	    build_path(buf, getenv("PINEHOME"), HELPFILE);
	    if(can_access(buf, 4) < 0)
	      *buf = '\0';
	}

	if(*buf == '\0'){		/* no? try same dir as pinerc */
	    int l = last_cmpnt(ps_global->pinerc) - ps_global->pinerc;

	    strncpy(buf, ps_global->pinerc, l);
	    buf[l] = '\0';
	    strcat(buf, HELPFILE);

	    if(can_access(buf, 4) < 0)
	      *buf = '\0';
	}

	if(*buf == '\0'){		/* else, try same dir as PINE.EXE */
	    int l = last_cmpnt(ps_global->pine_exe) - ps_global->pine_exe;

	    strncpy(buf, ps_global->pine_exe, l);
	    buf[l] = '\0';
	    strcat(buf, HELPFILE);
	}

	if((helpfile = fopen(buf, "rb")) == NULL){
            q_status_message2(1,3,5,"No Help!  \"%s\" : %s",
			      buf,
                              error_description(errno));
	    return(NULL);
	}
    }

    if(fseek(helpfile, helpindex[index].offset, SEEK_SET) != 0){
/* problem in fseek error */
        q_status_message(1,3,5,"No Help!  Can't locate proper entry in help text file.");
	return(NULL);
    }

    fgets(buf, 255, helpfile);

    if(strncmp(helpindex[index].key, buf, strlen(helpindex[index].key))){
/* key isn't right, reindex help file error */
        q_status_message(1,3,5,"No Help!  Corrupt entry in help text file.");
	return(NULL);
    }

    if(coreleft() < (long)(80 * helpindex[index].lines)){
        q_status_message(1,3,5,"No Help!  Not enough memory to display help.");
	return(NULL);
    }

    htext = (char **)fs_get(sizeof(char *) * (helpindex[index].lines + 1));
    current_page = 0;
    for(i=0; i < helpindex[index].lines; i++){
	if(fgets(buf, 255, helpfile) == NULL){
	    htext[i] = NULL;
	    free_help_text(htext);
            q_status_message(1,3,5,"No Help!  Entry not in help text.");
	    return(NULL);
	}

	len = strlen(buf);
	if(len >= 2){				/* blast CRLF */
	    buf[len - 1] = '\0';
	    buf[len - 2] = '\0';
	}

	if(pages && srchstr(buf, "___----") != NULL)
	  pages[current_page++] = i;

	htext[i] = (char *)fs_get(len + 1);
	copy_fix_keys(htext[i], buf);
    }

    htext[i] = NULL;
    if(pages != NULL)
      pages[current_page] = -1;

    close_help_reference();
    return(htext);
}

/*
 * free_help_text - free the strings and array of pointers pointed to 
 *                  by text
 */
free_help_text(text)
char **text;
{
    int i = 0;

    while(text[i] != NULL)
      fs_give((void **)&text[i++]);

    fs_give((void **)&text);
}


/*
 * close_help_reference - free resources associated with help text on disk
 */
close_help_reference()
{
    if(helpindex)
      fs_give((void **)&helpindex);

    if(helpfile){
	fclose(helpfile);
	helpfile = NULL;
    }
}
#endif	/* DOS */


/*----------------------------------------------------------------------
     Make a copy of the help text for display, doing various substutions

  Args: a -- output string
        b -- input string

 Result: string copied into buffer, and any expressions of the form
 {xxx:yyy} are replaced by either xxx or yyy depending on whether
 we are using function keys or not.  _'s are removed except for the
 special case ^_, which is left as is.
  ----*/

void 
copy_fix_keys(a, b)
  register char *a, *b;
{
#ifdef	DOS
    register char *s = a;
#endif
    while(*b) {
	while(*b && *b != '{')
          if(*b == '^' && *(b+1) == '_') {
              *a++ = *b++;
              *a++ = *b++;
	  } else if(*b != '_') {
              *a++ = *b++;
	  } else {
              b++;
          }
	     
	if(*b == '\0') {
	    break;
	}

	if(F_ON(F_USE_FK,ps_global)) {
	    if(*b)
	      b++;
	    while(*b && *b != ':')
	      *a++ = *b++;
	    while(*b && *b != '}')
	      b++;
	    if(*b)
	      b++;
	} else {
	    while(*b && *b != ':')
	      b++;
	    if(*b)
	      b++;
	    while(*b && *b != '}')
	      *a++ = *b++;
	    if(*b)
	      b++;
	}
    }
    *a = '\0';
#ifdef	DOS
    b = s;			/* eliminate back-slashes from "a" */
    while(*s){
	if(*s == '\\')
	  s++;

        *b++ = *s++;
    }

    *b = '\0';			/* tie off back-slashed-string */
#endif
}



/*----------------------------------------------------------------------
     Get the help text in the proper format and call scroller

    Args: text   -- The help text to display (from pine.help --> helptext.c)
          title  -- The title of the help text 
  
  Result: format text and call scroller

  The pages array contains the line number of the start of the pages in
the text. Page 1 is in the 0th element of the array.
The list is ended with a page line number of -1. Line number 0 is also
the first line in the text.
  -----*/
void
#ifdef	DOS

void
helper(short index, char *title, int from_composer)
{
    char **text;
    int  pages[100];

    if((text = get_help_text(index, pages)) == NULL){
	return;
    }

    ClearScreen();
    scrolltool((void *)text, title, pages,
               from_composer ? ComposerHelpText : HelpText,
	       CharStarStar, (struct attachment *)NULL);
    ClearScreen();

    free_help_text(text);
#else

void
helper(char *text[], char *title, int from_composer)
{
    register char **t, **t2;
    char          **new_text;
    int             pages[100], current_page, in_include;
    char          **new_text_nr; /* non-register version of new_text */
    char            *line, line_buf[256];
    FILE           *file;
    int		    alloced_lines = 0;
    int		    need_more_lines = 0;


    dprint(1, (debugfile, "\n\n    ---- HELPER ----\n"));
    /*----------------------------------------------------------------------
            First copy the help text and do substitutions
      ----------------------------------------------------------------------*/

    for(t = text ; *t != NULL; t++);
    alloced_lines = t - text;
    new_text = (char **)fs_get((alloced_lines + 1) * sizeof(char *));

    current_page = 0;
    in_include   = 0;
    file         = NULL;
    for(t = text, t2 = new_text; *t != NULL;) {
        int inc_line_cnt;
		/* -2 is LINES_ABOVE and -3 is LINES_BELOW from scrolltool */
        int lines_to_use = ps_global->ttyo->screen_rows - 2 - 3;
        if(in_include) {
            /*--- Inside an ___include ... ___end_include section ---*/
            if(file != NULL) {

                /*--- Read next line out of include file ---*/

		/* Add a page break every so often.  This is stupid in that
		 * it adds a page break even if there are no more lines.
		 */
                if(inc_line_cnt++ % lines_to_use == 0) {
		    /* this is a big long string, don't delete part of it */
                    line = "                                                                         ___----";
                }else {
                    line = fgets(line_buf, sizeof(line_buf), file);
		}
                if(line == NULL) {
                    fclose(file);
                    in_include = 0;
                    continue;
                }
                if(line[strlen(line)-1] == '\n')
                    line[strlen(line)-1] = '\0';
            } else {
                /*--- File wasn't there, copy the default text ---*/
                if(struncmp(*t, "___end_include", 14) == 0){
                    in_include = 0;
                    t++;
                    continue;
                } else {
                    line = *t;
                }
            }
        } else {
            if(struncmp(*t, "___include", 10) == 0) {
                /*-- Found start of an ___include ... ___end_include block --*/
                char *p, *q, **pp;
		int cnt;
                in_include = 1;
                inc_line_cnt = (current_page > 0)
		                  ? t2 - new_text - pages[current_page-1]
				  : t2 - new_text;
                for(p = (*t)+10; *p && isspace(*p); p++);
                strcpy(line_buf, p);
                for(q = line_buf; *q && !isspace(*q); q++);
                *q = '\0';
                dprint(9, (debugfile, "About to open \"%s\"\n", line_buf));
                file = fopen(line_buf, "r");
                if(file != NULL) {
		  /*
		   * Calculate the size of the included file,
		   * might have to resize new_text.
		   */
		   for(cnt=0; fgets(line_buf, sizeof(line_buf), file) != NULL;
								   cnt++);
                   /* add lines so we can insert page breaks if necessary */
                   cnt = cnt + (inc_line_cnt-1+cnt)/(lines_to_use - 1);
		   /* rewind */
		   (void)fseek(file, 0L, 0);
		    /*
		     * Skip t forward to the end_include so that we know
		     * how many lines we're skipping.
		     */
		   pp = t;
                   while(*t && struncmp(*t, "___end_include", 14) != 0)
                     t++;
		   /* we need cnt more lines but freed up t-pp+1 lines */
		   need_more_lines += cnt - (t - pp + 1);
		   if(need_more_lines > 0) {
		     int offset = t2 - new_text;
		     alloced_lines += need_more_lines;
		     need_more_lines = 0;
		     fs_resize((void **)&new_text,
			       (alloced_lines + 1) * sizeof(char *));
		     t2 = &new_text[offset];
		   }
		}else {
                  dprint(1, (debugfile, "Helptext Failed open \"%s\": \"%s\"\n",
                             line_buf, error_description(errno)));
		}
                t++;
                continue;
            }  else {
                /*-- The normal case, just another line of help text ---*/
                line = *t;
            }
        }

        /*-- line now contains the text to use, where ever it came from --*/
        dprint(9, (debugfile, "line-->%s<-\n", line));
	if(srchstr(line, "___----") != NULL) {
	    pages[current_page++] = t2 - new_text;
	}
	*t2 = (char *)fs_get(strlen(line) + 1);
        copy_fix_keys(*t2, line);
        t2++;
	if(!in_include || (in_include && file == NULL))
            t++;
    }
    *t2 = *t;
    pages[current_page] = -1;

    dprint(7, (debugfile, "helper PAGE COUNT %d\n", current_page));
    { int i;
        for(i = 0 ; pages[i] != -1; i++) 
         dprint(7, (debugfile, "helper PAGE %d line %d [%s]\n",i+1, pages[i],
                    new_text[pages[i]]));
    }

		    
    /* This is mostly here to get the curses variables for line and column
        in sync with where the cursor is on the screen. This gets warped
	when the composer is called because it does it's own stuff */
    ClearScreen();
    scrolltool((void *)new_text, title, pages,
               from_composer ? ComposerHelpText : HelpText, CharStarStar,
	           (struct attachment *)NULL);
    ClearScreen();

    for(t2 = new_text; *t2 != NULL; t2++)
      fs_give((void **)t2);

    new_text_nr = new_text;
    fs_give((void **)&new_text_nr);
#endif	/* DOS */
}


void
print_all_help()
{
#ifdef	DOS
    short t;
    char **l, **h, buf[500];
#else
    char ***t, **l, buf[500];
#endif
    int     line_count;

    if(open_printer("all 50 pages of help text ") == 0) {
#ifdef	DOS
        for(t = 0; t < LASTHELP; t++) {
	    if((h = get_help_text(t, NULL)) == NULL){
		return;
    	    }
            for(l = h; *l != NULL; l++) {
#else
        for(t = h_texts; *t != NULL; t++) {
            line_count = 0;
            for(l = *t; *l != NULL; l++) {
#endif
                copy_fix_keys(buf, *l);
                print_text(buf);
                print_char('\n');
                line_count++;
            }

            if(line_count <= 10){
		print_text(NEWLINE);
		print_text(NEWLINE);
	    }
	    else
              print_char(ctrl('L'));
        }
        close_printer();
    }
}
