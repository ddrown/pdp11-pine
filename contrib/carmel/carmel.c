/*----------------------------------------------------------------------

    T H E   C A R M E L   M A I L   F I L E   D R I V E R

 Author(s):   Laurence Lundblade
              Baha'i World Centre
              Data Processing
              Haifa, Israel
	      Internet: lgl@cac.washington.edu or laurence@bwc.org
              September 1992


The Carmel mail file stores messages in individual files and
implements folders or mailboxes with index files that contain
references to the files a nd a full c-client envelope in an easily
parsed form. It was written as a needed part of the pod mail file
driver with hopes that it might be useful otherwise some day. It has
only been run with the pod driver.

Advantages over Berkeley format and driver:
  + Opening mail folder is very fast
  + Expunge is fast
  + Check point is very fast
  + Memory usage is much lower
  + Search of message headers is fast
Disadvantages:
  - Fetching a large message is slow
  - Searching the message bodies is slow
  - Sorting the mailbox is slow

         Index File Format
         -----------------

The first line of the file is always:
  "\254\312--CARMEL-MAIL-FILE-INDEX--\n"

It is followed by index entries which are of the format:
---START-OF-MESSAGE---\007\001nnnnnnnnnn     
Ufrads______________________________
Zxxxxxx
D..... <Many fields here, labeled by the first letter in the line>
       <Fields may be repeated>


The index is almost an ASCII file. With the first version of this
driver it is not advisable to edit this file, lest the byte counts get
disrupted.  In the future it will be editable. The file starts with
two binary bytes so the file(1) utility will report it as "data" to
discourage would be hackers from messing with it. The ^G is also in
each index entry for the same reason. If you are reading this file you
probably know enough to edit the index file without destroying it.
Other parts of the format are designed for the easiest possible
parsing. The idea was to have a file format that was reasonable to
fiddle for debugging, to discourage inexperienced folks for fiddling
it and to be efficient for use by the program.


      Routines and data structures
      ----------------------------

C-CLIENT INTERFACE FUCTIONS
  carmel_valid       - check to see if a mailbox is valid for carmel mail files
  carmel_isvalid     - actual work of checking
  carmel_file        - generate path names for checking carmel mailboxes
  carmel_find        - generate list of carmel mailboxes
  carmel_sift_files  - select mailboxes out of list, used with scandir
  carmel_find_bboars - dummy routine, doesn't do anything

  carmel_open        - initial phase of opening a mailbox
  carmel_open2       - real work of opening a mailbox, shared with pod driver
  carmel_close       - close a mail stream

  carmel_fetchfast   - fetch "fast" message info, noop for this driver
  carmel_fetchflags  - fetch the flags, a noop for this driver
  carmel_fetchstructure - fetch and envelope and possibly body  

  carmel_fetchheader - fetch the text header of the message
  carmel_fetchtext   - fetch the text of the message (no header included)
  carmel_fetchbody   - fetch the text of a body part of a message

  carmel_setflag     - Set a flag for a message sequence
  carmel_clearflag   - Clear a flag for a message sequence
  
  carmel_search      - Invoke the search facilities

  carmel_ping        - Check for new mail and see if still alive
  carmel_check       - Checkpoint the message statuses to the disk
  carmel_expunge     - Delete all the messages marked for delete 
  
  carmel_copy        - Copy a message to another mailbox
  carmel_move        - Move a message to another mailbox
  
  carmel_gc          - Garbage collection, a noop for this driver
  carmel_cache       - The required c-client cache handler, doesn't do much

  carmel_append      - Append a message to a mail folder

SUPPORT FUNCTIONS
  carmel_bodystruct  - Fetch the body structure for a carmel message
  carmel_parse_address - Parse the address out of a carmel index
  carmel_parse_addr_field - Parse individual address field out of carmel index
  carmel_write_index - Write an entry into a carmel index
  carmel_index_address - Write an address into a carmel index

  carmel_readmsg     - Read a message file into memory, header text or both
  carmel_spool_mail  - Get new mail out of spoold mail file
  carmel_copy_msg_file - Make copy of messsage file when copying (unused)
  
  carmel_lock        - Lock a carmel index for read or write
  carmel_unlock      - Unlock a carmel index
  carmel_update_lock - Touch lock mod time, marking it as active
  carmel_bezerk_lock - Lock the spool mail file Berkeley style
  carmel_bezerk_unlock - Unlock the spool mail file
  
  carmel_check_dir   - Check that directory exists and is writeable 
  carmel_new_data_file - Get file number for new message file
  carmel_calc_paths  - Calculate path names for carmel driver

  carmel_parse_bezerk_status - Parse the "Status: OR" field in mail headers
  carmel_getflags    - Turn the named flags into a bit mask
  carmel_new_mc      - Get pointer to new MESSAGECACHE, allocating if needed

  carmel_append2     - The real work of append a message to a mailbox
  
  carmel_search-*    - A bunch of search support routines
 
  month_abbrev2      - Returns three letter month abbreviation given a name
  carmel_rfc822_date - Parse a date string, into MESSAGECACHE structure
  carmel_date2string - Generate a date string from MESSAGECACHE
  carmel_parse_date  - Parse date out of a carmel index
  next_num           - Called to parse dates
 
  strucmp2           - Case insensitive strcmp()
  struncmp2          - Case insensitive strncmp()
  
 ----------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "osdep.h"
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "mail.h"
#include "carmel.h"
#include "rfc822.h"
#include "misc.h"


static int   carmel_sift_files();
static BODY *carmel_bodystruct();
static ADDRESS *carmel_parse_address();
static char    *carmel_parse_addr_field();
static int      carmel_index_address();
static void     carmel_spool_mail();
static int      carmel_copy_msg_file();
static int      carmel_bezerk_lock();
static void     carmel_bezerk_unlock();
static int      carmel_new_data_file();
static char    *carmel_calc_paths();
static short    carmel_getflags();
static MESSAGECACHE *carmel_new_mc();
static char	carmel_search_all();
static char	carmel_search_answered();
static char	carmel_search_deleted();
static char	carmel_search_flagged();
static char	carmel_search_keyword();
static char	carmel_search_new();
static char	carmel_search_old();
static char	carmel_search_recent();
static char	carmel_search_seen();
static char	carmel_search_unanswered();
static char	carmel_search_undeleted();
static char	carmel_search_unflagged();
static char	carmel_search_unkeyword();
static char	carmel_search_unseen();
static char	carmel_search_before();
static char	carmel_search_on();
static char	carmel_search_since();
static unsigned long carmel_msgdate();
static char	carmel_search_body();
static char	carmel_search_subject();
static char	carmel_search_text();
static char	carmel_search_bcc();
static char	carmel_search_cc();
static char	carmel_search_from();
static char	carmel_search_to();
typedef char (*search_t)  ();
static search_t carmel_search_date();
static search_t carmel_search_flag();
static search_t carmel_search_string();
static void     carmel_date2string();
static void     carmel_parse_date();
static int      next_num();



/*------ Driver dispatch used by MAIL -------*/

DRIVER carmeldriver = {
  (DRIVER *) NIL,		/* next driver */
  carmel_valid,			/* mailbox is valid for us */
  carmel_find,			/* find mailboxes */
  carmel_find_bboards,		/* find bboards */
  carmel_open,			/* open mailbox */
  carmel_close,			/* close mailbox */
  carmel_fetchfast,		/* fetch message "fast" attributes */
  carmel_fetchflags,		/* fetch message flags */
  carmel_fetchstructure,	/* fetch message envelopes */
  carmel_fetchheader,		/* fetch message header only */
  carmel_fetchtext,		/* fetch message body only */
  carmel_fetchbody,		/* fetch message body section */
  carmel_setflag,		/* set message flag */
  carmel_clearflag,		/* clear message flag */
  carmel_search,		/* search for message based on criteria */
  carmel_ping,			/* ping mailbox to see if still alive */
  carmel_check,			/* check for new messages */
  carmel_expunge,		/* expunge deleted messages */
  carmel_copy,			/* copy messages to another mailbox */
  carmel_move,			/* move messages to another mailbox */
  carmel_gc,			/* garbage collect stream */
  carmel_append,                /* Append message to a mailbox */
  NULL,                 /* no routines for create, delete and rename yet */
  NULL, 
  NULL,
  "carmel"
};



/*-- Some string constants used in carmel indexes --*/
char *carmel_s_o_m     = "---START-OF-MESSAGE---";
int   carmel_s_o_m_len = 22;
char *carmel_s_o_f     =   "\254\312--CARMEL-MAIL-FILE-INDEX--\n";



/*-- Buffers used for various reasons, also used by pod driver --*/
char carmel_20k_buf[20000], carmel_path_buf[CARMEL_PATHBUF_SIZE],
     carmel_error_buf[200];



/*----------------------------------------------------------------------
    Carmel mail validate mailbox

Args: - mailbox name

Returns: our driver if name is valid, otherwise calls valid in next driver
 ---*/

DRIVER *carmel_valid (name)
	char *name;
{
    return carmel_isvalid (name) ? &carmeldriver :
      (carmeldriver.next ? (*carmeldriver.next->valid) (name) : NIL);
}



/*----------------------------------------------------------------------
  Open the mailbox and see if it's the correct format

Args: name -- name of the mailbox, not fully qualified path

Returns: 0 if is is not valid, 1 if it is valid

The file must be a regular file and start with the string in the
variable carmel_s_o_f. It has a magic number of sorts
  ----*/
int
carmel_isvalid (name)
	char *name;
{
    char tmp[MAILTMPLEN];
    struct stat sbuf;
    int    fd;
      			    /* if file, get its status */
    if(*name == '{')
      return(0);  /* Check for imap folder*/
  
    carmel_file(tmp, name);
    if(stat(tmp, &sbuf) < 0)
      return(0);

    if(!S_ISREG(sbuf.st_mode))
      return(0);

    fd = open(carmel_path_buf, O_RDONLY);
    if(fd < 0)
      return(0);

    if(read(fd, carmel_20k_buf, 200) <= 0)
      return(0);

    close(fd);

    if(strncmp(carmel_20k_buf, carmel_s_o_f, strlen(carmel_s_o_f)))
       return(0);

    return(1);
}



/*----------------------------------------------------------------------
   Carmel mail build file name for Carmel index files

Args: destination string
        name   - name of folder to build name for

Returns: the absolute path of the carmel index in given buffer

This functions and the next one know where index files or mail folders are

This needs testing
 ----*/

char *
carmel_file(dst, name)
     char *dst;
     char *name;
{
    if (*name == '/')      /* absolute path? */
      strcpy(dst, name);
    else 		         /* relative path */
      sprintf(dst, "%s/%s/%s.cx", getpwuid (geteuid ())->pw_dir,
	      CARMEL_DIR,name);
    return dst;
}



/*----------------------------------------------------------------------
   Carmel mail find list of mailboxes

Args:  stream -- mail stream to find mailboxes for
       pat    -- wildcard pattern to match (currently unused)

Returns nothing, the results are passed back by calls to mm_log

This needs testing
 ----*/

void 
carmel_find (stream, pat)
    MAILSTREAM *stream;
    char *pat;
{
    char tmp[MAILTMPLEN];
    struct direct **namelist, **n;
    int num;

    sprintf(tmp, "%s/%s", LOCAL->home_dir, CARMEL_DIR);
    num = scandir(tmp, &namelist, carmel_sift_files, NULL);

   for(n = namelist; num > 0; num--, n++) {
	mm_mailbox((*n)->d_name);
	free(*n); 
    }
    free(namelist); 
}



/*----------------------------------------------------------------------
   This is used by scandir to determine which files in the directory
 are treated as mail files
  ----*/
static int
carmel_sift_files(dir)
     struct direct *dir;
{
    if(dir->d_name[0] == '.')
      return(0);
    else
      return(1);
}



/*----------------------------------------------------------------------
  Carmel mail find list of bboards; always NULL, no bboards 
 ----*/
void carmel_find_bboards (stream,pat)
	MAILSTREAM *stream;
	char *pat;
{
  /* Always a no-op, Carmel file format doesn't do news */
}



/*----------------------------------------------------------------------
  Open a carmel mail folder/stream

Args: stream -- stream to by fully opened

Returns: it's argument or NULL of the open fails

This needs testing and more code, see pod_open for examples
  ----*/

MAILSTREAM *
carmel_open (stream)
	MAILSTREAM *stream;
{
    char tmp[MAILTMPLEN];
    struct hostent *host_name;

    /* close old file if stream being recycled */
    if (LOCAL) {
        carmel_close (stream);		/* dump and save the changes */
        stream->dtb = &carmeldriver;	/* reattach this driver */
        mail_free_cache (stream);	/* clean up cache */
    }

    mailcache = carmel_cache;

    /* canonicalize the stream mailbox name and save it */
    carmel_file (tmp,stream->mailbox);
    if(!strcmp (tmp, stream->mailbox)) {
        fs_give ((void **) &stream->mailbox);
        stream->mailbox = cpystr (tmp);
    }

    /* Allocate local stream */
    stream->local = fs_get (sizeof (CARMELLOCAL));

    stream->readonly = 1; /* Read-only till more work is done */

    LOCAL->msg_buf             = NULL;
    LOCAL->msg_buf_size        = 0;
    LOCAL->buffered_file       = NULL;
    LOCAL->msg_buf_text_start  = NULL;
    LOCAL->msg_buf_text_offset = 0;
    stream->msgno              = -1;
    stream->env                = NULL;
    stream->body               = NULL;
    stream->scache             = 1;
    LOCAL->calc_paths          = carmel_calc_paths;
    LOCAL->aux_copy            = NULL;
    LOCAL->new_file_on_copy    = 1;

    gethostname(tmp, MAILTMPLEN); /* get local host name */
    LOCAL->host = cpystr ((host_name = gethostbyname (tmp)) ?
                            host_name->h_name : tmp);
  
    if(carmel_open2(stream, (*(LOCAL->calc_paths))(LOCAL->home_dir,
						   stream->mailbox)) < 0)
      return(NULL);

    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,stream->recent);

    return(stream);
}



/*----------------------------------------------------------------------
    Do the real work of opening a Carmel folder. 

Args: stream -- The mail stream being opened
      file   -- The actual Carmel index file

Returns: -1 if the open fails
          0 if it succeeds

This is shared between the Carmel driver and the Pod driver.

Here, the status, size and date (fast info) of a message entry
is read out of the index file into the MESSAGECACHE structures.
To make the reading efficient these items are at the beginning 
of each entry and there is a byte offset to the next entry.
If the byte offset is wrong (as detected by looking for the
start of message string) then the index is read line by line
until it synchs up. This allows manual editing of the index.
However, the first two lines of an index entry cannot be
edited because mail_check() writes them in place. If these
lines have been edited it is detected here and the folder is
deemed corrupt.
  ---*/
carmel_open2(stream, file)
     MAILSTREAM *stream;
     char       *file;
{
    int           found;
    long          nmsgs, file_pos, next, recent, last_line_read;
    MESSAGECACHE *mc;
    struct stat   sb;

    if(stat(file, &sb) < 0) {
	sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
  	mm_log(carmel_error_buf, ERROR);
        return(-1);
    }

    LOCAL->index_size = sb.st_size;
    
    LOCAL->index_stream = fopen(file, stream->readonly ? "r" : "r+");
    if(LOCAL->index_stream == NULL) {
	sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
  	mm_log(carmel_error_buf, ERROR);
        return(-1);
    }

    recent            = 0;
    nmsgs             = 0;
    LOCAL->cache_size = 0;
    LOCAL->mc_blocks  = NULL;

    /*---- Read line with magic number, which we already checked ------*/
    if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),LOCAL->index_stream)==NULL)
      goto done_reading;

    file_pos = ftell(LOCAL->index_stream);  
    if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),LOCAL->index_stream) ==NULL)
      goto done_reading;
    if(strncmp(carmel_20k_buf, carmel_s_o_m, carmel_s_o_m_len) != 0)
      goto bomb;

    while(1) {
	if(strlen(carmel_20k_buf) != carmel_s_o_m_len + 13)
	  goto bomb;

        nmsgs++;
        next      = atol(carmel_20k_buf+24);
	mc        = carmel_new_mc(stream, nmsgs);
        mc->data1 = file_pos;

	/*-- Get the status line, It must be the first line in the entry ----*/
        if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),
		 LOCAL->index_stream) == NULL)
	  goto done_reading;
	if(*carmel_20k_buf != 'U' || strlen(carmel_20k_buf) != 35)
	  goto bomb; /* Invalid format! */
        mc->flagged  = carmel_20k_buf[1] == 'F';
        mc->answered = carmel_20k_buf[3] == 'A';
        mc->deleted  = carmel_20k_buf[4] == 'D';
        mc->seen     = carmel_20k_buf[5] == 'S';
        mc->recent   = 0;

        /*---- Get the rest of the other interesting entries -----*/
        found = 0;
        while(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),
		    LOCAL->index_stream) != NULL &&
	      found < 3 &&
	      strncmp(carmel_20k_buf, carmel_s_o_m, carmel_s_o_m_len))
          if (*carmel_20k_buf == 'Z') {
              mc->rfc822_size = atol(carmel_20k_buf+1);
              found++;
          } else if(*carmel_20k_buf == 'D') {
	      carmel_parse_date(mc, carmel_20k_buf+1);
              found++;
          } else if(*carmel_20k_buf == 'P') {
	      mc->data2 = atoi(carmel_20k_buf+1);
	      found++;
	  }

	/*-------- Now find the next index entry ---------*/
	last_line_read = ftell(LOCAL->index_stream);
	file_pos += next;
        fseek(LOCAL->index_stream, file_pos, 0); /* try the offset first */
        if(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
		 LOCAL->index_stream) == NULL) 
	   break;
        if(strncmp(carmel_20k_buf, carmel_s_o_m, carmel_s_o_m_len) != 0){
	    /*-- Didn't match what was seeked to, back off and read lines --*/ 
            fseek(LOCAL->index_stream, last_line_read, 0);
	    do {
		file_pos = ftell(LOCAL->index_stream);
	        if(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
			 LOCAL->index_stream) == NULL)
	          goto done_reading;
	    } while(strncmp(carmel_20k_buf,carmel_s_o_m,carmel_s_o_m_len)!= 0);
	}
    }

    /* Bring in the new mail now */
    /*    mail_ping(stream); BUG... this needs work! */

  done_reading:
    stream->nmsgs  = nmsgs;
    stream->recent = recent;
    return(0);

  bomb:
    fclose(LOCAL->index_stream);
    mm_log("Mailbox is corrupt. Open failed", ERROR);
    return(-1);
}



/*----------------------------------------------------------------------
     Carmel mail close

Args: stream -- stream to close

 ----*/

void
carmel_close (stream)
	MAILSTREAM *stream;
{
    if (LOCAL) {			/* only if a file is open */
	carmel_check (stream);		/* dump final checkpoint */
	if(LOCAL->host)
	  fs_give ((void **) &LOCAL->host);
	if(LOCAL->msg_buf)
	  fs_give ((void **) &LOCAL->msg_buf);
				    /* nuke the local data */
	fs_give ((void **) &stream->local);
	stream->dtb = NIL;		/* log out the DTB */
    }
}



/*----------------------------------------------------------------------
    Carmel mail fetch fast information.

This is a no-op because the data is always available as it is read in to
the message cache blocks when the folder is opened
----*/
void carmel_fetchfast (stream,sequence)
	MAILSTREAM *stream;
	char *sequence;
{
    return;
}



/*----------------------------------------------------------------------
    Carmel mail fetch flags.

This is a no-op because the data is always available as it is read in to
the message cache blocks when the folder is opened
----*/
void carmel_fetchflags (stream,sequence)
	MAILSTREAM *stream;
	char *sequence;
{
    return;			/* no-op for local mail */
}



/*----------------------------------------------------------------------
  Carmel mail fetch message structure
 Args: stream -- stream to get structure for
       msgno  -- Message number to fetch data for
       body   -- Pointer to place to return body strucuture, may be NULL

If the request is the for the same msgno as last time, the saved copy
of the envelope and/or body structure is returned.

To get the envelope the Carmel index file itself must be read and parsed,
but this is fast because it is easy to parse (by design) and the amount of
data is small.

To get the body, the whole message is read into memory and then parsed
by routines called from here. Doing this for a large message can be slow,
so it is best not to request the body if it is not needed. (body == NULL)
 ----*/

ENVELOPE *
carmel_fetchstructure (stream, msgno, body)
	MAILSTREAM *stream;
	long        msgno;
	BODY      **body;
{
    MESSAGECACHE *mc;
    ENVELOPE    **env;
    BODY        **b;
  
    env = &stream->env;		
    b   = &stream->body;

    if (msgno != stream->msgno){
        /* flush old poop if a different message */
	mail_free_envelope (env);
	mail_free_body (b);
    }
    stream->msgno = msgno;

    mc = MC(msgno);

    if(*env == NULL) {
	*env = mail_newenvelope();
      
	fseek(LOCAL->index_stream, mc->data1, 0);
	fgets(carmel_20k_buf, sizeof(carmel_20k_buf), LOCAL->index_stream);
	if(strncmp(carmel_20k_buf, carmel_s_o_m, carmel_s_o_m_len))
	  return(NULL); /* Oh ooo */
      
	 carmel_date2string(carmel_20k_buf, mc);
	 (*env)->date = cpystr(carmel_20k_buf);
      
	 while(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
		     LOCAL->index_stream) != NULL &&
	       strncmp(carmel_20k_buf, carmel_s_o_m, carmel_s_o_m_len)) {
	     carmel_20k_buf[strlen(carmel_20k_buf) - 1] = '\0';
	     switch(*carmel_20k_buf) {
	       case 'F':
		 (*env)->from = carmel_parse_address(carmel_20k_buf,
						     (*env)->from,
						     LOCAL->host);
		 break;
	       case 'T':
		 (*env)->to = carmel_parse_address(carmel_20k_buf,
						   (*env)->to,
						   LOCAL->host);
		 break;
	       case 'C':
		 (*env)->cc = carmel_parse_address(carmel_20k_buf,
						   (*env)->cc,
						   LOCAL->host);
		 break;
	       case 'B':
		 (*env)->bcc = carmel_parse_address(carmel_20k_buf,
						    (*env)->bcc,
						    LOCAL->host);
		 break;
	       case 'E':
		 (*env)->sender = carmel_parse_address(carmel_20k_buf,
						       (*env)->sender,
						       LOCAL->host);
		 break;
	       case 'R':
		 (*env)->reply_to = carmel_parse_address(carmel_20k_buf,
							 (*env)->reply_to,
							 LOCAL->host);
		 break;
	       case 'H':
		 (*env)->return_path =carmel_parse_address(carmel_20k_buf,
							   (*env)->return_path,
							   LOCAL->host);
		 break;
	       case 'J':
		 (*env)->subject     = cpystr(carmel_20k_buf+1);
		 break;
	       case 'I':
		 (*env)->message_id  = cpystr(carmel_20k_buf+1);
		 break;
	       case 'L':
		 (*env)->in_reply_to = cpystr(carmel_20k_buf+1);
		 break;
	       case 'N':
		 (*env)->newsgroups  = cpystr(carmel_20k_buf+1);
		 break;
	       case 'r':
		 (*env)->remail      = cpystr(carmel_20k_buf+1);
		 break;
	       default:
		 break;
	     }
	 }
    }

    if(body != NULL) {
	if(*b == NULL) {
            /* Have to fetch the body structure too */
	    *b  = carmel_bodystruct(stream, mc);
	}
	*body = *b;
    }
  
    return(*env);    /* return the envelope */
}




/*----------------------------------------------------------------------
  Carmel mail fetch message header

Args: stream -- 
      msgno

Returns: pointer to text of mail header. Returned string is null terminated
         and consists of internel storage. It will be valid till the next
         call to mail_fetchheader() or mail_fetchtext(). An empty
         string is returned if the fetch fails.
 ----*/

char *
carmel_fetchheader (stream, msgno)
	MAILSTREAM *stream;
	long msgno;
{
    char          *header;
    MESSAGECACHE  *mc;

    mc     = MC(msgno);
    header = carmel_readmsg(stream, 1, 0, mc->data2);

    return(header == NULL ? "" : header);
}



/*----------------------------------------------------------------------
  Carmel mail fetch message text (only)

Args: stream -- 
      msgno

Returns: pointer to text of mail message. Returned string is null terminated
         and consists of internel storage. It will be valid till the next
         call to mail_fetchheader() or mail_fetchtext(). An empty
         string is returned if the fetch fails.
 ----*/

char *
carmel_fetchtext (stream, msgno)
	MAILSTREAM *stream;
	long msgno;
{
    MESSAGECACHE *mc;
    char         *m;

    mc = MC(msgno);

    if (!mc->seen) {		/* if message not seen before */
        mc->seen = T;		/* mark as seen */
        LOCAL->dirty = T;	/* and that stream is now dirty */
    }

    m = carmel_readmsg(stream, 0, mc->rfc822_size, mc->data2);

    return(m == NULL ? "" : m);
}



/*----------------------------------------------------------------------
    Fetch MIME body parts

Args: stream
      msgno
      section -- string section number spec. i.e. "1.3.4"
      len     -- pointer to return len in

Returns: The contents of the body part, or NULL
 ---*/

char *
carmel_fetchbody (stream, msgno, section, len)
	MAILSTREAM    *stream;
	long           msgno;
	char          *section;
	unsigned long *len;
{
    char	 *base;
    BODY	 *b;
    PART	 *part;
    int		  part_num;
    long	  offset;
    MESSAGECACHE *mc;

    if (carmel_fetchstructure(stream, msgno, &b) == NULL || b == NULL)
      return(NULL);

    if(section == NULL || *section == '\0')
      return(NULL);

    part_num = strtol(section, &section, 10);
    if(part_num <= 0)
      return(NULL);

    base   = carmel_fetchtext(stream, msgno);
    offset = 0;

    do {			 /* until find desired body part */
	if (b->type == TYPEMULTIPART) {
	    part = b->contents.part;	/* yes, find desired part */
	    while (--part_num && (part = part->next));
	    if (!part) 
              return (NIL);	/* bad specifier */

	    /* Found part, get ready to go further */
	    b = &part->body;
	    if(b->type == TYPEMULTIPART && *section == '\0')
	      return(NULL); /* Ran out of section numbers, needed more */
	       
	    base += offset;		/* calculate new base */
	    offset = part->offset;	/* and its offset */

	} else if (part_num != 1) {
	    return NIL; /* Not Multipart, better be part 1 */
        }

				    /* need to go down further? */

	if(*section == 0) {
	    break;
	} else { 
	    switch (b->type) {
	      case TYPEMESSAGE:	/* embedded message, calculate new base */
		base += offset + b->contents.msg.offset;
		offset = 0;		/* no offset any more */
		b = b->contents.msg.body;
		/* got its body, drop into multipart case*/
  
	      case TYPEMULTIPART:	     /* multipart, get next section */
		if ((*section++ == '.') &&
		    (part_num = strtol (section, &section,10)) > 0)
		  break; /* Found the part */
  
	      default:			/* bogus subpart specification */
		return(NULL);
	    }
	}
    } while (part_num);

    mc = MC(msgno);
				  /* lose if body bogus */
    if ((!b) || b->type == TYPEMULTIPART)
      return(NULL);

    if (!mc->seen) {		/* if message not seen before */
      mc->seen = T;		/* mark as seen */
      LOCAL->dirty = T;		/* and that stream is now dirty */
    }

    *len = b->size.ibytes;
    return(base + offset);
}



/*----------------------------------------------------------------------
 *  Carmel mail set flag
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 */

void carmel_setflag (stream,sequence,flag)
	MAILSTREAM *stream;
	char *sequence;
	char *flag;
{
  MESSAGECACHE *elt;
  long i;
  short f = carmel_getflags (stream,flag);
  if (!f) return;		/* no-op if no flags to modify */
				/* get sequence and loop on it */
  if (mail_sequence (stream,sequence))
    for (i = 1; i <= stream->nmsgs; i++){
	elt = MC(i);
	if (elt->sequence) {
				    /* set all requested flags */
	  if (f&fSEEN) elt->seen = T;
	  if (f&fDELETED) elt->deleted = T;
	  if (f&fFLAGGED) elt->flagged = T;
	  if (f&fANSWERED) elt->answered = T;
	  /* Could be more creative about keeping track to see if something
	     really changed */
	  LOCAL->dirty = T;
	}
    }
}



/*----------------------------------------------------------------------
 * Carmel mail clear flag
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 */

void carmel_clearflag (stream,sequence,flag)
	MAILSTREAM *stream;
	char *sequence;
	char *flag;
{
  MESSAGECACHE *elt;
  long i = stream->nmsgs;
  short f = carmel_getflags (stream,flag);
  if (!f) return;		/* no-op if no flags to modify */
				/* get sequence and loop on it */
  if (mail_sequence (stream,sequence))
    for(i = 1; i <= stream->nmsgs; i++) {
	elt = MC(i);
	if (elt->sequence) {
				    /* clear all requested flags */
	  if (f&fSEEN) elt->seen = NIL;
	  if (f&fDELETED) elt->deleted = NIL;
	  if (f&fFLAGGED) elt->flagged = NIL;
	  if (f&fANSWERED) elt->answered = NIL;
	       		    /* clearing either seen or deleted does this */
	  /* Could be more creative about keeping track to see if something
	     really changed */
	  LOCAL->dirty = T;	/* mark stream as dirty */
	}
    }
}



/* Carmel mail search for messages
 * Accepts: MAIL stream
 *	    search criteria
 */

void carmel_search (stream,criteria)
	MAILSTREAM *stream;
	char *criteria;
{
  long i,n;
  char *d;
  search_t f;

				/* initially all searched */
  for (i = 1; i <= stream->nmsgs; ++i) mail_elt (stream,i)->searched = T;
				/* get first criterion */
  if (criteria && (criteria = strtok (criteria," "))) {
				/* for each criterion */
    for (; criteria; (criteria = strtok (NIL," "))) {
      f = NIL; d = NIL; n = 0;	/* init then scan the criterion */
      switch (*ucase (criteria)) {
      case 'A':			/* possible ALL, ANSWERED */
	if (!strcmp (criteria+1,"LL")) f = carmel_search_all;
	else if (!strcmp (criteria+1,"NSWERED")) f = carmel_search_answered;
	break;
      case 'B':			/* possible BCC, BEFORE, BODY */
	if (!strcmp (criteria+1,"CC"))
	  f = carmel_search_string (carmel_search_bcc,&d,&n);
	else if (!strcmp (criteria+1,"EFORE"))
	  f = carmel_search_date (carmel_search_before,&n);
	else if (!strcmp (criteria+1,"ODY"))
	  f = carmel_search_string (carmel_search_body,&d,&n);
	break;
      case 'C':			/* possible CC */
	if (!strcmp (criteria+1,"C"))
	  f = carmel_search_string (carmel_search_cc,&d,&n);
	break;
      case 'D':			/* possible DELETED */
	if (!strcmp (criteria+1,"ELETED")) f = carmel_search_deleted;
	break;
      case 'F':			/* possible FLAGGED, FROM */
	if (!strcmp (criteria+1,"LAGGED")) f = carmel_search_flagged;
	else if (!strcmp (criteria+1,"ROM"))
	  f = carmel_search_string (carmel_search_from,&d,&n);
	break;
      case 'K':			/* possible KEYWORD */
	if (!strcmp (criteria+1,"EYWORD"))
	  f = carmel_search_flag (carmel_search_keyword,&d);
	break;
      case 'N':			/* possible NEW */
	if (!strcmp (criteria+1,"EW")) f = carmel_search_new;
	break;

      case 'O':			/* possible OLD, ON */
	if (!strcmp (criteria+1,"LD")) f = carmel_search_old;
	else if (!strcmp (criteria+1,"N"))
	  f = carmel_search_date (carmel_search_on,&n);
	break;
      case 'R':			/* possible RECENT */
	if (!strcmp (criteria+1,"ECENT")) f = carmel_search_recent;
	break;
      case 'S':			/* possible SEEN, SINCE, SUBJECT */
	if (!strcmp (criteria+1,"EEN")) f = carmel_search_seen;
	else if (!strcmp (criteria+1,"INCE"))
	  f = carmel_search_date (carmel_search_since,&n);
	else if (!strcmp (criteria+1,"UBJECT"))
	  f = carmel_search_string (carmel_search_subject,&d,&n);
	break;
      case 'T':			/* possible TEXT, TO */
	if (!strcmp (criteria+1,"EXT"))
	  f = carmel_search_string (carmel_search_text,&d,&n);
	else if (!strcmp (criteria+1,"O"))
	  f = carmel_search_string (carmel_search_to,&d,&n);
	break;
      case 'U':			/* possible UN* */
	if (criteria[1] == 'N') {
	  if (!strcmp (criteria+2,"ANSWERED")) f = carmel_search_unanswered;
	  else if (!strcmp (criteria+2,"DELETED")) f = carmel_search_undeleted;
	  else if (!strcmp (criteria+2,"FLAGGED")) f = carmel_search_unflagged;
	  else if (!strcmp (criteria+2,"KEYWORD"))
	    f = carmel_search_flag (carmel_search_unkeyword,&d);
	  else if (!strcmp (criteria+2,"SEEN")) f = carmel_search_unseen;
	}
	break;
      default:			/* we will barf below */
	break;
      }
      if (!f) {			/* if can't determine any criteria */
	sprintf(carmel_error_buf,"Unknown search criterion: %s",criteria);
	mm_log (carmel_error_buf,ERROR);
	return;
      }
				/* run the search criterion */
      for (i = 1; i <= stream->nmsgs; ++i)
	if (mail_elt (stream,i)->searched && !(*f) (stream,i,d,n))
	  mail_elt (stream,i)->searched = NIL;
    }
				/* report search results to main program */
    for (i = 1; i <= stream->nmsgs; ++i)
      if (mail_elt (stream,i)->searched) mail_searched (stream,i);
  }
}




/*----------------------------------------------------------------------
 * Carmel mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long
carmel_ping (stream)
    MAILSTREAM *stream;
{
    struct stat sb;
    char path[1000], *mailfile;

    if(carmel_update_lock(stream, stream->mailbox, READ_LOCK) < 0) {
	/* Yuck! Someone messed up our lock file, this stream is hosed */
	mm_log("Mailbox updated unexpectedly! Mailbox closed", ERROR);
	stream->readonly = 1;
	return NIL;
    }

    /*--- First check to see if the Carmel index grew ----*/
    /* BUG, will this really work on NFS since the file is held open? */
    stat((*LOCAL->calc_paths)(LOCAL->home_dir, stream->mailbox, 0, 0), &sb);
    if(sb.st_size > LOCAL->index_size) {
	/* Pull in the new mail.... */
	/* BUG, for now only checking is on inbox */
    }

    if(sb.st_size < LOCAL->index_size) {
	/* Something bad happened, mail box shrunk on us, shutdown */
	stream->readonly = 1;
	mm_log("Mailbox shrank unexpectedly! Mailbox closed", ERROR);
	return NIL;
    }

    if(!stream->readonly &&
       ((LOCAL->pod && strcmp(stream->mailbox, "MAIL") == 0) ||
       (!LOCAL->pod && strcmp(stream->mailbox, "inbox") == 0))) {
	/* If it's the inbox we've got to check the spool file */
	mailfile = getenv("MAIL") == NULL ? MAILFILE : getenv("MAIL");
	sprintf(path, mailfile, getpwuid(geteuid())->pw_name);
        if(stat(path, &sb) < 0)
 	  return(T);  /* No new mail...*/
	if(sb.st_size > 0) {
	    mm_critical(stream);
	    if(carmel_lock(stream, stream->mailbox, WRITE_LOCK) < 0) {
		mm_nocritical(stream);
		return(T);
	    }
	    carmel_spool_mail(stream, path, stream->mailbox); /* go get it */
	    carmel_unlock(stream, stream->mailbox, WRITE_LOCK);
	    mm_nocritical(stream);
	}
    } 
    
    return T;			
}





/*----------------------------------------------------------------------
    This checks for new mail and checkpoints the mail file

Args -- The stream to check point

 ----*/

void 
carmel_check (stream)
	MAILSTREAM *stream;
{
    int msgno;
    MESSAGECACHE *mc;

    if(stream->readonly || LOCAL == NULL)
      return; /* Nothing to do in readonly or closed case */

    carmel_ping(stream); /* check for new mail (Ping always checks) */

    if(!LOCAL->dirty)
      return; /* Nothing to do */

    mm_critical(stream);
    if(carmel_lock(stream, stream->mailbox, WRITE_LOCK) < 0) {
	mm_nocritical(stream);
        return; /* Couldn't get a write lock, nothing we can do */
    }

    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
	mc = MC(msgno);
	fseek(LOCAL->index_stream, mc->data1 +  carmel_s_o_m_len  + 13, 0);
        if(fprintf(LOCAL->index_stream,
		   "U%c%c%c%c%c____________________________\n",
		   mc->flagged  ? 'F' : 'f',
		   mc->recent   ? 'R' : 'r',
		   mc->answered ? 'A' : 'a',
		   mc->deleted  ? 'D' : 'd',
		   mc->seen     ? 'S' : 's') == EOF) {
	    /* BUG .. Need some error check here */
	}
    }
    fflush(LOCAL->index_stream);

    carmel_unlock(stream, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    mm_log("Check completed", NULL);
}
    




/*----------------------------------------------------------------------
  Carmel mail expunge mailbox

Args: stream  -- mail stream to expunge

 ----*/

void carmel_expunge (stream)
	MAILSTREAM *stream;
{
    char        *index_file;
    long         msgno, new_msgno;
    FILE         *new_index;
    MESSAGECACHE *mc, *new_mc;
    ENVELOPE     *envelope;
    int           save_e;
    struct stat   sb;
    
    if (stream->readonly) {
        if(!stream->silent)
	  mm_log ("Expunge ignored on readonly mailbox",NIL);
        return;
    }
    
    mm_critical(stream);
    carmel_lock(stream, stream->mailbox, WRITE_LOCK);

    strcpy(carmel_path_buf,
	   (*LOCAL->calc_paths)(LOCAL->home_dir, stream->mailbox, 0, 0));
    strcpy(carmel_path_buf + strlen(carmel_path_buf) - 3, ".ex");


    new_index = fopen(carmel_path_buf, "w");
    if(new_index == NULL) {
	goto fail;
    }
    if(fprintf(new_index, carmel_s_o_f) == EOF)
      goto fail;

    new_msgno = 1;
    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
	mc = MC(msgno);
	if(mc->deleted) {
	    mm_expunged(stream, new_msgno);
	    continue;
	}

	if(msgno != new_msgno) {
	    new_mc        = MC(new_msgno);
	    *new_mc       = *mc;
	    new_mc->msgno = new_msgno;
	    mc            = new_mc;
	}

	envelope = carmel_fetchstructure(stream, msgno, NULL);
	if(envelope == NULL)
	  goto fail;

        /* get this after envelope to offset is still valid in old file */
        mc->data1 = ftell(new_index); 

	if(carmel_write_index(envelope, mc, new_index) < 0)
	  goto fail;
	new_msgno++;
    }

    index_file = (*LOCAL->calc_paths)(LOCAL->home_dir, stream->mailbox, 0, 0);

    /*--- Close it to make sure bits are really on disk across NFS. ---*/
    if(fclose(new_index) != EOF) {
	/*--- copy of index was a success, rename it ---*/
        unlink(index_file);
        link(carmel_path_buf, index_file); 

	/*--- Save the mail index size ----*/
        stat(index_file, &sb);
        LOCAL->index_size = sb.st_size;

        stream->nmsgs = new_msgno - 1;
	mm_exists(stream, stream->nmsgs);
	
	fclose(LOCAL->index_stream);
	LOCAL->index_stream = fopen(index_file, "r+");
    }
    unlink(carmel_path_buf); 
    carmel_unlock(stream, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    return;

  fail:
    save_e = errno;
    if(new_index != NULL)
      fclose(new_index);
    unlink(carmel_path_buf); 
    sprintf(carmel_error_buf, "Expunge failed: %s", strerror(save_e));
    mm_log(carmel_error_buf, WARN);
    carmel_unlock(stream, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    return;
}



/*----------------------------------------------------------------------
   Carmel mail copy message(s)

 Args: stream    - mail stream
       sequence  - message sequence
       mailbox   - destination mailbox

 Returns: T if copy successful, else NIL
  ----*/
long 
carmel_copy(stream,sequence,mailbox)
	MAILSTREAM *stream;
	char       *sequence;
	char       *mailbox;
{
    ENVELOPE     *e;
    MESSAGECACHE *mc, new_mc;
    long          msgno, file_no, file_pos;
    int           fail, new;
    char         *file_name, *line;
    FILE         *dest_stream;
    struct stat   sb;

    if (!mail_sequence (stream,sequence)) /* get sequence to do */
      return(NIL);

    /*--- Get the file open (creating if needed) first ----*/
    file_name = (*LOCAL->calc_paths)(LOCAL->home_dir, mailbox, 0, 0);
    if(file_name == NULL) {
	mm_log("Can't copy message; invalid folder name", ERROR);
	return(NIL);
    }
    if(stat(file_name, &sb) < 0)
      new = 1;
    else
      new = 0;
    dest_stream = fopen(file_name, "a+");
    if(dest_stream == NULL) {
	sprintf(carmel_error_buf, "Can't copy message to %s: %s",
		mailbox, strerror(errno));
	mm_log(carmel_error_buf, ERROR);
	return(NIL);
    }

    mm_critical(stream);

    if(carmel_lock(stream, mailbox, WRITE_LOCK) < 0) {
	mm_nocritical(stream);
	sprintf(carmel_error_buf,"Mailbox %s locked, can't copy messages to it",
		mailbox);
	mm_log(carmel_error_buf, ERROR);
	fclose(dest_stream);
	return(NIL);
    }


    /*--- Get the destination Carmel index open ----*/
    if(new) 
      fprintf(dest_stream, "\254\312--CARMEL-MAIL-FILE-INDEX--\n");

    file_pos = ftell(dest_stream);
    fail = 0;


    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
	mc = MC(msgno);
	if(!mc->sequence)
	  continue;

	new_mc = *mc;

	if(LOCAL->new_file_on_copy) {
	    new_mc.data2 = carmel_copy_msg_file(stream, mc->data2);
	    if((long)new_mc.data2 < 0) {
  	        fail = 1;
		break;
	    }
	}

	e = carmel_fetchstructure(stream, msgno, NULL);
	if(carmel_write_index(e, &new_mc, dest_stream) < 0) {
	    fail = 1;
	    break;
	}

	if(LOCAL->pod && LOCAL->aux_copy != NULL) {
	    if((*LOCAL->aux_copy)(stream->local, mailbox, e, &new_mc)) {
  	        fail = 1;
		break;
	    }
	}
    }

    if(fail) {
	ftruncate(fileno(dest_stream), file_pos);
    }

    fclose(dest_stream);

    carmel_unlock(stream, mailbox, WRITE_LOCK);

    mm_nocritical(stream);

    return(!fail);
}



/*----------------------------------------------------------------------
   Carmel mail move message(s)


  Returns: T if move successful, else NIL
 ----*/

long
carmel_move (stream,sequence,mailbox)
	MAILSTREAM *stream;
	char *sequence;
	char *mailbox;
{
  long          i;
  MESSAGECACHE *elt;

  if (!(mail_sequence (stream,sequence) &&
	carmel_copy (stream,sequence,mailbox))) return NIL;
				/* delete all requested messages */
  for (i = 1; i <= stream->nmsgs; i++)
    elt = MC(i);
    if (elt->sequence) {
      elt->deleted = T;		/* mark message deleted */
      LOCAL->dirty = T;		/* mark mailbox as dirty */
    }
  return T;
}



/*----------------------------------------------------------------------
 * Carmel garbage collect stream
 * Accepts: Mail stream
 *	    garbage collection flags
 */

void carmel_gc (stream, gcflags)
	MAILSTREAM *stream;
	long gcflags;
{
    /* No garbage collection in Carmel driver, not much to collect */
}



/*----------------------------------------------------------------------
    Handle MessageCache for carmel mail driver

Args: stream --
      msgno -- message number
      op    -- operation code

The carmel format keeps MESSAGECACHE entries in core for all messages
in the open mail folder so there isn't any cache flushing and rereading
that has to go on.
  ----*/
void *
carmel_cache(stream, msgno, op)
     	MAILSTREAM *stream;
	long msgno;
	long op;
{
    switch((int)op) {
      case CH_MAKEELT:
	return(MC(msgno));
	break;

      default:
	break;
    }
}
	


/*----------------------------------------------------------------------
    Append a message to a mailbox

Args: mailbox -- The message to append the mail folder too
      message -- Message header and text in rfc822 \r\n format to append

Returns: T if all went OK, NIL if not
 ----*/
long
carmel_append(stream, mailbox, message)
     MAILSTREAM *stream;
     char       *mailbox,
     STRING     *message;
{
    CARMELLOCAL local;

    /*---- A fake local data structure to pass to other functions---*/
    local.calc_paths = carmel_calc_paths;
    local.home_dir   = cpystr(getpwuid (geteuid ())->pw_dir);
    local.pod        = 0;
    local.host       = ""; /* BUG need some code here */
    local.aux_copy   = NULL;

    return(carmel_append2(&local, mailbox, message));
}



      



/*----------------------------------------------------------------------
    Fetch the body structure for a camel message

Args: stream -- MAILSTREAM
      mc     -- The incore message cache entry for the message

Returns: body structure

Note, the envelope that results from the parse here is ignored. Only
the envelope from the index is actually used in the Carmel format.
 ----*/
static BODY *
carmel_bodystruct(stream, mc)
     MESSAGECACHE *mc;
     MAILSTREAM   *stream;
{
    char     *header, *text, *file;
    ENVELOPE *e_place_holder;
    BODY     *b;

    header = carmel_readmsg(stream, 1,  mc->rfc822_size, mc->data2);
    if(text == NULL)
      return(NULL);

    text = carmel_readmsg(stream, 0, mc->rfc822_size, mc->data2);
    if(text == NULL)
      return(NULL);

    INIT(&string_struct, mail_string, (void *)"", 0);
    rfc822_parse_msg(&e_place_holder, &b, header, strlen(header),
		     text, strlen(text), LOCAL->host, carmel_20k_buf);

    mail_free_envelope(&e_place_holder);

#ifdef BWC
    if(LOCAL->pod && b->type == TYPETEXT && (b->subtype == NULL ||
       strcmp(b->subtype, "PLAIN") == 0)) {
	/* Most mail in a pod mail store is in the BWC-Glyph character
           set, but there is no tag, so we fake it here 
         */
	b->subtype = cpystr("X-BWC-Glyph");
    }
#endif

    return(b);
}



/*----------------------------------------------------------------------
    Parse an address in a Carmel format mail index

Args: line  -- The line from the index to parse
      addr  -- The address list to add to (if there is one)

Returns: address list
 ----*/
static ADDRESS *
carmel_parse_address(line, addr, localhost)
     char    *line, *localhost;
     ADDRESS *addr;
{
    ADDRESS *a;

    if(addr == NULL) {
	addr = mail_newaddr();
	a = addr;
    } else {
        for(a = addr; a!= NULL && a->next != NULL; a = a->next);
	a->next = mail_newaddr();
	a = a->next;
    }

    line++;  /* Skip the tag chacater */
    a->personal = carmel_parse_addr_field(&line);
    a->mailbox  = carmel_parse_addr_field(&line);
    a->host     = carmel_parse_addr_field(&line);
    a->adl      = carmel_parse_addr_field(&line);
    if(a->host == NULL)
      a->host = cpystr(localhost); /* host can't be NULL */
    return(addr);
}



/*----------------------------------------------------------------------
   Parse the next address field out of a carmel address index entry

Args: string -- pointer to pointer to string

Returns: field in malloced string or NULL

Input string is a bunch of substrings separated by ^A. This function scans 
for the next ^A or end of string, cuts it out and returns it. The original
strings passed in is mangled
----*/
static char *
carmel_parse_addr_field(string)
     char **string;
{
    char *p, end, *start;

    start = p  = *string;
    while(*p > '\001')  /* Find end of sub string or string */
      p++;
    if((p - *string) == 0) {
	if(*p) p++;
	*string = p;
        return(NULL); /* If nothing found return nothing */
    }
    end = *p;       /* Save terminating character (^A or \0) */
    *p = '\0';     
    if(end)         /* If not end of string, get ready for next call */
      p++;
    *string = p;    /* Return pointer to next substring */
    return(cpystr(start));
}


    
    
/*----------------------------------------------------------------------
     Write an entry into a carmel index

Args: e      -- Envelope
      mc     -- Message Cache element
      stream -- File stream to write to

Returns: 0 if OK, -1 if failed
----*/
carmel_write_index(e, mc, stream)
     ENVELOPE     *e;
     MESSAGECACHE *mc;
     FILE         *stream;
{
    long f_start, f_end;

    f_start = ftell(stream);

    if(fprintf(stream, "%s\007\001xxxxxxxxxx\n", carmel_s_o_m) == EOF)
      goto blah;
    if(fprintf(stream, "U%c%c%c%c%c____________________________\n",
	    mc->flagged  ? 'F' : 'f',
	    mc->recent   ? 'R' : 'r',
	    mc->answered ? 'A' : 'a',
	    mc->deleted  ? 'D' : 'd',
	    mc->seen     ? 'S' : 's') == EOF)
      goto blah;
    if(fprintf(stream, "Z%d\n", mc->rfc822_size) == EOF)
      goto blah;
    if(fprintf(stream, "D%d\001%d\001%d\001%d\001%d\001%d\001%d\001%d\n",
	    mc->year+1969, mc->month, mc->day, mc->hours, mc->minutes,
	    mc->seconds, mc->zhours * (mc->zoccident ? 1 : -1),
	       mc->zminutes) == EOF)
       goto blah;
    if(fprintf(stream, "Svmail\n") == EOF)
       goto blah;
    if(fprintf(stream, "P%d\n",mc->data2) == EOF)
       goto blah;
    if(carmel_index_address(e->to,   'T', stream) < 0)
       goto blah;
    if(carmel_index_address(e->from, 'F', stream) < 0)
       goto blah;
    if(carmel_index_address(e->cc,   'C', stream) < 0)
       goto blah;
    if(carmel_index_address(e->bcc,  'B', stream) < 0)
       goto blah;
#ifdef HAVE_RESENT
    if(carmel_index_address(e->resent_to,   't', stream) < 0)
       goto blah;
    if(carmel_index_address(e->resent_from, 'f', stream) < 0)
       goto blah;
    if(carmel_index_address(e->resent_cc,   'c', stream) < 0)
       goto blah;
    if(carmel_index_address(e->resent_bcc,  'b', stream) < 0)
       goto blah;
#endif
    if(carmel_index_address(e->return_path, 'H', stream) < 0)
       goto blah;
    if(carmel_index_address(e->sender,      'E', stream) < 0)
       goto blah;
    if(carmel_index_address(e->reply_to,    'R', stream) < 0)
       goto blah;
    if(e->in_reply_to != NULL)
      if(fprintf(stream, "L%s\n", e->in_reply_to) == EOF)
       goto blah;
    if(e->remail != NULL)
      if(fprintf(stream, "r%s\n", e->remail) == EOF)
       goto blah;
    if(e->message_id != NULL)
      if(fprintf(stream, "I%s\n", e->message_id) == EOF)
       goto blah;
    if(e->newsgroups != NULL)
      if(fprintf(stream, "N%s\n", e->newsgroups) == EOF)
       goto blah;
    if(e->subject != NULL)
      if(fprintf(stream, "J%s\n", e->subject) == EOF)
        goto blah;

    /*--- figure out and write the offset ---*/
    f_end = ftell(stream);
    if(fseek(stream, f_start, 0) < 0)
      goto blah;
    if(fprintf(stream, "%s\007\001%10d\n", carmel_s_o_m, f_end - f_start)==EOF)
      goto blah;
    if(fseek(stream, f_end, 0) < 0)
      goto blah;

    return(0);

  blah:
    /* Index entry is a bit of a mess now. Maybe we should try to truncate */
    return(-1);
}



/*----------------------------------------------------------------------
    Write an address entry into a carmel index

Args: addr   -- addresslist
      field  -- Field character specifier
      stream -- File stream to write to

Returns 0 if OK, -1 on error writing
 ---*/ 
static
carmel_index_address(addr, field, stream)
    ADDRESS *addr;
    int      field;
    FILE    *stream;
{
    ADDRESS *a;

    for(a = addr; a != NULL; a = a->next) {
	if(fprintf(stream, "%c%s\001%s\001%s\001%s\n",
                   field,
		   a->personal == NULL ? "" : a->personal,
		   a->mailbox  == NULL ? "" : a->mailbox,
		   a->host     == NULL ? "" : a->host,
		   a->adl      == NULL ? "" : a->adl) == EOF)
	  return(-1);
    }
    return(0);
}
    


/*----------------------------------------------------------------------
   Real work of reading mail data files

Args: stream 
      header_only -- return header if set, text if not
      file_size   -- The file size if known (saves a stat)
      file        -- name of the file to read

Returns buffer with text stored in internel buffer. The Carmel format
buffers the text of the current message and header in an internal
buffer. The buffer never shrinks and is expanded as needed, up to a
maximum. The text in the buffer is in CRLF format and is read in line
by line using stdio. It is believed this will be efficient on whatever
machine it is running on and will not use too much memory.  (There's
some extra memory used for buffering in stdio.) If a request is made
first for only the header, then only the header will be read in.  This
is a big efficiency win when the file is large and only the header is
needed. (In the Carmel format the header is genera lly not used, and
when it is it is with the body to do a MIME parse, but the pod format
does read the headers in to create the Carmel index.) An estimate is
made of the size needed to expand the file to convert the line ends
from LF to CRLF. The estimate alloca tes 10% extra space. If it
doesn't fit, the whole buffer will be expanded by 50% and the whole
read done over. When the header is read in a 30K buffer is allocated
initially (if the buffer is smaller than that initially). The 50%
increase is applied to the buffer when reading only the header.
  ----*/

char *
carmel_readmsg(stream, header_only, file_size, file_num)
     MAILSTREAM *stream;
     int         header_only;
     int         file_num;
     long        file_size;
{
    FILE       *msg_stream;
    char       *p, *p_end, *file_name;
    int         past_header, not_eof;
    long        max_read;
    struct stat st;

    file_name = (*LOCAL->calc_paths)(LOCAL->home_dir,
				     stream->mailbox, file_num, 0);

    /*---- Check out what we have read already -----*/
    if(LOCAL->buffered_file != NULL &&
       strcmp(LOCAL->buffered_file, file_name) == 0) {
	/* The file is the same. Now have we read in the part
           that is wanted? If so return it.
         */
	if(header_only || LOCAL->buffered_header_and_text) {
  	    if(header_only) {
                return(LOCAL->msg_buf);
	    } else {
	        return(LOCAL->msg_buf_text_start);
	    }
	}
    } else {
	/*-- Different file than last time, reset a few things --*/
	LOCAL->buffered_header_and_text = 0;
	LOCAL->msg_buf_text_offset      = 0L;
	if(LOCAL->buffered_file != NULL)
	  fs_give((void **)&(LOCAL->buffered_file));
    }

    /*----- Open the file ------*/
    /* Would actually be more efficient not to use stdio here */
    msg_stream = fopen(file_name, "r");
    if(msg_stream == NULL) {
	strcat(file_name, ".wid");
        msg_stream = fopen(file_name, "r");
	if(msg_stream == NULL)
           return(NULL);
    }

    /*---- Check the size of the file ----*/
    if(file_size == 0 && stat(file_name, &st) >= 0)
      file_size = st.st_size;


    /* ------Pick an amount to read -------
       Assume the header is less than MAX_HEADER. We don't want to
       allocate buffer for the whole message if we are just getting
       the header. 
     */
    max_read    = (file_size * 11) / 10;
    past_header = 0;
    p           = LOCAL->msg_buf;
    if(header_only) {
        max_read = min(max_read, CARMEL_MAX_HEADER);
    } else if(LOCAL->msg_buf_text_offset > 0) {
	past_header = 1;
	p = LOCAL->msg_buf_text_start;
        fseek(msg_stream, LOCAL->msg_buf_text_offset, 0);
    }
    if(max_read > CARMEL_MAXMESSAGESIZE)
      max_read = CARMEL_MAXMESSAGESIZE;


    /*----- Loop (re)reading the whole file 'till it fits ---*/
    /* This will fit the first time for all but the 
       strangest cases.
     */
    do {
	/*---- Make sure buffer is the right size ----*/	      
	if(LOCAL->msg_buf_size < max_read) {
	    /* Buffer not big, enough start whole read over */
	    if(LOCAL->msg_buf != NULL)
	      fs_give((void **)&(LOCAL->msg_buf));
	    LOCAL->msg_buf      = fs_get(max_read);
	    LOCAL->msg_buf_size = max_read;
	    fseek(msg_stream, 0, 0);
	    past_header = 0;
            p = LOCAL->msg_buf;
	}
    
	p_end = LOCAL->msg_buf + LOCAL->msg_buf_size - 3;

	while(p < p_end && (not_eof =(fgets(p, p_end-p, msg_stream) != NULL))){
	    if(*p == '\n' && !past_header) {
		*p++ = '\0';
		past_header = 1;
		LOCAL->msg_buf_text_offset = ftell(msg_stream);
		LOCAL->msg_buf_text_start  = p;
		if(header_only)
 		  goto done;
		else
		  continue;
	    }
	    p += strlen(p) - 1;    
	    *p++ = '\r';
	    *p++ = '\n';  
	}
	*p = '\0';
	if(!not_eof)
	  goto done;

	/* If we're here, the buffer wasn't big enough, which
           is due to a message with most lines less than 10 characters 
           (the 10% addition for turning LF to CRLF wasn't enough).
           Increase it by 50% and try again, till we hit
           the largest message we can read 
          */
        max_read = min(CARMEL_MAXMESSAGESIZE, (max_read * 15) / 10);
        fseek(msg_stream, 0, 0);
    } while (1);
  done:
    if(p == p_end) 
      mm_log("Message truncated. It's is too large", WARN);

    fclose(msg_stream);

    /*---- Save the name of the file buffered file ----*/
    LOCAL->buffered_file             = cpystr(file_name);
    LOCAL->buffered_header_and_text |= !header_only;

    return(header_only ? LOCAL->msg_buf : LOCAL->msg_buf_text_start);
}



/*----------------------------------------------------------------------
    Get the new mail out of the spooled mail file
  
 Args: stream  -- The inbox stream to add mail too
       spool   -- The path name of the spool file
       mailbox -- Name user sees for this, used only for error reporting

 Result:

 - Lock the spool mail file with bezerk_lock
 - Get the carmel index open and remember where we started in it
 - Make buffer big enough for biggest header we'll mess with
 - Loop reading all the message out of the spool file:
   - Get a new data file for the message and open it
   - Read the header of the message into the big buffer while...
    - writing the message into the new data file
   - finish writing the text of the message into the data file
   - If all went well bump the message count and say it exists
   - Parse the header of the message to get an envelope, date and size
   - Write an entry into the carmel index
 - Unlink and create (to zero) the spool file and unlock it

 ----*/
static void
carmel_spool_mail(stream, spool, mailbox) 
     MAILSTREAM *stream;
     char       *spool, *mailbox;
{
    char	 *p, *eof;
    int		  n, size, fd, in_header;
    long	  file_pos, index_file_pos, byte_count, start_of_append;
    FILE	 *spool_stream, *data_stream;
    ENVELOPE	 *envelope;
    BODY	 *b;
    MESSAGECACHE *mc;

    /*--- Get the locks set and files open-----*/
    fd = carmel_bezerk_lock(spool, mailbox);
    if(fd < 0)
      return;
    spool_stream = fdopen(fd, "r");
    fseek(LOCAL->index_stream, 0L, 2);
    start_of_append = ftell(LOCAL->index_stream);

    /*--- Make buffer big enough for largest allowable header ----*/
    if(LOCAL->msg_buf_size < CARMEL_MAX_HEADER) {
	if(LOCAL->msg_buf != NULL)
	  fs_give((void **)&(LOCAL->msg_buf));
	LOCAL->msg_buf_size = CARMEL_MAX_HEADER;
	LOCAL->msg_buf = fs_get(CARMEL_MAX_HEADER);
    }

    /*---- Read (and ignore) the first line with the "From " in it ----*/
    eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream); 

    /*----- Loop getting messages ----*/
    while(eof != NULL) {

	/*----- get a data file for the new message ----*/
        n = carmel_new_data_file(stream->local, stream->mailbox);
        data_stream = fopen((*LOCAL->calc_paths)(LOCAL->home_dir,
						 stream->mailbox,n,0),"w");
	if(data_stream == NULL)
	  goto backout;
        file_pos  = ftell(spool_stream);
        p         = LOCAL->msg_buf;
        in_header = 1;
	byte_count = 0L;
	

	/*--------------------------------------------------------------------
            Read the message in line by line, writing it out to the
            new data file. Also acculamuate a copy of the header in
            a buffer for parsing
          ---*/
	eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream);
        while(eof != NULL){
	    if(carmel_20k_buf[0] == 'F' && carmel_20k_buf[1] == 'r' &&
	       carmel_20k_buf[2] == 'o' && carmel_20k_buf[3] == 'm' &&
	       carmel_20k_buf[4] == ' ')
	      break;
    
	    if(in_header) {
		if(*carmel_20k_buf == '\n') {
		    /* Encountered first blank line, end of header */
		    in_header = 0;
		    *p = '\0';
		} else {
		    if(p - LOCAL->msg_buf + strlen(carmel_20k_buf) >
		       LOCAL->msg_buf_size){
			/* out of room in buffer, end it */
			in_header = 0;
			*p = '\0';
		    } else {
			strcpy(p, carmel_20k_buf);
			p +=strlen(p);
		    }
		}
	    }

	    /*----- Write the message into the file -----*/
	    byte_count += strlen(carmel_20k_buf);
	    if(carmel_20k_buf[0] == '>' && carmel_20k_buf[1] == 'F' &&
	       carmel_20k_buf[2] == 'r' && carmel_20k_buf[3] == 'o' &&
	       carmel_20k_buf[4] == 'm' && carmel_20k_buf[5] == ' ')  {
  	        if(fputs(carmel_20k_buf + 1, data_stream) == EOF)
		  goto backout;
		byte_count -= 1;
	    } else {
    	        if(fputs(carmel_20k_buf, data_stream) == EOF)
		  goto backout;
	    }
	    eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream);
	}
	fclose(data_stream);

	stream->nmsgs++;
	mm_exists(stream, stream->nmsgs);
	
	mc = carmel_new_mc(stream, stream->nmsgs);

	/*------ Parse the message header ------*/
	rfc822_parse_msg(&envelope, &b, LOCAL->msg_buf, strlen(LOCAL->msg_buf),
			 "", 0, LOCAL->host, carmel_20k_buf);
	carmel_parse_bezerk_status(mc, LOCAL->msg_buf);
	carmel_rfc822_date(mc, LOCAL->msg_buf);
	mc->rfc822_size = byte_count; 
	mc->data2 = n;
	mc->data1 = ftell(LOCAL->index_stream);
	mc->recent = 1;

 	/*----- Now add the message to the Carmel index ------*/
	carmel_write_index(envelope, mc, LOCAL->index_stream);

	mail_free_envelope(&envelope);
    }

    fclose(spool_stream);
    unlink(spool);
    close(creat(spool, 0600));
    carmel_bezerk_unlock(fd, spool);
    return;

  backout:
    mm_log("Error incorporating new mail", ERROR);
    fflush(LOCAL->index_stream);
    ftruncate(fileno(LOCAL->index_stream), start_of_append);
    carmel_bezerk_unlock(fd, spool);
}



/*----------------------------------------------------------------------
   Copy the actual data file when copying a message

Returns: -1 for failure
            otherwise the number of the new file, 

BUG: This code is untested and unused

  ----*/	
static  
carmel_copy_msg_file(stream, orig_file_number, new_mailbox)
     MAILSTREAM *stream;
     int         orig_file_number;
     
{
    char *file_name;
    int   wide, e, new_file_num, n, orig_fd, new_fd;

    /*---- Open the orginal file ----*/
    wide = 0;
    file_name = (*LOCAL->calc_paths)(LOCAL->home_dir,
				     new_mailbox,orig_file_number,0);
    orig_fd = open(file_name, O_RDONLY);
    if(orig_fd < 0 && LOCAL->pod) {
        strcat(file_name, ".wid");
        orig_fd = open(file_name, O_RDONLY);
        if(orig_fd < 0)
          goto bomb;
        wide = 1;
    } else {
	goto bomb;
    }

    /*----- Open and create the new file ------*/
    new_file_num = carmel_new_data_file(stream->local, new_mailbox);
    if(new_file_num < 0)
      goto bomb;
    file_name = (*LOCAL->calc_paths)(LOCAL->home_dir,
				     new_mailbox, new_file_num, 0);
    if(wide)
      strcat(file_name, ".wid");
    new_fd = open(file_name, O_WRONLY | O_CREAT, 0600);
    if(new_fd < 0) {
	goto bomb;
    }

    /*---- Copy the bits ------*/
    e = 0;
    while((n = read(orig_fd, carmel_20k_buf, sizeof(carmel_20k_buf))) >0) {
	if(write(new_fd, carmel_20k_buf, n) < 0) {
	    e = errno;
  	    break;
	}
    }

    /*---- Close the streams and handle any errors ----*/
    close(orig_fd);
    close(new_fd);

    if(e == 0)
      return(new_file_num);  /* All is OK */
    
    /*--- something didn't go right ---*/
  bomb:
    unlink(file_name);
    sprintf(carmel_error_buf, "Error copying message to %s: %s",
	    new_mailbox, strerror(errno));
    mm_log(carmel_error_buf, ERROR);
    return(-1);
}






/*----------------------------------------------------------------------
       Locking for Carmel and Pod formats

Args: stream --  Mail stream we're operating on
      file   --  Mail file name
      write  --  Flag indicating we want write access

Retuns: -1 if lock fails, 0 if it succeeds

- There are two locks. Plain locks and write locks. They are created
  about the same way, but have different names. The time out on the write
  locks is much shorter, because it should never be held for very long.

- Hitching (links in file system) post locking is used so it will work
  across NFS. Flock() could be used as it has two advantages. First it
  is more efficient, second the locks will disolve automatically when the 
  process dies. The efficiency is not of great concern, and the
  process should not (theoretically) die unless it crashes due to a bug
  or it is abnormally terminated. The advantage of locking across NFS
  is considered greater than the advantages of flock().

- The mod time of the lock file is updated everytime mail_check()
  or mail_ping() is called and the mod time on the lock file is recorded.
  This is so it can be determined that the lock file is current.

- When a read lock file over 30 or a write lock over 5 minutes old is
  encountered it is assumed the lock is old and should be overridden
  (because the process crashed or was killed). 

- Everytime the mod time on the lock file is updated (on calls to
  mail_check() and mail_ping()), the mod time of the lock file is
  checked against the record of what it was last set to. If the mod times 
  don't match the lock has been broken and overridden. Then the original
  Pine should go into read-only mode.... This is only likely to happen if
  someone suspends (^Z's) the process for more than 30 minutes, and another
  process is started.
  ----*/

carmel_lock(stream, file, write)
     MAILSTREAM *stream;
     char       *file;
     int         write;
{
    char        *hitch, *lock, error_mess[200];
    struct stat sb;
    int         n, link_result, hitch_fd, timer_set, l;
    long        override, timer;

    /* Set the length of time for override. It is 5 minutes for a 
       write lock (no process should have it for that long) and
       30 minutes for a read lock, that's 30 minutes since the
       lock file was last touched. */
    override = 60 * (write ? 5 : 30);
    timer    = -1;

    /*--- Get the full path of the index ---*/
    lock = (*LOCAL->calc_paths)(LOCAL->home_dir, file, 0, 0);
    /*---- Chop off ".cx" ----*/
    lock[strlen(lock)-3] = '\0';
    l = strlen(lock);
    hitch = carmel_path_buf;
    hitch = strcpy(hitch, lock);
    strcat(lock, write ? ".wl" : ".rl");
    
    do {
	/*--- First create hitching post ----*/
        for(n = time(0) % 6400; ; n += 10007) {
	    /* Get random file name, that's not too long */
	    sprintf(hitch + l, "_%c%c", '0' + (n % 80) , '0' + (n/80));
	    if(stat(hitch, &sb) < 0)
	      break; /* Got a name that doesn't exist */
	}
	hitch_fd = open(hitch, O_CREAT, 0666);
	if(hitch_fd < 0) {
	    sprintf(error_mess, "Error creating lock file \"%s\": %s",
		    hitch, strerror(errno));
	    mm_log(error_mess, WARN);
	    return(-1);
	}
	close(hitch_fd);
    
	/*----- Got a hitching post, now try link -----*/
	link_result = link(hitch, lock);
	stat(lock, &sb);
	unlink(hitch);
	if(link_result == 0 && sb.st_nlink == 2) {
	    /*------ Got the lock! ------*/
	    if(write)
   	      LOCAL->write_lock_mod_time = sb.st_mtime;
	    else
   	      LOCAL->read_lock_mod_time = sb.st_mtime;
	    return(0);
	}

	/*----- Check and override if lock is too old -----*/
	if(sb.st_mtime + override < time(0)) {
	    unlink(lock); /* Lock is old, override it */
	    timer = 100; /* Get us around the loop again */
	    continue;
	} else {
	    if(timer < 0) /* timer not set */
  	      timer = sb.st_mtime + override - time(0);
	}

	/*----- Will user wait till time for override? -----*/
	if(!write || timer > 5 * 60) {
	    return(-1); /* Not more that 5 minutes */
	}

	/*----- Try again, and tell user we're trying -------*/
	if(!(timer % 15)) {
	    sprintf(error_mess,
		    "Please wait. Mailbox %s is locked for %d more seconds",
		    file, timer);
	    mm_log(error_mess, WARN);
	}
        timer--;
	sleep(1);
    } while(timer > 0);

    return(-1);
}



/*----------------------------------------------------------------------
   Unlock a carmel mail stream

  ----*/
void
carmel_unlock(stream, file, write)
     MAILSTREAM *stream;
     char       *file;
     int         write;
{
    char        *lock;
    struct stat  sb;

    /*---- Get full path of the index ----*/
    lock  = (*LOCAL->calc_paths)(LOCAL->home_dir, file, 0, 0);
    strcpy(lock + strlen(lock) - 3, write ? ".wl" : ".rl");

    if(stat(lock, &sb) < 0)
      return; /* Hmmm... lock already broken */

    if(sb.st_mtime !=
       (write ? LOCAL->write_lock_mod_time : LOCAL->read_lock_mod_time))
      return; /* Hmmm... not our lock */
    
    unlink(lock);
}

    
    
/*----------------------------------------------------------------------
    Keep the mod date on the lock file fresh 

Args: stream --
      file   -- the name of the mailbox the lock is for
      write  -- set if this is a write lock

Returns: 0 if update was OK
        -1 if something bad happened, like the lock was stolen
 ----*/
carmel_update_lock(stream, file, write)
     MAILSTREAM *stream;
     char       *file;
     int         write;
{
    char        *lock;
    struct       timeval tp[2];
    struct stat  sb;

    if(stream->readonly)
      return(0);

    lock =  (*LOCAL->calc_paths)(LOCAL->home_dir, file, 0, 0);
    strcpy(lock + strlen(lock) - 3, write ? ".wl" : ".rl");

    if(stat(lock, &sb) < 0) {
	/* Lock file stolen, oh oh */
	return(-1);
    }

    if(sb.st_mtime !=
       (write ? LOCAL->write_lock_mod_time : LOCAL->read_lock_mod_time)) {
	/* Not our lock anymore , oh oh */
	return(-1);
    }

    gettimeofday (&tp[0],NIL);	/* set atime to now */
    gettimeofday (&tp[1],NIL);	/* set mtime to now */
    utimes(lock, tp);

    if(write)
      LOCAL->write_lock_mod_time = tp[1].tv_sec;
    else
      LOCAL->read_lock_mod_time = tp[1].tv_sec;
    return(0);
}



/*----------------------------------------------------------------------
   Berkeley open and lock mailbox

This is mostly ripped off from the Bezerk driver
 ----*/

static int 
carmel_bezerk_lock (spool, file)
	char *spool, *file;
{
  int fd,ld,j;
  int i = BEZERKLOCKTIMEOUT * 60 - 1;
  struct timeval tp;
  struct stat sb;
  char *hitch, *lock;

  lock = carmel_path_buf;
  sprintf(lock, "%s.lock", spool);
  do {				/* until OK or out of tries */
    gettimeofday (&tp,NIL);	/* get the time now */
#ifdef NFSKLUDGE
  /* SUN-OS had an NFS, As kludgy as an albatross;
   * And everywhere that it was installed, It was a total loss.  -- MRC 9/25/91
   */
				/* build hitching post file name */
    hitch = carmel_20k_buf;
    sprintf(hitch, "%s.%d.%d.",carmel_path_buf,time (0),getpid ());
    j = strlen (hitch);	/* append local host name */
    gethostname (hitch + j,(MAILTMPLEN - j) - 1);
				/* try to get hitching-post file */
    if ((ld = open (hitch, O_WRONLY|O_CREAT|O_EXCL,0666)) < 0) {
				/* prot fail & non-ex, don't use lock files */
      if ((errno == EACCES) && (stat (hitch, &sb))) *lock = '\0';
      else {			/* otherwise something strange is going on */
	sprintf (carmel_20k_buf,"Error creating %s: %s",lock,strerror (errno));
	mm_log (carmel_20k_buf, WARN);	/* this is probably not good */
				/* don't use lock files if not one of these */
	if ((errno != EEXIST) && (errno != EACCES)) *lock = '\0';
      }
    }
    else {			/* got a hitching-post */
      chmod (hitch,0666);	/* make sure others can break the lock */
      close (ld);		/* close the hitching-post */
      link (hitch,lock);	/* tie hitching-post to lock, ignore failure */
      stat (hitch, &sb);	/* get its data */
      unlink (hitch);		/* flush hitching post */
      /* If link count .ne. 2, hitch failed.  Set ld to -1 as if open() failed
	 so we try again.  If extant lock file and time now is .gt. file time
	 plus timeout interval, flush the lock so can win next time around. */
      if ((ld = (sb.st_nlink != 2) ? -1 : 0) && (!stat (lock,&sb)) &&
	  (tp.tv_sec > sb.st_ctime + BEZERKLOCKTIMEOUT * 60)) unlink (lock);
    }

#else
  /* This works on modern Unix systems which are not afflicted with NFS mail.
   * "Modern" means that O_EXCL works.  I think that NFS mail is a terrible
   * idea -- that's what IMAP is for -- but some people insist upon losing...
   */
				/* try to get the lock */
    if ((ld = open (lock,O_WRONLY|O_CREAT|O_EXCL,0666)) < 0) switch (errno) {
    case EEXIST:		/* if extant and old, grab it for ourselves */
      if ((!stat (lock,&sb)) && tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60)
	ld = open (lock,O_WRONLY|O_CREAT,0666);
      break;
    case EACCES:		/* protection fail, ignore if non-ex or old */
      if (stat (lock,&sb) || (tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60))
	*lock = '\0';		/* assume no world write mail spool dir */
      break;
    default:			/* some other failure */
      sprintf (tmp,"Error creating %s: %s",lock,strerror (errno));
      mm_log (tmp,WARN);	/* this is probably not good */
      *lock = '\0';		/* don't use lock files */
      break;
    }
    if (ld >= 0) {		/* if made a lock file */
      chmod (tmp,0666);		/* make sure others can break the lock */
      close (ld);		/* close the lock file */
    }
#endif
    if ((ld < 0) && *lock) {	/* if failed to make lock file and retry OK */
      if (!(i%15)) {
	sprintf (carmel_20k_buf,"Mailbox %s is locked, will override in %d seconds...",
		 file,i);
	mm_log (carmel_20k_buf, WARN);
      }
      sleep (1);		/* wait 1 second before next try */
    }
  } while (i-- && ld < 0 && *lock);
				/* open file */
  if ((fd = open (spool, O_RDONLY)) >= 0) flock (fd, LOCK_SH);
  else {			/* open failed */
    j = errno;			/* preserve error code */
    if (*lock) unlink (lock);	/* flush the lock file if any */
    errno = j;			/* restore error code */
  }
  return(fd);
}



/*----------------------------------------------------------------------
   Berkeley unlock and close mailbox
 ----*/
static void 
carmel_bezerk_unlock (fd, spool)
	int fd;
	char *spool;
{
    sprintf(carmel_path_buf, "%s.lock", spool);
    
    flock (fd, LOCK_UN);	  /* release flock'ers */
    close (fd);			  /* close the file */
 				  /* flush the lock file if any */
    if (*carmel_path_buf) unlink (carmel_path_buf);
}



/*----------------------------------------------------------------------
    Make sure directory exists and is writable

Args: dir  - directory to check, should be full path
      
Result: returns -1 if not OK along with mm_logging a message
                 0 if OK
----*/

carmel_check_dir(dir)
     char *dir;
{
    struct stat sb;
    char   error_mess[150];

    if(stat(dir, &sb) < 0) {
	if(mkdir(dir, S_IRWXU) < 0) {
	    sprintf(error_mess, "Error creating directory %-.30s %s",
		    dir, strerror(errno));
	    mm_log(error_mess, WARN);
	    return(-1);
	}
    } else if(!(sb.st_mode & S_IFDIR)) {
	sprintf(error_mess, "Warning: %s is not a directory",dir);
	mm_log(error_mess, WARN);
	return(-1);

    } else if(access(dir, W_OK) != 0) {
	sprintf(error_mess, "Warning: unable to write to %-.30s %s",
		dir, strerror(errno));
	mm_log(error_mess, WARN);
        return(-1);
    }
    return(0);
}

    
    
/*----------------------------------------------------------------------
    Return the number for the next message file

Args: stream  -- the Mail stream for the new data file
      mailbox -- 

Returns: file number or -1
  ----*/
static 
carmel_new_data_file(local, mailbox)
     CARMELLOCAL *local;
     char        *mailbox;
{
    char *path, num_buf[50], e_buf[100];
    int   fd, num, bytes_read;

    /*---- Get the full path of the .MAXNAME file for index ----*/
    path = (*local->calc_paths)(local->home_dir, mailbox, 0, 1);

    fd = open(path, O_RDWR|O_CREAT, 0666);
    if(fd < 0) {
	sprintf(e_buf, "Error getting next number of mail file: %s",
		strerror(errno));
        mm_log(e_buf, ERROR);
        return(-1);
    }

    bytes_read = read(fd, num_buf, sizeof(num_buf));
    if(bytes_read < 6) {
        num  = 100000;
    } else {
        num = atoi(num_buf) + 1;
	if(num >= 999999)
	  num = 100000;
    }
    sprintf(num_buf, "%-6d\n", num);
    lseek(fd, 0, 0);
    write(fd, num_buf, strlen(num_buf));
    close(fd);
    return(num);
}



/*----------------------------------------------------------------------
   Do all the file name generation for carmel driver

This generates that absolute paths for an index, or a data file or
the .MAXNAME file.

This is untested!
  ----*/
static char *
carmel_calc_paths(home_dir, mailbox, data_file_num, maxname)
     char       *mailbox, *home_dir;
     int         data_file_num, maxname;
{
    static char path[CARMEL_PATHBUF_SIZE], num[20];
    char       *p;

    if(data_file_num || maxname) {
	if(maxname)
	  strcpy(num, ".MAXNAME");
	else
	  sprintf(num, "%d", data_file_num);
	/*---- Generate a data file name ----*/
	if(*mailbox == '/') {
	    strcpy(path, mailbox);
   	    for(p = path + strlen(path) - 1; *p != '/' && p > path; p--);
	    if(p == path) {
                /* Pretty weird, mail is in root */
	        sprintf(path, "/%s", num); 
	    } else {
		/* Tack the file name on the end */
	        sprintf(p, "/%s", num); 
	    }

	} else {
	    sprintf(path, "%s/%s/%s", home_dir, CARMEL_DIR, num);
	}
    } else {

	/*------- Generate an index file name ------*/
	if(*mailbox == '/')
	  sprintf(path, "%s.cx", mailbox);
	else
	  sprintf(path,"%s/%s/%s.cx", home_dir, CARMEL_DIR, mailbox);
    }
    
    return(path);
}



/*----------------------------------------------------------------------
   Find and parse the status line in a mail header

Args:  mc     -- the message cache where status is to be stored
       header -- the message header to parsen
 ----*/
void
carmel_parse_bezerk_status(mc, header)
     MESSAGECACHE *mc;
     char *header;
{
    register char *p;
    for(p = header; *p; p++) {
	if(*p != '\n')
	  continue;
	p++;
	if(*p != 'S' && *p != 'X')
	  continue;
	if(strncmp(p, "Status: ",  8) == 0 || strncmp(p,"X-Status: ",10)== 0) {
            mc->recent = 1;
            for(p += *p == 'X' ? 10: 8; *p && *p != '\n'; p++)
              switch(*p) {
		case 'R': mc->seen    = 1; break;
		case 'O': mc->recent  = 0; break;
		case 'D': mc->deleted = 1; break;
		case 'F': mc->flagged = 1; break;
		case 'A': mc->flagged = 1; break;
	      }
	}
    }
}
	


/*----------------------------------------------------------------------
  Turn a string of describing flags into a bit mask

Args:  stream -- mail stream, unused
       flag   -- string flag list

Returns:  returns short with bits set; bits are defined in carmel.h
 ----*/
static short
carmel_getflags (stream,flag)
	MAILSTREAM *stream;
	char *flag;
{
  char *t, tmp[100];
  short f = 0;
  short i,j;
  if (flag && *flag) {		/* no-op if no flag string */
				/* check if a list and make sure valid */
    if ((i = (*flag == '(')) ^ (flag[strlen (flag)-1] == ')')) {
      mm_log ("Bad flag list",ERROR);
      return NIL;
    }
				/* copy the flag string w/o list construct */
    strncpy (tmp,flag+i,(j = strlen (flag) - (2*i)));
    tmp[j] = '\0';
    t = ucase (tmp);	/* uppercase only from now on */

    while (*t) {		/* parse the flags */
      if (*t == '\\') {		/* system flag? */
	switch (*++t) {		/* dispatch based on first character */
	case 'S':		/* possible \Seen flag */
	  if (t[1] == 'E' && t[2] == 'E' && t[3] == 'N') i = fSEEN;
	  t += 4;		/* skip past flag name */
	  break;
	case 'D':		/* possible \Deleted flag */
	  if (t[1] == 'E' && t[2] == 'L' && t[3] == 'E' && t[4] == 'T' &&
	      t[5] == 'E' && t[6] == 'D') i = fDELETED;
	  t += 7;		/* skip past flag name */
	  break;
	case 'F':		/* possible \Flagged flag */
	  if (t[1] == 'L' && t[2] == 'A' && t[3] == 'G' && t[4] == 'G' &&
	      t[5] == 'E' && t[6] == 'D') i = fFLAGGED;
	  t += 7;		/* skip past flag name */
	  break;
	case 'A':		/* possible \Answered flag */
	  if (t[1] == 'N' && t[2] == 'S' && t[3] == 'W' && t[4] == 'E' &&
	      t[5] == 'R' && t[6] == 'E' && t[7] == 'D') i = fANSWERED;
	  t += 8;		/* skip past flag name */
	  break;
	default:		/* unknown */
	  i = 0;
	  break;
	}
				/* add flag to flags list */
	if (i && ((*t == '\0') || (*t++ == ' '))) f |= i;
	else {			/* bitch about bogus flag */
	  mm_log ("Unknown system flag",ERROR);
	  return NIL;
	}
      }
      else {			/* no user flags yet */
	mm_log ("Unknown flag",ERROR);
	return NIL;
      }
    }
  }
  return f;
}


/*----------------------------------------------------------------------
   Get a pointer to a MESSAGECACHE entry, allocating if needed
 
Args: stream -- message stream
      num    -- Message number to allocate on for

Returns: The MESSAGECACHE entry

The message caches are allocated in blocks of 256 to save space taken up by
pointers in a linked list and allocation overhead. The mc_blocks 
data structure is an array that points to each block. The macro MC() 
defined in carmel.h returns a pointer to a MESSAGECACHE given
a message number. This function here can be called when a MESSAGECACHE
is needed to a message number that might be new. It allocates a new
block if needed and clears the MESSAGECACHE returned.

The MESSAGECACHE entries are currently about 28 bytes which implies 28Kb
must be used per 1000 messages. If memory is limited this driver will be 
limited in the number of messages it can handle, and the limit is due to
the fact that these MESSAGECACHEs must be in core.

It might be possible to reduce the size of each entry by a few bytes if
the message numbers were reduced to a short, and the mostly unused keywords
were removed. It would also be nice to add a day of the week (3 bits)
  ----*/
static MESSAGECACHE *
carmel_new_mc(stream, num)
     MAILSTREAM *stream;
     int num;
{
    MESSAGECACHE *mc;
      
    /* Make sure we've got a cache_entry */
    if(num >= LOCAL->cache_size) {
        if(LOCAL->mc_blocks == NULL)
          LOCAL->mc_blocks=(MESSAGECACHE **)fs_get(sizeof(MESSAGECACHE *));
        else
          fs_resize((void **)&(LOCAL->mc_blocks), ((num >>8) + 1) *
                                                  sizeof(MESSAGECACHE *));
        LOCAL->mc_blocks[num >> 8] = (MESSAGECACHE *)
          fs_get(256 * sizeof(MESSAGECACHE));
        LOCAL->cache_size = ((num >> 8) + 1) * 256;
    }
    
    mc = MC(num);

    mc->user_flags = 0;
    mc->lockcount  = 0;
    mc->seen       = 0;
    mc->deleted    = 0;
    mc->flagged    = 0;
    mc->answered   = 0;
    mc->recent     = 0;
    mc->searched   = 0;
    mc->sequence   = 0;
    mc->msgno      = num;
    
   /* Don't set the date, the size and the extra data,
      assume they will be set 
    */
    return(mc);
}



/*----------------------------------------------------------------------
  Do the read work of appending a message to a mailbox
 
Args: local -- The carmel local data structure, (a some what fake incomplete
               one, set up for the purposes here)
      mailbox -- Name of mailbox to append to
      message -- The rfc822 format of the message with \r\n's

Returns: T if all went OK, NIL if it did not

 - Make sure index exists or can be created
 - lock index for write
 - get a data file name
 - Put the text in the file
 - Parse the string to get envelope and message cache
 - Write the entry into the index
 - Unlock the index

BUG: This needs some locking and some error messages

  ----*/
carmel_append2(local, mailbox, message)
     char *mailbox, *message;
     CARMELLOCAL *local;
{
    char         *index_name, *data_name, *p;
    ENVELOPE     *envelope;
    BODY         *b;
    MESSAGECACHE  mc;
    FILE         *index_file, *data_file;
    struct stat   sb;
    int           new;
    STRING        string_struct;

    index_name = (*local->calc_paths)(local->home_dir, mailbox, 0, 0);
    if(index_name == NULL)
      goto bomb;

    if(stat(index_name, &sb) < 0) {
	new = 1;
    } else {
	new = 0;
    }

    index_file = fopen(index_name, new ? "w" : "a");
    if(index_file == NULL)
      goto bomb;

    if(new)
      if(fputs(carmel_s_o_f, index_file) == EOF)
	goto bomb;

/*    carmel_lock(mailbox); Don't lock for now */

    mc.data2 = carmel_new_data_file(local, mailbox);
    mc.rfc822_size = strlen(message);
    carmel_rfc822_date(&mc, message);
    mc.deleted    = 0;
    mc.seen       = 1;
    mc.flagged    = 0;
    mc.answered   = 0;
    mc.recent     = 0;
    mc.user_flags = 0;

    data_name = (*local->calc_paths)(local->home_dir, mailbox, mc.data2, 0);

    /*------- Write the message to the file --------*/
    data_file = fopen(data_name, "w");
    for(p = message; *p;) {
	if(*p == '\r' && *(p+1) == '\n') {
	    fputc('\n', data_file);
	    p+=2;
	} else {
	    fputc(*p, data_file);
	    p++;
	}
    }
    fclose(data_file);

    /*------ Parse the message to get envelope and message cache ----*/
    INIT(&string_struct, mail_string, (void *)"", 0);
    rfc822_parse_msg(&envelope, &b, message, strlen(message), &string_struct, 
		     local->host, carmel_20k_buf);
    carmel_parse_bezerk_status(&mc, message);
    carmel_rfc822_date(&mc, message);

    /*------ Write the entry into the index ------*/
    if(carmel_write_index(envelope, &mc, index_file) < 0)
      goto bomb;

    if(local->aux_copy != NULL)
      (*local->aux_copy)(local, mailbox, envelope, &mc);

    mail_free_envelope(&envelope);

    fclose(index_file);
    return(T);

  bomb:
    return(NIL);
}






/* Search support routines
 * Accepts: MAIL stream
 *	    message number
 *	    pointer to additional data
 *	    pointer to temporary buffer
 * Returns: T if search matches, else NIL
 */

static char 
carmel_search_all (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return T;			/* ALL always succeeds */
}


static char 
carmel_search_answered (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->answered ? T : NIL;
}


static char 
carmel_search_deleted (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->deleted ? T : NIL;
}


static char 
carmel_search_flagged (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->flagged ? T : NIL;
}


static char 
carmel_search_keyword (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return NIL;			/* keywords not supported yet */
}


static char 
carmel_search_new (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  MESSAGECACHE *elt = MC(msgno);
  return (elt->recent && !elt->seen) ? T : NIL;
}

static char 
carmel_search_old (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->recent ? NIL : T;
}


static char 
carmel_search_recent (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->recent ? T : NIL;
}


static char 
carmel_search_seen (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->seen ? T : NIL;
}


static char 
carmel_search_unanswered (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->answered ? NIL : T;
}


static char 
carmel_search_undeleted (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->deleted ? NIL : T;
}


static char 
carmel_search_unflagged (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->flagged ? NIL : T;
}


static char 
carmel_search_unkeyword (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return T;			/* keywords not supported yet */
}


static char 
carmel_search_unseen (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return MC(msgno)->seen ? NIL : T;
}

static char 
carmel_search_before (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return (char) (carmel_msgdate (stream,msgno) < n);
}


static char 
carmel_search_on (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  return (char) (carmel_msgdate (stream,msgno) == n);
}


static char 
carmel_search_since (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
				/* everybody interprets "since" as .GE. */
  return (char) (carmel_msgdate (stream,msgno) >= n);
}


static unsigned long 
carmel_msgdate (stream,msgno)
	MAILSTREAM *stream;
	long msgno;
{
  struct stat sbuf;
  struct tm *tm;
  MESSAGECACHE *elt = MC(msgno);

  return (long) (elt->year << 9) + (elt->month << 5) + elt->day;
}

static char 
carmel_search_body (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  char *t =  carmel_fetchtext(stream,msgno);
  return (t && search (t,strlen (t),d,n)) ||
    carmel_search_body (stream,msgno,d,n);
}


static char 
carmel_search_subject (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  char *t = carmel_fetchstructure (stream,msgno,NULL)->subject;
  return t ? search (t,strlen (t),d,n) : NIL;
}


static char 
carmel_search_text (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  char *t = carmel_fetchheader (stream,msgno);
  return (t && search (t,strlen (t),d,n)) ||
    carmel_search_body (stream,msgno,d,n);
}

static char 
carmel_search_bcc (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  carmel_20k_buf[0] = '\0';
				/* get text for address */
  rfc822_write_address (carmel_20k_buf,
			carmel_fetchstructure (stream,msgno,NULL)->bcc);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel_search_cc (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  carmel_20k_buf[0] = '\0';
				/* get text for address */
  rfc822_write_address (carmel_20k_buf,
			carmel_fetchstructure (stream, msgno, NULL)->cc);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel_search_from (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  carmel_20k_buf[0] = '\0';
				/* get text for address */
  rfc822_write_address (carmel_20k_buf,
			carmel_fetchstructure (stream,msgno,NULL)->from);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel_search_to (stream,msgno,d,n)
	MAILSTREAM *stream;
	long msgno;
	char *d;
	long n;
{
  carmel_20k_buf[0] = '\0';
				/* get text for address */
  rfc822_write_address (carmel_20k_buf,
			carmel_fetchstructure (stream, msgno, NULL)->to);
  return search (carmel_20k_buf,strlen (carmel_20k_buf),d,n);
}

/* Search parsers */


/* Parse a date
 * Accepts: function to return
 *	    pointer to date integer to return
 * Returns: function to return
 */

static search_t carmel_search_date (f,n)
	search_t f;
	long *n;
{
  long i;
  char *s;
  MESSAGECACHE elt;
				/* parse the date and return fn if OK */
  return (carmel_search_string (f,&s,&i) && mail_parse_date (&elt,s) &&
	  (*n = (elt.year << 9) + (elt.month << 5) + elt.day)) ? f : NIL;
}

/* Parse a flag
 * Accepts: function to return
 *	    pointer to string to return
 * Returns: function to return
 */

static search_t 
carmel_search_flag (f,d)
	search_t f;
	char **d;
{
				/* get a keyword, return if OK */
  return (*d = strtok (NIL," ")) ? f : NIL;
}


/* Parse a string
 * Accepts: function to return
 *	    pointer to string to return
 *	    pointer to string length to return
 * Returns: function to return
 */

static search_t 
carmel_search_string (f,d,n)
	search_t f;
	char **d;
	long *n;
{
  char *c = strtok (NIL,"");	/* remainder of criteria */
  if (c) {			/* better be an argument */
    switch (*c) {		/* see what the argument is */
    case '\0':			/* catch bogons */
    case ' ':
      return NIL;
    case '"':			/* quoted string */
      if (!(strchr (c+1,'"') && (*d = strtok (c,"\"")) && (*n = strlen (*d))))
	return NIL;
      break;
    case '{':			/* literal string */
      *n = strtol (c+1,&c,10);	/* get its length */
      if (*c++ != '}' || *c++ != '\015' || *c++ != '\012' ||
	  *n > strlen (*d = c)) return NIL;
      c[*n] = '\255';		/* write new delimiter */
      strtok (c,"\255");	/* reset the strtok mechanism */
      break;
    default:			/* atomic string */
      *n = strlen (*d = strtok (c," "));
      break;
    }
    return f;
  }
  else return NIL;
}





   
/*----------------------------------------------------------------------
   Some stuff to help out with the date parsing
 ---*/  
char *xdays2[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *
month_abbrev2(month_num)
     int month_num;
{
    static char *xmonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    if(month_num < 1 || month_num > 12)
      return("xxx");
    return(xmonths[month_num - 1]);
}

struct time_zone_names {
    char *name;
    int   hours;
    int    minutes;
} tz_names[]  = {
    {"GMT",   0,   0},
    {"PST",  -8,   0},
    {"PDT",  -7,   0},
    {"MST",  -7,   0},
    {"MDT",  -6,   0},
    {"CST",  -6,   0},
    {"CDT",  -5,   0},
    {"EST",  -5,   0},
    {"EDT",  -4,   0},
    {"JST",   9,   0},
    {"IST",   2,   0},
    {"IDT",   3,   0},
    {NULL,    0,   0}};
    
/*----------------------------------------------------------------------
   Parse a date string into into a structure
 
Args: mc -- mesage cache to with structure to receive data
      given_date -- full header with date string somewhere to be found

This parses a number of date formats and produces a cannonical date in
a structure. The basic format is:
   
 dd mm yy hh:mm:ss.t tz   

It will also handle:
  ww dd mm yy hh:mm:ss.t tz     mm dd yy hh:mm:ss.t tz
  ww dd mm hh:mm:ss.t yy tz     mm dd hh:mm:ss.t yy tz

It knows many abbreviations for timezones, but is not complete.
In general absolute time zones in hours +/- GMT are best.
  ----*/
void
carmel_rfc822_date(mc, given_date)
     char        *given_date;
     MESSAGECACHE *mc;
{
    char *p, **i, *q;
    int   month, year;
    struct time_zone_names *tz;

    mc->seconds  = 0;
    mc->minutes  = 0;
    mc->hours    = 30;
    mc->day      = 0;
    mc->month    = 0;
    mc->year     = 0;
    mc->zhours   = 0;
    mc->zminutes = 0;

    if(given_date == NULL)
      return;

    p = given_date;

    if(*p != 'D' && strncmp(p, "Date:",5))
      while(*p) {
          if(*p == '\n' && *(p+1) == 'D' && strncmp(p+1, "Date:", 5) == 0)
	    break;
	 p++;
      }
    if(!*p)
      return;

    p += 6; /* Skip "\nDate: " */
    while(isspace(*p))
      p++;
	   
    /* Start with month, weekday or day ? */
    for(i = xdays2; *i != NULL; i++) 
      if(struncmp2(p, *i, 3) == 0) /* Match first 3 letters */
        break;
    if(*i != NULL) {
        /* Started with week day .. buz over it*/
        while(*p && !isspace(*p) && *p != ',')
          p++;
        while(*p && (isspace(*p) || *p == ','))
          p++;
    }
    if(isdigit(*p)) {
        mc->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }
    for(month = 1; month <= 12; month++)
      if(struncmp2(p, month_abbrev2(month), 3) == 0)
        break;
    if(month < 13) {
        mc->month = month;

    } 
    /* Move over month, (or whatever is there) */
    while(*p && !isspace(*p) && *p != ',' && *p != '-')
       p++;
    while(*p && (isspace(*p) || *p == ',' || *p == '-'))
       p++;

    /* Check again for day */
    if(isdigit(*p) && mc->day == -1) {
        mc->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }

    /*-- Check for time --*/
    for(q = p; *q && isdigit(*q); q++);
    if(*q == ':') {
        /* It's the time (out of place) */
        mc->hours = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
            mc->minutes = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
                mc->seconds = atoi(p);
                while(*p && !isspace(*p))
                  p++;
            }
        }
        while(*p && isspace(*p))
          p++;
    }
    
    /* Get the year 0-50 is 2000-2050; 50-100 is 1950-1999 and
                                           101-9999 is 101-9999 */
    if(isdigit(*p)) {
        year = atoi(p);
        if(year < 50)   
          year += 2000;
        else if(year < 100)
          year += 1900;
	mc->year = year - 1969; 
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
    if(*q == ':' && mc->hours == 30) {
        mc->hours = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
	    p++;
            mc->minutes = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
		p++;
                mc->seconds = atoi(p);
                while(*p && !isspace(*p) && *p != '+' && *p != '-')
                  p++;
            }
        }
    }
    while(*p && isspace(*p))
      p++;


    /*-- The time zone --*/
    if(*p) {
        if(*p == '+' || *p == '-') {
            char tmp[3];
            mc->zoccident = (*p == '+' ? 1 : 0);
            p++;
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            mc->zhours = atoi(tmp);
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            mc->zminutes *= atoi(tmp);
        } else {
	    for(tz = tz_names; tz->name != NULL; tz++) {
		if(struncmp2(p, tz->name, strlen(tz->name)) ==0) {
		    if(tz->hours >= 0) {
			mc->zhours = tz->hours;
			mc->zoccident = 1;
		    } else {
			mc->zhours    = -tz->hours;
			mc->zoccident = 0;
		    }
		    mc->minutes = tz->minutes;
		    break;
		}
	    }
        }
    }
    if(mc->hours == 30)
      mc->hours = 0;
}


      
/*----------------------------------------------------------------------
    Print the date from the MESSAGECACHE into the string
 ----*/
static void
carmel_date2string(string, mc)
     char         *string;
     MESSAGECACHE *mc;
{
    sprintf(string, "%d %s %d %d:%02d:%02d %s%04d",
	    mc->day, month_abbrev2(mc->month), 1969+mc->year,
	    mc->hours, mc->minutes, mc->seconds, mc->zoccident ? "-" : "",
	    mc->zhours * 100 + mc->zminutes);
}



/*----------------------------------------------------------------------
    Read the date into a structure from line in Carmel index

 Args: mc     -- The structure to contain the date (and other things)
       string -- String to be parsed. Format is:
                  "yyyy^Amm^Add^Ahh^Amm^Ass^Azh^Azm"

 ----*/
static void
carmel_parse_date(mc, string)
     MESSAGECACHE *mc;
     char         *string;
{
    int n;
    mc->year    = next_num(&string) - 1969;
    mc->month   = next_num(&string);
    mc->day     = next_num(&string);
    mc->hours   = next_num(&string);
    mc->minutes = next_num(&string);
    mc->seconds = next_num(&string);

    n = next_num(&string);
    if(n < 0) {
        mc->zoccident = 0;
	mc->zhours    = -n;
    } else {
	mc->zoccident = 1;
	mc->zhours    = n;
    }
    mc->zminutes = next_num(&string);      
}



/*----------------------------------------------------------------------
   Read the next number out of string and return it, advancing the string
  ----*/
static
next_num(string)
     char **string;
{
    int   n;
    char *p;

    if(string == NULL)
      return(0);

    p = *string;
    n = atoi(p);
    while(*p > '\001')
      p++;
    if(*p)
      *string = p+1;
    else
      *string = NULL;
    return(n);
}



    
/*--------------------------------------------------
     A case insensitive strcmp()     
  
   Args: o, r -- The two strings to compare

 Result: integer indicating which is greater
  ---*/
strucmp2(o, r)
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
struncmp2(o, r, n)
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

