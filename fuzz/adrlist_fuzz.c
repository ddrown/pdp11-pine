/*
 * Focused libFuzzer harness for imap/ANSI/c-client/rfc822.c's
 * rfc822_parse_adrlist() -- narrower than rfc822_fuzz.c, built while
 * chasing a heap-corruption lead (bad-free on an ADDRESS pointer)
 * that a full rfc822_parse_msg() fuzz run kept finding but that
 * didn't reproduce standalone. Calling rfc822_parse_adrlist()
 * directly, with no header/MIME parsing in the way, narrows the
 * search space to just address/group-list parsing.
 *
 * Build/run: see fuzz/build.sh's pattern; same c-client.a + mm_stubs.c.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mail.h"
#include "osdep.h"
#include "rfc822.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char *str;
    ADDRESS *lst = NULL;
    size_t i;

    if (size == 0)
      return 0;

    for (i = 0; i < size; i++)
      if (data[i] == '\0')
        break;
    size = i;
    if (size == 0)
      return 0;

    str = malloc(size + 1);
    memcpy(str, data, size);
    str[size] = '\0';

    rfc822_parse_adrlist(&lst, str, "localhost");

    mail_free_address(&lst);
    free(str);
    return 0;
}
