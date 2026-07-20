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

### Remove subscribe/unsubscribe entirely, simplify `dummy_find()` to just call `dummy_find_all()`

- [ ] Raised while checking whether subscribe/unsubscribe means
      anything for local mbox/bezerk mailboxes (it's not purely an
      IMAP/NNTP concept -- `bezerk_subscribe()`/`bezerk_unsubscribe()`
      (`bezerk.c`) are real, calling `sm_subscribe()`/`sm_unsubscribe()`
      (`c-client/sm_unix.c`), which read/write a per-user
      `~/.mailboxlist` file). But nothing in `pine/*.c` calls
      `context_subscribe()`/`context_unsubscribe()` or
      `mail_subscribe()`/`mail_unsubscribe()` anywhere (confirmed by
      grep -- zero hits), so `~/.mailboxlist` is never created in this
      build, and `dummy_find()`'s subscription-database check
      (`dummy.c`) always takes its "no database" branch and falls
      straight through to `dummy_find_all()`. The whole subscribe/
      unsubscribe path is dead weight end to end.
      **Proposed cut**: delete `sm_subscribe()`/`sm_unsubscribe()`/
      `sm_read()` from `sm_unix.c` (and their prototypes in `mail.h`)
      entirely; delete `bezerk_subscribe()`/`bezerk_unsubscribe()` from
      `bezerk.c`, replacing their `DRIVER` slots with the same kind of
      unreachable-stub treatment already used elsewhere in this driver-
      shrinking effort (see `TODO-pdp11-done.md`). Since no subscription
      database can ever exist once `sm_subscribe()` is gone,
      `dummy_find()` degenerates to exactly `dummy_find_all()`'s body --
      delete `dummy_find()` and repoint its callers (`bezerk_find()`,
      and `mail_find()`'s no-stream fallback call once the "make dummy a
      plain helper" item above lands) directly at `dummy_find_all()`.
      `phile.c` has the same `sm_subscribe`/`sm_unsubscribe` calls but
      isn't compiled in this build (not in `makefile.lnx`), so no
      changes needed there.
      Needs the standard trace-before-delete verification pass (rebuild,
      full smoketest suite, check warning count) before landing.
      **Goes further than just stubbing**: unlike other slots trimmed
      in this driver-shrinking effort (where the `DRIVER` struct shape
      was always kept, only the assigned functions swapped for shared
      stubs), `mail_subscribe()`/`mail_unsubscribe()` themselves (the
      generic `c-client/mail.c` dispatchers, not just bezerk's
      implementations) have zero callers anywhere in `pine/*.c` *or*
      `c-client/*.c` -- the whole concept is unreachable end to end, not
      just one driver's implementation of it. So this can go all the
      way: delete `mail_subscribe()`/`mail_unsubscribe()` from `mail.c`
      (and their prototypes/`#define` shortenings in `mail.h`), then
      remove the `subscribe`/`unsubscribe` fields from the `DRIVER`
      struct definition itself, updating `bezerkdriver`'s literal (the
      one remaining struct) to drop those two positional entries.

