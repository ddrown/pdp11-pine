# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

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

### Drop `pico/ansi.c` -- permanently dead, never-enabled alternate terminal driver

- [ ] Found while double-checking scope after the `#ifdef ANSI`/K&R
      cleanup above (unrelated despite the similar name --
      `pico/ansi.c` guards its body on `ANSI_DRIVER`, not `ANSI`).
      This file is an alternate ANSI/VT100-terminal-escape-sequence
      driver, a build-time alternative to the termcap-based driver in
      `pico/tcap.c` (which is what this build actually uses). Its own
      header comment says as much: "The routines in this file provide
      support for ANSI style terminals over a serial line... It
      compiles into nothing if not an ANSI device." `ANSI_DRIVER` is
      never defined anywhere in this tree (grepped `pico/`, no
      `-DANSI_DRIVER` in `pico/makefile.lnx`), so the file's entire
      body (`#if ANSI_DRIVER` at line 55 through `#endif` at line 146
      -- the `TERM term = {...}` dispatch struct, `ansimove`/`ansieeol`/
      `ansieeop`/`ansibeep`/etc.) has never once compiled here.
      **Not quite inert, though**: `ansiopen()` (lines 148-165) sits
      *outside* that `#if ANSI_DRIVER` guard, so it compiles for real
      every build -- but its only reference anywhere (a `TERM` struct
      entry at line 82) is itself inside the dead block, so it's an
      orphaned, always-unreferenced function. It isn't caught by the
      now-standing `-Wunused-function` check (see
      `TODO-pdp11-done.md`'s `-Wunused-parameter`/`-Wunused-function`
      audit) because `ansiopen` has external linkage (no `static`) --
      that warning only fires on `static` functions, since an
      externally-linked one could in principle be called from another
      translation unit. Confirmed via `nm` on a standalone compile
      that `ansiopen`'s body and the file's unused `rcsid[]` string
      are the only two symbols this file actually contributes: 126
      bytes text + 50 bytes data.
      **Proposed shape**: delete `pico/ansi.c` outright and drop
      `ansi.o`/`ansi.c` from `pico/makefile.lnx`'s `OFILES`/`CFILES`.
      Grepped `pico/efunc.h`/`estruct.h`/`edef.h` and every other
      `pico/*.c` -- nothing outside `ansi.c` itself references
      `ansiopen` or any of the `ansi*` symbols, so this is a clean,
      no-dependency deletion, not a stub-and-decide-later situation
      like the `read_pinerc()` cut was.
      **Verification**: standard discipline -- rebuild, full clean
      rebuild, warning count (expect unchanged), full smoketest suite,
      before/after `size` (small but real win this time, unlike the
      `#ifdef ANSI` cleanup -- `ansiopen`/`rcsid` were actually being
      compiled in, ~176 bytes per the standalone `nm` check above).

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

