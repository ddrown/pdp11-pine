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

### Dead-code sweep, bucket 3: `init.c`, `filter.c`, `status.c`, `os.c`

- [ ] Third bucket, ~24 sites total: `init.c` (11), `filter.c` (10 --
      also has a `FIRST_FOLDER_TRY` guard, needs the same
      never-defined confirmation as `LWP`/`PCTCP`/`PCNFS` above before
      cutting), `status.c` (2), `os.c` (1).

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

