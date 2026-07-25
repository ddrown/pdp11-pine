# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Remove `NFSKLUDGE` -- irrelevant for a local-mbox-only, no-NFS target

- [ ] `imap/ANSI/c-client/bezerk.c`'s `bezerk_lock()` has an
      `#ifdef NFSKLUDGE ... #else ... #endif` pair (~lines 1044-1101)
      choosing between two mailbox-lock-file creation strategies: the
      `NFSKLUDGE` branch builds a "hitching post" temp file and
      `link()`s it into place (a dance needed only because NFS's
      `O_EXCL` isn't reliably atomic over the wire), while the
      `#else` branch just does a plain `open(...,O_EXCL,...)`, which
      is atomic and sufficient on any real local filesystem. This
      target is UUCP-connected, local-mbox-only, with no networking
      at all (see `README.md`/`CLAUDE.md`) -- the mail spool is never
      NFS-mounted, so the `NFSKLUDGE` branch can never be the right
      choice here.
    - `NFSKLUDGE` is `#define`'d in `os_lnx.h` (this build's dev
      target), which is why it was left alone during the `pico`/
      `c-client` dead-code survey (correctly identified as "genuinely
      defined for the header this build uses," not yet as "wrong for
      this project"). But of the other `os_*.h` variants, `os_bsi.h`
      (a BSD-derived target, the closest existing precedent to the
      eventual 2.11BSD cross-build) does **not** define it -- neither
      do `os_mac.h`/`os_sco.h`/`os_t20.h`. That's a strong signal
      `NFSKLUDGE` shouldn't be defined for this project's target
      either, dev build included, since the dev build's whole point
      is to mirror the real target's behavior (local mbox, no NFS).
    - Proposed fix: remove `#define NFSKLUDGE` from `os_lnx.h`, then
      collapse `bezerk.c`'s `#ifdef NFSKLUDGE`/`#else`/`#endif` down
      to just the `#else` (plain `O_EXCL`) body, using the same
      never-defined-macro collapse technique used throughout this
      project (grep-confirm no header this build uses defines it,
      then delete the dead branch).
    - Unlike most of this project's `#ifdef` collapses, this one
      changes *live, exercised* locking behavior on the dev build too
      (currently takes the hitching-post path on every lock, would
      switch to plain `O_EXCL`) -- so budget for real verification,
      not just a clean rebuild: confirm mailbox locking still works
      under concurrent access (e.g. two `bin/pine` instances opening
      the same mbox, or pine + sendmail delivering concurrently),
      not just that the smoketest suite stays green (it likely
      doesn't stress lock contention at all).

### Investigate removing the DEBUG/dprint() infrastructure entirely

- [ ] Disabling the `-DDEBUG` build define (`pine/makefile.lnx`'s
      `DEBUG=` var, see `TODO-pdp11-done.md`) was a **-35152 byte**
      win on its own -- by far the largest single change in this
      project -- because `dprint(n,x)` (`pine.h`) already collapses to
      a true no-op without it. That confirms the underlying machinery
      itself (189 `dprint()` call sites across 20 files, the
      `debug`/`debugfile` globals, `init_debug()`'s log-file-rotation
      logic in `init.c`, the `-d`/`#ifdef DEBUG` argv handling in
      `args.c`) is real compiled weight whenever it *is* turned on,
      not preprocessor-stripped noise -- it's just off by default now.
    - Investigate whether this whole subsystem is worth deleting
      outright rather than leaving it as an opt-in build knob: a
      PDP-11 with no `gdb`/coredump-analysis tooling and no spare
      address space for a 35KB debug logger may never realistically
      use it, but weigh that against losing a real diagnostic tool for
      bring-up debugging on the actual target hardware, where this
      kind of textual trace may be the *only* debugging option
      available. Worth checking whether `TODO-pdp11.md`'s still-open
      ASan/`-fanalyzer` item covers the same debugging needs a
      different way before committing to deletion.
    - If kept, at minimum leave `-DDEBUG` off by default (already
      done) so production/size-measurement builds never pay for it by
      accident.

### Remove read-only mode (`-o` flag)

- [ ] `-o` (`pine/args.c`) sets `pine_state->open_readonly_on_startup`,
      consumed exactly once in `mailcmd.c` (~line 1513) to OR
      `OP_READONLY` into the mail-open flags for the first folder
      opened at startup, then self-clears (`= 0`) so it only applies
      once. Given this target is a single-user local mail station (no
      concurrent multi-user access to worry about, no reason to open
      defensively read-only), investigate whether this feature is
      worth keeping at all.
    - **Careful**: this is distinct from `MAILSTREAM`'s general
      `->readonly` field (c-client, used more broadly -- e.g.
      `signals.c` sets a stream `readonly` after certain crash/dead-
      stream cleanup paths, `mailcmd.c`/`mailview.c`/`screen.c` check
      it for display/UI purposes). Only `-o` and
      `open_readonly_on_startup` are the removal target; the general
      readonly-stream machinery those other sites use stays untouched.

### Make suspend (`^Z`) the default, remove the `-z` flag

- [ ] `-z` (`pine/args.c`) turns on the `F_CAN_SUSPEND` feature bit,
      checked at ~8 call sites (`mailbox.c`, `imap.c`, `mailindx.c`,
      `mailview.c`, `folder.c`, `status.c`, `pine.c`, `ttyin.c`,
      `send.c`) almost always paired with a `have_job_control()` check
      (`pine/os.c`) that already tests the real runtime capability.
      Since 2.11BSD has job control, make `^Z` suspend always
      available (equivalent to always passing `-z`) and remove the
      flag: collapse every `F_ON(F_CAN_SUSPEND,...)`/
      `F_OFF(F_CAN_SUSPEND,...)` check to its fixed-true outcome,
      relying solely on the already-present `have_job_control()` guard
      where the two are currently paired.
    - **Bigger finding**: after the `F_USE_FK`/`F_EXPANDED_FOLDERS`
      removals (`TODO-pdp11-done.md`), `F_CAN_SUSPEND` (bit 0) is now
      the **only** feature bit left in `pine.h`. Once it's collapsed
      away too, the entire feature-list abstraction becomes dead code:
      `F_ON`/`F_OFF`/`F_TURN_ON`/`F_TURN_OFF`/`F_SET` macros,
      `process_feature_list()`-adjacent plumbing already removed
      earlier, and the `feature_list` `BITMAP` field in `struct pine`
      itself. Treat this as a two-phase job like the `F_USE_FK` cut:
      phase 1 collapses `F_CAN_SUSPEND`'s own readers, phase 2 deletes
      the now-fully-unused feature-bit framework it was the last
      tenant of.

### Turn on Tier 2 warning flags: sign-compare, shadow, strict-prototypes

- [ ] `-Wsign-compare`/`-Wshadow`/`-Wstrict-prototypes`/
      `-Wmissing-prototypes` are now all enabled in both
      `makefile.lnx`s (landed via the `two` remote's parallel work,
      see the `two/main` merge commit -- CLAUDE.md's memory has
      background on that second `pine-two` checkout). Most of
      `-Wstrict-prototypes`/`-Wmissing-prototypes` is already fixed by
      that commit; `-Wsign-compare` and `-Wshadow` are still wide
      open. A clean `./build clean && ./build lnx` as of the
      `init.c`/`filter.c`/`status.c`/`os.c` dead-code-sweep bucket 3
      commit shows 164 raw warnings, dominated by `-Wshadow`'s
      recurring `declaration of 's' shadows a global declaration`
      (many `.c` files reuse a local named `s` that collides with
      some global) and `-Wsign-compare`'s `size_t`-vs-`int`/`long`
      comparisons (worth extra attention on this target -- 16-bit
      `int` truncation on the PDP-11 makes sign bugs bite harder than
      on x86-64) plus a handful of leftover
      `-Wstrict-prototypes`/`-Wmissing-prototypes` stragglers (e.g.
      `os.c`/`status.c`). Treat as an opportunistic "fix while already
      touching that function" goal rather than a single mechanical
      pass, same pattern as the completed `-Wold-style-definition`
      sweep.

### Formalize an ASan build target and try `-fanalyzer`

- [ ] CLAUDE.md's verification discipline already leans on ad hoc
      ASan (`cc -fsanitize=address -g` on a standalone extracted repro
      file) for the local-array-overflowing-into-adjacent-stack-
      variable bugs valgrind can't see. Promote that from a manual
      per-bug extraction step to a real build target -- e.g. `./build
      lnx-asan` or an `ASAN=1` knob added to `pine/makefile.lnx`/
      `pico/makefile.lnx` that appends `-fsanitize=address -g` to
      `CFLAGS`/`LDFLAGS` -- so the full smoketest suite can be run
      once under ASan instrumentation directly, rather than only
      reaching for it after a bug is already suspected. Separately,
      try a one-off `-fanalyzer` pass (GCC's deeper static analyzer --
      use-after-free, double-free, null-deref, leak paths) over both
      trees; it's slow and can be noisy on older C, so evaluate signal
      quality on this codebase before deciding whether it's worth a
      standing CI-style target or just an occasional manual check.

