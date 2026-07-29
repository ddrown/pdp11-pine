# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Fix pico/random.c's sprintf into a too-small buffer[80], flagged by -Wformat-overflow= under ASAN=1

- [ ] Found incidentally while sweeping for more instances of the
      trailing-backslash/unbounded-pointer-walk bug class (see
      `TODO-pdp11-done.md`'s "Sweep for the general ... bug shape"
      entry): building with the *correctly*-invoked `./build lnx
      ASAN=1` (as opposed to the no-op `ASAN=1 ./build lnx`
      environment-variable form -- see that same entry's process
      note) surfaces one warning that the plain build doesn't:
      `random.c:114:74: warning: 'sprintf' may write a terminating nul
      past the end of the destination [-Wformat-overflow=]`.
    - The line: `sprintf(buffer,"line %d of %d (%d%%%%), character %ld
      of %ld (%d%%%%)", ...)` into `char buffer[80]`. Worst-case
      substitution widths (two `int`s up to 10 digits each, two `long`
      percentages up to 3 digits each, two `long` counters up to
      ~19-20 digits each on a 64-bit `long`, plus ~35 bytes of fixed
      text/literal `%` characters) add up to comfortably over 80
      bytes, which is why the compiler's static analysis flags it --
      this is a real, if impractical-to-hit-by-accident (would need a
      buffer with billions of lines/exabytes of characters open),
      buffer-sizing bug, a different shape than the pointer-walk-past-
      NUL bugs in the entry above (it's a classic "format string's
      theoretical max width exceeds the destination buffer" issue, the
      kind `sprintf`/`strcat` audits elsewhere in this codebase have
      already found and fixed).
    - Not yet fixed -- out of scope for the sweep that found it (a
      different bug class), left for a dedicated pass. Likely fix:
      either grow `buffer[]` to comfortably cover the worst case, or
      switch to `snprintf` with the existing size.

### Track down a small memory leak found while re-verifying the third fuzzing lead's fix

- [ ] The third fuzzing lead (heap corruption in `rfc822_parse_adrlist()`
      -- see `TODO-pdp11-done.md`) is fixed. Re-running the fuzzers to
      confirm (22M+ execs on `adrlist_fuzz`, 14M+ on the full
      `rfc822_fuzz`, zero further crashes) turned up a separate, minor
      LeakSanitizer finding instead: a 2-byte `cpystr()` allocation not
      reachable from any of `env->from`/`sender`/`reply_to`/etc. by the
      time `mail_free_envelope()` runs, on input `From:<a;I<EYE(N)`.
      Confirmed via `gdb`-assisted tracing that the three "a" mailbox
      copies this input's `From:` header produces (the real one plus
      the auto-derived `sender`/`reply_to` copies from
      `rfc822_cpy_adr()`) are *not* the leaking allocation -- all three
      were confirmed freed by cross-referencing their exact addresses
      against every `free()` call in the same process run. The actual
      leaking 2-byte string's identity wasn't pinned down in the time
      available (multiple `malloc(2)`-sized strings exist for this
      input -- three "a"s plus a personal-name "I" -- and a
      `gdb`-Python attempt to auto-diff every `malloc(2)`/`free()`
      pair hit tooling friction rather than a real answer).
    - Not a security bug (no crash, no corruption, bounded to 2 bytes
      per malformed message, and pine is an interactive reader, not a
      long-running server accumulating leaks) -- much lower priority
      than the crash-class bugs found so far, left for a future
      session. `From:<a;I<EYE(N)` reproduces it deterministically and
      standalone (unlike the crash-class bugs), so it should be a much
      easier trace than those were -- likely just needs `gdb`'s
      `commands`/`finish` combination used carefully within a *single*
      process run rather than across separate invocations (addresses
      aren't stable between runs).

### Set up a second libFuzzer harness for pine's own consuming-side code

- [ ] Harness 1 (parser-level, `fuzz/rfc822_fuzz.c` -- see
      `TODO-pdp11-done.md`) is done and already found and fixed five
      real bugs (three trailing-backslash overflows across
      `rfc822_parse_word()`/`rfc822_quote()`, plus an unsigned-
      underflow SEGV twice in `rfc822_parse_content()`'s multipart
      scanner). It only covers `rfc822_parse_msg()`
      and what it calls into, though -- it stops at producing a valid
      `ENVELOPE`/`BODY`, never at rendering one.
    - **Harness 2 -- structure-aware, consuming-side**: target pine's
      *own* formatting/display code that consumes an already-parsed
      `ENVELOPE`/`BODY` (e.g. `pine/newmail.c`'s `new_mail_mess()`,
      `pine/mailview.c`'s attachment/parameter display, `pine/reply.c`'s
      quoting) directly, building the `ENVELOPE` by hand from
      fuzzer-provided bytes (splitting them across `personal`/
      `mailbox`/`host`/`subject`/etc. fields, e.g. via a
      `FuzzedDataProvider`-style splitter) rather than parsing real
      message bytes. This is the harness that would have actually
      caught the CVE-2000-0909 recurrence -- that bug lived several
      calls downstream of the parser, in pine's own buffer-composition
      code, which harness 1 alone would never reach since it stops at
      producing a valid `ENVELOPE`, not at rendering one. Slower to
      write (needs to construct realistic-enough `ENVELOPE`/`BODY`
      structs by hand) but higher-value given where the bugs found so
      far have actually clustered.
    - Dev-machine-only tooling, like harness 1 (no PDP-11 cross-compiler
      exists yet, same tier as the `ASAN=1` target and the one-off
      `-fanalyzer` pass) -- not something that needs to run on or
      represent the actual target hardware, just another way to find
      bugs before they'd bite there.

### Investigation targets: low-smoketest-coverage files

- [ ] `python3 test/run_smoketests.py --coverage` surfaces several files
      with low line coverage. Coverage gaps mostly reflect interactive
      branches the smoketest suite doesn't drive yet, not necessarily
      dead code -- each candidate branch needs the same per-site
      grep-confirm treatment as every other cut in this file before
      touching it. `folder.c` is done (see `TODO-pdp11-done.md`: audited
      for dead code, found none -- both its dead-code sweep and the
      multi-collection cut already got it -- and added smoketest
      coverage for the Folder List screen instead, 16.1% -> 49.5%).
      `mailcmd.c` is also done (see `TODO-pdp11-done.md`: found one
      fully-dead function -- `mcdate2ctime()`, zero callers -- and one
      dead-fallout collapse left behind by the F_* cut's digit-jump
      removal (`jump_to()`'s always-`'\0'` `first_num` param and three
      now-unreachable `command`-range checks at its call site); added
      Jump/Search smoketest coverage; 24.4% -> 29.1%, `bin/pine` text
      -408 bytes). Remaining files, listed as places to go looking,
      not confirmed cuts:
    - `reply.c` -- 36.8% (181/492 lines).
    - `signals.c` -- 29.8% (34/114) and `ttyin.c` -- 37.8% (186/492) are
      smaller but also worth a look.

### Strip stray form-feed (`^L`) page-break characters from older source files

- [ ] Several of the original UW files (`imap/ANSI/c-client/mail.c`,
      `bezerk.c`, and likely others of the same vintage) have literal
      form-feed characters as printer page-break separators before
      function comment blocks. Not a size issue (they're inside comments/
      whitespace, stripped or irrelevant to the compiled output either
      way) -- this is pure tooling-friction cleanup: they've repeatedly
      broken the `Edit` tool's exact-string matching during past cuts
      (most recently the `mail_search`/`bezerk_search` removal, see
      `TODO-pdp11-done.md`), forcing a fallback to `sed` with explicit
      line ranges every time. Worth a mechanical sweep at some point:
      `grep -rl $'\f'` across `pine/` and `imap/ANSI/c-client/` to find
      every affected file, then strip the character (careful to keep
      the surrounding blank-line spacing consistent with the rest of
      each file's convention).

