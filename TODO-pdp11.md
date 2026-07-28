# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Track down the third fuzzing lead: heap corruption in address/group parsing

- [ ] Continued harness-1 fuzzing (see `TODO-pdp11-done.md`) found and
      fixed two real integer-underflow bugs in `rfc822_parse_content()`,
      but kept turning up a third, distinct class of crash even after
      both fixes: allocator magic-number mismatches and bad-frees
      inside `mail_free_address()`'s recursive list teardown, implying
      something writes through a corrupted/garbage pointer somewhere
      *before* the eventual free. Every triggering input is
      `:`/`;`-heavy, pointing at `rfc822_parse_adrlist()`'s RFC822
      group-address syntax (`group-name: addr1, addr2;`) -- in
      particular, its end-of-group handling checks `n >= 0` (`n` is
      group nesting depth) before doing `last->next = (adr =
      mail_newaddr ());`, which is only safe if `last` is non-NULL; a
      manual trace didn't conclusively prove `last` can be NULL there
      (an earlier "bad mailbox" error path seems to intercept the
      specific case tried by hand) but didn't rule it out either.
    - Never reproduced via standalone replay of a saved crash input,
      not even across a 1,000,000-run deterministic corpus replay or
      with `ASAN_OPTIONS=quarantine_size_mb=0` forcing immediate chunk
      reuse -- classic delayed-effect wild-write signature (the
      *detection* site is far from the *cause*). One occurrence's
      stack trace had no harness/target frames at all, just libFuzzer's
      own internal `free()` calls, ruling out a harness artifact.
    - `fuzz/adrlist_fuzz.c` (new, see `fuzz/build.sh`) calls
      `rfc822_parse_adrlist()` directly and reproduces the same crash
      class faster (~150K-1.7M execs vs. several M through the full
      parser) -- start there. A longer run (tens of minutes+) or a
      debugger-assisted bisection (e.g. hardware watchpoints on
      freshly-`mail_newaddr()`'d structs) is probably needed next,
      since fuzzing alone hasn't produced a minimizable, reliably-
      reproducing case yet despite several attempts.

### Set up a second libFuzzer harness for pine's own consuming-side code

- [ ] Harness 1 (parser-level, `fuzz/rfc822_fuzz.c` -- see
      `TODO-pdp11-done.md`) is done and already found and fixed three
      real bugs (`rfc822_parse_word()`, twice in
      `rfc822_parse_content()`). It only covers `rfc822_parse_msg()`
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

### Collapse dead `feature-list` (`F_*`) checks

- [ ] Full investigation and phased plan already written and saved at
      `~/.claude/plans/polymorphic-hopping-parrot.md` -- not yet executed.
      Summary: `.pinerc` is never read or written, so
      `process_feature_list()` (`pine/init.c`), the only code that ever
      populates a feature bit from the `feature-list` variable, is a
      permanent no-op. Of pine's 19 feature bits, only three
      (`F_USE_FK`, `F_CAN_SUSPEND`, `F_EXPANDED_FOLDERS`) have a second,
      real write path (command-line flags in `args.c`); the other 16 are
      permanently OFF for the life of every run, turning every
      `F_ON()`/`F_OFF()` check on them into a provably-fixed-outcome dead
      branch -- 28 call sites across `mailcmd.c`, `mailindx.c`,
      `mailview.c`, `pine.c`, `reply.c`.
    - Biggest single target on the board: `mailcmd.o` is the largest
      object file in the build (29KB text), and its `process_cmd()` is
      the single largest function in all of pine (7.3KB). 13 of the 28
      dead-check sites are in `mailcmd.c`.
    - Phase 2 (after all 28 call sites are collapsed) removes the now
      fully-dead `process_feature_list()`/`feat_list[]`/`feature-list`
      variable machinery itself and renumbers the surviving `F_*` bits.
    - See the plan file for the full per-site breakdown, ordering, and
      verification checklist.

### Remove the keyboard-lock feature (`pine/other.c`)

- [ ] `pine/other.c` (`lock_keyboard()`, bound to `k`/PF10 on the main
      menu) password-locks the terminal until the same password is
      re-entered. 0% smoketest coverage, but more importantly out of
      scope for a UUCP-connected, single-user, local-mbox PDP-11 mail
      station -- confirmed with the user to cut entirely, not just a
      dead-code removal.
    - Delete `pine/other.c` outright (`lock_keyboard()`,
      `redraw_kl_body()`, `draw_klocked_body()`, `redraw_klocked_body()`).
      Remove the `k`/PF10 case in `pine/pine.c`'s main menu dispatch
      (~line 606-611) and its keymenu entry, the `lock_keyboard()`
      prototype in `pine/pine.h`, and `other.c`/`other.o` from
      `pine/makefile.lnx`. Check `h_oe_keylock` (help text referenced by
      `lock_keyboard()`) for other users before deleting its help entry.

### Investigation targets: low-smoketest-coverage files

- [ ] `python3 test/run_smoketests.py --coverage` surfaces several files
      with low line coverage. Coverage gaps mostly reflect interactive
      branches the smoketest suite doesn't drive yet, not necessarily
      dead code -- each candidate branch needs the same per-site
      grep-confirm treatment as every other cut in this file before
      touching it. Listed as places to go looking, not confirmed cuts:
    - `folder.c` -- 16.1% (132/822 lines). Large file, lowest coverage
      of any non-trivial source file in the tree.
    - `mailcmd.c` -- 23.7% (257/1086 lines). Overlaps heavily with the
      `feature-list` collapse above (`process_cmd()` is most of this
      file); re-check coverage after that lands, since many of the
      remaining gaps may just be the newly-collapsed dead branches.
    - `reply.c` -- 36.8% (181/492 lines).
    - `signals.c` -- 29.8% (34/114) and `ttyin.c` -- 35.4% (174/492) are
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

