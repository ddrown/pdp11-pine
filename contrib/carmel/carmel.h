/*----------------------------------------------------------------------

    T H E   C A R M E L   M A I L   F I L E   D R I V E R

 Author(s):   Laurence Lundblade
              Baha'i World Centre
              Data Processing
              Haifa, Israel
	      Internet: lgl@cac.washington.edu or laurence@bwc.org
              September 1992

  ----------------------------------------------------------------------*/

/* Command bits from carmel_getflags() */

#define fSEEN 1
#define fDELETED 2
#define fFLAGGED 4
#define fANSWERED 8


#define READ_LOCK   0
#define WRITE_LOCK  1

/* Carmel I/O stream local data */

typedef struct _local {
  MESSAGECACHE **mc_blocks;
  long           cache_size;
  FILE          *index_stream;
  char          *msg_buf;
  unsigned long  msg_buf_size;
  unsigned long  msg_buf_text_offset;
  char          *msg_buf_text_start;
  char          *buffered_file;
  char          *host;
  char          *home_dir;
  long           read_lock_mod_time, write_lock_mod_time;
  long           index_size;
  unsigned int   dirty:1;
  unsigned int   pod:1;  /* Is the really a pod mail file? */
  unsigned int   buffered_header_and_text:1;
  unsigned int   new_file_on_copy:1;
  char        *(*calc_paths)();
  int          (*aux_copy)();
} CARMELLOCAL;


#define MC(x) (&(LOCAL->mc_blocks[(x) >> 8][(x) & 0xff]))
#define LOCAL ((CARMELLOCAL *) stream->local)
#define CARMEL_MAXMESSAGESIZE 20000000 /* 20Mb */
#define CARMEL_MAX_HEADER     64000    /* 64K for DOS */
#define CARMEL_DIR  "cmmail"  /* Directory in users home */
#define CARMEL_PATHBUF_SIZE  1024

#define BEZERKLOCKTIMEOUT 5

/* Function prototypes */

DRIVER	    *carmel_valid();
int	     carmel_isvalid();
char	    *carmel_file();
void	     carmel_find();
void	     carmel_find_bboards();
MAILSTREAM  *carmel_open();
int          carmel_open2();
void         carmel_close();
void         carmel_fetchfast();
void         carmel_fetchflags();
ENVELOPE    *carmel_fetchstructure();
char        *carmel_fetchheader();
char        *carmel_fetchtext();
char        *carmel_fetchbody();
void         carmel_setflag();
void         carmel_clearflag();
void         carmel_search();
long         carmel_ping();
void         carmel_check();
void         carmel_expunge();
long         carmel_copy();
long         carmel_move();
void         carmel_gc();
void        *carmel_cache();
long         carmel_append();

int          carmel_write_index();
char        *carmel_readmsg();
int          carmel_append2();
int          carmel_lock();
void         carmel_unlock();
int          carmel_update_lock();
int          carmel_check_dir();
void         carmel_parse_bezerk_status();
char        *month_abbrev2();
void         carmel_rfc822_date();
int          strucmp2();
int          struncmp2();
