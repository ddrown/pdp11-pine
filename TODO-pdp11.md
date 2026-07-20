# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Standing goal: eliminate compiler warnings -- DONE
- [x] `./build clean && ./build lnx` now produces 0 warnings. Finished
      by clearing the last 3 `-Wold-style-definition` warnings plus all
      22 pointer/int-width cast warnings and both `wait(2)`
      incompatible-pointer-type warnings -- see `TODO-pdp11-done.md`
      for the per-site writeup, including one real bug found and fixed
      (`pine/osdep/os-lnx.h`'s `HAVE_WAIT_UNION` was wrong for this
      target's glibc).

### Turn on Tier 1 warning flags: format-string and buffer/overlap checks

- [ ] Now that the build is a clean 0-warning baseline, add the
      warning flags that hit closest to this project's standing
      buffer-overflow audit, since they're static and free: `-Wformat=2`,
      `-Wstringop-overflow=4`, `-Wstringop-truncation`, `-Wrestrict`,
      `-Wreturn-type`, `-Wdangling-else`, `-Wmisleading-indentation`,
      `-Wsequence-point`, `-Wtype-limits`, `-Waddress`, `-Wnonnull`.
      Trial-compiled every `.c` file standalone with `-fsyntax-only`
      (flags not yet added to either makefile) to size this up without
      touching the real build:
      - `pico/*.c`: 2x `-Wrestrict` (overlapping-argument UB, e.g.
        `strcpy`/`memcpy`-family calls with aliasing src/dst -- exactly
        the class of bug the manual `sprintf`/`strcat` audit is
        looking for), 2x `-Wformat-security`, 1x `-Wformat-extra-args`,
        1x `-Wreturn-type`, 1x `-Wdangling-else`, 2x
        `-Wmisleading-indentation`.
      - `pine/*.c`: 57x `-Wreturn-type`, 51x `-Wformat=`, 17x
        `-Wformat-nonliteral`, 1x each of `-Wformat-security`,
        `-Wformat-extra-args`, `-Wdangling-else`, `-Wsequence-point`,
        `-Wtype-limits`, `-Wnonnull`, 2x `-Waddress`. Caveat: pine's
        `.c` files all pull in shared headers (`headers.h` etc.) via
        one `#include`, so a single warning-triggering declaration in
        a shared header is counted once per translation unit that
        includes it -- the real number of distinct fix sites is
        smaller than the raw per-file count above; re-count post-fix
        by unique file:line rather than raw occurrences.
      **Proposed shape**: add the flag list above to both
      `pico/makefile.lnx`'s and `pine/makefile.lnx`'s `CFLAGS`, then
      work the resulting warning list to 0 following the same
      verification discipline as the just-finished warnings sweep
      (rebuild, confirm each fix against the actual bug it's flagging
      rather than blanket-suppressing, full smoketest run, before/after
      warning count). Do this tier before Tier 2/3 below since it's
      the highest bug-density, lowest-noise set.

### Turn on Tier 2 warning flags: sign-compare, shadow, strict-prototypes

- [ ] Follow-on to Tier 1 above, once that's landed and 0-warning
      again. These flags have a real bug-catching rate but are noisier
      -- treat as an opportunistic "fix while already touching that
      function" goal, same pattern as the completed
      `-Wold-style-definition` sweep, rather than a single mechanical
      pass: `-Wsign-compare` (16 hits in `pico/*.c`, 38 in `pine/*.c`
      trial-compiled standalone -- signed/unsigned comparison bugs,
      worth extra attention on this target since 16-bit `int`
      truncation on the PDP-11 makes sign bugs bite harder than on
      x86-64), `-Wshadow` (36 in pico, 8 in pine -- variable shadowing),
      `-Wstrict-prototypes`/`-Wmissing-prototypes` (5/16 in pico --
      declaration-side K&R leftovers the just-finished
      definition-side ANSI sweep didn't touch; pine's standalone count
      is inflated by the shared-header fan-out described in the Tier 1
      entry above, so re-scope by unique site once counted for real).

### Audit `-Wunused-parameter`/`-Wunused-function` before enabling either

- [ ] Trial-compiling with `-Wall -Wextra` also surfaces 79x
      `-Wunused-parameter` and 38x `-Wunused-function` in `pico/*.c`
      alone (18x/0x in `pine/*.c`). Do **not** blanket-enable these --
      this is a callback-heavy, function-pointer-dispatch codebase
      (see the `DRIVER`/`stream->dtb` entry below), so most of the
      unused-parameter hits are intentionally-unused callback
      arguments required by a shared function-pointer signature, not
      bugs. Instead: (1) treat `-Wunused-function` hits as a dead-code
      audit list -- each one is a candidate for outright deletion
      (fits this project's general size-reduction goal, not just the
      warnings goal), verify each is truly unreachable before cutting;
      (2) leave `-Wunused-parameter` off entirely, or scope it file-by-
      file only where dispatch isn't involved, since a wall of
      intentional-callback noise will bury real hits from the tiers
      above.

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

