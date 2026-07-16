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
- [ ] `./build clean && ./build lnx` currently produces 179 warnings.
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

### `pine/` UI files: dead/vestigial code found while surveying send.c, mailcmd.c, folder.c, mailview.c
-- DONE, see `TODO-pdp11-done.md`. All four files (`send.c`, `mailcmd.c`,
   `folder.c`, `mailview.c`) fully swept, including the `mailview.c`
   `FileStar` disk-paged scroll-buffer engine (investigated and cut
   outright rather than repurposed -- see the done-file write-up for
   why it wasn't a good repurposing candidate).

### Deliberate-simplification candidates in `pine/` UI files (need a decision, not just dead-code deletion)
- [ ] Found alongside the dead-code sweep above: code that's live and
      reachable in principle but functionally pointless given this
      target (no network, US-ASCII only). Cutting these is a product
      decision, not a bug fix, so listed separately from the confirmed
      dead code above.
      **`{host}`-remote folder-collection syntax** -- DONE, see
      `TODO-pdp11-done.md`. Cut fully (not just the four `folder.c`
      sites originally named here): `pine/context.c`'s
      `context_digest()` no longer parses a leading `{host...}`
      prefix at all, and `pine/folder.c`'s `set_ftype()`/
      `FTYPE_REMOTE`/`FTYPE_OLDTECH`/`FTYPE_ANON` handling in
      `find_folders_in_context()`/`folder_list_entry()`/
      `folder_exists()` is gone, along with the `type` field on
      `CONTEXT_S` (`pine.h`) that stored those flags.
      **Magic-number MIME-type sniffing** -- DONE, see
      `TODO-pdp11-done.md`. Removed the GIF/JPEG/TIFF/PostScript/
      `.snd`/AppleFile/ZIP magic-number detection block from
      `pine/send.c`'s `set_mime_types()`, plus every other
      `body->type == TYPEOTHER` dead branch it left behind (confirmed:
      both real call sites always pass a `TYPETEXT`/`ENCOTHER` body).
      The function's still-necessary job -- picking
      7BIT/8BIT/BINARY `Content-Transfer-Encoding` for the user's
      typed text and setting `subtype=PLAIN`/`charset=US-ASCII` --
      was kept, just with the dead type-guessing stripped out.
      **UW Message-ID size-stats decoration** -- DONE, see
      `TODO-pdp11-done.md`. Removed `update_message_id()`/
      `mime_stats()`/`mime_recur()`/`encode_bits()`/`logbase2()` and
      their `call_mailer()` call site from `pine/send.c` entirely;
      `generate_message_id()` (`pine/reply.c`) already produces a
      fully valid, unique `Message-ID:` on its own (hostname + pid +
      timestamp + counter char), so nothing else needed to change.
      **Restricted ("demo") mode lockdown** (`pine/send.c`
      `check_addresses()` 1181-1218; also `pine/mailcmd.c` 1334, 2695):
      recipient/attachment-save lockdown only reachable via a CLI flag
      (UW's public-demo-account kiosk mode) -- irrelevant to a
      single-user PDP-11 station. Bundle with any broader decision on
      whether restricted-mode is cut project-wide.
      **Multi-collection navigation** (`pine/mailcmd.c`):
      `CNTXT_INCMNG`/`next_folder()` ^P/^N navigation scattered through
      delete/tab-to-next-unread/folder-open. Not remote-specific, but
      worth deciding whether multi-mailbox collections stay in scope at
      all before cutting -- not dead today, still fully wired.
      **ISO-2022 escape-sequence handling** (`pine/mailview.c`):
      the escape table and `match_iso2022()` (868-890), used only in
      `format_scroll_text()`'s line-width accounting (1833-1845) to
      avoid counting escape sequences toward display width. Small
      (~25 lines), self-contained, harmless if left, but a residual
      i18n accommodation under the US-ASCII-only mandate. Low priority.
      Not yet decided or removed, any of it.
      (Separately, not a cut-list item: `pine/mailview.c`'s
      `describe_mime()` has an unbounded `MimeShow alts[50]` array --
      the code's own comment says "BUG -- no bounds checking" -- this
      belongs in the fixed-size-buffer audit section above, not here.)

