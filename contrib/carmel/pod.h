/*----------------------------------------------------------------------

         T H E   P O D   M A I L   D R I V E R

  Author(s):   Laurence Lundblade
               Baha'i World Centre
               Data Processing
               Haifa, Israel
 	       Internet: lgl@cac.washington.edu or laurence@bwc.org
               September 1992
  
  ----------------------------------------------------------------------*/


/* Most of the real stuff is defined in carmel.h */

/* Function prototypes */

DRIVER	   *pod_valid();
int	    pod_isvalid();
char	   *pod_file  ();
void        pod_find  ();
void        pod_find_bboards  ();
MAILSTREAM *pod_open  ();
void        pod_close();
char       *pod_calc_paths();
int         pod_copy();
int         pod_init();
long        pod_append();
long        pod_create();
long        pod_delete();
long        pod_rename();
