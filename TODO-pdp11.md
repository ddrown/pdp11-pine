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
