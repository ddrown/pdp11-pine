# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

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

