/*
 * Program:	Support for running Pine's composer as a separate exec'd
 *		process (see TODO-pdp11.md / TODO-pdp11-done.md).
 *
 * When pine forks/execs the composer (`pico -P <handoff> <result>`,
 * see main.c), this file supplies everything the resulting process
 * needs that would otherwise come from pine directly: the handoff/
 * result file formats, and trivial standalone implementations of the
 * PICO struct's 8 callbacks (see pico.h). None of this links against
 * c-client -- the handoff/result files carry plain strings pine has
 * already built (via outgoing2strings()/strings2outgoing(), unchanged
 * on pine's side), and the message body crosses via a plain text file
 * (read with readin(), written back with writeout() -- see pico.c's
 * pico(), which takes that path when PICO.bodyfile is set).
 */
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"osdep.h"
#include	"pico.h"
#include	"estruct.h"
#include	"edef.h"
#include	"efunc.h"

#define	FIELDTAGLEN	31

static char **fcc_list = NULL;
static int    fcc_count = 0;


/*
 * write_field - write one length-prefixed field to a handoff/result file
 */
void
write_field(fp, tag, buf)
FILE *fp;
char *tag, *buf;
{
    long len = buf ? strlen(buf) : 0L;

    fprintf(fp, "%s %ld\n", tag, len);
    if(len)
      fwrite(buf, 1, (size_t)len, fp);
    fputc('\n', fp);
}


/*
 * read_field - read one length-prefixed field, checking its tag matches
 *              what the caller expects. Returns a malloc'd, NUL-terminated
 *              string (possibly empty), or NULL on a format/tag mismatch
 *              or short read.
 */
char *
read_field(fp, tag)
FILE *fp;
char *tag;
{
    char got_tag[FIELDTAGLEN + 1];
    char linebuf[FIELDTAGLEN + 24];
    long len;
    char *buf;

    if(fgets(linebuf, sizeof(linebuf), fp) == NULL)
      return(NULL);

    if(sscanf(linebuf, "%31s %ld", got_tag, &len) != 2
       || strcmp(got_tag, tag) != 0 || len < 0)
      return(NULL);

    if((buf = (char *)malloc((size_t)len + 1)) == NULL)
      return(NULL);

    if(len && fread(buf, 1, (size_t)len, fp) != (size_t)len){
	free(buf);
	return(NULL);
    }

    buf[len] = '\0';
    (void)fgetc(fp);		/* trailing newline written by write_field */
    return(buf);
}


/*
 * compose_read_handoff - read the handoff file pine wrote and build a
 *                        PICO struct from it. The 8 callbacks are left
 *                        for the caller (main.c) to wire to this file's
 *                        stub functions.
 */
PICO *
compose_read_handoff(path)
char *path;
{
    FILE *fp;
    PICO *pm;
    char *flagbuf;
    int   i;

    if((fp = fopen(path, "r")) == NULL)
      return(NULL);

    if((pm = (PICO *)calloc(1, sizeof(PICO))) == NULL){
	fclose(fp);
	return(NULL);
    }

    pm->pine_anchor  = read_field(fp, "ANCHOR");
    pm->pine_version = read_field(fp, "VERSION");
    pm->tobuf        = read_field(fp, "TO");
    pm->ccbuf        = read_field(fp, "CC");
    pm->bccbuf       = read_field(fp, "BCC");
    pm->subbuf       = read_field(fp, "SUBJECT");
    pm->fccbuf       = read_field(fp, "FCC");
    pm->bodyfile     = read_field(fp, "BODYFILE");
    flagbuf          = read_field(fp, "FLAGS");

    if(!pm->pine_anchor || !pm->pine_version || !pm->tobuf || !pm->ccbuf
       || !pm->bccbuf || !pm->subbuf || !pm->fccbuf || !pm->bodyfile
       || !flagbuf){
	fclose(fp);
	free(pm);
	return(NULL);
    }

    pm->tolen  = strlen(pm->tobuf);
    pm->cclen  = strlen(pm->ccbuf);
    pm->bcclen = strlen(pm->bccbuf);
    pm->sublen = strlen(pm->subbuf);
    pm->fcclen = strlen(pm->fccbuf);
    pm->pine_flags = (unsigned)atoi(flagbuf);
    free(flagbuf);

    /* FCCLIST <n>\n followed by n lines, one folder name each */
    {
	char listhdr[64];

	if(fgets(listhdr, sizeof(listhdr), fp) != NULL
	   && sscanf(listhdr, "FCCLIST %d", &fcc_count) == 1 && fcc_count > 0){
	    fcc_list = (char **)calloc((size_t)fcc_count, sizeof(char *));
	    for(i = 0; i < fcc_count; i++){
		char line[512];

		if(fgets(line, sizeof(line), fp) == NULL){
		    fcc_count = i;
		    break;
		}

		line[strcspn(line, "\n")] = '\0';
		if((fcc_list[i] = (char *)malloc(strlen(line) + 1)) != NULL)
		  strcpy(fcc_list[i], line);
	    }
	}
	else
	  fcc_count = 0;
    }

    fclose(fp);
    return(pm);
}


/*
 * compose_write_result - write the edited fields and final editor status
 *                        back for pine to read. The body itself already
 *                        went back to pm->bodyfile via pico()'s writeout()
 *                        call, so it isn't repeated here.
 */
void
compose_write_result(path, status, pm)
char *path;
int   status;
PICO *pm;
{
    FILE *fp;

    if((fp = fopen(path, "w")) == NULL)
      return;

    fprintf(fp, "STATUS %d\n", status);
    write_field(fp, "TO", pm->tobuf);
    write_field(fp, "CC", pm->ccbuf);
    write_field(fp, "BCC", pm->bccbuf);
    write_field(fp, "SUBJECT", pm->subbuf);
    write_field(fp, "FCC", pm->fccbuf);

    fclose(fp);
}


/*
 * ---- PICO struct callback stubs ----
 *
 * These replace pine's clear_cursor_pos/init_keyboard/Raw/helper/
 * new_mail/folders_for_fcc/build_address wiring (pine/send.c) for a
 * composer running as its own process: no pine mail state or c-client
 * is available here, only what pine put in the handoff file.
 */

void
compose_clearcur()
{
    /* Nothing to reset -- there's no separate mail-index screen
     * underneath this process to protect the cursor position of. */
}


void
compose_keybinit(fkeys)
int fkeys;
{
    /* Standalone terminal setup already happened, unconditionally,
     * in vtinit()/ttopen() -- nothing left to do per Pmaster. */
}


int
compose_raw_io(state)
int state;
{
    /* Only called by bktoshell()'s ^Z path. ttopen()/ttclose() already
     * skip touching termios whenever Pmaster is set (assuming it's
     * already correctly configured), so there's nothing to toggle --
     * the real toggle happens via ttclose()/ttopen() around the
     * SIGTSTP itself. */
    return(0);
}


void
compose_helper(text, title, from_composer)
char *text[], *title;
int   from_composer;
{
    emlwrite("No help screens in this build -- see the pine(1) man page",
	     NULL);
}


void
compose_showmsg(ch)
int ch;
{
    /* pine's display_message() replays queued status messages; there's
     * no such queue here, so there's nothing to show. */
}


long
compose_newmail(cursor_moved, force, flags)
int *cursor_moved;
int  force, flags;
{
    /* No live mail-index in this process to poll -- new mail surfaces
     * normally on return to the mail-reading process instead. */
    return(-1L);
}


int
compose_buildaddr(src, dest, errmsg)
char *src, *dest, **errmsg;
{
    /*
     * No c-client here to expand/canonicalize addresses -- copy the
     * text through unchanged. Full canonicalization still happens
     * back in pine (strings2outgoing()'s rfc822_parse_adrlist()) once
     * this process exits, so addresses still resolve correctly on
     * send/postpone; there's just no live on-screen feedback while
     * editing To/Cc/Bcc.
     */
    strcpy(dest, src);
    return(0);
}


int
compose_folders(return_string)
char *return_string;
{
    char prompt[256];
    char answer[256];
    int  i, n;

    if(fcc_count == 0){
	emlwrite("No folder list available", NULL);
	return(0);
    }

    strcpy(prompt, "Fcc");
    for(i = 0; i < fcc_count && i < 8; i++){
	char tag[40];

	sprintf(tag, " %d)%.20s", i + 1, fcc_list[i]);
	if(strlen(prompt) + strlen(tag) < sizeof(prompt) - 24)
	  strcat(prompt, tag);
    }
    strcat(prompt, " (number or name): ");

    answer[0] = '\0';
    if(mlreply(prompt, answer, sizeof(answer), 0) != TRUE || answer[0] == '\0')
      return(0);

    n = atoi(answer);
    if(n >= 1 && n <= fcc_count)
      strcpy(return_string, fcc_list[n - 1]);
    else
      strcpy(return_string, answer);

    return(1);
}
