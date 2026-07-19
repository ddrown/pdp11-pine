# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Standing goal: eliminate compiler warnings
- [ ] `./build clean && ./build lnx` currently produces 179 warnings.
      Not a single task to pull and finish -- an ongoing, opportunistic
      goal (see CLAUDE.md's "Code style: prefer ANSI-style function
      signatures"): when a function is already being touched for some
      other reason, clean up its warning(s) too, rather than doing one
      dedicated mechanical sweep. Breakdown as of this writing:
      - 155x `-Wold-style-definition`: K&R-style function definitions
        (params declared below the signature line rather than inline).
        Nearly all in `pico/` (`display.c`, `composer.c`, `browse.c`,
        `osdep.c`, `pico.c`, `line.c`, `basic.c`, `buffer.c`, `file.c`,
        `search.c`, `random.c`, `region.c`, `window.c`, `word.c`,
        `bind.c`, `tcap.c`, `fileio.c`, `main.c`, `compose_stub.c`),
        plus one in `pine/osdep/includer.c`. Mechanical, low-risk,
        exactly the ANSI-signature conversion CLAUDE.md describes.
      - 12x `-Wint-to-pointer-cast` / 10x `-Wpointer-to-int-cast`: casts
        between pointers and integers of different sizes, in
        `pico/display.c`, `pico/composer.c`, `pico/browse.c`,
        `pico/pico.c`, `pico/main.c`, `pico/line.c`, `pine/screen.c`,
        `pine/signals.c`, `pine/misc.c`. These are 64-bit-vs-original-
        32-bit-assumption artifacts (this codebase predates 64-bit
        pointers) -- worth individually checking each site actually
        fits in the truncated width rather than blanket-suppressing;
        higher risk than the style-only warnings above, so don't batch
        these in with opportunistic K&R cleanups without looking at
        each one.
      - 2x `-Wincompatible-pointer-types`: `pine/send.c:811` and
        `:1513` both call `wait((union wait *)&status)` -- worth
        checking against this build's actual libc `wait(2)` prototype
        (`int *status`, not `union wait *`) rather than assuming the
        cast is still correct; possibly a leftover from an older BSD
        `wait()` signature.

### gcov-flagged dead-code candidates in `pine/*.c` (no call site found anywhere)
- [ ] Compiled from a `python3 test/run_smoketests.py --coverage` run
      cross-referenced with `ctags`/`grep` across `pine/`+`pico/`: 19
      functions that gcov shows as 0% executed *and* have no real
      call site anywhere in the tree, direct or by bare name/function-
      pointer (the smoketest suite's 25 cases don't come close to
      covering every command, so a much larger "zero-hit but has a
      real caller" set was set aside separately -- those are test-gap
      candidates, not dead-code candidates). Full detail, per-file line
      coverage, and the reachable-but-untested list are in a private
      Claude artifact from the 2026-07-19 session (ask to have it
      regenerated/re-shared if the link isn't handy -- the method is
      just the `--coverage` run above plus `gcov -f -o pine pine/<file>.c`
      per file, since the script's own summary deletes the per-function
      `.gcov` detail but leaves the `.gcda` counters in place).
      **None of these are proven dead yet** -- each needs the same
      trace-before-delete verification as every other cut in
      `TODO-pdp11-done.md` before removal (this list already had 8
      false positives -- `gf_wrap`/`gf_rich2plain`/`gf_8bit_qp`/
      `gf_qp_8bit`/`gf_binary_b64`/`folder_screen`/`build_header_cache`/
      `term_signal` -- pulled back out after a closer look found they're
      registered by bare name via `gf_link_filter()`/a callback-pointer
      assignment/`signal()`, not called directly, so grepping for
      `name(` alone missed them).
      `pine/init.c:1647 init_driver` and `pine/pine.c:1538 count_unseen`
      have no reference of any kind, not even a header prototype --
      best starting point.
      `pine/context.c`: `mm_bboard` (59), `context_find` (287),
      `context_subscribe` (495), `context_unsubscribe` (512) -- possibly
      leftover bboard/newsgroup-subscribe support from before news was
      excluded from the c-client build (see this file's "In progress /
      already done" note in `TODO-pdp11-done.md`).
      `pine/filter.c`: `gf_set_writec` (508), `gf_busy` (1726).
      `pine/strings.c`: `sqzspaces` (134), `strcat_nocr` (342),
      `crlf2lf` (385), `month_num` (440), `enth_string` (959),
      `rfc822date_to_ctime` (1015), `lf2crlf` (1037).
      `pine/ttyout.c`: `InsertChar` (754), `DeleteChar` (781),
      `PutLine3` (1146), `CleartoEOS` (1255).

