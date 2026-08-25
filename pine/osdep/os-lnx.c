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

 This contains most of Pine's interface to the local operating system
and hardware.  Hopefully this file, os-xxx.h and makefile.xxx are the
only ones that have to be modified for most ports.  Signals.c, ttyin.c,
and ttyout.c also have some dependencies.  See the doc/tech-notes for
notes on porting Pine to other platforms.  Here is a list of the functions
required for an implementation:


  File System Access
     can_access          -- See if a file can be accessed
     file_size           -- Return the number of bytes in the file
     is_writable_dir     -- Check to see if directory exists and is writable
     create_mail_dir     -- Make a directory
     rename_file         -- change name of a file
     build_path          -- Put together a file system path
     last_cmpnt          -- Returns pointer to last component of path
     expand_foldername   -- Expand a folder name to full path
     fnexpand            -- Do filename exansion for csh style "~"
     filter_filename     -- Make sure file name hasn't got weird chars
     disk_quota          -- Check the user's disk quota
     read_file           -- Read whole file into memory (for small files)
     create_tmpfile      -- Just like ANSI C tmpfile function
     temp_nam            -- Almost like common tempnam function
     fget_pos,fset_pos   -- Just like ANSI C fgetpos, fsetpos functions

  Abort
     coredump            -- Abort running Pine dumping core if possible

  System Name and Domain
     hostname            -- Figure out the system's host name, only
                              used internally in this file.
     getdomainnames      -- Figure out the system's domain name
     canonical_name      -- Returns canonical form of host name

  Job Control
     have_job_control    -- Returns 1 if job control exists
     stop_process        -- What to do to stop process when it's time to stop
			      (only used if have_job_control returns 1)

  System Error Messages (in case given one is a problem)
     error_description   -- Returns string describing error

  System Password and Accounts
     gcos_name           -- Parses full name from system, only used
			      locally in this file so if you don't use it you
			      don't need it
     get_system_login    -- Returns user's login name 
     get_system_fullname -- Returns user's full name 
     get_system_homedir  -- Returns user's home directoru
     get_system_passwd   -- Returns enscrypted password
     local_name_lookup   -- Get full name of user on system
     change_passwd       -- Calls system password changer

  MIME utilities
     mime_can_display    -- Can we display this type/subtype?
     gf_play_sound       -- Play audio MIME if possible

  Other stuff
     srandom             -- Dummy srandom if you don't have this function

  ====*/


#include "headers.h"



/*----------------------------------------------------------------------
       Check if we can access a file in a given way

   Args: file      -- The file to check
         mode      -- The mode ala the access() system call, see ACCESS_EXISTS
                      and friends in pine.h.

 Result: returns 0 if the user can access the file according to the mode,
         -1 if he can't (and errno is set).
 ----*/
int
can_access(file, mode)
    char *file; 
    int   mode;
{
    return(access(file, mode));
}



/*----------------------------------------------------------------------
      Return the number of bytes in given file

    Args: file -- file name

  Result: the number of bytes in the file is returned or
          -1 on error, in which case errno is valid
 ----*/
long
file_size(file)
    char *file;
{
    struct stat buffer;

    if (stat(file, &buffer) != 0)
        return(-1L);
    return(buffer.st_size);
}



/*----------------------------------------------------------------------
      Check to see if a directory exists and is writable by us

   Args: dir -- directory name

 Result:       returns 0 if it exists and is writable
                       1 if it is a directory, but is not writable
                       2 if it is not a directory
                       3 it doesn't exist.
  ----*/
int
is_writable_dir(dir)
    char *dir;
{
    struct stat sb;

    if(stat(dir, &sb) < 0)
      /*--- It doesn't exist ---*/
      return(3);

    if(!S_ISDIR(sb.st_mode))
      /*---- it's not a directory ---*/
      return(2);

    if(can_access(dir, 07))
      return(1);
    else
      return(0);
}



/*----------------------------------------------------------------------
      Create the mail subdirectory.

  Args: dir -- Name of the directory to create
 
 Result: Directory is created.  Returns 0 on success, else -1 on error
	 and errno is valid.
  ----*/
int
create_mail_dir(dir)
    char *dir;
{
    if(mkdir(dir, 0700) < 0)
      return(-1);

    (void)chmod(dir, 0700);
    /* Some systems need this, on others we don't care if it fails */
    (void)chown(dir, getuid(), getgid());
    return(0);
}



/*----------------------------------------------------------------------
      Rename a file

  Args: tmpfname -- Old name of file
        fname    -- New name of file
 
 Result: File is renamed.  Returns 0 on success, else -1 on error
	 and errno is valid.
  ----*/
int
rename_file(tmpfname, fname)
    char *tmpfname, *fname;
{
    return(rename(tmpfname, fname));
}



/*----------------------------------------------------------------------
      Paste together two pieces of a file name path

  Args: pathbuf      -- Put the result here
        pathbuf_size -- Size of pathbuf, including the terminating NUL
        first_part   -- of path name
        second_part  -- of path name

 Result: New path is in pathbuf, truncated to fit if the combined pieces
	 don't -- first_part/second_part can both be arbitrarily long
	 (e.g. an attacker-influenced MIME attachment filename), while
	 pathbuf is a caller-fixed-size buffer. Note that we don't have
	 to check for /'s at end of first_part and beginning of
	 second_part since multiple slashes are ok.
  ----*/
void
build_path(pathbuf, pathbuf_size, first_part, second_part)
    char *pathbuf, *first_part, *second_part;
    size_t pathbuf_size;
{
    size_t len;

    /* No snprintf() here -- this fragment is also assembled into
     * os-bsd.c (see pine/makefile.bsd), and 2.11BSD's libc predates
     * snprintf's standardization (same finding as pico/random.c's
     * fix, TODO-pdp11-done.md). Truncating concatenation done by hand
     * instead, since first_part/second_part can be arbitrarily long
     * (e.g. attacker-influenced MIME attachment filenames) and must
     * never overflow the caller-fixed-size pathbuf. */
    if(first_part == NULL)
      strncpy(pathbuf, second_part, pathbuf_size - 1);
    else{
	strncpy(pathbuf, first_part, pathbuf_size - 1);
	pathbuf[pathbuf_size - 1] = '\0';
	len = strlen(pathbuf);
	if(len < pathbuf_size - 1){
	    pathbuf[len++] = '/';
	    pathbuf[len] = '\0';
	    strncpy(pathbuf + len, second_part, pathbuf_size - 1 - len);
	}
    }

    pathbuf[pathbuf_size - 1] = '\0';
}



/*----------------------------------------------------------------------
      Return pointer to last component of pathname.

  Args: filename     -- The pathname.
 
 Result: Returned pointer points to last component in the input argument.
  ----*/
char *
last_cmpnt(filename)
    char *filename;
{
    register char *p;

    for(p = filename + strlen(filename) -1; p > filename; p--){
        if(*p == '/' && *(p+1) != '\0')
          return(p+1);
    }
    return(p);
}



/*----------------------------------------------------------------------
      Expand a folder name, taking account of the folders_dir and `~'.

  Args: filename -- The name of the file that is the folder
 
 Result: The folder name is expanded in place.  
         Returns 0 and queues status message if unsuccessful.
         Input string is overwritten with expanded name.
         Returns 1 if successful.

BUG should limit length to MAXPATH
  ----*/
int
expand_foldername(filename)
    char *filename;
{
    char         temp_filename[MAXPATH+1];


    if(filename[0] != '{') {
        char *ill = filter_filename(filename);
        if(ill != NULL) {
            q_status_message1(1, 1,3, "%s in folder name", ill);
            return(0);
        }
    }

    strcpy(temp_filename, filename);
    if(strucmp(temp_filename, "inbox") == 0) {
        strcpy(filename, ps_global->VAR_INBOX_PATH == NULL ? "inbox" :
               ps_global->VAR_INBOX_PATH);
    } else if(temp_filename[0] == '{') {
        strcpy(filename, temp_filename);
    } else if(temp_filename[0] == '*') {
        strcpy(filename, temp_filename);
    } else if (temp_filename[0] == '~'){
        if(fnexpand(temp_filename, sizeof(temp_filename)) == NULL) {
            char *p = strindex(temp_filename, '/');
    	    if(p != NULL)
    	      *p = '\0';
    	    q_status_message1(1, 1,2,
                    "Error expanding folder name: \"%s\" unknown user",
    	       temp_filename);
    	    return(0);
        }
        strcpy(filename, temp_filename);
    } else if(temp_filename[0] == '/') {
        strcpy(filename, temp_filename);
    } else {
        sprintf(filename,"%s/%s", ps_global->folders_dir, temp_filename);
    }
    
    return(1);
}



/*----------------------------------------------------------------------
       Expand the ~ in a file ala the csh (as home directory)

   Args: buf --  The filename to expand (nothing happens unless begins with ~)
         len --  The length of the buffer passed in (expansion is in place)

 Result: Expanded string is returned using same storage as passed in.
         If expansion fails, NULL is returned
 ----*/
char *
fnexpand(buf, len)
    char *buf;
    int len;
{
    struct passwd *pw;
    register char *x,*y;
    char name[20];
    
    if(*buf == '~') {
			/* name is fixed-size -- x still walks the
			   whole ~username to find where it ends (for
			   x - buf below), but stop writing into name
			   once it's full so an oversized username
			   can't overflow it; getpwnam() on the
			   resulting truncated name will just fail to
			   match, same as any other unknown username */
        for(x = buf+1, y = name; *x != '/' && *x != '\0'; x++)
          if (y < name + sizeof(name) - 1) *y++ = *x;
        *y = '\0';
        if(x == buf + 1) 
          pw = getpwuid(getuid());
        else
          pw = getpwnam(name);
        if(pw == NULL)
          return((char *)NULL);
        if(strlen(pw->pw_dir) + strlen(buf) > (size_t)len) {
          return((char *)NULL);
        }
        rplstr(buf, x - buf, pw->pw_dir);
    }
    return(len ? buf : (char *)NULL);
}



/*----------------------------------------------------------------------
    Filter file names for strange characters

   Args:  file  -- the file name to check
 
 Result: Returns NULL if file name is OK
         Returns formatted error message if it is not
  ----*/
char *
filter_filename(file)
    char *file;
{
#ifdef ALLOW_WEIRD
    static char illegal[] = {'\177', '\0'};
#else
    static char illegal[] = {'"', '#', '$', '%', '&', '\'','(', ')','*',
                          ',', ':', ';', '<', '=', '>', '?', '[', ']',
                          '\\', '^', '|', '\177', '\0'};
#endif
    static char error[MAXPATH+100];
    char ill_file[MAXPATH+1], *ill_char, *ptr, e2[10];

    ptr = file;
    while (*ptr == ' ') ptr++;	/* leading spaces GONE! */

    if(*ptr == '*')
      ptr++; /* Allow * at beginning for news groups */

    while(*ptr && *ptr > ' ' && strindex(illegal, *ptr) == 0)
      ptr++;

    if(*ptr != '\0') {
        if(*ptr == ' ') {
            ill_char = "<space>";
        } else if(*ptr == '\n') {
            ill_char = "<newline>";
        } else if(*ptr == '\r') {
            ill_char = "<carriage return>";
        } else if(*ptr == '\t') {
    	    ill_char = "<tab>";
        } else if(*ptr < ' ') {
            sprintf(e2, "control-%c", *ptr + '@');
            ill_char = e2;
        } else if (*ptr == '\177') {
    	    ill_char = "<del>";
        } else {
    	    e2[0] = *ptr;
    	    e2[1] = '\0';
    	    ill_char = e2;
        }
        if(ptr != file) {
            strncpy(ill_file, file, ptr - file);
            ill_file[ptr - file] = '\0';
            sprintf(error,"Character \"%s\" after \"%s\" not allowed",ill_char,
                    ill_file);
        } else {
            sprintf(error,
                    "First character, \"%s\", not allowed",
                    ill_char);
        }
            
        return(error);
    }
    return((char *)NULL);
}




/*----------------------------------------------------------------------
    Read whole file into memory

  Args: filename -- path name of file to read

  Result: Returns pointer to malloced memory with the contents of the file
          or NULL

This won't work very well if the file has NULLs in it and is mostly
intended for fairly small text files.
 ----*/
char *
read_file(filename)
    char *filename;
{
    int         fd;
    struct stat statbuf;
    char       *buf;

    fd = open(filename, O_RDONLY);
    if(fd < 0)
      return((char *)NULL);

    fstat(fd, &statbuf);

    buf = fs_get(statbuf.st_size + 1);

    /*
     * On some systems might have to loop here, if one read isn't guaranteed
     * to get the whole thing.
     */
    if(read(fd, buf, statbuf.st_size) != statbuf.st_size) {
        close(fd);
        return((char *)NULL);
    }
    close(fd);

    buf[statbuf.st_size]= '\0';

    return(buf);
}



/*----------------------------------------------------------------------
   Create a temporary file, the name of which we don't care about 
and that goes away when it is closed.  Just like ANSI C tmpfile.
  ----*/
FILE  *
create_tmpfile()
{
    return(tmpfile());
}



/*
 * This routine is derived from BSD4.3 code,
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 */
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)tmpnam.c	4.5 (Berkeley) 6/27/88";
#endif /* LIBC_SCCS and not lint */
/*----------------------------------------------------------------------
      Return a unique file name in a given directory.  This is not quite
      the same as the usual tempnam() function, though it is very similar.
      We want it to use the TMP environment variable only if dir is NULL,
      instead of using TMP regardless if it is set.

  Args: dir      -- The directory to create the name in
        prefix   -- Prefix of the name
 
 Result: Malloc'd string equal to new name is returned.  It must be free'd
	 by the caller.  Returns the string on success and NULL on failure.
  ----*/
static char *our_mktemp(char *);

char *
temp_nam(char *dir, char *prefix)
{
    struct stat buf;
    char *f, *name;

    if (!(name = malloc((unsigned int)MAXPATHLEN)))
        return((char *)NULL);

    if (!dir && (f = getenv("TMPDIR")) && !stat(f, &buf) &&
                         S_ISDIR(buf.st_mode) &&
			 !can_access(f, WRITE_ACCESS|EXECUTE_ACCESS)) {
        (void)strcpy(name, f);
        goto done;
    }

    if (dir && !stat(dir, &buf) &&
                         S_ISDIR(buf.st_mode) &&
	                 !can_access(dir, WRITE_ACCESS|EXECUTE_ACCESS)) {
        (void)strcpy(name, dir);
        goto done;
    }

#ifndef P_tmpdir
#define	P_tmpdir	"/usr/tmp"
#endif
    if (!stat(P_tmpdir, &buf) &&
                         S_ISDIR(buf.st_mode) &&
			 !can_access(P_tmpdir, WRITE_ACCESS|EXECUTE_ACCESS)) {
        (void)strcpy(name, P_tmpdir);
        goto done;
    }

    if (!stat("/tmp", &buf) &&
                         S_ISDIR(buf.st_mode) &&
			 !can_access("/tmp", WRITE_ACCESS|EXECUTE_ACCESS)) {
        (void)strcpy(name, "/tmp");
        goto done;
    }
    return((char *)NULL);

done:
    (void)strcat(name, "/");

    if (prefix)
        (void)strcat(name, prefix);
    (void)strcat(name, "XXXXXX");
    return(our_mktemp(name));
}


/*
 * This routine is derived from BSD4.3 code,
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * We use this instead of mktemp() since we know of at least one stupid
 * mktemp() (AIX3.2) which breaks things.
 */
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mktemp.c	5.7 (Berkeley) 6/27/88";
#endif /* LIBC_SCCS and not lint */

static int
_gettemp(char *as)
{
	register char	*start, *trv;
	struct stat	sbuf;
	unsigned int	pid;

	pid = getpid();

	/* extra X's get set to 0's */
	for (trv = as; *trv; ++trv);
	while (*--trv == 'X') {
		*trv = (pid % 10) + '0';
		pid /= 10;
	}

	/*
	 * check for write permission on target directory; if you have
	 * six X's and you can't write the directory, this will run for
	 * a *very* long time.
	 */
	for (start = ++trv; trv > as && *trv != '/'; --trv);
	if (*trv == '/') {
		*trv = '\0';
		if (stat(as, &sbuf) || !S_ISDIR(sbuf.st_mode))
			return(0);
		*trv = '/';
	}
	else if (stat(".", &sbuf) == -1)
		return(0);

	for (;;) {
		if (stat(as, &sbuf))
			return(errno == ENOENT ? 1 : 0);

		/* tricky little algorithm for backward compatibility */
		for (trv = start;;) {
			if (!*trv)
				return(0);
			if (*trv == 'z')
				*trv++ = 'a';
			else {
				if (isdigit(*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/*NOTREACHED*/
}

static char *
our_mktemp(char *as)
{
	return(_gettemp(as) ? as : (char *)NULL);
}



/*----------------------------------------------------------------------
     Abort with a core dump
 ----*/
void
coredump()
{
    abort();
}




/*----------------------------------------------------------------------
       Get the current host and domain names

    Args: hostname   -- buffer to return the hostname in
          hsize      -- size of buffer above
          domainname -- buffer to return domain name in
          dsize      -- size of buffer above

  Result: The system host and domain names are returned. If the full host
          name is akbar.cac.washington.edu then the domainname is
          cac.washington.edu.

On Internet connected hosts this look up uses /etc/hosts and DNS to
figure all this out. On other less well connected machines some other
file may be read. If there is no notion of a domain name the domain
name may be left blank. On a PC where there really isn't a host name
this should return blank strings. The .pinerc will take care of
configuring the domain names. That is, this should only return the
native system's idea of what the names are if the system has such
a concept.
 ----*/
void
getdomainnames(hostname, hsize, domainname, dsize)
    char *hostname, *domainname;
    int   hsize, dsize;
{
    char           *dn, hname[MAX_ADDRESS+1];
    struct hostent *he;

    gethostname(hname, MAX_ADDRESS);

    he = gethostbyname(hname);

    if(he == NULL && strlen(hname) == 0) {
        strcpy(hostname, "");
    } else if(he == NULL) {
        strncpy(hostname, hname, hsize - 1);
    } else {
        strncpy(hostname, he->h_name, hsize-1);
    }
    hostname[hsize-1] = '\0';


    if((dn = strindex(hostname, '.')) != NULL) {
        strncpy(domainname, dn+1, dsize-1);
    } else {
        strncpy(domainname, hostname, dsize-1);
    }
    domainname[dsize-1] = '\0';
}



/*----------------------------------------------------------------------
       Return canonical form of host name ala c-client (UNIX version).

   Args: host      -- The host name

 Result: Canonical form, or input argument (worst case)
 ----*/
char *
canonical_name(char *host)
{
    struct hostent *hent;
    char hostname[MAILTMPLEN];
    char tmp[MAILTMPLEN];
    extern char *lcase(char *);
                                /* domain literal is easy */
    if (host[0] == '[' && host[(strlen (host))-1] == ']')
      return host;

    strcpy (hostname,host);       /* UNIX requires lowercase */
                                /* lookup name, return canonical form */
    return (hent = gethostbyname (lcase (strcpy (tmp,host)))) ?
      hent->h_name : host;
}



/*----------------------------------------------------------------------
     This routine returns 1 if job control is available.  Note, thiis
     could be some type of fake job control.  It doesn't have to be
     real BSD-style job control.
  ----*/
int
have_job_control()
{
    return 1;
}


/*----------------------------------------------------------------------
    If we don't have job control, this routine is never called.
  ----*/
int
stop_process()
{
    kill(0, SIGSTOP);
    return(0);
}



/*----------------------------------------------------------------------
       Return string describing the error

   Args: errnumber -- The system error number (errno)

 Result:  long string describing the error is returned
  ----*/
char *
error_description(errnumber)
    int errnumber;
{
    static char buffer[50];

    strncpy(buffer, strerror(errnumber), sizeof(buffer));

    return ( (char *) buffer);
}



/*----------------------------------------------------------------------
  This collection of routines looks up the login name and password on the
system. For things like PC's it's OK for these to return NULL since there
is no system login. Other code will figure out who the user actually is.
  ----*/
static struct passwd *unix_pwd = NULL;

/*----------------------------------------------------------------------
      Pull the name out of the gcos field if we have that sort of /etc/passwd

   Args: gcos_field --  The long name or GCOS field to be parsed
         logname    --  Replaces occurances of & with logname string

 Result: returns pointer to buffer with name
  ----*/
static char *
gcos_name(char *gcos_field, char *logname)
{
    static char fullname[MAX_FULLNAME+1];
    register char *fncp, *gcoscp, *lncp, *end;

    /* full name is all chars up to first ',' (or whole gcos, if no ',') */
    /* replace any & with logname in upper case */

    for(fncp = fullname, gcoscp= gcos_field, end = fullname + MAX_FULLNAME - 1;
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
}


char *
get_system_login()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return((char *)NULL);
    }
    return(unix_pwd->pw_name);
}
char *
get_system_fullname()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return((char *)NULL);
    }
    return(gcos_name(unix_pwd->pw_gecos, unix_pwd->pw_name));
}
char *
get_system_homedir()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid());
        if(unix_pwd == NULL)
          return((char *)NULL);
    }
    return(unix_pwd->pw_dir);
}
char *
get_system_passwd()
{
    if(unix_pwd == NULL) {
        unix_pwd = getpwuid(getuid()); /* Always do real lookup in case */
                                       /* password is changed and kb locked */
        if(unix_pwd == NULL)
          return((char *)NULL);
    }
    return(unix_pwd->pw_passwd);
}


/*----------------------------------------------------------------------
      Look up a userid on the local system and return rfc822 address

 Args: name  -- possible login name on local system

 Result: returns NULL or pointer to static string rfc822 address.
  ----*/
char *
local_name_lookup(char *name)
{
    struct passwd *pw;
    static char    buf[100];

    pw = getpwnam(name);
    if(pw == NULL)
      return((char *)NULL);

    strcpy(buf, gcos_name(pw->pw_gecos, name));

    return(buf);
}



/*----------------------------------------------------------------------
       Call the system to change the passwd
 
It would be nice to talk to the passwd program via a pipe or ptty so the
user interface could be consistent, but we can't count on the the prompts
and responses from the passwd program to be regular so we just let the user 
type at the passwd program with some screen space, hope he doesn't scroll 
off the top and repaint when he's done.
 ----*/        
int
change_passwd()
{
    char cmd_buf[100];

    int i;
    for(i = 1; i < ps_global->ttyo->screen_rows; i ++) {
        MoveCursor(i, 0);
        CleartoEOLN();
    }

    MoveCursor(5, 0);
    fflush(stdout);

    Raw(0);
    strcpy(cmd_buf, "passwd");
    system(cmd_buf);
    sleep(3);
    Raw(1);
    init_username(ps_global);
    return(0);
}



/*----------------------------------------------------------------------
       Can we display this type/subtype?

   Args: type      -- the MIME type to check
         subtype   -- the MIME subtype
         params    -- parameters

 Result: returns 1 if the type is displayable, 0 otherwise.
 ----*/
int
mime_can_display(type, subtype, params)
    int type;
    char *subtype;
    PARAMETER *params;
{
  switch(type) {
    case TYPETEXT:
        /* we always try to display text now */
	return 1;

    case TYPEAUDIO:
     return(0);

    case TYPEIMAGE:
      /* no image-viewer support in this build -- see TODO-pdp11-done.md */
      return(0);


    case TYPEAPPLICATION:
      return(0);

    case TYPEMULTIPART:
      return(1);

    case TYPEMESSAGE:
      return(1);

    default:
      return(0);
  }
}



/*----------------------------------------------------------------------
       We don't know how to play audio.
 ----*/
void
gf_play_sound(f, c, flg)
    FILTER_S *f;
    int       c, flg;
{
}



/*----------------------------------------------------------------------
   This is just a call to the ANSI C fgetpos function.
  ----*/
int
fget_pos(FILE *stream, fpos_t *ptr)
{
    return(fgetpos(stream, ptr));
}


/*----------------------------------------------------------------------
   This is just a call to the ANSI C fsetpos function.
  ----*/
int
fset_pos(FILE *stream, fpos_t *ptr)
{
    return(fsetpos(stream, ptr));
}


