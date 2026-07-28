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

echo "Built fuzz/rfc822_fuzz and fuzz/adrlist_fuzz. Run against their"
echo "seed corpora with:"
echo "  mkdir -p fuzz/findings"
echo "  fuzz/rfc822_fuzz  fuzz/findings fuzz/corpus/rfc822"
echo "  fuzz/adrlist_fuzz fuzz/findings fuzz/corpus/adrlist"
