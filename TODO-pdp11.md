# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Fix mis-parenthesized ternary in the disk-full "drop to shell" prompt

- [ ] `pine/mailbox.c` and `pine/imap.c` each have an identical
      duplicated `bezerk`-write-failure recovery prompt with:
      `while(!(ch == 1 ? ctrl('Z') : '!' || ch == '\r'))` (found while
      collapsing `F_CAN_SUSPEND` to always-true -- see
      `TODO-pdp11-done.md`; the `1` used to be
      `F_ON(F_CAN_SUSPEND,ps_global)`, which was itself always the
      same bug). `?:` binds looser than both `==` and `||`, so this
      actually parses as `(ch == 1) ? ctrl('Z') : ('!' || (ch ==
      '\r'))` -- and since `'!'` is a nonzero constant, the `else`
      branch is *always* truthy regardless of `ch`. Combined with the
      outer `!(...)`, the loop condition is false on the very first
      keystroke almost no matter what was typed (only `ch==1`, which
      real key codes essentially never are, takes the `ctrl('Z')`
      branch) -- so this "keep bleeping until `^Z`/`!`/Return" retry
      loop actually falls through to the `switch(ch)` below after
      reading just one keystroke, silently doing nothing for any key
      besides the three real cases instead of re-prompting. Almost
      certainly meant `ch == (F_ON(F_CAN_SUSPEND,ps_global) ?
      ctrl('Z') : '!') || ch == '\r'`.
    - This is real but low-severity (only reachable via a mailbox
      write failure, e.g. a full disk, and the visible symptom --
      accepting a stray keystroke instead of re-bleeping -- is minor).
      Left as-is rather than fixed inline to keep the `F_CAN_SUSPEND`
      collapse a pure behavior-preserving substitution; fix both
      identical sites together, with a real repro (but don't fill the disk) per CLAUDE.md's
      verification discipline, and add a regression test if the
      harness can drive it.

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

