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
      Update: this specific call chain is now moot. Confirmed while
      surveying `pine/folder.c` for dead code (see the new section
      below) that `pico/main.c` wires `Pmaster->folders` to
      `compose_folders()` (`pico/compose_stub.c`), not
      `folders_for_fcc()` -- the composer-subprocess redesign replaced
      the Ctrl-T Fcc picker's c-client `context_apply()` call with a
      flat `FCCLIST`-over-the-handoff-file protocol (`fcc_list`/
      `fcc_count` in `compose_stub.c`, populated by
      `fcc_candidate_list()` in `send.c`). `folders_for_fcc()` itself
      is confirmed to have zero remaining callers anywhere in the tree.
      Update: ran the dedicated sweep this note called for (grep +
      triage over every `sprintf`/`strcat`/`strcpy`/`rfc822_cat`/
      `rfc822_address_line` call into a fixed buffer across `pine/*.c`,
      `pico/*.c`, and the compiled c-client files). Findings, most
      concerning first:
      - [x] `pine/mailview.c`'s `describe_mime()` -- three real
        overflow bugs (`string[200]`, `numx[50]`, `alts[50]`, plus a
        bonus `a->number[20]` heap overflow), all reachable just by
        *viewing* a crafted message. Fixed; full write-up in
        `TODO-pdp11-done.md`.
      - [x] `pine/mailcmd.c`'s `export_message()` -- confirmed and
        fixed, along with two more instances of the same bug class
        found while chasing its repro (`imap/ANSI/c-client/mail.c`'s
        `mail_fetchfrom()`, reachable just by opening FOLDER INDEX;
        `pine/reply.c`'s `pretty_addr_string()`, reachable just by
        viewing any message). Full write-up in `TODO-pdp11-done.md`.
      - [x] `pine/mailcmd.c`'s `save_attachment()` -- confirmed and
        fixed, along with two more overflows in the same function found
        while chasing its repro (a fixed `prompt_buf[200]` "file already
        exists" prompt, and three `tmp_20k_buf`-embedding status-message
        calls with no `grow_tmp_20k_buf()` beforehand). Full write-up in
        `TODO-pdp11-done.md`.
      - [x] `pine/send.c`'s `pine_write_body_header()`/
        `pine_rfc822_output_body()` -- confirmed and fixed (needed an
        actual Send, not Postpone, to reach with the crafted part
        intact -- see the write-up for why). Also fixed two related
        bugs in the same function found along the way: a
        `param->value == NULL` crash risk (not independently confirmed
        reachable) and a real NULL-callback crash on the no-recipient/
        Fcc-only Send path (confirmed, needed to make the regression
        test not risk sending real mail through this dev box's live
        Postfix). Full write-up in `TODO-pdp11-done.md`.
      - [x] `imap/ANSI/c-client/mail.c`'s `mail_valid_net_parse()`,
        `bezerk.c`'s `bezerk_file()`/`bezerk_isvalid()`, `dummy.c`'s
        `dummy_file()`, `pine/context.c`'s `context_find()`/
        `context_find_all()`/`context_digest()`, and `pine/folder.c`'s
        `folder_exists()` -- turned out to be a chain of six related
        overflows, not the two lower-priority items originally guessed
        here (below), reachable just by typing a long string into the
        composer's Fcc field and hitting Send (no `.pinerc` editing
        needed, contrary to what this note assumed). `context_digest()`
        alone is confirmed still config-only. Full write-up, including
        which bugs the two new regression tests each catch, in
        `TODO-pdp11-done.md`.
      - False positive, checked and ruled out: `bezerk.c`'s
        `LOCAL->buf` (the other site this item originally named) is
        `fs_get`'d dynamically (always >= `CHUNK` = 8192 bytes, never
        shrunk below it), and every `sprintf` into it writes only short
        `%.30s`-capped or small-integer diagnostic text -- safe as is.
- [x] `pico/display.c`'s `resize_pico()` (~line 1693) had a similar
      uninitialized-`pscreen`-cell gap to the one fixed in `vtinit()`,
      on the terminal-resize path. Fixed with a fill loop mirroring
      `vscreen`'s existing `vteeol()` approach (not a blanket `memset`,
      since that would also wipe the `v_flag = VFCHG` just set on these
      rows). Confirmed via `valgrind --track-origins=yes` against
      standalone `bin/pico` (composing through `bin/pine` doesn't work
      for this check -- the composer is a separate exec'd subprocess
      valgrind won't follow without `--trace-children=yes`): two
      "Conditional jump or move depends on uninitialised value(s)"
      errors in `updateline()`, traced to `resize_pico()`'s `pscreen`
      allocation, before the fix; both gone after. New regression test
      `test/cases/test_composer_resize_grow.py`. Full write-up in
      `TODO-pdp11-done.md`.
- [x] New finding from the `resize_pico()` valgrind repro above, now
      fixed: `pico/main.c`'s startup "open a file named on argv" path
      did `strcpy(bp->b_fname, argv[carg])` with no length check --
      `b_fname` is `char[NFILEN=80]`, and the enclosing `BUFFER` is a
      plain `malloc(sizeof(BUFFER))` (`bfind()`, `pico/buffer.c`) with
      `b_fname` not the last field, so a long enough argv overflowed
      straight past the end of that heap block. `readin()`
      (`pico/file.c`) had the identical unbounded
      `strcpy(bp->b_fname, fname)` right after, called with the same
      string. Both bounded to `NFILEN-1` with explicit NUL
      termination. Only reachable via direct standalone
      `bin/pico <path>` use with a path over 79 characters -- pine's
      own composer uses a separate `-P <handoff> <result>` dispatch
      path that never touches this code, so not reachable from any
      mail-composing flow. Confirmed via `valgrind` (two "Invalid
      write" heap-overflow errors, 0 bytes past a 152-byte block,
      before the fix; clean after) and also reproduces as a bare
      `SIGSEGV` with no instrumentation at all. New regression test
      `test/cases/test_pico_standalone_longpath.py`. Full write-up in
      `TODO-pdp11-done.md`.

### `pine/` UI files: dead/vestigial code found while surveying send.c, mailcmd.c, folder.c, mailview.c
- [ ] Read through the four largest files under `pine/` end to end
      looking for removal candidates. Findings split into two buckets:
      confirmed-dead-or-DOS-only code (this section, safe mechanical
      cuts) and reachable-but-pointless-for-this-target code that needs
      a judgment call (next section).
      **`pine/send.c`**: `file_type()` and the `FileTypes`/`IsText`/
      `IsText8`/`IsBinary` typedef (~lines 1652-1690, declared
      `pine.h:758`) -- zero callers anywhere in the repo, a leftover
      from the since-removed compose-side attachment-typing code.
      `ENCODE_FROMS`-guarded blocks in `set_mime_types()` (3 sites) --
      the macro is commented out in every `osdep/os-*.h`, dead in any
      configured build. Plus all `#ifdef DOS` sites (~28, confirmed
      `DOS` is never `#define`d in any build this project uses -- only
      the old `makefile.msc` toolchain references it), including all of
      `dos_valid_from()` (406-562), the DOS FCC hack (`dawz_string`/
      `d.fd`, 1365-1381), DOS binary-detection branches in
      `set_mime_types`, and the DOS-only X-Sender/X-Warning header block
      (2375-2393).
      **`pine/mailcmd.c`**: the Zoom (558-565), Unexclude (760-768),
      Apply (771-779), Bounce (877-884), Flag (887-894), and Pipe
      (897-906) command-dispatch cases -- these only ever printed "not
      implemented yet" in stock Pine 3.89, nothing to port; delete the
      cases and their `F_ENABLE_*` feature-flag checks. Two `nntp`
      driver-name string comparisons (2505, 2835) -- no `nntp.c` exists
      anywhere in this tree (also nested inside dead DOS blocks, so
      double-dead). Blocks gated on undefined macros --
      `X_MAXMESSAGES`/`X_NEVER`/`X_FINISH`/`OLDWAY`/`LATER` (lines 639,
      715, 1818, 2038, 2560) -- vestigial commented-alternative code
      that never builds. A stale header comment (line 58) referencing an
      address-capture function that no longer exists. Plus ~30
      `#ifdef DOS` sites (short-cache open mode, `flush_index_cache()`
      calls, the DOS FCC hack in `save()`, `dos_gets`/`append_file`,
      `fixpath()`, a `temp_nam` DOS-suffix variant, a `coreleft()`
      memory check).
      **`pine/folder.c`**: `collection_lister`/`display_collections`/
      `create_collection_display`/`redraw_collection_screen` --
      forward-declared twice (118-122, 146-150) but never defined or
      referenced anywhere in the tree; an apparent stub for a
      multi-collection browse dialog that was never built. Trivial:
      delete the 8 prototype lines. `compare_sizes()` (1728-1733) plus
      its two prototypes (134, 162) -- zero callers, and the `qsort()`
      it was written for isn't called anywhere in the file (the real
      folder-list layout uses `create_folder_display()`'s manual width
      heuristic instead). `folders_for_fcc()` (222-254) and the
      `GetFcc` code path it's the sole entry point for -- confirmed
      dead above, superseded by `fcc_candidate_list()`/`compose_folders()`
      once the composer became a separate process; every
      `case GetFcc:` inside `folder_lister()`'s switches (titlebar
      ~404, help ~675, save branch ~744-767) is unreachable too.
      Moderate-effort removal since `GetFcc` cases are interleaved with
      the still-live `FolderMaint`/`OpenFolder`/`SaveMessage` cases in
      the same switches. Plus `#ifdef DOS`/`DOSXXX` sites across the
      `FOLDER_S`/`FLIST` array layer (2570, 2609, 2640, 2662, 2829,
      2882 -- an alternate disk-backed storage scheme for DOS's tiny
      memory model) and case-insensitive-compare branches elsewhere
      (1678, 1887, 2496, 2702, 2760).
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
      Not yet removed, any of it.

### Deliberate-simplification candidates in `pine/` UI files (need a decision, not just dead-code deletion)
- [ ] Found alongside the dead-code sweep above: code that's live and
      reachable in principle but functionally pointless given this
      target (no network, US-ASCII only). Cutting these is a product
      decision, not a bug fix, so listed separately from the confirmed
      dead code above.
      **`{host}`-remote folder-collection syntax** (`pine/folder.c`):
      `FTYPE_REMOTE`/`FTYPE_OLDTECH`/`FTYPE_ANON` detection in
      `set_ftype()` (2181-2211), the remote branch of
      `find_folders_in_context()` (2365-2391), the `(Remote)`/`(Local)`
      labels in `folder_list_entry()` (1162-1165), and the
      remote-stream-opening branch in `folder_exists()` (1936-1949).
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

