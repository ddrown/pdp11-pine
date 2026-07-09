# CLAUDE.md

Guidance for Claude Code sessions working in this repo.

## What this project is

A fork of University of Washington's Pine 3.89, being cut down to fit
the address space of a PDP-11 running 2.11BSD. The target machine has
no TCP/IP — it's a UUCP-connected, local-mbox-only mail station,
outgoing delivery via a local `sendmail`. See `README.md` for the
project pitch and `TODO-pdp11.md` for the authoritative, detailed cut
list, size measurements, and write-ups of every bug found so far.
**Read `TODO-pdp11.md` before starting work** — it is the running log
of what's done, what's in progress, and why past decisions were made.

## Build and test

```
./build lnx              # build bin/pine and bin/pico (native Linux dev build)
./build clean && ./build lnx   # full clean rebuild
python3 test/run_smoketests.py            # run the smoketest suite
python3 test/run_smoketests.py --coverage # same, with gcov coverage summary
```

There is no PDP-11 cross-compiler available here; `lnx` is the stand-in
dev target used to iterate on source before an eventual 2.11BSD
cross-build. Binary size (`nm -S --size-sort`, or just `ls -la bin/pine`)
is a first-class metric — this project's whole point is shrinking it.

Tests live in `test/cases/test_*.py`, each with a `run()` function
that drives `bin/pine` through a real pty via `test/smoketest.py`'s
`Session` class (pexpect-based). `run_smoketests.py` auto-discovers
every case. When a bug fix changes observable behavior, add a
regression test here — see existing cases (e.g.
`test_attach_overflow.py`) for the header-comment convention: explain
the bug, how it was reached, how it was fixed, and what the test does.

## Generated files — edit the source, not the copy

Several files are build-time generated and must not be hand-edited
directly, or the fix will vanish on the next clean rebuild:
- `pine/osdep/os-lnx.c` is assembled from fragments under
  `pine/osdep/` by `pine/osdep/makedep` (see `pine/makefile.lnx`) —
  fix the fragment, not the assembled file.
- `pico/osdep.c`/`pico/osdep.h` are a plain build-time `cp` of
  `pico/os_unix.c`/`pico/os_unix.h` (`pico/makefile.lnx`) and are
  gitignored — edit `os_unix.c`/`os_unix.h`.
- `pine/helptext.c`/`pine/helptext.h` are generated from `pine/pine.hlp`
  (the real tracked source) — edit the `.hlp` file and regenerate.

## Verification discipline for bug fixes

This codebase has had a running audit for fixed-size-buffer overflows
(`sprintf`/`strcat`/manual char-array writes), since a PDP-11 target
has none of the incidental hardening a modern glibc/gcc build has (no
stack protector, different stack layout) — bugs invisible here can be
fatal there. When fixing one:

1. Rebuild (`./build lnx`).
2. Reproduce the bug *before* fixing it. Prefer valgrind for heap
   issues. **Valgrind does not reliably catch a local array
   overflowing into an adjacent same-frame stack variable** — if a
   valgrind repro shows 0 errors but the code is clearly wrong,
   extract the vulnerable logic into a standalone minimal C file and
   compile it with `cc -fsanitize=address -g` (gate old/new behavior
   with `#ifdef FIXED` to compile both), or find a real unmitigated
   crash instead.
3. Apply the fix.
4. Rebuild and reproduce again, confirming a clean result.
5. Run the full smoketest suite (`python3 test/run_smoketests.py`) —
   it should stay green.
6. Add a regression test under `test/cases/` when the harness can
   observe the difference (e.g. the process crashing vs. not).
7. Update `TODO-pdp11.md` with a detailed write-up: what the bug was,
   how it's reachable, how it was verified, how it was fixed.

## Git workflow

- Never commit unless the user explicitly says so (e.g. "commit it").
  Finishing a fix/verification cycle is not itself permission to commit.
- Never push, force-push, amend, or run other destructive/history-editing
  git commands unless explicitly asked.
- Prefer small, focused commits per fix/task rather than bundling
  unrelated changes.

## Working style established in this project

- Tasks are pulled one at a time from `TODO-pdp11.md`'s cut list /
  audit findings. Complete one fully (investigate → fix → verify →
  document) before moving to the next, and don't start the next one
  unprompted — wait for the user's next instruction.
- When auditing a shared/global buffer or pattern (e.g. `pico/edef.h`'s
  16KB global scratch buffer `s`), it's fine — expected, even — to find
  and fix real bugs beyond the nominally-assigned task if they turn up
  during the investigation, as long as each is verified and documented
  individually.
