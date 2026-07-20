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

### Shrink `c-client/dummy.c`'s unreachable driver slots
- DONE, see `TODO-pdp11-done.md`. The original "delete dummy.c
  entirely, fold into bezerk.c" idea turned out unsafe (`mail.c`'s
  `mail_valid()` hardcodes the driver name `"dummy"` to keep folder
  creation's "already exists" check working) -- scoped down instead to
  collapsing 24 of `dummydriver`'s 33 slots (proven unreachable by
  tracing every dispatch path) into 6 shared stub functions, plus the
  matching dead bboard-forwarding slots in `bezerk.c`. `mbox.c` needed
  a small fixup to keep building (see the item below); `phile.c` has
  the same pattern but isn't built at all.

### Make `dummy` a plain helper `mail.c`/`bezerk.c` calls, not a registered `DRIVER`

- [ ] Raised after the driver-slot shrink above: dummy doesn't
      conceptually need to be a `DRIVER` at all -- it's really a small
      set of helper functions (`dummy_find`/`dummy_find_all`'s
      directory-scan search, `dummy_open`/`dummy_append`'s
      correct-error-message + `[TRYCREATE]` hint) that `bezerk.c`
      already calls directly as plain functions in two cases
      (`bezerk_find`/`bezerk_find_all`). If the whole thing worked that
      way, there'd be no `DRIVER` struct to keep populated at all, and
      the remaining 9 load-bearing functions could shrink to plain
      helpers with no signature-matching stub games needed.
      **The catch**: `c-client/mail.c`'s own generic `mail_create()`/
      `mail_delete()`/`mail_rename()` distinguish "a real driver already
      handles this mailbox" from "only the catch-all placeholder
      matched" by checking the literal driver name `"dummy"` inside
      `mail_valid()` (its `nopen` flag) -- that's specifically what
      requires dummy to remain a second, distinctly-named entry in the
      driver chain today.
      **Refined design** (simpler than editing `mail_valid()`'s
      exclusion in place): stop `mail_link()`-ing `dummydriver`
      entirely, so `maildrivers` only ever contains `bezerkdriver`.
      `bezerk_valid()` stays strict (requires the file to really
      exist) -- unchanged. With dummy out of the chain,
      `mail_valid()`'s loop naturally returns NIL for a nonexistent
      local mailbox with no special-casing needed at all, so the
      `nopen && !strcmp(factory->name,"dummy")` check can simply be
      deleted -- `mail_create`/`mail_delete`/`mail_rename`'s "already
      exists" checks then work correctly for free. The dummy fallback
      behavior only needs to be added explicitly, as plain function
      calls (not through a `DRIVER` struct), in the handful of `mail.c`
      entry points that actually want it when nothing matched:
      - `mail_open()`: call dummy's error-reporting helper (today's
        `dummy_open()` logic) when `mail_valid()` returns NIL, instead
        of just `mail_valid()`'s own generic "no such mailbox" message.
      - `mail_append()`: same, for the `[TRYCREATE]`-hint behavior
        (today's `dummy_append()`).
      - `mail_find()`/`mail_find_all()`'s no-stream case: today this
        loop iterates every registered driver and calls each one's
        `find`/`find_all` slot directly (`d->find_all(NIL,pat)`), and
        `dummy_find_all` is currently the *only* one of those that does
        anything when `stream` is NIL (`bezerk_find_all`'s forwarding
        is itself gated on `stream` being non-NIL) -- so removing dummy
        from the chain would silently break "search all mailboxes with
        no open stream" unless this loop also gets an explicit direct
        call to the dummy search helpers added.
      **Scope**: touches `c-client/mail.c` in three specific spots
      (`mail_valid()`, `mail_open()`, `mail_append()`, plus
      `mail_find()`/`mail_find_all()`) instead of the single name-string
      check today -- each change is simple and explicit, but it's still
      shared driver-chain infrastructure `mail.c` uses generically, not
      just the two locally-linked drivers, so it needs its own careful
      design/verification pass, not a quick follow-on.

### smoketest.py's `HOME` env override doesn't isolate default-folder-path creation
- [ ] Discovered while manually verifying the dummy.c/bezerk.c cut
      above: `c-client/os_lnx.c:195-198`'s `myhomedir()` resolves the
      real passwd-database home directory via `getpwuid(geteuid())`,
      never checking `getenv("HOME")`. `test/smoketest.py`'s `Session`
      sets `env["HOME"]` to a scratch tempdir for isolation, but any
      code path that builds a mailbox path via `myhomedir()` (e.g.
      `dummy_file()`/`bezerk_file()`'s default case, used whenever a
      folder name isn't `-f`-given explicitly) ignores that override
      and resolves against the *real* developer's home directory
      instead. Confirmed by hand: saving a message to a new folder
      name during manual pty testing actually created a real file
      under the real `~/mail/`, not the scratch `$HOME/mail/` the test
      harness assumes -- verified via `bwrap --bind <scratch-dir>
      /home/abob/mail` to redirect it safely for repeat testing.
      No existing smoketest currently exercises a default-relative
      folder-creation path (all use `-f <explicit mbox path>`), so this
      hasn't caused visible contamination yet, but any future test that
      does (e.g. a regression test for the cut above) needs either the
      same bwrap-bind-mount treatment `_wrap_with_sendmail_stub()`
      already uses for sendmail, or some other harness-level fix, or it
      will silently write into whoever's real home directory runs the
      suite.

### Is `c-client/mbox.c` dead weight in the build?

- [ ] `c-client/makefile.lnx` compiles `mbox.o` into `c-client.a`, but
      pine never `mail_link()`s `mboxdriver` -- only `bezerkdriver` and
      `dummydriver` are linked (`pine/pine.c:345-346`), and grepping
      `pine/*.c` for `mboxdriver`/`mbox_valid` turns up nothing outside
      `mbox.c` itself. Since `c-client.a` is a static archive, an
      unreferenced member normally isn't pulled into the final link --
      but worth confirming with `nm bin/pine | grep mbox_` (or a
      before/after `size` comparison from actually removing `mbox.o`
      from the archive) whether it's really contributing zero bytes
      today, or whether something incidental (e.g. a stray direct
      reference, or the object still getting pulled in some other way)
      is dragging it in anyway. If it's already contributing nothing,
      this is a build-file cleanup (drop `mbox.o` from
      `c-client/makefile.lnx`) rather than a code cut; if it's
      surprisingly present in the binary, that's a real, unexpected
      win to chase down.

