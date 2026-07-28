/*
 * libFuzzer harness for imap/ANSI/c-client/rfc822.c's rfc822_parse_msg()
 * -- "harness 1" from TODO-pdp11.md's fuzzing item.
 *
 * This is the real trust boundary for this project: any bytes that
 * end up in the local mbox (via sendmail/UUCP) get parsed here before
 * pine ever sees an ENVELOPE/BODY. Treats the whole fuzzer input as
 * one raw RFC822 message and splits it into header/body at the first
 * blank line, mirroring how bezerk.c itself finds that boundary
 * before calling rfc822_parse_msg() (see bezerk_parse_line()'s
 * `case '\n': /' end of header '/ m->body = ++s;`).
 *
 * Build (from repo root, after a normal `./build lnx` has produced
 * imap/ANSI/c-client/c-client.a) via fuzz/build.sh, or directly:
 *
 *   clang -fsanitize=fuzzer,address -g \
 *       -I imap/ANSI/c-client \
 *       fuzz/rfc822_fuzz.c fuzz/mm_stubs.c imap/ANSI/c-client/c-client.a \
 *       -o fuzz/rfc822_fuzz
 *
 * Run against the curated (tracked) seed corpus, saving anything new
 * libFuzzer discovers into fuzz/findings/ (gitignored) instead of
 * growing the tracked corpus in place:
 *
 *   mkdir -p fuzz/findings
 *   fuzz/rfc822_fuzz fuzz/findings fuzz/corpus/rfc822
 *
 * Any crash gets written to ./crash-<hash> (also gitignored) -- copy
 * genuinely interesting ones into fuzz/corpus/rfc822 by hand once
 * fixed, so they become permanent regression seeds.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mail.h"
#include "osdep.h"
#include "rfc822.h"

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

    /* bezerk.c's own message-boundary scan (bezerk_parse()) treats an
       embedded NUL byte in the raw mbox bytes as end-of-message (see
       its `case '\0': /' end of message '/`) and truncates m->header/
       m->headersize there -- so rfc822_parse_msg() never actually
       sees a NUL byte inside its headersize-bounded region in real
       operation. Truncate here too, or the fuzzer "finds" crashes
       that are really just this harness feeding rfc822_parse_msg()
       byte sequences bezerk.c itself would never hand it. */
    for (i = 0; i < size; i++)
      if (data[i] == '\0')
        break;
    size = i;
    if (size == 0)
      return 0;

    msg = malloc(size);
    memcpy(msg, data, size);

    /* find the header/body boundary: first blank line. If there
       isn't one, treat the whole input as header with an empty body
       -- rfc822_parse_msg()'s own header-scan loop stops at whatever
       point it likes regardless. */
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

    /* real callers (bezerk_fetchstructure(), c-client/bezerk.c) size
       their scratch buffer as max(headersize,bodysize)+1 (LOCAL->buf)
       -- rfc822_parse_content()'s multipart-recursion path reuses the
       same scratch buffer to collect each part's own header lines out
       of the *body* stringstruct, so it must be at least as large as
       the body too, not just the outer header. Mirror that exactly so
       we're fuzzing the parser itself, not an undersized-harness
       artifact. */
    tmp = malloc((headersize > bodysize ? headersize : bodysize) + 1);

    rfc822_parse_msg(&env, &body, header, headersize, &bs, "localhost", tmp);

    mail_free_envelope(&env);
    mail_free_body(&body);
    free(tmp);
    free(msg);
    return 0;
}
