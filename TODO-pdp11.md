# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

Buffer right-sizing (`tmp_20k_buf`, `s`, `temp_format` -- not feature
cuts, but required regardless) is now fully done; see
`TODO-pdp11-done.md`.

### Standing goal: eliminate compiler warnings
- [ ] `./build clean && ./build lnx` currently produces 180 warnings.
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
      - 1x `-Wpointer-compare`: `pine/folder.c:2067`, comparing a
        pointer to `'\0'` (likely meant to dereference it first) --
        worth a standalone look, smells like a real (if probably
        harmless here) bug rather than a style nit.

### Follow-up: buffer-overflow security audit (raised alongside the above)
- [ ] Both `tmp_20k_buf`'s old fixed-size behavior and `temp_format`'s
      current one share the same root issue: code that formats
      unbounded/attacker-influenced content (recipient addresses,
      subjects, filenames from a mailbox) into a buffer sized only by
      guess/precedent, with no bounds checking at the point of the write
      itself (`sprintf`/`strcat`/`rfc822_cat` don't know the buffer size).
      `tmp_20k_buf` is fixed for its one truly unbounded use
      (`pine_rfc822_header`) via a matching size estimate, but this
      class of bug should get a dedicated sweep: grep for other
      `sprintf`/`strcat`/`rfc822_cat`/`rfc822_address_line`-style calls
      into fixed local/global buffers (e.g. `char tmp[MAILTMPLEN]` in
      `send.c`'s per-MIME-part header writer, `bezerk.c`'s `LOCAL->buf`)
      and confirm each one's real worst-case input is actually bounded by
      its buffer size, rather than "has been big enough so far".
      Two more instances already found while auditing `tmp_20k_buf`
      itself (both described above, now fixed): `compose_mail()`'s
      postponed-message-resume read loop, and `display_parameters()`'s
      MIME-parameter formatting (remote-content-driven, the more
      concerning of the two). A third, more severe instance found while
      auditing `s` (also described above, now fixed): `pico/attach.c`'s
      `ParseAttach()` overflowing `SyncAttach()`'s fixed *stack* arrays
      via unchecked `strcpy()`s from an unbounded parse buffer — an
      actual stack smash confirmed under `valgrind`, reachable by
      ordinary composer typing/pasting, no crafted email needed.
      A fourth candidate, found while closing out `s`'s sizing (see
      `TODO-pdp11-done.md`) but not yet fixed: `pico/composer.c`'s
      Ctrl-T Fcc-folder-picker passes the shared `s` straight to
      `(*Pmaster->folders)()` (pine's `folders_for_fcc()`,
      `pine/folder.c`), which can in turn call `context_apply()`
      (`pine/context.c`) -- both assume their caller's buffer is at
      least `MAXPATHLEN` (+ a `MAILTMPLEN`-sized context pattern)
      bytes, matching this whole class of bug. Not confirmed
      reachable in practice (needs a locally-configured
      folder-collection context long enough to matter), so `s` was
      just sized with headroom over this combined worst case rather
      than fixing the root call chain -- fold into this sweep instead
      of re-opening the `s` item.
- [ ] `pico/display.c`'s `resize_pico()` (~line 1693) has a similar
      uninitialized-`pscreen`-cell gap to the one just fixed in
      `vtinit()` above, but on the terminal-resize path: new rows added
      via `malloc`, and the grown tail of existing rows' `CELL` arrays
      via `realloc`, are left uninitialized. `vscreen`'s equivalent gap
      is already handled there (an explicit `vteeol()` call fills the
      newly-grown column range, gated on `old_ncol < term.t_ncol`), but
      there's no matching call for `pscreen`. Not fixed alongside
      `vtinit()` because `resize_pico()` sets `v_flag = VFCHG` (not 0) on
      these rows, so a blanket `memset` would wipe that flag too — the
      correct fix needs to clear only the grown tail region (mirroring
      `vscreen`'s `vteeol()` approach) rather than the whole-block
      `memset` used in `vtinit()`, and touches resize/redraw logic that's
      riskier to change without a resize-path regression test.
      Note: `resize_pico()` was actually *dead code* on this Linux
      build until the `s`-sizing pass (see `TODO-pdp11-done.md`) fixed
      a missing `#include <sys/ioctl.h>` that had silently compiled out
      every `#ifdef TIOCGWINSZ` block (including both call sites of
      `resize_pico()` itself) -- so this uninitialized-memory gap is
      now genuinely reachable via a real terminal resize, not just a
      latent, unreachable bug.

### c-client: dead TCP/`server_login` code in `osdep.o`
- [ ] Investigated why `imap/ANSI/c-client/c-client.a` is 500KB+: mostly
      an illusion -- its makefile's `CFLAGS = -g -O` leaves every member
      unstripped, and `strip --strip-debug` alone takes it from 514KB to
      164KB with no code changes (same story for `bin/pine`'s on-disk
      size vs. its real `size`-reported text+data+bss). Worth remembering
      when eyeballing `ls -la` on any of these.
      Separately, a real (if small) dead-code pocket was found in
      `osdep.o` (built from `imap/ANSI/c-client/os_lnx.c`, the Linux
      stand-in for whatever os-dependent file eventually targets
      2.11BSD): checked every symbol `osdep.o` defines against what the
      other seven linked `.o` files (`mail bezerk mbox dummy rfc822 misc
      sm_unix`) reference, then cross-checked the remainder against all
      of `pine/`, `pico/`, and `imap/` for any caller outside c-client.
      Now that `smtp.o`/`phile.o` are gone (see `TODO-pdp11-done.md`),
      the entire TCP client stack in `osdep.o` is unreachable -- its only
      callers were `smtp.c`, which isn't compiled:
      `tcp_open`, `tcp_aopen`, `tcp_getline`, `tcp_getbuffer`,
      `tcp_getdata`, `tcp_sout`, `tcp_soutr`, `tcp_close`, `tcp_abort`,
      `server_login`. Also entirely unreferenced anywhere: `hdname`,
      `uname`, `tcp_host`, `tcp_localhost`. Together ~2.9KB of
      text/data/bss -- confirmed `rfc822_date`, living in the same file,
      is *not* part of this (it's called directly by `pine/send.c:860`).
      Not yet removed. Small in isolation, but cheap/clean to cut now,
      and more importantly this whole TCP/`server_login` block shouldn't
      be carried over at all when a real 2.11BSD os-dependent file gets
      written to replace `os_lnx.c`'s role -- no TCP/IP on the target
      means none of it is relevant there either.

