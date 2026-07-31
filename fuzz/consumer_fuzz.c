/*
 * libFuzzer harness for pine's own envelope/MIME-structure display
 * code -- "harness 2" from TODO-pdp11.md's fuzzing item.
 *
 * Harness 1 (fuzz/rfc822_fuzz.c) stops once rfc822_parse_msg() has
 * produced a valid ENVELOPE/BODY -- it never exercises the code that
 * actually *renders* one. That's exactly where CVE-2000-0909's
 * recurrence lived (pine/newmail.c's new_mail_mess(), several calls
 * downstream of the parser -- see TODO-pdp11-done.md). This harness
 * picks up where harness 1 stops: reuse rfc822_parse_msg() (same
 * header/body splitting convention as rfc822_fuzz.c) to get a *real*,
 * parser-produced ENVELOPE/BODY -- that already gives arbitrarily
 * long/weird field content (rfc822_cpy()/cpystr() never cap anything
 * upstream), so there's no need to hand-roll a separate
 * FuzzedDataProvider-style splitter to get "interesting" structures --
 * then feed the result straight into pine's own consuming/display
 * functions:
 *
 *   - pine/mailview.c's format_envelope() (Date/From/To/Cc/Bcc/
 *     Reply-To/Newsgroups/Subject formatting for the message view),
 *     which calls pine/reply.c's pretty_addr_string() for every
 *     address field -- the line-wrapping address formatter.
 *   - pine/mailview.c's describe_mime() (recursively walks the whole
 *     MIME part tree building the attachment list), which calls
 *     type_desc()/format_mime_size()/mime_can_display() on each leaf
 *     part.
 *   - pine/mailview.c's display_parameters() on the top-level body's
 *     Content-Type parameters.
 *
 * None of these touch a live MAILSTREAM (unlike e.g. format_message(),
 * which calls mail_fetchtext() on a real stream -- out of scope for
 * this harness), so they're callable directly against a hand-produced
 * ENVELOPE/BODY without needing to fake a whole mailbox driver.
 * Everything else mailview.o/reply.o/filter.o reference but this
 * harness never actually reaches at runtime (screen drawing, command
 * dispatch, the composer, live-mailbox operations) is stubbed out in
 * fuzz/pine_ui_stubs.c.
 *
 * Build via fuzz/build.sh, or see it for the exact command.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "headers.h"

extern void describe_mime(BODY *, char *, int, int);
extern void zero_atmts(struct attachment *);
extern void format_envelope(ENVELOPE *, gf_io_t);
extern void display_parameters(PARAMETER *);

static int discard_putc(int c)
{
    return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *msg, *header, *bodyptr, *tmp;
    unsigned long headersize, bodysize;
    size_t i;
    ENVELOPE *env = NULL;
    BODY *body = NULL;
    STRING bs;

    if (size == 0)
      return 0;

    /* same NUL-truncation convention as rfc822_fuzz.c -- see its
       header comment for why (mirrors bezerk_parse()'s end-of-message
       handling, so this harness doesn't "find" crashes on byte
       sequences bezerk.c would never hand rfc822_parse_msg() in real
       operation). */
    for (i = 0; i < size; i++)
      if (data[i] == '\0')
        break;
    size = i;
    if (size == 0)
      return 0;

    msg = malloc(size);
    memcpy(msg, data, size);

    header = msg;
    headersize = size;
    bodyptr = NULL;
    bodysize = 0;
    for (i = 0; i + 1 < size; i++) {
        if (msg[i] == '\n' && msg[i+1] == '\n') {
            headersize = i + 1;
            bodyptr = msg + i + 2;
            bodysize = size - (i + 2);
            break;
        }
    }

    memset(&bs, 0, sizeof(bs));
    INIT(&bs, mail_string, (void *)(bodyptr ? bodyptr : ""), bodysize);
    tmp = malloc((headersize > bodysize ? headersize : bodysize) + 1);

    rfc822_parse_msg(&env, &body, header, headersize, &bs, "localhost", tmp);

    if (env)
      format_envelope(env, (gf_io_t) discard_putc);

    if (body) {
        /* mirrors mailview.c's own pairing (format_message(), just
           before its own describe_mime() call) -- ps_global->atmts is
           allocated once, up front, in main() below, and reused
           (grown, never shrunk) across iterations; zero_atmts() frees
           each iteration's individual description strings without
           freeing the array itself. */
        zero_atmts(ps_global->atmts);
        describe_mime(body, "", 1, 1);
        display_parameters(body->parameter);
    }

    mail_free_envelope(&env);
    mail_free_body(&body);
    free(tmp);
    free(msg);
    return 0;
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    /* mirrors pine/pine.c's own init_vars()-adjacent setup (see
       pine.c's ps_global->atmts/atmts_allocated initialization) --
       describe_mime() grows this via fs_resize() as needed, same as
       real operation. */
    ps_global = (struct pine *) calloc(1, sizeof(struct pine));
    ps_global->atmts = (struct attachment *) fs_get(sizeof(struct attachment));
    ps_global->atmts_allocated = 1;
    ps_global->atmts->description = NULL;
    return 0;
}
