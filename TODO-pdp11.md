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
      - 1x `-Wpointer-compare`: `pine/folder.c:1998` (line number as of
        the `folder.c` dead-code sweep; was `:2067` before it), comparing
        a pointer to `'\0'` (likely meant to dereference it first) --
        worth a standalone look, smells like a real (if probably
        harmless here) bug rather than a style nit.

### `pine/` UI files: dead/vestigial code found while surveying send.c, mailcmd.c, folder.c, mailview.c
- [ ] Read through the four largest files under `pine/` end to end
      looking for removal candidates. Findings split into two buckets:
      confirmed-dead-or-DOS-only code (this section, safe mechanical
      cuts) and reachable-but-pointless-for-this-target code that needs
      a judgment call (next section).
      **`pine/send.c`** -- DONE, see `TODO-pdp11-done.md`.
      **`pine/mailcmd.c`** -- DONE, see `TODO-pdp11-done.md`.
      **`pine/folder.c`** -- DONE, see `TODO-pdp11-done.md`.
      **`pine/mailview.c`**: the `FileStar` (disk-paged) branch of the
      scroll-buffer engine -- `ScrollFile()` (1960-2001),
      `make_file_index()` (2013-2040), and the `FileStar` branches
      inside `format_scroll_text()`, `scroll_scroll_text()`,
      `search_scroll_text()`. The `src` variable that would select
      `FileStar` is itself only ever set inside `#ifdef DOS` blocks in
      `mail_view_screen()`, so on this (or any non-DOS) build `src`
      stays `CharStar` for the whole file and this path can never
      execute. Caveat before deleting outright: this existed to page
      very large messages from disk on a memory-starved DOS box, and a
      PDP-11 has *less* RAM than that DOS target did -- worth deciding
      whether to repurpose this for the PDP-11's own large-message case
      rather than just cut it; treat as a design decision, not a
      routine dead-code sweep. Plus `#ifdef DOS` branches in
      `mail_view_screen()` (162-185, 207-228: incore-vs-file storage
      choice, `dos_gets`/`append_file` fetch path) and in
      `zero_scroll_text()` (1739-1746).
      `pine/send.c`/`mailcmd.c`/`folder.c` done (see above);
      `mailview.c` not yet removed.

### Deliberate-simplification candidates in `pine/` UI files (need a decision, not just dead-code deletion)
- [ ] Found alongside the dead-code sweep above: code that's live and
      reachable in principle but functionally pointless given this
      target (no network, US-ASCII only). Cutting these is a product
      decision, not a bug fix, so listed separately from the confirmed
      dead code above.
      **`{host}`-remote folder-collection syntax** (`pine/folder.c`,
      line numbers as of the `folder.c` dead-code sweep):
      `FTYPE_REMOTE`/`FTYPE_OLDTECH`/`FTYPE_ANON` detection in
      `set_ftype()` (2112-2142), the remote branch of
      `find_folders_in_context()` (2296-2322), the `(Remote)`/`(Local)`
      labels in `folder_list_entry()` (1105-1108), and the
      remote-stream-opening branch in `folder_exists()` (1873-1881).
      Not structurally dead -- only reachable if a pinerc
      `folder-collections`/`incoming-folders` line begins with `{`,
      which nothing on a UUCP-only station would configure. Removing it
      means deciding to disallow `{...}`-prefixed collection syntax
      outright, not just deleting inert code.
      **Magic-number MIME-type sniffing** (`pine/send.c`,
      `set_mime_types()`): GIF/JPEG/TIFF/PostScript/`.snd`/AppleFile/ZIP
      content detection -- vestigial from when the composer could
      attach arbitrary binary files; now the only body ever passed
      through is the user's own typed text, which always resolves to
      `TEXT/PLAIN; charset=US-ASCII`.
      **UW Message-ID size-stats decoration** (`pine/send.c`):
      `update_message_id()`/`mime_stats`/`mime_recur`/`encode_bits`/
      `logbase2` (~1859-1982, ~100 lines) -- cosmetic diagnostic token
      spliced into the outgoing `Message-ID:` header, no functional
      value to a single-user station.
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

