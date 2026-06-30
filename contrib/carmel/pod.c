/*----------------------------------------------------------------------

         T H E   P O D   M A I L   D R I V E R

  Author(s):   Laurence Lundblade
               Baha'i World Centre
               Data Processing
               Haifa, Israel
 	       Internet: lgl@cac.washington.edu or laurence@bwc.org
               September 1992
  
The Pod mail format is home brew mail file format that has messages saved
in individual files and an index file that references them. It is probably
not of interest to anyone away from it's origin. 

This driver reads and writes the format in conjunction with the carmel
drivers. The carmel file driver does most of the work as most of the
operations are done on the created auxhiliary carmel index and the
pod index is updated when the mail file is closed

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
#include "pod.h"
#include "rfc822.h"
#include "misc.h"


/* Driver dispatch used by MAIL. The pod driver shares most of
   its data structures and functions with the carmel driver
 */

DRIVER poddriver = {
  (DRIVER *) NIL,		/* next driver */
  pod_valid,			/* mailbox is valid for us */
  pod_find,			/* find mailboxes */
  pod_find_bboards,		/* find bboards */
  pod_open,			/* open mailbox */
  pod_close,			/* close mailbox */
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
  pod_append,                   /* Append message to a mailbox */
  pod_create,
  pod_delete,
  pod_rename,
  "pod"
};



static int   pod_sift_files(); 
static int   vmail2carmel();
static void  pod_recreate_index();
static int   write_pod_index();
static int   pod_reset_index_list();



extern char carmel_20k_buf[20000], carmel_path_buf[], carmel_error_buf[];



/*----------------------------------------------------------------------
  Carmel mail validate mailbox

Args: name -- name to check

Returns: our driver if name is valid, otherwise calls valid in next driver
 ---*/

DRIVER *pod_valid (name)
	char *name;
{
  return pod_isvalid (name) ? &poddriver :
    (poddriver.next ? (*poddriver.next->valid) (name) : NIL);
}



/*----------------------------------------------------------------------
    Check and see if a named mailbox is a valid pod mail index

Args: name -- name or path of mail file

Returns:  0 if it is not a valid mailbox
          1 if it is.
      
   A pod index must be a regular file, readable, and have the second word in
the file be "index", upper or lower case.
  ----*/
int 
pod_isvalid (name)
	char *name;
{
    struct stat sbuf;
    int         fd;
    char       *p;
				  /* if file, get its status */
    if(*name == '{')
      return(0);  /* Check for imap folder*/
  
    pod_file(carmel_path_buf, name);
    if(stat(carmel_path_buf, &sbuf) < 0)
      return(0);
  
    if(!S_ISREG(sbuf.st_mode))
      return(0);
  
    fd = open(carmel_path_buf, O_RDONLY);
    if(fd < 0)
      return(0);
  
    if(read(fd, carmel_20k_buf, 200) <= 0)
      return(0);
    carmel_20k_buf[199] = '\0';
  
    close(fd);
  
    for(p = carmel_20k_buf; *p && !isspace(*p); p++); /* frist word */
    for(; *p && isspace(*p); p++);  /* space */
    lcase(p);                       /* lower case the "index" */
    if(!*p || strncmp(p, "index", 5))  
      return(0);
  
    return(1);
}



/*----------------------------------------------------------------------
   Carmel mail build file name

 Args: dst --  destination string
       name -- source

 Returns: it's first argument

This also maps the name "inbox" into the index "MAIL".
 ----*/
char *
pod_file (dst,name)
     char *dst;
     char *name;
{
    if (*name == '/')      /* absolute path? */
      strcpy (dst, name);
    else if(strucmp2(name, "inbox") == 0)
      sprintf (dst,"%s/.vmail/index/MAIL", getpwuid (geteuid ())->pw_dir);
    else
      sprintf (dst,"%s/.vmail/index/%s", getpwuid (geteuid ())->pw_dir, name);
    return dst;
}



/*----------------------------------------------------------------------
   This is used by scandir to determine which files in the directory
 are treated as mail files
  ----*/
static int
pod_sift_files(dir)
     struct direct *dir;
{
    if(dir->d_name[0] == '.')
      return(0);
    else if(strcmp(dir->d_name, "MAIL") == 0)
      return(0);
    else
      return(1);
}



/* ----------------------------------------------------------------------
  Pod find list of mail boxes
  Args: mail stream
        pattern to search
 
  This scans the ".vmail/index" directory for a list of folders
  (indexes in vmail terms).
 ----*/
void 
pod_find (stream, pat)
	MAILSTREAM *stream;
	char *pat;
{
    char            tmp[MAILTMPLEN];
    struct direct **namelist, **n;
    int             num;

    sprintf(tmp, "%s/.vmail/index", LOCAL->home_dir);
    num = scandir(tmp, &namelist, pod_sift_files, NULL);

   for(n = namelist; num > 0; num--, n++) {
	mm_mailbox((*n)->d_name);
	free(*n); 
    }
    free(namelist); 
}



/*----------------------------------------------------------------------
   Find bulliten boards is a no-op for pod, there are none
 */
void 
pod_find_bboards (stream,pat)
     MAILSTREAM *stream;
     char *pat;
{
  /* Always a no-op */
}



/*----------------------------------------------------------------------
  Pod mail open

 Args: stream 

 Returns: stream on success, NIL on failure.

The complex set of comparison determine if we get it for read, write or not at all.

The folder will be open for write if:
   - It is not locked
   - The pod index is writeable 
   - The carmel index is writeable
   - The carmel index is openable and not corrupt

The folder open will fail if:
   - The pod index does exist
   - The pod carmel index is missing
        AND the folder is locked or unwritable

The folder is opened readonly if:
   - It is locked
   - The carmel index is not writable
   - The pod index is not writable

 ----*/

MAILSTREAM *
pod_open (stream)
	MAILSTREAM *stream;
{
    char            pod_index_path[CARMEL_PATHBUF_SIZE];
    char            carmel_index_path[CARMEL_PATHBUF_SIZE];
    char            tmp[MAILTMPLEN], tmp2[MAILTMPLEN],  *mailbox;
    struct hostent *host_name;
    struct stat     pod_stat, carmel_stat;
    int             carmel_up_to_date;
    
    mailcache = carmel_cache;

    /* close old file if stream being recycled */
    if (LOCAL) {
        carmel_close (stream);		/* dump and save the changes */
        stream->dtb = &poddriver;	/* reattach this driver */
        mail_free_cache (stream);	/* clean up cache */
    }

    /* Allocate local stream */
    stream->local              = fs_get (sizeof (CARMELLOCAL));
    LOCAL->pod                 = 1;
    LOCAL->msg_buf             = NULL;
    LOCAL->msg_buf_size        = 0;
    LOCAL->buffered_file       = NULL;
    LOCAL->msg_buf_text_start  = NULL;
    LOCAL->msg_buf_text_offset = 0;
    LOCAL->dirty               = NIL;
    stream->msgno              = -1;
    stream->env                = NULL;
    stream->body               = NULL;
    stream->scache             = 1;
    LOCAL->calc_paths          = pod_calc_paths;
    LOCAL->aux_copy            = pod_copy;
    LOCAL->index_stream        = NULL;
    LOCAL->new_file_on_copy    = 0;

    
#ifdef BWC
    LOCAL->host = cpystr("bwc.org");
#else
    gethostname(tmp, MAILTMPLEN); /* get local host name */
    LOCAL->host = cpystr ((host_name = gethostbyname (tmp)) ?
                            host_name->h_name : tmp);
#endif

    /*----- Get the home directory ------*/
    LOCAL->home_dir = cpystr(getpwuid (geteuid ())->pw_dir);

    /*---- Sort out the mailbox name -----*/
    mailbox = cpystr(stream->mailbox);
    lcase(mailbox);
    if(strcmp(mailbox, "inbox") == 0){
        strcpy(mailbox, "MAIL"); /* Inbox for Pod mail format */
    } else {
        mailbox = cpystr(stream->mailbox); /* copy back regular case */
    }
    if(stream->mailbox != NULL)
      fs_give((void **)&(stream->mailbox));
    stream->mailbox = cpystr(mailbox);

    /*------ Figure out the file paths -------*/
    sprintf(pod_index_path, "%s/.vmail/index/%s", LOCAL->home_dir, mailbox);
    strcpy(carmel_index_path, pod_calc_paths(LOCAL->home_dir, mailbox, 0 , 0));

    /*------ Does the POD index exist? Fail if not ------*/
    if(stat(pod_index_path, &pod_stat) < 0) {
	sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
	mm_log(carmel_error_buf, ERROR);
        return(NULL);  /* FAIL, no pod index */
    }

    /*----- Determine if carmel index is up to date -----*/
    if(stat(carmel_index_path, &carmel_stat) >= 0 &&
       carmel_stat.st_mtime >= pod_stat.st_mtime)
      carmel_up_to_date = 1;
    else
      carmel_up_to_date = 0;

    /*------ Do we have write access to the Pod mail index? ----*/
    if(access(pod_index_path, W_OK) != 0)
      stream->readonly = 1;

    /*------ If POD index R/O and Carmel out of date then fail -----*/
    if(stream->readonly && !carmel_up_to_date) {
	mm_log("Can't open mailbox; Unable to write carmel index", ERROR);
        return(NULL);
    }

    /*-------- Case for R/O stream or failed lock ---------*/
    if(stream->readonly || carmel_lock(stream, mailbox, READ_LOCK) < 0) {
	/* If carmel is out of date here that's OK, we just will see old data */
	if(access(carmel_index_path, R_OK) == 0) {
	    stream->readonly = 1;
  	    goto open_it;
	} else {
	    /*-- Can't lock, and there's no carmel index, best to fail -*/
	    mm_log("Can't open mailbox, can't lock to create carmel index",
		   ERROR);
  	    return(NULL);
        }
    } 
    /* Index is now read locked */

    /*---- Got an up to date carmel index and write access too ----*/
    if(carmel_up_to_date)
      if(access(carmel_index_path, W_OK) == 0) {
          goto open_it;
      } else {
	  stream->readonly = 1;
	  goto open_it;
      }

    /*---- If needed recreation of carmel index fails, go readonly -----*/
    if(vmail2carmel(pod_index_path, carmel_index_path, stream, LOCAL->host,
		    mailbox) < 0) {
	carmel_unlock(stream, mailbox, READ_LOCK);
        stream->readonly = 1;
    }

  open_it:
    /*-- carmel_open2 shouldn't fail after all this check, but just in case -*/
    if(carmel_open2(stream, carmel_index_path) < 0) {
	/* carmel_open2 will take care of the error message */
	carmel_unlock(stream, mailbox, READ_LOCK);
	if (LOCAL->host) fs_give ((void **) &LOCAL->host);
	if (LOCAL->msg_buf) fs_give ((void **) &LOCAL->msg_buf);
				    /* nuke the local data */
	fs_give ((void **) &stream->local);
        return(NULL);
    }

    if(stream->readonly)
	mm_log("Mailbox opened READONLY", WARN);

    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,stream->recent);

    fs_give((void **)&mailbox);    
    return(stream);
}



/*----------------------------------------------------------------------
   Create a carmel index for a pod index
  
 Args: folder        -- the full path name existing pod folder
       carmel_folder -- the full path name of carmel index to create
       stream        -- MAILSTREAM to operate on
       host          -- host name
       
 Returns: 0 if all is OK
         -1 if it failed 

  This reads the Pod index, and the headers of the pod messages to 
construct entries for a carmel index.
 ----*/
static
vmail2carmel(folder, carmel_folder, stream, host, mailbox)
     MAILSTREAM   *stream;
     char         *folder, *carmel_folder;
     char         *host, *mailbox;
{
    FILE        *pod_stream;
    FILE        *carmel_stream;
    ENVELOPE    *e;
    BODY        *b;
    MESSAGECACHE mc;
    char         pathbuf[CARMEL_PATHBUF_SIZE], *message, *p, *save_subject;
    struct stat  st;

    mm_log("Recreating Pine index....", WARN);

    if(carmel_lock(stream, mailbox, WRITE_LOCK) < 0)
      return(-1);
    
    pod_stream = fopen(folder,"r");
    if(pod_stream == NULL) {
	carmel_unlock(stream, mailbox, WRITE_LOCK);
        return(-1);
    }

    carmel_stream = fopen(carmel_folder, "w");
    if(carmel_stream == NULL) {
	carmel_unlock(stream, mailbox, WRITE_LOCK);	
        return(-1);
    }

    fprintf(carmel_stream, "\254\312--CARMEL-MAIL-FILE-INDEX--\n");

    /* The first ....  line */
    fgets(carmel_20k_buf, sizeof(carmel_20k_buf), pod_stream); 

    while(fgets(carmel_20k_buf, sizeof(carmel_20k_buf), pod_stream) != NULL) {
	sprintf(pathbuf, "%s/.vmail/msg/%6.6s", getpwuid(geteuid())->pw_dir,
		carmel_20k_buf);
	mc.data1      = 0L;
	mc.data2      = atol(carmel_20k_buf); /* The file name/number */
	mc.deleted    = 0;
	mc.seen       = 1;
	mc.flagged    = 0;
	mc.answered   = 0;
	mc.recent     = 0;
	mc.user_flags = 0;
	if(stat(pathbuf, &st) >= 0 ||
	   stat(strcat(pathbuf, ".wid"), &st) >= 0) {
	    save_subject = cpystr(carmel_20k_buf + 27);
	    save_subject[strlen(save_subject) - 1] = '\0';
            mc.rfc822_size = st.st_size;
            message = carmel_readmsg(stream, 1, 0, mc.data2);
	    rfc822_parse_msg(&e, &b, message, strlen(message), "", 0,
			     host, carmel_20k_buf);
	    carmel_parse_bezerk_status(&mc, message);
	    carmel_rfc822_date(&mc, message);
	    if(e->subject == NULL ||
	     strncmp(save_subject,e->subject,strlen(save_subject))){
		if(e->subject != NULL)
		  fs_give((void **)(&(e->subject)));
		if(strlen(save_subject))
		  e->subject = cpystr(save_subject);
		else
		  e->subject = NULL;
	    }
	} else {
	    carmel_20k_buf[strlen(carmel_20k_buf) - 1] = '\0';
            e = mail_newenvelope();
	    e->subject = cpystr(carmel_20k_buf+27);
	    e->from  = mail_newaddr();
	    e->from->mailbox = cpystr(carmel_20k_buf+17);
	    for(p = e->from->mailbox; *p && !isspace(*p); p++);
	    *p = '\0';
	    mc.hours     = 1;
	    mc.minutes   = 0;
	    mc.seconds   = 0;
	    mc.zoccident = 0;
	    mc.zhours    = 0;
	    mc.zminutes  = 0;
	    mc.day       = 1;
	    mc.month     = 1;
	    mc.year      = 1;
	}
	carmel_write_index(e, &mc, carmel_stream);
	mail_free_envelope(&e);
    }

    fclose(pod_stream);
    fclose(carmel_stream);

    carmel_unlock(stream, mailbox, WRITE_LOCK);

    mm_log("Recreating Pine index.....Done", WARN);

    return(1);
}



#define TESTING /* Make a copy of pod index before rewriting */

/*----------------------------------------------------------------------`
   Close a pod mail folder

Args: stream -- Mail stream to close

This will cause the pod index to be recreated

  ----*/
void 
pod_close(stream)
	MAILSTREAM *stream;
{
    if (LOCAL) {			/* only if a file is open */
	if(LOCAL->index_stream != NULL) {
	    if(!stream->readonly) {
		carmel_check (stream);		/* dump final checkpoint */
		pod_recreate_index(stream);
                carmel_unlock(stream, stream->mailbox, READ_LOCK);
	    }
    
	    fclose(LOCAL->index_stream);

	    if (LOCAL->host) fs_give ((void **) &LOCAL->host);
	    if (LOCAL->msg_buf) fs_give ((void **) &LOCAL->msg_buf);
				    /* nuke the local data */
	    fs_give ((void **) &stream->local);
	}
        stream->dtb = NIL;		/* log out the DTB */
    }
}



/*----------------------------------------------------------------------
   Rewrite the pod index from the carmel index
 
Args: -- stream  to be recreated

BUG: could probably use some error checking here
  ----*/
static void
pod_recreate_index(stream)
     MAILSTREAM *stream;
{
#ifdef TESTING
    char          copy[1000];
#endif
    long          msgno;
    MESSAGECACHE *mc;
    ENVELOPE     *envelope;
    FILE         *pod_index;
    struct stat   sb;
    char         *cm_index_file;
    struct timeval tp[2];

    mm_critical(stream);

    sprintf(carmel_path_buf, "%s/.vmail/index/%s", LOCAL->home_dir,
    	stream->mailbox);

    /*---- Get the first index line out of old index ----*/
    *carmel_20k_buf = '\0';
    pod_index = fopen(carmel_path_buf, "r");
    if(pod_index != NULL) {
        fgets(carmel_20k_buf,sizeof(carmel_20k_buf), pod_index);
        fclose(pod_index);
    }

#ifdef TESTING
    /*--- Make a copy of the index ---*/
    sprintf(copy, "%s/.vmail/index/.%s.COD-FUR-%d",
    	LOCAL->home_dir, stream->mailbox, time(0));
    link(carmel_path_buf, copy);
    unlink(carmel_path_buf);
#endif		

    pod_index = fopen(carmel_path_buf, "w");

    if(pod_index == NULL) 
	goto bomb;
	
    if(fputs(carmel_20k_buf, pod_index) == EOF)
      goto bomb;
    
    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
        mc = MC(msgno);
    	envelope = carmel_fetchstructure(stream, msgno, NULL);
    	if(write_pod_index(pod_index, mc, envelope) < 0)
    	  goto bomb;
    }
    if(fclose(pod_index) == EOF)
      goto bomb;

    /*---- Get mod time of carmel index ---*/
    cm_index_file = (*(LOCAL->calc_paths))(LOCAL->home_dir,
					   stream->mailbox, 0, 0);
    if(stat(cm_index_file, &sb) >= 0) {
	tp[0].tv_sec  = sb.st_atime;
	tp[0].tv_usec = 0;
	tp[1].tv_sec  = sb.st_mtime;
	tp[1].tv_usec = 0;
    
	/*---- Set Pod index to have same mod time ---*/
	sprintf(carmel_path_buf, "%s/.vmail/index/%s", LOCAL->home_dir,
	    stream->mailbox);
	utimes(carmel_path_buf, tp);
    }
    mm_nocritical(stream);

    sprintf(carmel_error_buf, "Rewrote Pod index \"%s\" with %d messages",
	    stream->mailbox, msgno-1);
    mm_log(carmel_error_buf, ERROR);

    return;

  bomb:
    mm_nocritical(stream);
    sprintf(carmel_error_buf, "Error recreating pod index: %s",
   	    strerror(errno));
    mm_log(carmel_error_buf, ERROR);
}



/*----------------------------------------------------------------------
  Do all the file name generation

Needs work for absolute paths 
  ----*/
static char *
pod_calc_paths(home_dir, mailbox, data_file_num, maxname)
     char       *mailbox, *home_dir;
     int         data_file_num, maxname;
{
    static char path[1000], num[20];
    char       *p;

    if(data_file_num || maxname) {
	if(maxname)
	  strcpy(num, ".MAXNAME");
	else
	  sprintf(num, "msg/%d", data_file_num);
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
	    sprintf(path, "%s/.vmail/%s", home_dir, num);
	}
    } else {

	/*------- Generate an index file name ------*/
	if(*mailbox == '/')
          /* Needs work here */
	  return(NULL);
	else
	  sprintf(path,"%s/.vmail/index/.%s.cx", home_dir, mailbox);
    }

/*    sprintf(carmel_debug, "POD: calc_paths returning \"%s\"\n", path);
    mm_dlog(carmel_debug);  */

    return(path);
}



/*----------------------------------------------------------------------
    Copy pod messages, this is the auxhiliary copy, called from carmel_copy

Args:  local -- Fake CAMELLOCAL structure
       mailbox -- Destination mailbox
       e       -- envelope
       mc      -- MESSAGECACHE entry

  ----*/
pod_copy(local, mailbox, e, mc)
     CARMELLOCAL  *local;
     char         *mailbox;
     ENVELOPE     *e;
     MESSAGECACHE *mc;
{
    char path[1000];
    FILE *pod_index;
    int   new;
    struct stat sb;

    sprintf(path, "%s/.vmail/index/%s", local->home_dir, mailbox);
    if(stat(path, &sb) < 0)
      new = 1;
    else
      new = 0;

    pod_index = fopen(path, "a+");
    
    if(new)
      fprintf(pod_index, "%-13.13s index.....\n", mailbox);

    if(write_pod_index(pod_index, mc, e) < 0) {
	fclose(pod_index);
	return(-1);
    }

    if(fclose(pod_index) == EOF)
      return(-1);

    return(0);
}


    
/*----------------------------------------------------------------------
    Make sure all the directories are there and writable for pod

Args: folders_dir   - suggested directory, ignored by pod

Result: returns 0 if OK, -1 if not
 ----*/
pod_init(folders_dir)
     char *folders_dir;
{
    char  *p;
    FILE  *mail_file;
    struct stat sb;

    sprintf(carmel_path_buf, "%s/.vmail", getpwuid(geteuid())->pw_dir);
    carmel_check_dir(carmel_path_buf);

    p = carmel_path_buf + strlen(carmel_path_buf);
    strcpy(p, "/msg");
    carmel_check_dir(carmel_path_buf);

    strcpy(p, "/index");
    carmel_check_dir(carmel_path_buf);

    strcat(p, "/MAIL");
    if(stat(carmel_path_buf, &sb) < 0) {
        mail_file = fopen(carmel_path_buf, "w");
	if(mail_file == NULL) {
	    mm_log("Error creating \"MAIL\" folder", WARN);
	} else {
 	    fprintf(mail_file, "MAIL          index.....\n");
	    fclose(mail_file);
	}
    }

    strcpy(p, ".MAXNAME");
    if(stat(carmel_path_buf, &sb) < 0) {
	mail_file = fopen(carmel_path_buf, "w");
	if(mail_file == NULL) {
	    mm_log("Error creating .MAXNAME file");
	} else {
	    fprintf(mail_file, "100000");
	    fclose(mail_file);
	}
    }
    return(0);
}



/*----------------------------------------------------------------------
   Write an entry into a pod index
 
Args: file -- The open file stream
      mc   -- the MESSAGECACHE 
      envelope -- The envelope to write

Returns: 0 if all is OK, -1 if not
  ----*/	
static 	
write_pod_index(file, mc, envelope)
     FILE         *file;
     MESSAGECACHE *mc;
     ENVELOPE     *envelope;
{
    if(fprintf(file, "%6d %02d %-.3s %2d %-9.9s %-.50s\n",
	       mc->data2, mc->day,
	       month_abbrev2(mc->month),
	       (mc->year + 1969) % 100,
	       envelope != NULL && envelope->from != NULL &&
	       envelope->from->mailbox != NULL ?
	         envelope->from->mailbox :  "",
	       envelope != NULL && envelope->subject != NULL ?
	         envelope->subject : "") == EOF)
      return(-1);
    else
      return(0);
}



/*----------------------------------------------------------------------
  ----*/    
long
pod_append(stream, mailbox, message)
     MAILSTREAM *stream;
     char       *mailbox,
     STRING     *message;
{
    CARMELLOCAL local;

    /*---- A fake local data structure to pass to other functions---*/
    local.calc_paths = pod_calc_paths;
    local.home_dir   = cpystr(getpwuid (geteuid ())->pw_dir);
    local.pod        = 1;
    local.host       = ""; /* BUG need some code here */
    local.aux_copy   = pod_copy;

    carmel_append2(&local, mailbox, message);
}



/*----------------------------------------------------------------------
  ----*/
long
pod_create(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
    char *mb, *path;
    struct stat sb;
    FILE *f;

    mb = cpystr(mailbox);
    lcase(mb);
    if(strcmp(mb, "inbox") == 0) {
	fs_give((void **)&mb);
        return(NIL);
    }
    fs_give((void **)&mb);

    if(*mailbox != '/')
      sprintf(carmel_path_buf, "%s/.vmail/index/%s",
	      getpwuid (geteuid ())->pw_dir, mailbox);
    else
      strcpy(carmel_path_buf, mailbox);

    if(stat(carmel_path_buf, &sb) >= 0)
      return(NIL);

    f = fopen(carmel_path_buf, "w");
    if(f == NULL) 
      goto bomb;

    if(fprintf(f, "%-13.13s index.....\n", mailbox) == EOF)
      goto bomb;

    if(fclose(f) == EOF)
      goto bomb;

    pod_reset_index_list();
    return(T);

  bomb:
/*    sprintf(carmel_error_buf, "Error creating mailbox %s: %s",
	    mailbox, strerror(errno));
    mm_log(carmel_error_buf, ERROR); */
    return(NIL);
}


/*----------------------------------------------------------------------
  ----*/
long
pod_delete(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
    char *mb, *path;
    
    mb = cpystr(mailbox);
    lcase(mb);
    if(strcmp(mb, "inbox") == 0) {
	fs_give((void **)&mb);
        return(NIL);
    }
    fs_give((void **)&mb);

    /*----- First the Pod index ------*/
    if(*mailbox != '/')
      sprintf(carmel_path_buf, "%s/.vmail/index/%s",
	      getpwuid (geteuid ())->pw_dir, mailbox);
    else
      strcpy(carmel_path_buf, mailbox);

    if(unlink(carmel_path_buf) < 0) {
/*	sprintf(carmel_error_buf, "Error deleting mailbox %s: %s",
		mailbox, strerror(errno));
	mm_log(carmel_error_buf, ERROR); */
	return(NIL);
    }

    /*------ Second the carmel index, quietly -----*/
    path = pod_calc_paths(cpystr(getpwuid (geteuid ())->pw_dir),
			  mailbox, 0, 0);
    unlink(path);

    pod_reset_index_list();
    return(T);
}



/*----------------------------------------------------------------------
  ----*/
long
pod_rename(stream, orig, new)
     MAILSTREAM *stream;
     char       *orig;
     char       *new;
{
    char path_buf[CARMEL_PATHBUF_SIZE], *new_path;

    /*---- First the Pod index -----*/
    sprintf(carmel_path_buf, "%s/.vmail/index/%s",
	      getpwuid (geteuid ())->pw_dir, orig);    

    sprintf(path_buf, "%s/.vmail/index/%s",
	      getpwuid (geteuid ())->pw_dir, new);

    if(rename(carmel_path_buf, path_buf)< 0) 
	return(NIL);

    /*----- Next the Carmel index, quietly ------*/
    strcpy(carmel_path_buf, pod_calc_paths(getpwuid (geteuid ())->pw_dir,
					   orig, 0, 0));
    new_path = pod_calc_paths(getpwuid (geteuid ())->pw_dir, new, 0, 0);
    rename(carmel_path_buf, new_path);

    pod_reset_index_list();
    return(T);
}



/*----------------------------------------------------------------------
  This really just removes the file Pod uses for the list of 
 folders, because Pod will just recreate it from the indexes themselves.
  ----*/
static 
pod_reset_index_list()
{
    sprintf(carmel_path_buf, "%s/.inda", getpwuid (geteuid ())->pw_dir);
    unlink(carmel_path_buf);

    sprintf(carmel_path_buf, "%s/.indf", getpwuid (geteuid ())->pw_dir);
    unlink(carmel_path_buf);
}

    
