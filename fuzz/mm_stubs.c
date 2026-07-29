/*
 * Minimal stub implementations of the mm_*() callbacks c-client
 * expects its calling application (normally pine, see pine/mailbox.c)
 * to provide. The fuzz harnesses only exercise pure parsing code
 * (rfc822.c), which never legitimately needs to talk to a real
 * terminal/user, so these just no-op or record to stderr.
 *
 * mm_fatal() aborts on purpose: c-client considers this an
 * unrecoverable internal condition, which is itself worth libFuzzer
 * catching and reporting as a crash for manual triage.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mail.h"
#include "osdep.h"

void mm_exists(MAILSTREAM *stream, long number) { }
void mm_expunged(MAILSTREAM *stream, long number) { }
void mm_flags(MAILSTREAM *stream, long number) { }
void mm_notify(MAILSTREAM *stream, char *string, long errflg) { }
void mm_mailbox(char *string) { }
void mm_bboard(char *string) { }
void mm_log(char *string, long errflg) { }
void mm_dlog(char *string) { }
void mm_login(char *host, char *user, char *pwd, long trial) { }
void mm_critical(MAILSTREAM *stream) { }
void mm_nocritical(MAILSTREAM *stream) { }
long mm_diskerror(MAILSTREAM *stream, long errcode, long serious) { return 1; }
void mm_fatal(char *string)
{
    fprintf(stderr, "mm_fatal: %s\n", string);
    abort();
}
