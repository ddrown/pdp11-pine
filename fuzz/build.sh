#!/bin/sh
# Builds the libFuzzer harnesses under fuzz/. Run from the repo root
# after a normal `./build lnx` has produced imap/ANSI/c-client/c-client.a.
set -e
cd "$(dirname "$0")/.."

clang -fsanitize=fuzzer,address -g \
    -I imap/ANSI/c-client \
    fuzz/rfc822_fuzz.c fuzz/mm_stubs.c imap/ANSI/c-client/c-client.a \
    -o fuzz/rfc822_fuzz

clang -fsanitize=fuzzer,address -g \
    -I imap/ANSI/c-client \
    fuzz/adrlist_fuzz.c fuzz/mm_stubs.c imap/ANSI/c-client/c-client.a \
    -o fuzz/adrlist_fuzz

# Harness 2 -- links the real pine/mailview.o, pine/reply.o,
# pine/strings.o and pine/filter.o (built by a normal `./build lnx`)
# against fuzz/pine_ui_stubs.c's stand-ins for everything else those
# object files reference (screen drawing, command dispatch, the
# composer, live-mailbox operations) but this harness's target
# functions never actually reach. -ansi dropped (unlike the real
# pine/pico build) since fuzz/pine_ui_stubs.c's stub signatures aren't
# trying to match K&R call-compatibility, just link cleanly.
clang -fsanitize=fuzzer,address -g \
    -I imap/ANSI/c-client -I pine \
    fuzz/consumer_fuzz.c fuzz/pine_ui_stubs.c fuzz/mm_stubs.c \
    pine/mailview.o pine/reply.o pine/strings.o pine/filter.o \
    imap/ANSI/c-client/c-client.a \
    -o fuzz/consumer_fuzz

echo "Built fuzz/rfc822_fuzz, fuzz/adrlist_fuzz and fuzz/consumer_fuzz."
echo "Run against their seed corpora with:"
echo "  mkdir -p fuzz/findings"
echo "  fuzz/rfc822_fuzz   fuzz/findings fuzz/corpus/rfc822"
echo "  fuzz/adrlist_fuzz  fuzz/findings fuzz/corpus/adrlist"
echo "  fuzz/consumer_fuzz fuzz/findings fuzz/corpus/consumer"
