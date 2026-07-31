/*
 * Stub implementations for pine-internal functions that
 * fuzz/consumer_fuzz.c's target functions (format_envelope(),
 * describe_mime(), display_parameters(), and reply.c's
 * pretty_addr_string()) never actually reach at runtime, but whose
 * containing object files (mailview.o, reply.o, filter.o) still
 * reference -- screen drawing, command dispatch, the composer
 * subprocess, and live-mailbox operations. None of these are part of
 * what this harness is fuzzing; they just need to exist so the linker
 * is happy. Mirrors fuzz/mm_stubs.c's approach for c-client's mm_*
 * callbacks.
 *
 * grow_tmp_20k_buf() is the one exception -- it's given a real,
 * faithful implementation (mirroring pine/pine.c's own), since
 * display_parameters() genuinely depends on it growing tmp_20k_buf
 * correctly; a no-op stub here would turn a real bug into a harness
 * artifact (an overflow of an under-sized buffer this harness itself
 * failed to grow, not one pine's real code would ever produce).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers.h"

struct pine  *ps_global;
char         *tmp_20k_buf;
static size_t tmp_20k_buf_size;
char          version_buff[50] = "pine-fuzz";

/* help-text arrays: real ones live in the generated pine/helptext.c
   (built from pine/pine.hlp), not linked here -- empty is fine, since
   nothing in this harness's call graph ever displays help text. */
char *h_mail_view[] = { NULL };
char *h_mail_text_att_view[] = { NULL };
char *h_oe_searchview[] = { NULL };

#define TMP_20K_BUF_BASELINE 20480

void grow_tmp_20k_buf(size_t need)
{
    size_t newsize;

    if (need <= tmp_20k_buf_size)
      return;

    newsize = tmp_20k_buf_size ? tmp_20k_buf_size : TMP_20K_BUF_BASELINE;
    while (newsize < need)
      newsize *= 2;

    tmp_20k_buf = tmp_20k_buf ? (char *)realloc(tmp_20k_buf, newsize)
                              : (char *)malloc(newsize);
    tmp_20k_buf_size = newsize;
}

/* real logic is trivial and display-only (which viewer/none can show
   a given MIME type) -- not memory-safety relevant, so just always
   say yes rather than link the real (os.c) version and its deps. */
int mime_can_display(int type, char *subtype, PARAMETER *params)
{
    return 1;
}

int BeginScroll(int a, int b) { return 0; }
void ClearScreen(void) { }
void CleartoEOLN(void) { }
void EndScroll(void) { }
void MoveCursor(int a, int b) { }
void PutLine0n8b(int a, int b, char *c, int d) { }
int ScrollRegion(int a) { return 0; }
void check_point_change(void) { }
void clear_index_cache(void) { }
void clear_index_cache_ent(long a) { }
void display_message(int a) { }
void do_suspend(struct pine *a) { }
void draw_keymenu(struct key_menu *a, BITMAP b, int c, int d, int e,
                   OtherMenu f, int g) { }
int have_job_control(void) { return 0; }
void helper(char *a[], char *b, int c) { }
void main_menu_screen(struct pine *a) { }
long new_mail(int *a, int b, int c) { return 0; }
int optionally_enter(char *a, int b, int c, int d, int e, int f, char *g,
                      ESCKEY_S *h, HelpType i, int j) { return 0; }
int process_cmd(int a, int b, int c, int *d) { return 0; }
void q_status_message(int a, int b, int c, char *d) { }
void q_status_message1(int a, int b, int c, char *d, void *e) { }
void q_status_message2(int a, int b, int c, char *d, void *e, void *f) { }
int read_command(void) { return 0; }
void save_attachment(int a, long b, struct attachment *c) { }
char *set_titlebar(char *a, int b, TitleBarType c, long d, int e, int f)
{ return NULL; }
int streams_died(void) { return 0; }
void update_titlebar_page(int a) { }
void update_titlebar_percent(int a) { }
int validatekeys(int a) { return 0; }
void warn_other_cmds(void) { }
char *detach(long a, BODY *b, char *c, long *d, gf_io_t e, filter_t *f)
{ return NULL; }
int address_is_us(ADDRESS *a, struct pine *b) { return 0; }
void build_path(char *a, size_t b, char *c, char *d) { if (a && b) a[0] = '\0'; }
int can_access(char *a, int b) { return -1; }
char *error_description(int a) { return "error"; }
char *fnexpand(char *a, int b) { return a; }
void pine_free_body(BODY **a) { }
void pine_send(ENVELOPE *a, BODY **b, char *c, char *d, long e) { }
int want_to(char *a, int b, int c, HelpType d, int e) { return 0; }
FILE *create_tmpfile(void) { return NULL; }
int fget_pos(FILE *a, fpos_t *b) { return -1; }
int fset_pos(FILE *a, fpos_t *b) { return -1; }
char *temp_nam(char *a, char *b) { return NULL; }
char *read_file(char *a) { return NULL; }
