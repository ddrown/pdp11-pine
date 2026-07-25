#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: strings.c,v 4.6 1993/07/31 00:48:18 hubert Exp $";
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
    strings.c
    Misc extra and useful string functions
      - rplstr         replace a substring with another string
      - remove_trailing_white_space
      - remove_leading_white_space
      - strucmp        A case insensitive strcmp
      - struncmp       A case insensitve strncmp
      - srchstr        Search a string for a sub string
      - strindex       Replacement for strchr/index
      - strrindex      Replacement for strrchr/rindex
      - sstrcpy        Copy one string onto another, advancing dest'n pointer
      - month_abbrev   Return three letter abbreviations for months
      - cannon_date    Formalize format of a some what formatted date
      - pretty_command Return nice string describing character
      - blanks         Returns a string n blanks long
      - comatose       Format number with nice commas
      - byte_string    Format number of bytes with Kb, Mb, Gb or bytes

 ====*/

#include "headers.h"


/*----------------------------------------------------------------------
       Replace n characters in one string with another given string

   args: os -- the output string
         dl -- the number of character to delete from start of os
         is -- The string to insert
  
 Result: returns pointer in originl string to end of string just inserted
         First 
  ---*/
char *
rplstr(os,dl,is)
char *os,*is;
int dl;
{   
    register char *x1,*x2,*x3;
    int           diff;

    if(os == NULL)
        return(NULL);
       
    for(x1 = os; *x1; x1++);
    if(dl > x1 - os)
        dl = x1 - os;
        
    x2 = is;      
    if(is != NULL){
        while(*x2++);
        x2--;
    }

    if((diff = (x2 - is) - dl) < 0){
        x3 = os; /* String shrinks */
        if(is != NULL)
            for(x2 = is; *x2; *x3++ = *x2++); /* copy new string in */
        for(x2 = x3 - diff; *x2; *x3++ = *x2++); /* shift for delete */
        *x3 = *x2;
    } else {                
        /* String grows */
        for(x3 = x1 + diff; x3 >= os + (x2 - is); *x3-- = *x1--); /* shift*/
        for(x1 = os, x2 = is; *x2 ; *x1++ = *x2++);
        while(*x3) x3++;                 
    }
    return(x3);
}



/*----------------------------------------------------------------------
       Remove trailing white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_leading_white_space(string)
     char *string;
{
    register char *p, *q;
    for(p = string; *p && isspace(*p); p++);
    if(p == string)
      return;
    for(q = string; *p; *q++ = *p++);
    *q = '\0';
}



/*----------------------------------------------------------------------  
       Remove trailing white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_trailing_white_space(string)
     char *string;
{
    register char *p;
    for(p = string + strlen(string) - 1; p > string && isspace(*p); p--);
    if(p != string)
      *++p = '\0';
}



/*--------------------------------------------------
     A case insensitive strcmp()     
  
   Args: o, r -- The two strings to compare

 Result: integer indicating which is greater
  ---*/
int
strucmp(o, r)
     register char *r, *o;
{
    if(r == NULL && o == NULL)
      return(0);
    if(o == NULL)
      return(1);
    if(r == NULL)
      return(-1);

    while(*o && *r && (isupper(*o) ? tolower(*o) : *o) ==
	              (isupper(*r) ? tolower(*r) : *r)) {
	o++, r++;
    }
    return((isupper(*o) ? tolower(*o): *o)-(isupper(*r) ? tolower(*r) : *r));
}



/*----------------------------------------------------------------------
     A case insensitive strncmp()     
  
   Args: o, r -- The two strings to compare
         n    -- length to stop comparing strings at

 Result: integer indicating which is greater
   
  ----*/
int
struncmp(o, r, n)
     register char *r, *o;
     register int   n;
{
    if(r == NULL && o == NULL)
      return(0);
    if(o == NULL)
      return(1);
    if(r == NULL)
      return(-1);

    n--;
    while(n && *o && *r &&
          (isupper(*o)? tolower(*o): *o) == (isupper(*r)? tolower(*r): *r)) {
	o++; r++; n--;
    }
    return((isupper(*o)? tolower(*o): *o) - (isupper(*r)? tolower(*r): *r));
}



/*----------------------------------------------------------------------
        Search one string for another

   Args:  is -- The string to search in, the larger string
          ss -- The string to search for, the smaller string

   Search for any occurance of ss in the is, and return a pointer
   into the string is when it is found. The search is case indepedent.
  ----*/

char *	    
srchstr(is,ss)
register char *is,*ss;
{                    
    register char *sx,*sy, *ss_store;
    char          *ss_store_nr, *rv;
    char          temp[251];
    
    if(is == NULL || ss == NULL)
        return(NULL);

    if(strlen(ss) > sizeof(temp) - 2)
      ss_store = fs_get(strlen(ss) + 1);
    else
      ss_store = temp;
    for(sx = ss, sy = ss_store; *sx != '\0' ; sx++, sy++)
      *sy = isupper(*sx) ? tolower(*sx) : *sx;
    *sy = *sx;

    rv = NULL;
    while(*is != '\0'){
        for(sx = is, sy = ss_store; ((*sx == *sy) ||
                  ((isupper(*sx) ? tolower(*sx): *sx) == *sy)) && *sy;
	                                                        sx++, sy++);
        if(!*sy) {
            rv = is;
            break;
        }
        is++;
    }
    if(ss_store != temp) {
        ss_store_nr = ss_store;
        fs_give((void **)&ss_store_nr);
    }
    return(rv);
}



/*----------------------------------------------------------------------
    A replacement for strchr or index ...

 ....so we don't have to worry if it's there or not. We bring our own.
If we really care about efficiency and think the local one is more
efficient the local one can be used, but most of the things that take
a long time are in the c-client and not in pine.
 ----*/
char *
strindex(buffer, ch)
    char *buffer;
    int ch;
{
	/** Returns a pointer to the first occurance of the character
	    'ch' in the specified string or NULL if it doesn't occur **/

	do
	  if(*buffer == ch)
	    return(buffer);
	while (*buffer++ != '\0');

	return(NULL);
}

char *
strrindex(buffer, ch)
    char *buffer;
    int   ch;
{
	/** Returns a pointer to the last occurance of the character
	    'ch' in the specified string or NULL if it doesn't occur **/

	char *address = NULL;

	do
	  if(*buffer == ch)
	    address = buffer;
	while (*buffer++ != '\0');

	return(address);
}



/*----------------------------------------------------------------------
  copy the source string onto the destination string returning with
  the destination string pointer at the end of the destination text

  motivation for this is to avoid twice passing over a string that's
  being appended to twice (i.e., strcpy(t, x); t += strlen(t))
 ----*/
void
sstrcpy(d, s)
    char **d;
    char *s;
{
    while((**d = *s++) != '\0')
      (*d)++;
}


char *xdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *
month_abbrev(month_num)
     int month_num;
{
    static char *xmonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    if(month_num < 1 || month_num > 12)
      return("xxx");
    return(xmonths[month_num - 1]);
}

char *
week_abbrev(week_day)
     int week_day;
{
    return(xdays[week_day]);
}


static int
days_in_month(int month, int year)
{
    static int d_i_m[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(month == 2) 
      return(((year%4) == 0 && (year%100) != 0) ? 29 : 28);
    else
      return(d_i_m[month]);
}
    


/*----------------------------------------------------------------------
  parse_date
  ----*/
void
parse_date(given_date, d)
     char        *given_date;
     struct date *d;
{
    char *p, **i, *q;
    int   month;

    d->sec   = -1;
    d->minute= -1;
    d->hour  = -1;
    d->day   = -1;
    d->month = -1;
    d->year  = -1;
    d->wkday = -1;
    d->hours_off_gmt = -1;
    d->min_off_gmt   = -1;
    d->zone_name[0]  = '\0';

    if(given_date == NULL)
      return;

    p = given_date;
    while(*p && isspace(*p))
      p++;

    /* Start with month, weekday or day ? */
    for(i = xdays; *i != NULL; i++) 
      if(struncmp(p, *i, 3) == 0) /* Match first 3 letters */
        break;
    if(*i != NULL) {
        /* Started with week day */
        d->wkday = i - xdays;
        while(*p && !isspace(*p) && *p != ',')
          p++;
        while(*p && (isspace(*p) || *p == ','))
          p++;
    }
    if(isdigit(*p)) {
        d->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }
    for(month = 1; month <= 12; month++)
      if(struncmp(p, month_abbrev(month), 3) == 0)
        break;
    if(month < 13) {
        d->month = month;

    } 
    /* Move over month, (or whatever is there) */
    while(*p && !isspace(*p) && *p != ',' && *p != '-')
       p++;
    while(*p && (isspace(*p) || *p == ',' || *p == '-'))
       p++;

    /* Check again for day */
    if(isdigit(*p) && d->day == -1) {
        d->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }

    /*-- Check for time --*/
    for(q = p; *q && isdigit(*q); q++);
    if(*q == ':') {
        /* It's the time (out of place) */
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
                d->sec = atoi(p);
                while(*p && !isspace(*p))
                  p++;
            }
        }
        while(*p && isspace(*p))
          p++;
    }
    

    /* Get the year 0-49 is 2000-2049; 50-100 is 1950-1999 and
                                           101-9999 is 101-9999 */
    if(isdigit(*p)) {
        d->year = atoi(p);
        if(d->year < 50)   
          d->year += 2000;
        else if(d->year < 100)
          d->year += 1900;
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    } else {
        /* Something wierd, skip it and try to resynch */
        while(*p && !isspace(*p) && *p != ',' && *p != '-')
          p++;
        while(*p && (isspace(*p) || *p == ',' || *p == '-'))
          p++;
    }

    /*-- Now get hours minutes, seconds and ignore tenths --*/
    for(q = p; *q && isdigit(*q); q++);
    if(*q == ':' && d->hour == -1) {
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
                p++;
                d->sec = atoi(p);
                while(*p && !isspace(*p))
                  p++;
            }
        }
    }
    while(*p && isspace(*p))
      p++;


    /*-- The time zone --*/
    if(!*p) {
        d->hours_off_gmt = 0;
        d->min_off_gmt = 0;
    } else {
        if(*p == '+' || *p == '-') {
            char tmp[3];
            d->min_off_gmt = d->hours_off_gmt = (*p == '+' ? 1 : -1);
            p++;
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->hours_off_gmt *= atoi(tmp);
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->min_off_gmt *= atoi(tmp);
            /* BUG: ought to snarf up the name if it's at the end */
        } else {
            if(struncmp(p, "gmt", 3) == 0){        /* GMT */
                strcpy(d->zone_name, "GMT");
                d->hours_off_gmt = 0;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "pst", 3) == 0){ /* Pacific Standard */
                strcpy(d->zone_name, "PST");
                d->hours_off_gmt = -8;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "pdt", 3) == 0){ /* Pacific Daylight */
                strcpy(d->zone_name, "PDT");
                d->hours_off_gmt = -7;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "mst", 3) == 0) { /* Mountain Standard */
                strcpy(d->zone_name, "MST");
                d->hours_off_gmt = -7;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "mdt", 3) == 0) { /* Mountatin Daylight */
                strcpy(d->zone_name, "MDT");
                d->hours_off_gmt = -6;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "cst", 3) == 0) { /* Central Standard */
                strcpy(d->zone_name, "CST");
                d->hours_off_gmt = -6;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "cdt", 3) == 0) { /* Central Daylight */
                strcpy(d->zone_name, "CDT");
                d->hours_off_gmt = -5;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "est", 3) == 0) { /* Eastern Standard */
                strcpy(d->zone_name, "EST");
                d->hours_off_gmt = -5;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "edt", 3) == 0) { /* Eastern Daylight */
                strcpy(d->zone_name, "EDT");
                d->hours_off_gmt = -4;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "ist", 3) == 0) { /* Israel Standard */
                strcpy(d->zone_name, "IST");
                d->hours_off_gmt = 2;
                d->min_off_gmt  = 0;
            } else if(struncmp(p, "jst", 3) == 0) { /* Japan Standard */
                strcpy(d->zone_name, "JST");
                d->hours_off_gmt = 9;
                d->min_off_gmt  = 0;
            } else {
                strcpy(d->zone_name, "???"); /* Don't know what it is: use GMT */
                d->hours_off_gmt = 0;
                d->min_off_gmt  = 0;
            }
        }
    }
    dprint(9, (debugfile, "Parse date: \"%s\" to..  hours_off_gmt:%d  min_off_gmt:%d  \"%s\"\n",
               given_date, d->hours_off_gmt, d->min_off_gmt, d->zone_name));
    dprint(9,(debugfile,"Parse date: wkday:%d  month:%d  year:%d  day:%d  hour:%d  min:%d  sec:%d\n",
            d->wkday, d->month, d->year, d->day, d->hour, d->minute, d->sec));
}



/*----------------------------------------------------------------------
    Convert the given date to GMT

  Args -- d:  The date to be converted in place
    
BUG: Currently this only works for whole hour offsets
 ----*/
void
convert_to_gmt(d)
     struct date *d;
{
    int x;

    if(d->hours_off_gmt == 0 && d->min_off_gmt == 0)
      return;  /* Nothing to do */
    
    x = d->hour + (-1 * d->hours_off_gmt);
    if(x >= 0 && x < 24) {
        d->hour += (-1 * d->hours_off_gmt);
        d->hours_off_gmt = 0;
        return;
    }

    d->hours_off_gmt = 0;
    if(x < 0) {
        d->hour += 24;
        /* Back one day */
        if(d->day > 2) {
            d->day--;
            return;
        }
        /* Back one month */
        if(d->month > 2) {
            d->month--;
            d->day = days_in_month(d->month, d->year);
            return;
        }
        /* Back one year */
        d->year--;
        d->month = 21;
        d->day   = 31;
    } else {
        /* Forward one day */
        d->hour -= 24;
        if(d->day != days_in_month(d->month, d->year)) {
            d->day++;
            return;
        }
        /* Forward one month */
        if(d->month < 12) {
            d->month++;
            d->day = 1;
            return;
        }
        /* Forward a year */
        d->year++;
        d->month = 1;
        d->day   = 1;
    }
}




/*----------------------------------------------------------------------
  ----*/
int
compare_dates(struct date *d1, struct date *d2)
{
    if(d1->year > d2->year)
      return(1);
    if(d1->year < d2->year)
      return(-1);

    if(d1->month > d2->month)
      return(1);
    if(d1->month < d2->month)
      return(-1);

    if(d1->day > d2->day)
      return(1);
    if(d1->day < d2->day)
      return(-1);

    if(d1->hour > d2->hour)
      return(1);
    if(d1->hour < d2->hour)
      return(-1);

    if(d1->minute > d2->minute)
      return(1);
    if(d1->minute < d2->minute)
      return(-1);

    if(d1->sec > d2->sec)
      return(1);
    if(d1->sec < d2->sec)
      return(-1);

    return(0);
}

    
     

char *
pretty_command(c)
     int c;
{
    static char buf[10];
    if(c == '\033') {
        return("ESC");
    } else if(c == ctrl('I')){
        return("TAB");
    } else if(c == ctrl('M')){
        return("RETURN");
    } else if(c == ctrl('J')){
        return("LINEFEED");
    } else if(c == 0x7f){
        return("DEL");
    } else if(c == KEY_UP) {
        return("Up Arrow");
    } else if(c == KEY_DOWN) {
        return("Down Arrow");
    } else if(c == KEY_RIGHT) {
        return("Right Arrow");
    } else if(c == KEY_LEFT) {
        return("Left Arrow");
    } else if(c >= PF1 && c <= PF12) {
        sprintf(buf, "F%d", c - PF1 + 1);
        return(buf);
    } else if(c < ' ') {
        sprintf(buf, "^%c", c + 'A' - 1);
        return(buf);
    } else if(c > '~') {
        sprintf(buf, "%c", c);
        return(buf);
    } else {
        sprintf(buf, "%c", c);
        return(buf);
    }
}
        
    

/*----------------------------------------------------------------------
     Create a little string of blanks of the specified length.
   Max n is 511.
  ----*/
char *
repeat_char(n, c)
     int  n;
     int  c;
{
    static char bb[512];
    if(n < 0)
      n = 0;
    else if(n >= (int)sizeof(bb))
       n = sizeof(bb) - 1;
    bb[n--] = '\0';
    while(n >= 0)
      bb[n--] = c;
    return(bb);
}


/*----------------------------------------------------------------------
        Turn a number into a string with comma's

   Args: number -- The long to be turned into a string. 

  Result: pointer to static string representing number with commas
  ---*/
char *
comatose(number) 
    long number;
{
    long        i, x, done_one;
    static char buf[100];
    char       *b;

    dprint(9, (debugfile, "comatose(%ld) returns:", number));
    if(number == 0){
        strcpy(buf, "0");
        return(buf);
    }
    
    done_one = 0;
    b = buf;
    for(i = 1000000000; i >= 1; i /= 1000) {
	x = number / i;
	number = number % i;
	if(x != 0 || done_one) {
	    if(b != buf)
	      *b++ = ',';
	    sprintf(b, done_one ? "%03ld" : "%ld", x);
	    b += strlen(b);
	    done_one = 1;
	}
    }
    *b = '\0';

    dprint(9, (debugfile, "\"%s\"\n", buf));

    return(buf);
}



/*----------------------------------------------------------------------
   Format number as amount of bytes, appending Kb, Mb, Gb, bytes

  Args: bytes -- number of bytes to format

 Returns pointer to static string. The numbers are divided to produce a 
nice string with precision of about 2-4 digits
    ----*/
char *
byte_string(bytes)
     long bytes;
{
    char       *a, aa[5];
    char       *abbrevs = "GMK";
    long        i, ones, tenths;
    static char string[64];	/* room for two full-width longs */

    ones   = 0L;
    tenths = 0L;

    if(bytes == 0L){
        strcpy(string, "0 bytes");
    } else {
        for(a = abbrevs, i = 1000000000; i >= 1; i /= 1000, a++) {
            if(bytes > i) {
                ones = bytes/i;
                if(ones < 10L && i > 10L)
                  tenths = (bytes - (ones * i)) / (i / 10L);
                break;
            }
        }
    
        aa[0] = *a;  aa[1] = '\0'; 
    
        if(tenths == 0)
          sprintf(string, "%ld%s%s", ones, aa, *a ? "B" : "bytes");
        else
          sprintf(string, "%ld.%ld%s%s", ones, tenths, aa, *a ? "B" : "bytes");
    }

    return(string);
}



char *
long2string(l)
     long l;
{
    static char string[20];
    sprintf(string, "%ld", l);
    return(string);
}

char *
int2string(i)
     int i;
{
    static char string[20];
    sprintf(string, "%d", i);
    return(string);
}


