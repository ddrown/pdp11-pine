/*
 * $Id: efunc.h,v 4.6 1993/11/08 23:10:36 mikes Exp $
 *
 * Program:	Pine's composer and pico's function declarations
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1993  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 */
/*	EFUNC.H:	MicroEMACS function declarations and names

		This file list all the C code functions used by MicroEMACS
	and the names to use to bind keys to them. To add functions,
	declare it here in both the extern function list and the name
	binding table.

	Update History:

	Daniel Lawrence
*/

#ifndef	EFUNC_H
#define	EFUNC_H


/*	External function declarations		*/
/* attach.c */
extern	char *prettysz(long);

/* compose_stub.c */
extern	struct pico_struct *compose_read_handoff(char *);
extern	void  compose_write_result(char *, int, struct pico_struct *);
extern	void  compose_clearcur(void);
extern	void  compose_keybinit(int);
extern	int   compose_raw_io(int);
extern	void  compose_helper(char *[], char *, int);
extern	void  compose_showmsg(int);
extern	long  compose_newmail(int *, int, int);
extern	int   compose_buildaddr(char *, char *, char **);

/* basic.c */
extern	int gotobol(int, int);
extern	int backchar(int, int);
extern	int gotoeol(int, int);
extern	int forwchar(int, int);
extern	int gotoline(int, int);
extern	int gotobob(int, int);
extern	int gotoeob(int, int);
extern	int forwline(int, int);
extern	int backline(int, int);
extern	int gotobop(int, int);
extern	int gotoeop(int, int);
extern	int forwpage(int, int);
extern	int backpage(int, int);
extern	int setmark(int, int);
extern	int swapmark(int, int);
extern	int setimark(int, int);
extern	int swapimark(int, int);

/* bind.c */
extern	int whelp(int, int);
extern	int wscrollw(int, int, char **,int);
extern	int normal(int, int (*)[2],int);
extern	void rebindfunc(int (*a)(int, int),int (*)(int, int));

/* browse.c */
extern	int FileBrowse(char *, char *, char *);
extern	int ResizeBrowser(void);

/* buffer.c */
extern	int anycb(void);
extern	struct BUFFER *bfind(char *, int, int);
extern	int bclear(struct BUFFER *);
extern	int packbuf(char **, int *, int);
extern	int readbuf(char **);

/* composer.c */
extern	void InitMailHeader(struct pico_struct *);
extern	int ResizeHeader(void);
extern	int HeaderEditor(int, int);
extern	void PaintHeader(int, int);
extern	void ArrangeHeader(void);
extern	int ToggleHeader(int);
extern	int HeaderLen(void);
extern	int UpdateHeader(void);
extern	int entry_line(int, int);
extern	int resolve_niks(int);
extern	void ShowPrompt(void);
extern	int packheader(void);
extern	void zotheader(void);

/* display.c */
extern	int vtinit(void);
extern	void vttidy(void);
extern	int update(void);
extern	void modeline(struct WINDOW *);
extern	void movecursor(int, int);
extern	void mlerase(void);
extern	int mlyesno(char *, int);
extern	int mlreply(char *, char *, int, int);
extern	int mlreplyd(char *, char *, int, int);
extern	void emlwrite(char *, void *);
extern	void mlwrite(char *, void *);
extern	void scrolldown(struct WINDOW *, int, int);
extern	void scrollup(struct WINDOW *, int, int);
extern	void pprints(int, int);
extern	int doton(int *, unsigned int *);
extern	int resize_pico(int, int);
extern	void zotdisplay(void);
extern	void pputc(int, int);
extern	void pputs(char *, int);
extern	void peeol(void);
extern	CELL *pscr(int, int);
extern	void pclear(int, int);
extern	int pinsert(CELL);
extern	int pdel(void);
extern	void wstripe(int, int, char *, int);
extern	void wkeyhelp(char *, char *);

/* file.c */
extern	int fileread(int, int);
extern	int insfile(int, int);
extern	int readin(char *, int);
extern	int filewrite(int, int);
extern	int filesave(int, int);
extern	int writeout(char *);
extern	int filename(int, int);

/* fileio.c */
extern	int ffropen(char *);
extern	int ffputline(CELL *, int);
extern	int ffgetline(char *, int);

/* line.c */
extern	struct LINE *lalloc(int);
extern	void lfree(struct LINE *);
extern	void lchange(int);
extern	int linsert(int, int);
extern	int geninsert(LINE **, short *, LINE *, int, int, int);
extern	int lnewline(void);
extern	int ldelete(int, int);
extern	void kdelete(void);
extern	int kinsert(int);
extern	int kremove(int);

/* osdep.c */
extern	int ttopen(void);
extern	int ttclose(void);
extern	int ttisslow(void);
extern	int ttputc(char);
extern	int ttflush(void);
extern	int ttgetc(void);
extern	void ttgetwinsz();
extern	int GetKey(void);
#ifdef	JOB_CONTROL
extern	int bktoshell(int, int);
#endif
extern	int fallowc(int);
extern	int fexist(char *, char *, long *);
extern	int isdir(char *, long *);
extern	char *gethomedir(int *);
extern	char *errstr(int);
extern	char *getfnames(char *, int *);
extern	void fioperr(int, char *);
extern	void fixpath(char *, int);
extern	int compresspath(char *, char *, int);
extern	void makename(char *, char *);
extern	int copy(char *, char *);
extern	int ffwopen(char *);
extern	int ffclose(void);
extern	FILE *P_open(char *);
extern	int P_close(FILE *);
extern	int worthit(int *);
extern	int o_insert(char);
extern	int o_delete(void);
extern	int pico_new_mail(void);
extern	int time_to_check(void);
extern	int sstrcasecmp(const void *, const void *);
#ifdef	DOS
extern	int register_mfunc(unsigned long (*)(int,int,int),int,int,int,int);
extern	void clear_mfunc(void);
extern	unsigned long pico_mouse(int, int, int);
#endif

/* pico.c */
extern	int pico(struct pico_struct *);
extern	int edinit(char *);
extern	int execute(int, int, int);
extern	int quickexit(int, int);
extern	int abort_composer(int, int);
extern	int suspend_composer(int, int);
extern	int wquit(int, int);
extern	int ctrlg(int, int);
extern	int rdonly(void);
extern	int pico_help(char **, char *, int);
extern	int zotedit(void);

/* random.c */
extern	int setfillcol(int, int);
extern	int showcpos(int, int);
extern	int tab(int, int);
extern	int newline(int, int);
extern	int forwdel(int, int);
extern	int backdel(int, int);
extern	int killtext(int, int);
extern	int yank(int, int);
extern  int getccol(int);

/* region.c */
extern	int killregion(int, int);
extern	int markregion(int);

/* search.c */
extern	int forwsearch(int, int);
extern	int readpattern(char *);
extern	int forscan(int *, char *, int);

/* window.c */
extern	int refresh(int, int);

/* word.c */
extern	int wrapword(void);
extern	int backword(int, int);
extern	int forwword(int, int);
extern	int fillpara(int, int);
extern	int inword(void);

/* tcap.c */
extern void o_scrollup(int row, int n);
extern void o_scrolldown(int row, int n);

/* region.c */
void unmarkbuffer();

#endif	/* EFUNC_H */
