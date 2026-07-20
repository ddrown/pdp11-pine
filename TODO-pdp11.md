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

### Remove the `DRIVER` dispatch abstraction entirely -- `bezerkdriver` is the only driver left

- [ ] Raised after the dummy-driver-shrinking effort: `bezerkdriver` is
      now the *only* thing ever `mail_link()`-ed into `maildrivers`
      (`pine/pine.c`) -- the whole point of `DRIVER`, supporting
      multiple pluggable mailbox-format backends dispatched
      polymorphically through function pointers, is unused in this
      build. Every `factory->foo(...)`/`stream->dtb->foo(...)`
      indirect call in `c-client/mail.c` could become a direct
      `bezerk_foo(...)` call instead.
      **Scope**: counted ~33 dispatch call sites in `mail.c` alone --
      `mail_open`/`mail_close`/`mail_append`/`mail_create`/
      `mail_delete`/`mail_rename`/`mail_find`/`mail_find_all`/
      `mail_find_bboards`/`mail_find_all_bboard`/`mail_expunge`/
      `mail_copy`/`mail_move`/`mail_ping`/`mail_check`/the
      `mail_fetch*`/`mail_setflag`/`mail_clearflag`/`mail_search`/
      `mail_gc`/`mail_subscribe_bboard`/`mail_unsubscribe_bboard`
      family -- plus a handful of `stream->dtb` references elsewhere
      (`bezerk.c` reattaching/clearing its own `dtb` field on
      open/close, and two in `pine/pine.c`'s `mail_valid_net()` calls,
      which are already dead in practice since this build has no
      remote/TCP-IP mailbox support). Much bigger blast radius than
      any single cut in the dummy-driver effort -- nearly every generic
      entry point in `mail.c` is built around `MAILSTREAM->dtb`
      indirection, not just a handful of spots.
      **Proposed shape**: delete the `DRIVER` struct/typedef, the
      `maildrivers` global and `mail_link()`, and `MAILSTREAM`'s `dtb`
      field; rewrite each of `mail.c`'s dispatcher functions to call
      the corresponding `bezerk_*` function directly (dropping the
      `mail_valid()`-based lookup-and-match step entirely, or
      collapsing it down to whatever's left of `bezerk_valid()`'s own
      logic once there's no chain to walk). `bezerk_open()`/
      `bezerk_close()`'s own `stream->dtb = &bezerkdriver` / `= NIL`
      reattachment lines go away too, since nothing reads that field
      once dispatch isn't indirected through it.
      **Needs its own careful design/verification pass**, not a quick
      follow-on -- this is core, deeply-shared c-client dispatch
      plumbing every mail operation flows through, unlike the
      previous, more narrowly-scoped dummy-driver cuts.

### Simplify `read_pinerc()` to a bootstrap-only stub (drop full `.pinerc` parsing)

- [ ] gcov flagged `read_pinerc()` (`pine/init.c:1133`) as low-coverage.
      Called twice per run (`init.c:328` for `SYSTEM_PINERC`/
      `ParseGlobal`, `init.c:337` for the user's own `.pinerc`/
      `ParseLocal`), and every currently-tested invocation only
      exercises its very first branch: `read_file(filename) == NULL`
      -> sets `first_time_user=1` (ParseLocal only) -> `return(-1)`
      (`init.c:1146-1151`). `test/smoketest.py`'s `Session` always
      points `-p` at a fresh, nonexistent scratch `.pinerc` and no
      `test/cases/*.py` ever seeds one with real content, so the whole
      rest of the function -- line/`pinerc_lines` counting and
      allocation, the line-by-line parse loop (comment lines, blank/
      continuation lines, variable-name lookup, obsolete-variable
      handling, unused-variable skip, `=`-syntax-error detection, quote
      stripping, comma-separated list parsing with continuation-line
      joining via `parse_list()`), and the `ParseLocal`-vs-`ParseGlobal`
      storage into `v->user_val`/`v->global_val` -- is dead as far as
      this project's test suite is concerned.
      **User has explicitly accepted this as a real functionality
      loss for the size win**: unlike the other items in this file,
      this is not proven dead code, it's a deliberate capability cut.
      Once landed, a hand- or Setup-written `.pinerc` (mail directory,
      feature list, signature file, etc.) will no longer be parsed --
      every variable silently stays at its compiled-in default on
      every run, for both the system-wide and per-user config files.
      **Proposed cut**: collapse `read_pinerc()` down to just the
      existing bootstrap branch (open the file; if it doesn't exist,
      set `first_time_user`/return `-1` same as today), deleting
      everything past that.
      **Needs verification before landing**:
      - Check for other callers of `parse_list()` before deleting it
        too (it's currently only invoked from `read_pinerc()`'s list-
        parsing branch).
      - Check what else reads `pinerc_lines` (the per-line struct this
        function builds) -- it looks like it's kept around for writing
        an updated `.pinerc` back out (e.g. from Setup/config-save
        screens); if so, that dependent code needs its own look rather
        than silently receiving an empty/uninitialized list.
      - Standard trace-before-delete pass: rebuild, full smoketest
        suite, warning count, before/after `size`.

