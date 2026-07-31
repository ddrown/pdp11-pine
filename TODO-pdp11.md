# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

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

