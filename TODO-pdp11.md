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

### Collapse dead `feature-list` (`F_*`) checks -- pinerc can never enable them

- [ ] Same root cause as the `variables[]` cut in `TODO-pdp11-done.md`:
      `.pinerc` is never read or written, so the `feature-list` variable's
      `global_val`/`user_val` are always empty and `process_feature_list()`
      (`pine/init.c`) is a permanent no-op. Confirmed via grep across all of
      `pine/*.c` for every `F_TURN_ON`/`F_TURN_OFF`/`F_SET` call site: of
      pine's 19 feature bits, only `F_USE_FK`, `F_CAN_SUSPEND`, and
      `F_EXPANDED_FOLDERS` have a second, real write path (`-k`/`-z`/`-l`
      command-line flags in `pine/args.c`, plus the "pinef" argv0 check and
      the composer's use-fkeys toggle in `pine.c`/`send.c`/`ttyin.c`) --
      those three stay live. The other 16 (`F_ENABLE_PIPE`,
      `F_ENABLE_FULL_HDR`, `F_ENABLE_BOUNCE`, `F_ENABLE_FLAG`,
      `F_ENABLE_ZOOM`, `F_ENABLE_APPLY`, `F_DEL_SKIPS_DEL`,
      `F_ENABLE_JUMP`, `F_AUTO_EXPUNGE`, `F_ENABLE_TAB_COMPLETE`,
      `F_AUTO_READ_MSGS`, `F_QUIT_WO_CONFIRM`, `F_SIG_AT_BOTTOM`,
      `F_INCLUDE_HEADER`, `F_MIME_FORWARD`, `F_OLD_GROWTH`) can only ever
      be set through the dead `process_feature_list()` path -- they are
      permanently OFF for the life of every run, forever. That makes ~28
      `F_ON()`/`F_OFF()` call sites across `pine/mailcmd.c` (13),
      `pine/mailindx.c` (6), `pine/mailview.c` (4), `pine/reply.c` (4), and
      `pine/pine.c` (1) provably-fixed-outcome dead branches -- e.g.
      `mailcmd.c:715`'s `if(F_OFF(F_ENABLE_FULL_HDR,...)) goto bogus;` means
      the `'h'` full-header-toggle command is unreachable today, and the
      `'0'`-`'9'` digit-jump-to-message shortcut (`mailcmd.c:487-498`) is
      likewise always disabled. `process_feature_list()` itself, its
      `feat_list[]` table, and the `feature-list` variable/`V_FEATURE_LIST`
      entry (currently kept as "live" in the `variables[]` cut) can also be
      removed outright once every reader of those 16 bits is collapsed --
      its only remaining live-write task (`F_USE_FK`/`F_CAN_SUSPEND`/
      `F_EXPANDED_FOLDERS`) is already fully covered by `args.c`'s
      command-line path.

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

