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

