#!/bin/sh
# Builds the libFuzzer harnesses under fuzz/. Run from the repo root
# after a normal `./build lnx` has produced imap/ANSI/c-client/c-client.a.
set -e
cd "$(dirname "$0")/.."

clang -fsanitize=fuzzer,address -g \
    -I imap/ANSI/c-client \
    fuzz/rfc822_fuzz.c fuzz/mm_stubs.c imap/ANSI/c-client/c-client.a \
    -o fuzz/rfc822_fuzz

echo "Built fuzz/rfc822_fuzz. Run it against the seed corpus with:"
echo "  mkdir -p fuzz/findings"
echo "  fuzz/rfc822_fuzz fuzz/findings fuzz/corpus/rfc822"
