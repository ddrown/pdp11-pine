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

### Remove `read_pinerc()` and `first_time_user` entirely

- [ ] Follow-on to the `read_pinerc()` stub cut above. `read_pinerc()`
      (`pine/init.c:1114`) is now an unconditional `return(0);` with an
      empty body -- its 3 call sites (`init.c:316`/`:318`/`:325`, the
      `ParseGlobal`/`ParseLocal` pine.conf and `.pinerc` reads) could
      just be deleted outright instead of calling a no-op, along with
      the function itself, its ANSI/K&R prototypes (`init.c:82`/`:90`),
      and the now-pointless `typedef enum {ParseLocal, ParseGlobal}
      ParsePinerc` (`init.c:79`) -- check first whether `ParsePinerc`
      is referenced anywhere else before deleting the typedef.
      `first_time_user` (`struct pine`, `pine.h:648`) had no setter
      anywhere in the tree even before this (`read_pinerc()`'s
      bootstrap version was the only one, and it's gone now) -- grepped
      to confirm its only remaining references are the two `if`
      conditions that read it: `pine.c:401`
      (`pine_state->first_time_user || pine_state->show_new_version`,
      with a nested `if(pine_state->first_time_user)
      new_user_or_version(first_time_message); else
      new_user_or_version(new_version_message);` -- the `first_time_user`
      branch is now dead, `new_version_message` always fires when this
      block runs at all) and `pine.c:1640`
      (`(ps_global->first_time_user || ps_global->show_new_version) &&
      other_cmds % 3 == 0 && other_cmds < 10`, part of the periodic "O
      command is optional" reminder -- `first_time_user` is redundant
      with `show_new_version` here now). Proposed cut: delete the
      `unsigned first_time_user:1` field from `struct pine`, simplify
      both `pine.c` conditions to just `show_new_version`, and delete
      the dead `if(pine_state->first_time_user)` branch at `pine.c:404`
      (folding straight to the `new_version_message` call).
      Standard trace-before-delete verification pass before landing:
      rebuild, full smoketest suite, warning count, before/after
      `size`. Since nothing in the test suite currently exercises the
      first-run welcome screen either way, this is unlikely to be
      test-observable -- consider whether a smoketest driving
      `show_new_version` (if there's a way to set it, e.g. a version
      marker file) is worth adding, or whether this is accepted as
      unverified-by-suite like the `read_pinerc()` cut itself was.

