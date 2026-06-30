#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adrbklib.c,v 4.6 1993/10/07 05:32:07 mikes Exp $";
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

#include "headers.h"
#include "adrbklib.h"

#ifdef	DOS
#define	ADRBK_NAME	"addrbook"
#define	TMP_ADRBK_NAME	"addrbook.tmp"
#else
#define	ADRBK_NAME	".addressbook"
#define	TMP_ADRBK_NAME	".addressbook.temp"
#endif


#ifndef MAXPATH
#define MAXPATH 1000   /* Longest file path we can deal with */
#endif
#define MAXLIST 1000   /* Longest address list we can deal with */
#define MAXFULLNAME 256 /* Longest full name we can deal with */


#ifdef ANSI
int adrbk_write(AdrBk *);
int cmp_addr(const QSType *, const QSType *);
int compare_addr_ents(const QSType *, const QSType *);
int compare_nicks(const QSType *, const QSType *);

#else

int adrbk_write();
int cmp_addr();
int compare_nicks();
int compare_addr_ents();

#endif



/*----------------------------------------------------------------------
    Open, read and parse the address book

  input: the filename to open if specified
         the users home directory if specified

 result: the address book is opened, read into memory and parsed
         returns AdrBk structure or NULL. In case of NULL check errno

   If the filename is NULL, the default will be used in the home_dir
   passed in. If the home_dir is NULL, the current dir will be used.
   If the filename is not NULL and is an absolute path just the filename
   will be used. Other wise it will be used relative to the home dir, or
   to the current dir depending on whether or not home_dir is NULL.
         
   Expected file format is:
      <nickname>\t<fullname>\t<addresses>\t<extra>\n
   The extra field is completely optional. If any fields are missing 
   they will just be considered blank. Completely blank lines will be
   ignored. Leading and trailing white space is ignored, as is white 
   white space around the tab delimiters (this can only be \b chars).
   The address field may be a comma separated list and extend over serveral
   lines if enclosed in parenthesis.

   Commas in the address field will cause problems, as will tabs in any
   field. Spaces are best kept out of the nick name field.

   See adrbk_close for comments on storage allocation.
  ----------------------------------------------------------------------*/
static char    empty[] = {'\0'};

AdrBk  *
adrbk_open(filename, homedir)
     char *filename;
     char *homedir;
{
    register char *p, *q, *next_field, field_end;
    char           path[MAXPATH], *temp_addr_list[MAXLIST];
    AdrBk         *ab;
    register AdrBk_Entry  **abe, *a, **abe2;
    AdrBk_Entry   *a_noregister;
    int            count;


    ab = (AdrBk *)fs_get(sizeof(AdrBk));
    ab->book_allocated = 30; /* 30's a nice number. 9 is a measured average*/
    ab->book_used = 0;
    ab->book = (AdrBk_Entry **)fs_get(ab->book_allocated *
                                              sizeof(AdrBk_Entry *));
    ab->book[0] = (AdrBk_Entry *) NULL;


    /*------------ figure out and save name of file to open ---------*/
    if(filename == NULL) {
        if(homedir != NULL) {
	    build_path(path, homedir, ADRBK_NAME);
            ab->filename = cpystr(path);
	    build_path(path, homedir, TMP_ADRBK_NAME);
            ab->temp_filename = cpystr(path);
        } else {
            ab->filename = cpystr(ADRBK_NAME);
            ab->temp_filename = cpystr(TMP_ADRBK_NAME);
        }
    } else {
#ifdef	DOS
        if(*filename == '\\') {
#else
        if(*filename == '/') {
#endif
            ab->filename = cpystr(filename);
        } else {
            if(homedir != NULL) {
		build_path(path, homedir, filename);
                ab->filename = cpystr(path);
            } else {
                ab->filename = cpystr(filename);
            }
        }
	q = "." ;
#ifdef DOS
	if (p = strrchr(ab->filename, '\\')) {
#else
	if (p = strrchr(ab->filename, '/')) {
#endif
	  *p = '\0' ;
	  q = ab->filename ;
	}
	ab->temp_filename = temp_nam(q, "adr") ;
	if (p)
#ifdef DOS
	  *p = '\\' ;
#else
	  *p = '/' ;
#endif
    }

    /*------ read the file into one chunk of memory -----*/
    ab->storage = read_file(ab->filename);
    if(ab->storage == NULL) {
        /*--- No address book, try creating one ----*/
        FILE *f = fopen(ab->filename, "w");
        if(f == NULL) {
            /*--- Create failed, bail out ---*/
            fs_give((void **)&ab);
            return((AdrBk *)NULL);
        }
        fclose(f);
        ab->storage     = NULL;
        ab->storage_end = NULL;
        ab->book[0]     = (AdrBk_Entry *)NULL;
        return(ab);
    }
    ab->storage_end = ab->storage + strlen(ab->storage);
    a = adrbk_newentry();

    /*------- Parse the file -----------*/
    field_end = 'x'; 
    for(p = ab->storage; field_end != '\0'; ){

        /*----- Skip blank lines ----*/
        for(q = p; *q && *q != '\n' && isspace(*q); q++);
        if(*q == '\0' || *q == '\n') {
            /*-- Line is blank ---*/
            field_end = *q;
            p = q + 1;
            continue;
        }

        /*---- we don't want any NULL entries, except for the extra field ---*/
        a->nickname = empty;
        a->fullname = empty;
        a->addr.addr= empty;
        a->addr.list= (char **)empty;


        /*------- nick name -----------*/
        while(*p && *p != '\n' && *p == ' ') p++;  /* skip leading space */
        a->nickname = p;
        while(*p && *p != '\n' && *p != '\t') p++; /* go over nickname */
        field_end = *p;
        next_field = p;
        if(*(p-1) == ' ')                   /* back over trailing spaces */
          while(p > a->nickname + 1 && *(p-1) == ' ') p--;
        *p = '\0';
        p = next_field + 1;
        if(field_end == '\n' || field_end == '\0')
          goto line_done;


        /*--------- full name -----------*/
        while(*p && *p !='\n' && *p == ' ') p++;  /* skip leading space */
        a->fullname = p;
        while(*p && *p !='\n' && *p != '\t') p++; /* go over full name */
        field_end = *p;
        next_field = p;
        if(*(p-1) == ' ')                     /* back over trailing spaces */
          while(p > a->fullname + 1 && *(p-1) == ' ') p--;
        *p = '\0';
        p = next_field + 1;
        if(field_end == '\n' || field_end == '\0') 
          goto line_done;


        /*----------- address (list) ----------*/
        while(*p && *p !='\n' && *p == ' ') p++;  /* skip leading space */

        field_end = '\0';
        if(*p == '(') {
	    /*---- it's a list of address in () ------*/
            char **ad = temp_addr_list;
	    do {
	        /*---- loop through addresses in this list ---*/
		p++;
                while(*p && *p != '\n' && *p != '\t' && isspace(*p))
                  p++;  /*skip leading space*/
		*ad = p;
		while(*p && *p!=',' && *p!=')' && *p!='\n' && *p!='\t') p++;
                if(*p == '\t' && field_end == '\n')
                  break;/* Tabs in address mean something bad. End this list */
		field_end = *p;
		next_field = p;
		if(*(p-1) == ' ')      /* trailing spaces */
		  while(p > *ad + 1 && *(p-1) == ' ') p--;
		*p = '\0';
		if(**ad) {
		    ad++;
                }
	    } while(field_end == '\n' || field_end == ',');
	    *ad = NULL;
	    a->addr.list = (char **)fs_get((ad - temp_addr_list + 1)
                                           * sizeof(char *));
	    memcpy((void *)a->addr.list, (void *)temp_addr_list,
		           (ad - temp_addr_list + 1) * sizeof(char *));
            sort_addr_list(a->addr.list);
	    a->tag = List;
            
        } else {
            /*---- test for old style address lists -----*/
            for(q = p; *q!=',' && *q!='\t' && *q!='\n' && *q!='\0'; q++);

            if(*q == ','){
                /*----- an old style address list with no ()-----*/
                char **ad = temp_addr_list;
                do {
                    while(*p && *p != '\n' && *p != '\t' && isspace(*p))
                      p++; /* skip leading white space */
                    *ad = p;
                    while(*p && *p != '\n' && *p != ',' && *p != '\t')
                      p++; /* traverse address */
                    field_end = *p;
                    next_field = p;
                    if(*(p - 1) == ' ')    /* back over trailing space */
                      while(p > *ad + 1 && *(p-1) == ' ') p--; 
                    *p = '\0';
                    if(**ad)
                      ad++;
                    p++;
                } while(field_end == ',');
                *ad = NULL;
                a->addr.list = (char **)fs_get((ad -temp_addr_list +1) *
                                               sizeof(char *));
                memcpy((void *)a->addr.list, (void *)temp_addr_list,
                               (ad - temp_addr_list + 1) * sizeof(char *));
                sort_addr_list(a->addr.list);
                a->tag = List;
            } else {
    
                /*---- A plain, single address -------*/
                a->addr.addr = p;
                while(*p && *p !='\n' && *p != '\t')
                  p++; /* traverse addr list */
                field_end = *p;
                next_field = p;
                if(*(p-1) == ' ')      /* trailing spaces */
                  while(p > a->addr.addr && *(p-1) == ' ') p--;
                *p = '\0';
                a->tag = Atom;
            }
        }
        p = next_field + 1;
        if(field_end != '\n' && field_end != '\0')  {
            /*----- carry along anything extra -----------*/
            while(*p && *p != '\n' && *p == ' ') p++;
            field_end = *p;
            if(*p != '\0' && *p != '\n') {
                a->extra = p;
                while(*p && *p != '\n') p++;
                field_end = *p;
                *p = '\0';
            }
            p++;
        }

     line_done:
        ab->book[ab->book_used++] = a;
        if(ab->book_used + 1 == ab->book_allocated) {
            ab->book_allocated *= 2;
            fs_resize((void **)&(ab->book), ab->book_allocated *
                                           sizeof(AdrBk_Entry *));
        }
        a = adrbk_newentry();

    }

    /*----- sort the entries ----*/
    qsort((QSType *) ab->book,
#ifndef DYN
          (size_t)ab->book_used, 
#else
          (int)ab->book_used, 
#endif
          sizeof(AdrBk_Entry *), compare_addr_ents);

    {
        /*---- This is faster for large address books ---*/
        AdrBk_Entry **temp_sort_list;
        char        *dupe = "Duplicate";
        
        temp_sort_list = (AdrBk_Entry **)fs_get((ab->book_used + 1) *
                                                     sizeof(AdrBk_Entry *));
        memcpy(temp_sort_list,ab->book, ab->book_used * sizeof(AdrBk_Entry *));
        qsort((QSType *)temp_sort_list,
#ifdef DYN
              (int)ab->book_used,
#else
              (size_t)ab->book_used,
#endif
              sizeof(AdrBk_Entry *), compare_nicks);
          
        /*----- Hack out duplicates -------*/
        for(abe = temp_sort_list ; abe < &temp_sort_list[ab->book_used - 1]; abe++)
          if(strucmp((*abe)->nickname, (*(abe+1))->nickname) == 0) {
              (*abe)->nickname = dupe; /* BUG need to free these */
          }
        count = 0;
        for(abe2 = abe = ab->book ; abe < &ab->book[ab->book_used]; abe++)
          if((*abe)->nickname != dupe) {
              *abe2++ = *abe;
              count++;
          }
        ab->book_used = count;
        fs_give((void **)&temp_sort_list);
    }
                    

    /*---- tie of end of list -----*/
    ab->book[ab->book_used] = (AdrBk_Entry *)NULL;

    a_noregister = a;
    fs_give((void **)&a_noregister);

    return(ab);
}



/*----------------------------------------------------------------------
    Compare two address book entries for qsort()
  ----------------------------------------------------------------------*/
int
compare_addr_ents(a, b)
    const QSType *a, *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    result = strucmp((*x)->fullname, (*y)->fullname);
    if(result == 0)
      result = strucmp((*x)->nickname, (*y)->nickname);
    if(result == 0){
        if((*x)->tag == Atom)
          result = strucmp((*x)->addr.addr, (*y)->addr.addr);
        else
          result = strucmp(*((*x)->addr.list),*((*x)->addr.list));
    }
    if(result == 0 && *x != *y)
      result = *x > *y ? -1 : 1;
      
    return(result);
}


int
compare_nicks(a, b)
    const QSType *a, *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    result = strucmp((*x)->nickname, (*y)->nickname);
    return(result);
}



/*----------------------------------------------------------------------
     Sort a simple list of pointers to strings
  ----------------------------------------------------------------------*/
int
cmp_addr(a1, a2)
    const QSType *a1, *a2;
{
    char *x = *(char **)a1, *y = *(char **)a2;

    return(strucmp(x, y));
}

sort_addr_list(list)
     char **list;
{
    register char **p;
    for(p = list; *p != NULL; p++);
    qsort((QSType *)list,
#ifdef DYN
          (p - list),
#else          
          (size_t)(p - list),
#endif          
          sizeof(char *), cmp_addr);
}




/*----------------------------------------------------------------------
    Return the size of the address book 
  ----------------------------------------------------------------------*/
unsigned int
adrbk_count(ab)
     AdrBk *ab;
{
    return(ab->book_used);
}



/*----------------------------------------------------------------------
     Return an indexed entry in the address book. Useful only
  for sequential access of address book, as indexes change when
  additions or deletions are made.
  ----------------------------------------------------------------------*/
AdrBk_Entry *
adrbk_get(ab, num)
     AdrBk       *ab;
     unsigned int num;
{
    if(num >= ab->book_used)
      return((AdrBk_Entry *)NULL);

    return(ab->book[num]);
}

    

/*----------------------------------------------------------------------
     Look up an entry in the address book given a nickname

  Input: address book, nickname to match

 Result: A pointer to and AdrBk_Entry is returned, or NULL if not found.
 

  Linear search because they're sorted by full name. This is probably
fast enough. Lookups usually need to be recursive in case the address
book references itself. This is left to the next level up.
adrbk_clearrefs() is provided to clear all the reference tags in
the address book for loop detetction.

 ----------------------------------------------------------------------*/
AdrBk_Entry *
adrbk_lookup(ab, nickname)
     AdrBk *ab;
     char  *nickname;
{
    register AdrBk_Entry **a;
    int                    cmp;

    cmp = 1; /* Not 0 */

    if(ab == NULL)
      return((AdrBk_Entry *)NULL);

    for(a = ab->book; a < &ab->book[ab->book_used]; a++) {
        cmp = strucmp((*a)->nickname, nickname);
        if(cmp == 0)
          break;
    }

    if(cmp == 0) {
        return(*a);
    } else {
        return((AdrBk_Entry *)NULL);
    }
}




/*----------------------------------------------------------------------
   
   Input:  Full name out of address book for formatting

  Result:  Returns pointer to internal buffer containing name formatted
           for a mail header.

If the name is in quotes or has no comma no change is made. Otherwise the
text before the first comma is moved to the end and the comma deleted.

  ----------------------------------------------------------------------*/
char *
adrbk_formatname(fullname)
     char *fullname;
{
    char       *comma;
    static char new_name[MAXFULLNAME];

    if(fullname[0] != '"'  && (comma = strindex(fullname, ',')) != NULL) {
        int   last_name_len = comma - fullname;
        comma++;
        while(*comma && isspace(*comma)) comma++;
        strcpy(new_name, comma);
        strcat(new_name, " ");
        strncat(new_name, fullname, last_name_len); 
    } else {
        strcpy(new_name, fullname);
    }
    return(new_name);
}
    



/*----------------------------------------------------------------------
    Input: address book

  For loop detection during address book look up. This clears all the 
referenced flags, then as the lookup proceeds the referenced flags can 
be checked and set.
 ----------------------------------------------------------------------*/  
void
adrbk_clearrefs(ab)
     AdrBk *ab;
{
    register AdrBk_Entry **a;

    if(ab == NULL)
      return;
    for(a = ab->book; a < &ab->book[ab->book_used]; a++)
      (*a)->referenced = 0;
}




/*----------------------------------------------------------------------
    Factory to allocate new AdrBk_Entries
 ----------------------------------------------------------------------*/
AdrBk_Entry *
adrbk_newentry()
{
    AdrBk_Entry *a;

    a = (AdrBk_Entry *)fs_get(sizeof(AdrBk_Entry));
    a->nickname = NULL;
    a->fullname = NULL;
    a->addr.addr= NULL;
    a->addr.list= NULL;
    a->extra    = NULL;
    a->tag      = Atom;

    return(a);
}


      
/*----------------------------------------------------------------------
     Add an entry to the address book, or modify existing entry
     
  Input: address book to add to
         pointer to return address of modified entry in
         nickname, fullname, addresses, and extra
         routine to call for confirmation if not NULL

 Result: return code:  0 all went well
                      -1 modification declined by user
                      -2 error writing address book, check errno
		      -3 on modification, the tag given didn't match existing tag
                      -4 tabs are in one of the fields passed in

  If the nickname exists in the address book already, the operation is
considered a modification even if the case does not match exactly,
otherwise it is an add. On modification the confirm routine is called
and must return 1 if modification is confirmed.  The entry the
operation occurs on is returned in new. All fields are set to those
passed in; that is passing in NULL even on a modification will set
those fields to NULL.  It is acceptable to pass in the current strings
in the entry in the case of modification. For address lists, the
structure passed in is what is used, so the storage has to all have
come from malloc or fs_give(). If the pointer passed in is the same as
the current field no change is made.
----------------------------------------------------------------------*/
adrbk_add(ab, new, nickname, fullname, address, tag, confirm)
     AdrBk        *ab;
     AdrBk_Entry **new;
     char            *nickname, *fullname, *address; /* address can be ** too*/
     Tag              tag;
#ifdef ANSI
     int (*confirm)(AdrBk_Entry *);
#else
     int (*confirm)();
#endif
{
    register AdrBk_Entry  *l, **a, **b, **c;
    AdrBk_Entry           *foo;
    int                    cmp;

    /* ---- Make sure there's no tabs in the stuff to add ------*/
    if((nickname != NULL && strchr(nickname, '\t') != NULL) ||
       (fullname != NULL && strchr(fullname, '\t') != NULL) ||
       (tag == Atom && address != NULL && strchr(address,  '\t') != NULL)) {
        return(-4);
    }

    /* Are we adding, or updating ? */
    l = adrbk_lookup(ab, nickname);
    if(l == NULL) {
        /*----- adding a new entry ----*/

        /*---- Make room for it -----*/
        if(ab->book_used + 1 == ab->book_allocated) {
            ab->book_allocated *= 2;
            fs_resize((void **)&(ab->book), ab->book_allocated *
		                                   sizeof(AdrBk_Entry *));
        }

        /*----- allocate it now so we can use compare_addr_ents() ----*/
        l           = adrbk_newentry();
        l->nickname = cpystr(nickname);
        l->fullname = fullname == NULL ? NULL : cpystr(fullname);
	l->tag      = tag;
	if(tag == Atom) {
            l->addr.addr = cpystr(address);
	} else {
	    l->addr.list = (char **)NULL;
	}


        /*--- Find slot for it ---*/
        foo = l; /* foo is non-register so we can take address of it */
        for(c = ab->book; *c != NULL; c++) {
            cmp = compare_addr_ents((QSType *)c, (QSType *)&foo);
            if(cmp >= 0)
              break;
        }

        /*------ slide the stuff down to make room -------*/
        a = &ab->book[ab->book_used];
        for(b = a + 1; a >= c; *b-- = *a--);

        *c = l;

        if(new != NULL)
          *new = l;
            
        ab->book_used++;

        
    } else {
        /*----- Updating an existing entry ----*/

	if(l->tag != tag)
	  return(-3);
#ifdef ANSI
        if(confirm != (int (*)(AdrBk_Entry *))NULL && (*confirm)(*new) == 0)
#else
        if(confirm != NULL && (*confirm)(*new) == 0)
#endif
          return(-1);


        /*-------
            See adrbk_close for explaination of storage management 
          -------*/
        if(l->nickname != nickname){
            if(l->nickname == NULL || strlen(nickname) > strlen(l->nickname)) {
                if(l->nickname > ab->storage_end || l->nickname < ab->storage)
                  fs_give((void **)&l->nickname);
                l->nickname = cpystr(nickname);
            } else {
                strcpy(l->nickname, nickname);
            }
        }

        if(fullname == NULL) {
            l->fullname = NULL;
        } else if (l->fullname != fullname) {
            if(l->fullname == NULL || strlen(fullname) > strlen(l->fullname)) {
                if(l->fullname > ab->storage_end || l->fullname < ab->storage)
                  fs_give((void **)&l->fullname);
                l->fullname = cpystr(fullname);
            } else {
                strcpy(l->fullname, fullname);
            }
        }

	if(tag == Atom) {
            /*---- Atom ----*/
	    if(address == NULL) {
		l->addr.addr = NULL;
	    } else if(l->addr.addr != address) {
		if(l->addr.addr==NULL || strlen(address)>strlen(l->addr.addr)){
		    if(l->addr.addr>ab->storage_end|| l->addr.addr<ab->storage)
		      fs_give((void **)&l->addr.addr);
		    l->addr.addr = cpystr(address);
		} else {
		    strcpy(l->addr.addr, address);
		}
	    }
	} else {
            /*---- List -----*/
            /* we don't mess with lists here */
	}


        /*---------- Make sure it's still in order ---------*/
        for(a = ab->book; *a != NULL; a++) 
            if(*a == l) /* find current spot in list */
              break; 

        if(a!=ab->book && compare_addr_ents((QSType *)a,(QSType *)(a-1)) < 0) {
            /*--- Out of order, needs to be moved up ----*/
            for(b = a - 1; b >= ab->book; b--)
              if(compare_addr_ents((QSType *)a, (QSType *)b) >= 0)
                break; /* find new position */
            for(c = a; c > b; c--) /* Shuffle things up */
              *c = *(c-1);
            *(c+1) = l;  /* put in it's new place */
        }

        if(a != &ab->book[ab->book_used-1] &&
            compare_addr_ents((QSType *)a, (QSType *)(a + 1)) > 0){
            /*---- Out of order needs, to be moved towards end of list ----*/
            for(b = a + 1; *b != NULL; b++)
              if(compare_addr_ents((QSType *)a, (QSType *)b) <= 0)
                break; /* find new position */
            for(c = a + 1; c < b; c++) /* Slide entries down */
              *(c-1) = *c;
            *(c-1) = l;                  /* put in it's new place */
        }


        /*---- return pointer if requested -----*/
        if(new != NULL)
          *new = l;


    }

    return(adrbk_write(ab));
}



/*----------------------------------------------------------------------
   Delete an entry from the address book

  Input: the address book structure, and a pointer to the entry to delete

 Result: returns:  0 if all went well
                  -1 if there is no such entry
                  -2 error writing address book, check errno

See adrbk_close of explaination of storage management
 ----------------------------------------------------------------------*/
adrbk_delete(ab, to_del)
     AdrBk *ab;
     AdrBk_Entry *to_del;
{
    register AdrBk_Entry **a, **b;
    char **p;

    for(a = ab->book; *a != NULL; a++) {
        if(*a == to_del)
          break;
    }

    if(*a == (AdrBk_Entry *)NULL) {
        return(-1);   /* entry not found, delete failed */
    }

    for(b = a + 1; *a != NULL; *a++ = *b++); /* slide it down */

    ab->book_used--;

    /*------ Now free the storage if appropriate ---------*/
    if (to_del->nickname != NULL && to_del->nickname != empty && (
         to_del->nickname > ab->storage_end || to_del->nickname < ab->storage))
      fs_give((void **)&to_del->nickname);

    if (to_del->fullname != NULL && to_del->nickname != empty && (
         to_del->fullname > ab->storage_end || to_del->fullname < ab->storage))
      fs_give((void **)&to_del->fullname);

    if(to_del->tag == Atom ) {
        if(to_del->addr.addr != NULL && to_del->nickname != empty && (
           to_del->addr.addr>ab->storage_end || to_del->addr.addr<ab->storage))
          fs_give((void **)&to_del->addr.addr);
    } else {
        if(to_del->addr.list != NULL) {
            for(p = to_del->addr.list; *p != NULL; p++) 
              if(*p != empty && ( *p > ab->storage_end || *p < ab->storage))
                fs_give((void **)p);
            fs_give((void **)&to_del->addr.list);
        }
    }

    if (to_del->extra != NULL && to_del->extra != empty && (
         to_del->extra > ab->storage_end || to_del->extra < ab->storage))
      fs_give((void **)&to_del->extra);

    fs_give((void **)&to_del);

    return(adrbk_write(ab)); 
}



/*----------------------------------------------------------------------
       Delete an address out of an address list

 Input: Address book, entry in address book to delete from, address to delete

Result: 0: Deletion complete, address book written
       -1: Address for deletion not found
       -2: Error writing address book. Check errno.

The address to be deleted is located by matching the string.
  ----------------------------------------------------------------------*/
adrbk_listdel(ab, entry, addr)
     AdrBk       *ab;
     AdrBk_Entry *entry;
     char           *addr;
{
    char **p, *to_free;

    for(p = entry->addr.list; *p != NULL; p++) 
        if(strcmp(*p, addr) == 0)
          break;

    if(*p == NULL)
      return(-1);

    /*---- free storage if its the thing to do -----*/
    to_free = NULL;
    if((*p > ab->storage_end || *p < ab->storage) && *p != empty)
      to_free = *p; /* freeing it now messes up next loop */

    /*---- slide all the entries below up ----*/
    while(*p != NULL) {
        *p = *(p+1);
        p++;
    }

    if(to_free != NULL)
      fs_give((void **)&to_free);

    return(adrbk_write(ab));
}


        
/*----------------------------------------------------------------------
       Add one address to an already existing address list

 Input: address book, address book entry to add to, address to add

Result: returns 0 : addition made, address book written
               -1 : addition to non-list attempted
               -2 : error writing address book -- check errno
  ----------------------------------------------------------------------*/
adrbk_listadd(ab, entry, addr, new_addr)
     AdrBk       *ab;
     AdrBk_Entry *entry;
     char           *addr, **new_addr;
{
    char **p;
    int    n;

    if(entry->tag != List)
      return(-1);

    /*--- count up size of list ------*/    
    for(p = entry->addr.list; p != NULL && *p != NULL; p++);

    /*----- make room at end of list for it ------*/
    if(entry->addr.list == NULL) {
        entry->addr.list = (char **)fs_get(2 * sizeof(char *));
        n = 1;
    } else {
        n = p - entry->addr.list;
        n++;
        fs_resize((void **)&entry->addr.list, sizeof(char *) * (n + 1));
    }

    /*----- Put it at the end -------*/
    (entry->addr.list)[n-1] = cpystr(addr);
    (entry->addr.list)[n]   = NULL;

    if(new_addr != NULL)
      *new_addr =(entry->addr.list)[n-1];

    /*---- sort it into the correct place ------*/
    sort_addr_list(entry->addr.list);

    return(adrbk_write(ab));
}

    
      
 
/*----------------------------------------------------------------------
    Close address book
 
 All that is done here is free the storage since the address book is 
rewritten on very change.

Storage management:

Initially all the strings in the address book come out of the 
large allocated buffer used to read the whole file in. This is quite efficient
in terms of CPU overhead. When entries are added or deleted it's not possible
to add to this pool so the strings are allocated individually. When it comes
time to free strings, any pointers into the big buffer are left alone and
get freed with the big buffer. Other pointers are deallocated individually.
 ----------------------------------------------------------------------*/
void
adrbk_close(ab)
     AdrBk *ab;
{
    register AdrBk_Entry  **a;
    char                  **p;
    
    fs_give((void **)&ab->storage);
      
    for(a = ab->book; *a != NULL; a++) {

        /*---- Free up any storage allocated after initial read in ----*/
        if ((*a)->nickname != NULL && (*a)->nickname != empty &&(
             (*a)->nickname > ab->storage_end || (*a)->nickname < ab->storage))
          fs_give((void **)&(*a)->nickname);

        if ((*a)->fullname != NULL && (*a)->fullname != empty &&(
             (*a)->fullname > ab->storage_end || (*a)->fullname < ab->storage))
          fs_give((void **)&(*a)->fullname);

        if ((*a)->tag == Atom) {
            if((*a)->addr.addr != NULL &&  (*a)->addr.addr != empty && (
              (*a)->addr.addr>ab->storage_end || (*a)->addr.addr< ab->storage))
              fs_give((void **)&(*a)->addr.addr);
        } else {
            if((*a)->addr.list != NULL) {
                for(p = (*a)->addr.list; *p != NULL ; p++)
                  if((*p > ab->storage_end || *p < ab->storage) && *p != empty)
                    fs_give((void **)p);
                fs_give((void **)&((*a)->addr.list));
            }
        }


        if ((*a)->extra != NULL && (*a)->extra != empty && (
             (*a)->extra > ab->storage_end || (*a)->extra < ab->storage))
          fs_give((void **)&(*a)->extra);
        
        fs_give((void **)a);
    }

    fs_give((void **)&ab->book);

    fs_give((void**)&ab->filename);

    fs_give((void**)&ab->temp_filename);

    fs_give((void **)&ab);
}
        


/*----------------------------------------------------------------------
     Write out the address book.

Format is as in comment in the adrbk_open routine. For long address
lists the lines are wrapped to be under 80 characters, hence the code
to keep track of the current line length. This is pretty fast so it can
be called on every change to the address book. SIGINT and SIGHUP are also
ignored during writing so the address book won't be messed up. Write is first
to a temporary file, which is then renamed to be the real address book so we
won't destroy the real address book in event of something like a full file 
system.

Writing a temp file and then renaming has the bad side affect of destroying
links and permissions. It also overrides any read only permissions on the 
mail file as unlink an rename ignore such permissions. we could make this code
less portable and check the permissions before writing and set the permissions
after writing if this became an issue.

Returns:   0 write was successful
          -2 write failed, check errno for reason.
  ----------------------------------------------------------------------*/

int
adrbk_write(ab)
     AdrBk *ab;
{
    FILE                 *ab_stream;
    register AdrBk_Entry **a;
    register char        **a2;
    int                    l;
#ifdef	DOS
    void                  (*save_sigint)(int);
#else
    void                  (*save_sigint)(), (*save_sighup)();
#endif

#define INDENT 8

#ifdef	DOS
    save_sigint = signal(SIGINT, SIG_IGN);
#else
    save_sigint = (void (*)())signal(SIGINT, SIG_IGN);
    save_sighup = (void (*)())signal(SIGHUP, SIG_IGN);
#endif

    if((ab_stream = fopen(ab->temp_filename, "w")) == NULL) {
        (void)signal(SIGINT, save_sigint);
#ifndef	DOS
        (void)signal(SIGHUP, save_sighup);
#endif
        return(-2);
    }

    for(a = ab->book; *a != NULL; a++) {  
        if(fputs((*a)->nickname, ab_stream) == EOF)
          goto io_error;
        l = strlen((*a)->nickname) + INDENT; 
        putc('\t', ab_stream);
        if((*a)->fullname != NULL) {
            if(fputs((*a)->fullname, ab_stream) == EOF)
              goto io_error;
            l += strlen((*a)->fullname)+ INDENT; 
        }
        putc('\t', ab_stream);
        if((*a)->addr.addr != NULL || (*a)->addr.list != NULL) {
	    if((*a)->tag == Atom) {
	        /*----- Atom: just one address ----*/
	      	if(fputs((*a)->addr.addr, ab_stream) == EOF)
		  goto io_error;
	    } else {
	        /*----- List: a distribution list ------*/
		putc('(', ab_stream);
		for(a2 = (*a)->addr.list; *a2 != NULL; a2++){
		    if(a2 != (*a)->addr.list)
		      putc(',',ab_stream);
		    if(l + strlen(*a2) > 78 && l != INDENT) {
		        /*--- break up long lines ----*/
		        if(fputs("\n          ", ab_stream) == EOF)
			  goto io_error;
		        l = INDENT;
		      }
		    if(fputs(*a2, ab_stream) == EOF)
		      goto io_error;
		    l += strlen(*a2) + 1;
		}
		putc(')',ab_stream);
	    }
        }
        if((*a)->extra != NULL) {
            putc('\t', ab_stream);
            if(fputs((*a)->extra, ab_stream) == EOF)
              goto io_error;
        }
        putc('\n',ab_stream);
    }
    if(fclose(ab_stream) == EOF)
      goto io_error;
    if(unlink(ab->filename) < 0)
      goto io_error;
    if(rename(ab->temp_filename, ab->filename) < 0)
      goto io_error;

    (void)signal(SIGINT, save_sigint);
#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif
    return(0);


  io_error:
    (void)signal(SIGINT, save_sigint);
#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif
    return(-2);
}


    



#ifdef DEBUG
/*----------------------------------------------------------------------
    Dump the address book to the standard output for debugging 
  ----------------------------------------------------------------------*/
adrbk_splat(ab)
     AdrBk *ab;
{
    int l;
    char cmd[20], **a2;
    register AdrBk_Entry **a;

    printf("filename:      \"%s\"\n",ab->filename);
    printf("storage:        %x\n", ab->storage);
    printf("book:           %x\n", ab->book);
    printf("book_used:      %d\n", ab->book_used);
    printf("book_allocated: %d\n", ab->book_allocated);
    printf("------------------------\n");
    l = 0;
    for(a = ab->book; *a != NULL; a++) {
      if((*a)->tag == Atom) {
       printf("[%s]\n[%s]\n[%s]\n[%s]\n\n", (*a)->nickname,
               (*a)->fullname == NULL ? "NULL" : (*a)->fullname,
               (*a)->addr.addr== NULL ? "NULL" : (*a)->addr.addr,
               (*a)->extra    == NULL ? "NULL" : (*a)->extra);
     } else {
       printf("[%s]\n[%s]\n[%s]\n", (*a)->nickname,
               (*a)->fullname == NULL ? "NULL" : (*a)->fullname,
               (*a)->extra    == NULL ? "NULL" : (*a)->extra);
       for(a2 = (*a)->addr.list; *a2 != NULL; a2++) 
	 printf("     [%s]\n", *a2);
       printf("\n");
     }

        if(l++ > 5) {
            l = 0;
            printf("--more-(press return)-");
            fgets(cmd, sizeof(cmd), stdin);
        }
    }
}

#endif




    
