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

### Fold `c-client/dummy.c` into `bezerk.c`, drop the rest

- [ ] Investigated while answering "is dummy.c's fallback logic needed,
      don't we just need mbox+bezerk?": `mbox.c` isn't even in the
      picture (see the separate TODO item below) -- the two drivers
      pine actually `mail_link()`s are `bezerkdriver` and `dummydriver`
      (`pine/pine.c:345-346`), and `bezerk.c` is *not* self-sufficient
      without `dummy.c`:
      - `bezerk_find`/`bezerk_find_all` literally call
        `dummy_find`/`dummy_find_all` -- the only implementation of the
        directory-scanning wildcard folder search lives in `dummy.c`,
        used by pine's Folder List / Find Folder screens.
      - `bezerk_valid()` requires the target file to already exist
        (`stat()` must succeed). Any mailbox that doesn't exist yet
        (new Save-folder, new Fcc target, etc.) falls through to
        `dummy_valid()` (matches anything), whose job is to fail with
        the *correct* message -- including the `[TRYCREATE] Must
        create mailbox before append` hint c-client's create-on-append
        convention depends on (`dummy_open`/`dummy_append`) -- rather
        than crashing or misbehaving.
      Everything else in `dummy.c` is provably unreachable rather than
      load-bearing: `dummy_open()` unconditionally `return NIL`s, so no
      stream can ever actually be opened with `dtb == &dummydriver`,
      which means the ~14 message-manipulation driver-table slots
      (`fetchfast`/`fetchflags`/`fetchstructure`/`fetchheader`/
      `fetchtext`/`fetchbody`/`setflag`/`clearflag`/`search`/`ping`/
      `check`/`expunge`/`copy`/`move`/`gc`) can never be invoked --
      each is just a `fatal("Impossible dummy_X")` one-liner kept only
      to satisfy the `DRIVER` struct's fixed layout. `subscribe`/
      `unsubscribe`/`subscribe_bboard`/`unsubscribe_bboard`/`create`/
      `delete` are six more one-liners (`return NIL`) sharing the
      identical signature `long(MAILSTREAM*, char*)`.
      **Proposed shape**: move the load-bearing pieces
      (`dummy_find`/`dummy_find_bboards`/`dummy_find_all`/
      `dummy_find_all_bboards`/`dummy_file`/the `dummy_open`+
      `dummy_append` TRYCREATE-hint error paths) directly into
      `bezerk.c` as `bezerk_`-prefixed statics (or just call them
      inline where `bezerk_find`/etc. currently forward to `dummy_*`),
      stop `mail_link`-ing a separate `dummydriver` entirely, and
      delete `dummy.c`/`dummy.h` and the now-orphaned `DRIVER
      dummydriver` struct along with the ~20 unreachable/duplicate
      stub functions. Also check `bezerk_find_bboards`/
      `bezerk_find_all_bboards`/`bezerk_subscribe_bboard`/
      `bezerk_unsubscribe_bboard` -- these currently just forward to
      the equivalent `dummy_*_bboard` calls, which are themselves
      already known-dead now that pine's own bboard/news UI is gone
      (see `TODO-pdp11-done.md`'s NNTP/news-strip item) -- likely
      candidates to drop rather than migrate.
      **Needs its own trace-before-delete pass** per CLAUDE.md's
      verification discipline: confirm nothing else in `c-client/`
      references `dummydriver`/`dummy_*` by name (`maildrivers` linked
      list traversal, etc.) before deleting, not just pine's own
      `mail_link()` call sites.

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

