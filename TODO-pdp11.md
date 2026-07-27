# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Investigate removing the DEBUG/dprint() infrastructure entirely

- [ ] Disabling the `-DDEBUG` build define (`pine/makefile.lnx`'s
      `DEBUG=` var, see `TODO-pdp11-done.md`) was a **-35152 byte**
      win on its own -- by far the largest single change in this
      project -- because `dprint(n,x)` (`pine.h`) already collapses to
      a true no-op without it. That confirms the underlying machinery
      itself (189 `dprint()` call sites across 20 files, the
      `debug`/`debugfile` globals, `init_debug()`'s log-file-rotation
      logic in `init.c`, the `-d`/`#ifdef DEBUG` argv handling in
      `args.c`) is real compiled weight whenever it *is* turned on,
      not preprocessor-stripped noise -- it's just off by default now.
    - Investigate whether this whole subsystem is worth deleting
      outright rather than leaving it as an opt-in build knob: a
      PDP-11 with no `gdb`/coredump-analysis tooling and no spare
      address space for a 35KB debug logger may never realistically
      use it, but weigh that against losing a real diagnostic tool for
      bring-up debugging on the actual target hardware, where this
      kind of textual trace may be the *only* debugging option
      available. Worth checking whether the now-formalized `ASAN=1`
      build target (`TODO-pdp11-done.md`) covers the same debugging
      needs a different way before committing to deletion.
    - If kept, at minimum leave `-DDEBUG` off by default (already
      done) so production/size-measurement builds never pay for it by
      accident.

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
      identical sites together, with a real repro (fill the target
      disk or otherwise force a mailbox write failure) per CLAUDE.md's
      verification discipline, and add a regression test if the
      harness can drive it.
