# PDP-11 / 2.11BSD size reduction — completed work log

Companion file to `TODO-pdp11.md`, which tracks goals and still-open
items. This file holds the detailed write-ups for everything already
finished — what was cut/fixed, how it was verified, and the size
impact — split out purely because the combined file had grown long
enough that the open items were hard to find. Nothing here is still
actionable; treat it as history, not a work queue.

## In progress / already done
- [x] c-client already excludes remote drivers: only `bezerk mbox phile dummy
      smtp rfc822 misc osdep sm_unix` are built (`imap/ANSI/c-client/makefile:52-56`).
      imap2 (remote IMAP), pop, nntp/nntpclient/news, mh, mmdf, tenex are unlinked.
- [x] Finished removing the address book. `addrbook.c`/`adrbklib.c`/`.h`/
      `abtest.c`/`aetest.c` stay deleted; `addrutil.c` was restored (it holds
      general RFC822 address-formatting utilities — build_address,
      addr_list_string, addr_string, address_is_us — still used throughout
      send.c/reply.c/mailindx.c, unrelated to the nickname address book).
      `expand_address()` (in addrutil.c) was simplified to drop the
      AdrBk-nickname/distlist-lookup branch, since that depended on the
      deleted adrbklib. Removed all call sites: main-menu AddrBook key/command
      (pine.c), Take-Address command (mailcmd.c grab_addr_cmd + TakeAddr menu
      entries in mailindx.c/mailview.c), the composer's ^T "to address book"
      hook (pico/pico.h + pico/composer.c), and end_adrbk() calls. Marked the
      `address-book` pinerc variable disabled (obsolete-style slot, so old
      pinerc files still parse) instead of renumbering V_* constants.
      Regenerated helptext.c/helptext.h from pine.hlp (the real tracked
      source — those two are gitignored build products) after removing the
      orphaned address-book help screens and fixing the composer/index/view
      help text that referenced Take Address / To AddrBk. Verified full
      rebuild + link succeeds and `pine` runs.

## Cuts to make (roughly priority order)

### 1. Drop SMTP network client (confirmed: no TCP/IP on target) — DONE
- [x] Removed `smtp.o` from `imap/ANSI/c-client/makefile.lnx` (dropped from
      `c-client.a` and its build rule; the `mtest` diagnostic target is now
      broken since `mtest.c` calls smtp_* directly, but `mtest` isn't part of
      the pine build so left as-is).
      Correction to original note: `sm_unix.c` is NOT the local-sendmail
      path — it manages `~/.mailboxlist` folder subscriptions, unrelated to
      mail delivery. Local delivery is `send.c: call_sendmail()`, which
      `execl`s `/usr/lib/sendmail` directly and never touched c-client.
- [x] `call_mailer` (`pine/send.c`) unconditionally calls `call_sendmail` now;
      deleted the SMTP branch, `pine_smtp_mail()`, `tidy_smtp_mess()`, the
      `#include "../c-client/smtp.h"`, and the DOS-only smtp-server prompt in
      `dos_valid_from()`. `l_putc`/`l_f`/`l_stream` were NOT touched — despite
      the misleading old comment ("calls smtp's soutr_t function"), that
      buffering machinery is generic and is what `call_sendmail` itself uses
      via `unix_puts` to pipe the message to sendmail.
- [x] `smtp-server` pinerc var marked disabled (same obsolete-slot pattern as
      `address-book`, keeping `V_SMTP_SERVER`'s numeric index stable rather
      than renumbering every `V_*` constant after it); removed the now-dead
      `VAR_SMTP_SERVER` macro and the current_val/quote-stripping code in
      `init.c`. Removed the orphaned `h_sticky_smtp` help text and the
      `smtp-server=` example line from `pine.hlp`'s sample-pinerc block
      (historical changelog mentions of smtp-server were left alone).
- [x] Verified: clean rebuild, `nm pine` shows no `smtp_*`/`pine_smtp_mail`
      symbols at all. text+data+bss dropped from 969422 to 901522 bytes.
      Did NOT do a live compose-and-send test — no MTA installed on this dev
      box, so that's unverified beyond static/link-level checks.

### 2. Strip vestigial NNTP/news UI (drivers already unlinked, so this is dead code) — DONE
- [x] Removed `CNTXT_NEWS`/`FTYPE_BBOARD` context-type bits and `IS_NEWS` macro
      from `pine/pine.h`; removed the `CNTXT_NEWS`-setting loop in
      `folder.c:init_folders()` and every `FTYPE_BBOARD`-conditioned branch in
      `folder.c` (folder listing/painting, add/rename/delete-folder "not
      implemented for bboards" stubs, find-list bboard-vs-mailbox split, sort
      hints, remote-mailbox-spec formatting). Removed now-dead
      `update_news_prefix()` (folder.c) and its two prototypes — its only
      caller was itself inside a permanently-`#ifdef LATER`'d-out block.
      Removed `context_find_bboards()` and `context_find_all_bboard()`
      (`context.c`) — zero callers anywhere in pine once the folder.c branch
      was gone — and their prototypes in `context.h`. Left `context_bboard()`/
      `mm_bboard()` (`mailbox.c`/`context.c`) alone: they're c-client callback
      glue referenced by the *linked* `dummy.c` driver's function-pointer
      table (`dummy_find_bboards`/`dummy_find_all_bboards`), so removing them
      would break the link even though pine itself never calls the code path
      that would invoke them. Confirmed `pine/imap.c` isn't even compiled
      (only `pine/mailbox.c` is, per `makefile.lnx`) so its duplicate/stale
      `context_bboard()` and `ps_global->anonymous` reference don't matter.
- [x] Removed the `nr_mode:1` and `anonymous:1` bitfields from `struct pine`
      (`pine.h`) and the `-nr`/`-a` command-line flags that set them
      (`args.c`) — this was Univ. of Washington's UWIN/clarinet kiosk
      "anonymous news-reader" mode, gated entirely behind a CLI flag with no
      other trigger, so it was all dead weight once news reading is gone.
      Swept every `ps_global->nr_mode` / `ps_global->anonymous` /
      `IS_NEWS` conditional across `mailcmd.c`, `mailindx.c`, `mailview.c`,
      `screen.c`, `reply.c`, `pine.c`, `send.c` and collapsed each to its
      permanently-false dead branch (removed dead `goto`s, dead keymenu
      arrays `nr_index_keymenu`/`nr_anon_index_keymenu`/`nr_view_keymenu`/
      `nr_anon_view_keymenu`, and the now-unreachable `pine_simple_send()` in
      `send.c`, whose only caller was the anonymous-mode forward-as-email
      path in `reply.c`). Note: `ps_global->mail_stream->anonymous` (a
      genuine c-client `MAILSTREAM` field for anonymous IMAP login) and
      `FTYPE_ANON`/`/anonymous` collection syntax (also real, unrelated IMAP
      anonymous-login support) were NOT touched — same name, different
      feature, still fully supported.
- [x] Trimmed news/bboard help text from `pine/pine.hlp`: the `news-collections`
      sample-pinerc block, the "You can also define collections for news..."
      paragraph and its `*`/`\"/nntp\"` bullet points, the bboard/`/nntp`
      lines in the folder COLLECTION SYNTAX list, and the `NEWS *{...}`
      example (all four near-duplicate copies across `h_folder_maint`,
      `h_folder_open`, `h_folder_save`, `h_folder_fcc`). Removed the whole
      "IV. READING NEWS" subsection of `h_news` (the release-notes screen),
      per the constraint to trim it but keep `h_news` itself since it's the
      general release-notes/getting-started screen. Left the historical
      changelog mentions of NNTP/newsgroups alone (they describe past
      releases, same policy as the earlier smtp-server changelog cleanup). A
      couple of minor stray "Bulletin Board"/"newsgroups" mentions remain
      in unrelated help topics (signature-file doc, PC-Pine auto-update doc)
      — low value, not touched.
      Regenerated `helptext.c`/`helptext.h` via `./cmplhelp.sh`/`./cmplhlp2.sh`.
- [x] Verified: clean rebuild with no errors/undefined references; grepped
      whole `pine/*.c` tree for leftover `nr_mode`/`IS_NEWS`/`FTYPE_BBOARD`/
      `CNTXT_NEWS`/`ps_global->anonymous` — none remain outside historical
      changelog text. Smoke-tested via a scripted pty session (open a
      hand-built local mbox folder, main menu, folder index, message view,
      both `?` help screens, back to main menu, quit-with-close) — no
      crashes. text+data+bss dropped from 901522 to 874402 bytes.

### 3. Cut down inline help system — DONE
- [x] Dropped full-screen context help entirely rather than just shrinking it.
      `pine/helptext.c` (the gitignored build product) is generated from
      `pine/pine.hlp` via `./cmplhelp.sh`/`./cmplhlp2.sh`; trimmed
      `pine.hlp` from 3207 lines down to 170 by emptying the 19 sections
      that fed full-screen help screens (`main_menu_tx`, `h_news`,
      `h_mail_text_att_view`, `h_mail_index`, `h_mail_view`,
      `h_folder_maint/open/save/fcc`, and all ten
      `h_composer`/`h_composer_to/cc/bcc/fcc/subject/attachment/browse/ins/search`
      arrays) down to just their `====` header markers (kept the header
      lines since ~15 call sites across `pine.c`/`folder.c`/`mailcmd.c`/
      `mailview.c`/`send.c`/`pico/*.c` still reference these symbols by
      name; deleting the symbols outright would've meant touching every
      call site for no extra byte savings, since the arrays are cheap
      pointers regardless of what's in them).
      Left the ~15 tiny `h_oe_*`/`h_wt_*`/`h_sticky_*`/`h_select_sort`/
      `h_mini_setup` topics alone (~60 lines total) — those aren't shown
      via the full-screen `helper()`/scrolltool path at all, they're
      2-line inline hints that `optionally_enter()`/`want_to()` print
      directly above the prompt line (`ttyin.c: help_text[0]`/`[1]`),
      already exactly the "short one-line prompt" style and cheap
      enough not to bother touching.
      Rewrote `pine/help.c`'s `helper()` (the function every one of
      those ~15 call sites still invokes) to ignore its `text[]`
      argument and just queue "No help screens in this build -- see the
      pine(1) man page" via `q_status_message()`, instead of running
      the old paging/`___include`/scrolltool machinery. Deleted
      `print_all_help()` (walked all topics to send them to a printer;
      pointless once topics are empty) and its sole caller, a hidden
      debug key (`'z'`) on the Main Menu in `pine.c`. Also deleted the
      DOS-only `get_help_text()`/`free_help_text()`/
      `close_help_reference()` functions from `help.c` (dead code for
      this non-DOS target, same rationale as earlier DOS-only removals)
      and the now-unused `copy_fix_keys()` helper they and the old
      `helper()` were the only callers of.
      Deleted `pine/helpindx.c` outright: it's a standalone DOS-only
      tool (only referenced by `makefile.msc`) that built a help-index
      file for content that no longer exists; left `makefile.msc`'s
      other already-stale references (e.g. `addrbook.obj` from the
      address-book removal) alone, matching the precedent that DOS
      makefiles aren't kept in sync since DOS isn't a build target.
- [x] Verified: clean `./build lnx` rebuild with no new warnings from
      `help.c`; scripted pty session opening a local mbox folder,
      pressing `?` in the Folder Index and Message Text screens (both
      showed the new one-line status message), then quitting — no
      crashes. text+data+bss dropped from 874402 to 642298 bytes (the
      generated `helptext.c` string data alone shrank from ~204KB to
      near nothing).

### 4. Drop filter/scoring rule engine — N/A, item was based on a wrong premise
- [x] Investigated `pine/filter.c` (~43KB) expecting a pattern-matching
      filter/score rule language (the incoming-mail scoring/filtering
      feature from later Pine/Alpine versions). That feature does not
      exist anywhere in this Pine 3.89 tree — it was added upstream well
      after this version. `pine/filter.c` is actually the generic
      character-filter-pipe library (`so_*`/`gf_*`: `gf_pipe`,
      `gf_link_filter`, `gf_binary_b64`/`gf_b64_binary`, `gf_qp_8bit`/
      `gf_8bit_qp`, `gf_rich2plain`, `gf_iso_filter`, `gf_wrap`, etc.) —
      core plumbing that MIME decoding, quoted-printable, base64,
      richtext-to-plain conversion, ISO-8859 stripping, and line-wrapping
      all run through, called from `mailview.c`, `reply.c`, `mailcmd.c`,
      and `send.c`. Nothing here is a removable feature; leaving it in
      place. No code changed for this item.

### 5. Restrict to US-ASCII (drop multi-charset MIME conversion) — DONE
- [x] There were no active ISO-8859-*/KOI8 conversion tables to remove: this
      pre-IMAP4rev1-era c-client has no charset-transcoding machinery at all
      (`imap/ANSI/c-client/*.c` only ever sets the literal string
      `"US-ASCII"`/`"CHARSET"` as a MIME parameter name/value), and pine's own
      "charset" handling was always just string labeling/comparison, never
      byte conversion. The only real table-based charset code was
      `pine/osdep/dosextra` (994 lines / 34.5KB of DOS codepage↔Latin-1
      lookup tables), entirely `#ifdef DOS`-gated and not built by
      `makefile.lnx` — deleted the file outright (only referenced by the
      DOS-only `makefile.msc`/`makefile.dos`/`osdep/makefile.dos`, which per
      the established precedent — see `helpindx.c` removal in item 3 — aren't
      kept in sync since DOS isn't a build target).
- [x] Removed `V_CHAR_SET` config option: marked the `character-set` pinerc
      variable disabled (same obsolete-slot pattern as `smtp-server`/
      `address-book` — numeric index kept stable rather than renumbering),
      removed the `VAR_CHAR_SET` macro and `CHAR_SET_TEXT` help text, and
      the `#ifdef DOS` codepage-table-selection block in `init.c` that used
      it (dead on this target; along with it, deleted the DOS-only
      `xlate_to_codepage`/`xlate_from_codepage` globals and their `#ifdef
      DOS` usage sites in `mailcmd.c`/`send.c`, which only ever fed from
      that block).
      Hardcoded the remaining live call sites to US-ASCII: `send.c`'s
      outgoing-MIME charset tagging now always writes `"US-ASCII"` instead
      of `ps_global->VAR_CHAR_SET`; `mailview.c`'s `type_desc()` dropped the
      nine `iso-8859-*` attachment-type-label branches (kept the
      `us-ascii`/"Unknown text" cases); `mailview.c`'s `match_charset()`
      dropped its now-always-"US-ASCII" `device_char_set` parameter, reduced
      to "is this text non-ASCII" (the incoming-text charset-mismatch
      warning banner is kept — an attachment can still *arrive* tagged as a
      non-ASCII charset even though we no longer let the user configure a
      matching display charset); `ttyin.c`'s two raw-mode setup blocks that
      conditionally enabled 8-bit passthrough (`ISTRIP`/`LPASS8`) based on
      `VAR_CHAR_SET` were simplified to always leave 8-bit stripping on.
      Also deleted the dead `#ifdef OLDWAY` block in `os.c`/`osdep/os-lnx.c`'s
      `mime_can_display()` (unreachable regardless of `OLDWAY` — it followed
      an unconditional `return 1`) since it was the last caller of the old
      two-charset-argument `match_charset()` signature.
- [x] Verified: clean rebuild with no new warnings; `nm bin/pine` shows no
      `dos_codepage`/`dos_version`/`read_xtable`/`cp437`-family symbols; full
      smoketest suite (5/5) still passes. text+data+bss dropped from 583162
      to 581554 bytes — a small win since almost none of this was live code
      on the Unix/PDP-11 target to begin with; the real value was closing
      out a TODO item whose premise (large removable conversion tables)
      didn't hold, and shedding the DOS-only dead weight that had built up
      around `V_CHAR_SET`.

### 6. Drop color + mouse support — DONE (premise mostly didn't hold either)
- [x] Neither ANSI/xterm color nor xterm mouse-tracking exist in this Pine
      3.89 tree — both are later Pine/Alpine additions. There's no
      `pine/color.c`, no `V_*COLOR*` pinerc variable, no color escape-sequence
      (SGR `\033[...m`) generation anywhere in `pine/*.c`/`pico/*.c`, and no
      xterm mouse-reporting escapes (`\033[?1000h/l` or similar) either.
      Every "color"/"mouse" hit in the tree is MS-DOS-only: `pine/ttyout.c`'s
      DOS-only `ClearScreen()` (BIOS `int86(0x10,...)` video interrupt, not
      ANSI), `pico/ibmpc.c` (EGA/VGA text-attribute routines for pico's IBM-PC
      screen driver), and a handful of `#if defined(DOS) && defined(MOUSE)`/
      `#ifdef MOUSE` blocks in `pine/ttyin.c`, `pine/mailindx.c`,
      `pine/screen.c`, `pico/display.c`, `pico/pico.c`, `pico/main.c` that
      wrap calls into the DOS INT 33h mouse driver
      (`pico/os_dos.c`/`os_dos.h`). None of this compiles on the Unix/PDP-11
      target — `DOS` is never defined by `makefile.lnx`/`pico/makefile.lnx` —
      so there was no color/mouse-specific branch-stripping to do in the
      shared screen-painting code (no color/mouse parameter is threaded
      through `output_keymenu()`, `Write_to_screen`, etc.).
      Deleted the three files that exist solely to implement DOS color/mouse
      and are referenced only by the DOS-only `pico/makefile.msc` (same
      "DOS makefiles aren't kept in sync" precedent as `dosextra`/
      `helpindx.c` in items 3/5): `pico/os_dos.c` (1459 lines), `pico/os_dos.h`
      (109 lines), `pico/ibmpc.c` (407 lines).
- [x] Verified: clean rebuild, no errors/warnings change, full smoketest
      suite (6/6) passes. text+data+bss unchanged (581554 bytes) — expected,
      since none of the deleted code was ever compiled into the Linux
      binary; the value here was closing out a TODO item whose premise
      (color/mouse support needing removal) didn't apply to this old a Pine
      version, while still shedding ~2000 lines of DOS-only dead source.

### Follow-up: broader MS-DOS dead-code sweep (not a numbered cut, general cleanup)
- [x] Surveyed the whole tree for remaining DOS-only material beyond item 6's
      color/mouse scope and removed the largest, safest chunks (~200KB
      identified total; skipped the long tail of scattered 2-15 line
      `#ifdef DOS` scraps in `send.c`/`pine.c`/`mailcmd.c`/`reply.c`/`init.c`/
      etc. as low-value for the effort):
      - Deleted c-client's five MS-DOS network-stack `osdep` variants
        (B&W/PC-NFS/Novell/PC-TCP/Waterloo TCP) and their DOS-only makefiles,
        confirmed unreferenced by `imap/ANSI/c-client/makefile.lnx`:
        `os_dbw.c/.h`, `os_dnf.c/.h`, `os_dnv.c/.h`, `os_dpc.c/.h`,
        `os_dwa.c/.h`, `dummydos.c`, `sm_dos.c`, and
        `makefile.dbw/.dnf/.dnv/.dpc/.dwa` (~75KB C source + 5 makefiles).
      - Deleted 16 `pine/osdep/*.dos` fragment files plus `os-dos.h`,
        `os-dos.ic`, and `pine/osdep/makefile.dos` (~39KB) — confirmed via
        `os-lnx.ic` that none of these fragments are ever `include()`'d for
        the Linux build (every other platform's `.ic` manifest only
        *mentions* them in comments, describing alternatives; only
        `os-dos.ic` actually `include()`s them). Left `pine/osdep/makefile`'s
        stale generic `os-dos.c:` rule alone (same "DOS makefiles/rules
        aren't kept in sync" precedent as elsewhere).
      - Removed the two giant, explicitly-labeled `#ifndef DOS ... #else
        /* DOS */ ... #endif` halves that switch entire files between Unix
        and DOS implementations: `pine/ttyout.c` (was 2189 lines, DOS half
        was lines 1374-2189, 816 lines) and `pine/ttyin.c` (was 1710 lines,
        DOS half was lines 846-1113, 268 lines) — removed each `#else`
        branch plus its now-redundant opening `#ifndef DOS`, leaving the
        Unix implementation unconditional. Left the ~15 smaller/scattered
        `#ifdef DOS` blocks in `ttyin.c` (mouse/DOS-keyboard bits past the
        giant switch) and in other files alone.
- [x] Verified: clean rebuild, `ttyout.c`/`ttyin.c` compile standalone with
      `gcc -fsyntax-only` producing no new warnings (confirms the `#ifndef
      DOS`/`#else`/`#endif` triples were correctly matched, not some inner
      nested conditional), full smoketest suite (6/6) passes. text+data+bss
      unchanged at 581554 bytes, as expected — every byte removed here was
      already dead-on-Linux code, never compiled into the binary. Value is
      source-tree size (~1084 lines / ~114KB of source deleted across
      c-client + osdep + ttyin/ttyout) and reduced confusion for future
      passes over these files, not runtime footprint.

### 7. Drop external editor + spell-check hooks — DONE
- [x] Removed the alternate-editor feature: deleted `alt_editor()`
      (`pico/os_unix.c`, the true source — `pico/osdep.c`/`.h` are gitignored
      generated copies) and its now-dead `MAXARGS` macro, the `^_` key
      binding (`pico/ebind.h`, guarded by `JOB_CONTROL`) and its prototypes
      (`pico/efunc.h`, both `ANSI`/K&R variants). Removed the `alt_ed` field
      from the `PICO` struct and the `P_ADVANCED`/`MDADVN` mode bit
      (`pico/pico.h`) that gated it — both were used solely by this feature.
      Simplified `pico/display.c`'s `modeline()`, which used to swap a
      key-help slot between "Alt Edit"/'_' and "Where is"/'W' depending on
      whether an alternate editor was configured, to always show "Where is".
      In `pine/send.c`, removed the `pbuf.alt_ed = ps_global->VAR_EDITOR;`
      and `pbuf.pine_flags |= F_ON(F_ENABLE_ALT_ED,...) ? P_ADVANCED : 0;`
      lines that wired it up from Pine's side. Removed the `F_ENABLE_ALT_ED`
      bit (`pine/pine.h`) and its `enable-alternate-editor-cmd` feature-list
      entry/old-growth-bundle references (`pine/init.c`) — feature bits are
      an in-memory bitmask, never serialized raw (feature-list is saved as
      names), so no obsolete-slot shim was needed; an old pinerc naming this
      feature just logs "unrecognized feature" at debug level, same as any
      other unknown name. Marked the `editor` pinerc variable disabled via
      the established obsolete-slot pattern (numeric `V_EDITOR` index kept
      stable) and removed `VAR_EDITOR`/`EDITOR_TEXT` and the
      `set_current_val(vars, V_EDITOR, TRUE)` call, since nothing reads it
      anymore.
- [x] Removed the `spell()` ispell/spell(1) integration: deleted
      `pico/spell.c` outright and dropped `spell.o`/`spell.c` from
      `pico/makefile.lnx`'s `OFILES`/`CFILES` (this actually removes linked
      code, unlike the DOS-only dead code removed in earlier items — spell.c
      was always compiled in). Removed the `SPELLER` macro
      (`pico/os_unix.h`, unconditionally defined to `/usr/bin/spell` for
      every non-COHERENT/AUX target including Linux) and every `#ifdef
      SPELLER`/`#else` pair gating it: the `^T` key binding (`pico/ebind.h`,
      two identical copies in the two keymap tables), the `spell()`
      prototype (`pico/efunc.h`, both variants), the F12 function-key
      mapping in the two standalone-pico `pfkm[]`/`fkm[]` tables
      (`pico/pico.c`, `pico/main.c` — collapsed to the permanent `#else`
      case, `CTRL|'D'`), and the "To Spell" vs. "Del Char" key-help label
      pairs in `pico/display.c`'s `modeline()` (collapsed to "Del Char").
      Removed the one-line "To invoke the spelling checker" entry from
      `pico/bind.c`'s standalone `^G` help-text array (Pine's own help
      screens were already gutted in item 3; this is pico's separate
      built-in help).
- [x] Verified: clean rebuild (`./build lnx`, without `./build clean` — see
      "Build process issues" below) with no new warnings; `nm bin/pine`
      shows no `spell`/`alt_editor` symbols. Full smoketest suite (6/6)
      passes. text+data+bss dropped from 581554 to 576617 bytes (497977 +
      18944 + 59696) — a real, if modest, win: unlike most earlier DOS-only
      removals, `spell.c` was genuinely linked into the Linux binary before
      this change.

### 8. Drop printer support — DONE
- [x] Deleted `pine/print.c` outright (`open_printer`/`close_printer`/
      `print_char`/`print_text*`/`send_printer`/`printer_ready`/
      `p_printer_error`) and dropped it from `pine/makefile.lnx`'s
      obj/cfiles lists and build rule (this removes real linked code, same
      as `spell.c` in item 7 -- not a DOS-only no-op removal).
      Removed every entry point that fed it: `print_message()`
      (`pine/mailcmd.c`, called from the index/message-view `'y'`/`OPF9`
      command, shared via `process_cmd()`), `print_folders()`
      (`pine/folder.c`, called from the folder-list screen's own `'y'`/
      `OPF9` case), and `print_to_printer()` (`pine/mailview.c`, called
      from the generic scroll-text viewer's `'y'`/`PF9` handling for
      attachment/help-text display). Removed the three now-dead command
      dispatch sites and blanked the six `"Y","Print"` key-menu label
      slots across `pine/folder.c`, `pine/mailindx.c`, and
      `pine/mailview.c` (kept the slot position as `{NULL,NULL,0}` rather
      than deleting it outright, since these are fixed 12-slot
      function-key grids shared with unrelated keys).
      Note: `FM_DO_PRINT` (`pine/pine.h`) and its handling inside
      `format_message()` (`pine/mailview.c`) were deliberately kept --
      despite the name, that flag is shared between printing and the
      Export command's flattened-text mode (`export_message()` in
      `pine/mailcmd.c` passes `FM_NEW_MESS|FM_DO_PRINT` to get plain,
      non-interactive formatting), and Export is an unrelated, still-live
      feature.
      Removed the printer-selection UI: `select_printer()`/
      `redraw_printer_select()` (`pine/pine.c`), the Setup mini-menu's
      `'p'`/"Printer" option (dropped from `setup_names[]` and the
      `radio_buttons()` letter string), and the now-orphaned
      `h_oe_choosep`/`h_oe_customp` help topics (`pine/pine.hlp`,
      regenerated `helptext.c`/`helptext.h` via `cmplhelp.sh`/
      `cmplhlp2.sh` -- gitignored build products).
      Marked the `printer`, `personal-print-command`, and
      `standard-printer` pinerc variables disabled via the established
      obsolete-slot pattern (numeric `V_*` indices kept stable), removed
      the now-dead `VAR_PRINTER`/`VAR_PERSONAL_PRINT_COMMAND`/
      `VAR_STANDARD_PRINTER` macros and their `global_val`/
      `set_current_val()` setup in `init.c`, and moved the
      `EXTRA_BEFORE_PRINTER` pinerc-section-divider anchor in
      `write_pinerc()` off of the now-obsolete (and thus skipped)
      `V_PRINTER` slot onto the next surviving variable,
      `V_LAST_TIME_PRUNE_QUESTION`, so freshly-written pinercs keep the
      "system-managed, don't edit below" divider in the right place.
      Left the unused `ANSI_PRINTER`/`DF_DEFAULT_PRINTER`/
      `DF_STANDARD_PRINTER` macros in `pine/osdep/os-lnx.h` alone --
      harmless dead defines, consistent with leaving similar unused
      per-platform-header constants in place elsewhere.
- [x] Verified: clean rebuild (`./build lnx`) with no new warnings (the
      three pre-existing warnings in `folder.c`/`mailcmd.c` just shifted
      line numbers). `nm bin/pine` shows no `print_message`/
      `print_folders`/`open_printer`/`close_printer`/`select_printer`/
      `print_char`/`print_text` symbols. Full smoketest suite (6/6)
      passes. text+data+bss dropped from 576617 to 567448 bytes (489080 +
      18768 + 59600) -- a real reduction, like item 7, since `print.c` was
      genuinely linked in.

### 9. Drop `phile` driver — DONE
- [x] Removed `phile.o` from `imap/ANSI/c-client/makefile.lnx` (dropped from
      the `c-client.a` archive member list, the `ar rc` command, and its
      build rule), same pattern as item 1's `smtp.o` removal. Left
      `phile.c`/`phile.h` themselves untouched (same precedent as
      `smtp.c`/`smtp.h`  — these are upstream c-client driver sources, only
      dropped from the build, not deleted).
- [x] Investigated first: `phile` (arbitrary Unix files as folders) was
      *never* linked into `bin/pine` in the first place — `pine/pine.c` only
      calls `mail_link()` for `bezerkdriver` and `dummydriver`; the only
      other reference to `philedriver` anywhere in the tree was
      `imap/ANSI/c-client/mtest.c` (the c-client diagnostic tool, not part of
      the pine build). Since `c-client.a` is a static archive, the linker
      was already excluding `phile.o` from `bin/pine` — confirmed via `nm`
      showing zero `phile_*`/`philedriver` symbols both before and after
      this change.
- [x] `mtest`'s build was already broken before this change (per item 1,
      `smtp.c` calls in `mtest.c`); removing `phile.o` adds another reason
      it won't link, left as-is since `mtest` isn't part of the pine build.
- [x] Verified: clean rebuild (`./build lnx`) with no new warnings (the
      pre-existing unrelated `imapd.c` fetch-attribute-table build failure,
      documented in item 8, is still present and still pre-existing — c-client
      driver removal doesn't touch `imapd.c`). Had to `rm -f c-client.a
      phile.o` manually once, since `make`'s dependency check considered the
      already-built `c-client.a` up to date against its (unchanged) `.o`
      prerequisites and wouldn't notice the archive-member list itself had
      changed in the makefile recipe. `ar t c-client.a` confirms `phile.o`
      is gone from the archive. `nm bin/pine` still shows no phile symbols.
      Full smoketest suite (6/6) passes. text+data+bss unchanged at 567448
      bytes (489080+18768+59600) — no size reduction, since `phile.o` was
      dead weight in the archive already, not linked into the final binary;
      this cut is about not compiling+archiving unused code each build, not
      about shrinking `bin/pine` itself.

### Follow-up: remove `mtest`/`imapd` diagnostic/server tools (not part of pine build)
- [x] Neither was ever part of the shipped `bin/pine` binary or its build
      path: `imap/ANSI/c-client/makefile.lnx`'s default `all:` target only
      builds `c-client.a`, and `./build lnx` never asked for the `mtest`
      target — confirmed `bin/mtest` never existed even before this change.
      `imapd` (the IMAP4 *server* daemon, for client/server mode) is
      irrelevant on a target with no TCP/IP per the top-of-file premise, and
      was already broken (`imapd.c`'s fetch-attribute-table build failure,
      noted as pre-existing in items 8 and 9).
- [x] Deleted `imap/ANSI/c-client/mtest.c` and its `mtest`/`mtest.o` build
      rules from `makefile.lnx`. Deleted `imap/ANSI/imapd/` entirely
      (`imapd.c`, `imapd.8c`, `Makefile`) — unlike the smtp.c/phile.c
      precedent of leaving unused c-client *driver* sources in place, these
      are whole standalone diagnostic/server programs with no other
      relationship to the pine build, so full removal (rather than an
      unbuilt leftover) was the better cut.
- [x] Updated the top-level `build` script: removed the `imapd`/`mtest` help
      text, the `imapd` symlink setup/teardown, the "Making Imapd" step, the
      `bin/mtest`/`bin/imapd` linking and `size` invocation, and the
      `imapd`-cleaning block from the `clean` target. Removed the stale
      `imapd` symlink left over from before this change (gitignored, not
      tracked).
- [x] Verified: clean rebuild (`rm -f c-client.a *.o` in c-client dir, then
      `./build lnx`) now completes with **zero errors** for the first time
      this session — previously every rebuild logged the pre-existing
      `imapd.c` fetch-attribute-table failure as noise to filter past.
      Full smoketest suite (6/6) passes. `size` unchanged: `bin/pine`
      567448 bytes (489080+18768+59600), since neither tool was ever linked
      into it.

## Buffer right-sizing (not feature cuts, but required regardless)
These fixed global buffers alone can blow a PDP-11 data segment:
- [x] `tmp_20k_buf` (20KB, `pine/pine.c`) — DONE, converted to dynamic
      allocation rather than shrunk to a fixed size. Investigation showed
      this buffer is NOT just a screen-line formatter: `send.c`'s
      `pine_rfc822_header()`/`pine_rfc822_output()` format the *entire*
      outgoing RFC822 header (To/Cc/Bcc address lists, Subject, etc.)
      straight into it via c-client's `rfc822_header_line`/
      `rfc822_address_line`/`rfc822_cat`, none of which take a length —
      they assume the caller already made the buffer "big enough". A
      message with many recipients can genuinely need several KB, so a
      fixed shrink to ~80 columns would have been a real buffer overflow,
      not just a size cut.
    - `pine/pine.c`: `tmp_20k_buf` is now `char *` (was `char[20480]`),
      starting at `TMP_20K_BUF_BASELINE` (2048 bytes) and grown via
      `grow_tmp_20k_buf(need)` (doubling, using `fs_get`/`fs_resize`, same
      alloc wrappers c-client itself uses). `pine/headers.h`'s
      `extern char tmp_20k_buf[]` became `extern char *tmp_20k_buf` +
      the `grow_tmp_20k_buf` prototype. All ~140 existing call sites
      across 10 files needed no changes — C doesn't distinguish
      `array[i]`/`sprintf(array,...)` from the pointer equivalent, so
      only the declaration and the two genuinely-unbounded call sites
      needed touching.
    - `pine/send.c`: added `pine_rfc822_header_size(env, body)` — a
      conservative *upper-bound* estimate (doubles every address's
      personal/mailbox/host/adl length to account for `rfc822_cat`'s
      quoting/escaping, plus fixed per-field/per-address slop) — called
      via `grow_tmp_20k_buf()` right before both call sites that build a
      full header (`call_sendmail`'s path and the Fcc-only path in
      `pine_send_mail`). This mirrors the *content*, not the screen, so it
      scales with however many recipients the user actually adds.
    - `pine/send.c`: decoupled the unrelated `l_putc`/`l_flush_net` output
      batching (used while piping the message body to sendmail, previously
      sharing/reusing `tmp_20k_buf` up to a hardcoded 16384-byte
      threshold) into its own small fixed `l_out_buf` (1024 bytes) — this
      was a pure write()-call-count optimization with no relation to
      header/content size, so tying its sizing to the header buffer no
      longer makes sense once the header buffer is dynamic.
    - Verified: clean rebuild, no new warnings; bss dropped from 59600 to
      40208 bytes (saving ~19KB of *always-resident* static footprint,
      more than a naive fixed shrink would have, since most sends never
      need more than the 2KB baseline). Added
      `test/cases/test_postpone_many_recipients.py`, a new smoketest that
      composes a message to 450 recipients and Postpones it (Postpone
      runs the same `append_message2`/`pine_rfc822_header` code path as
      real Send, but writes to `~/mail/postponed-mail` instead of
      exec'ing an MTA, so it's fully automatable without one) — this
      forces `tmp_20k_buf` through multiple `grow_tmp_20k_buf()` doublings
      and asserts the postponed file contains the first/middle/last
      addresses, the subject, and the body intact.
    - Found (via user review) two more unbounded writes into
      `tmp_20k_buf` that the initial pass missed, both now fixed:
      - `pine/send.c`'s `compose_mail()` "Continue postponed
        composition" path re-reads a saved postponed message by
        walking a pointer through `tmp_20k_buf` via repeated
        `fgets(p, 1024, fp)` with no capacity check, then hands
        `tmp_20k_buf` to `rfc822_parse_msg()` as scratch space for the
        longest unfolded logical header line (which can be nearly the
        whole header). With the buffer shrunk to a 2KB baseline this
        is a real, confirmed overflow: reproduced under `valgrind`
        (an "Invalid write" past the malloc'd block, severe enough to
        corrupt valgrind's own heap-block metadata and abort it) by
        seeding a fresh process with a pre-existing large postponed
        message and resuming it — composing-then-postponing in one
        process doesn't reproduce it, since `tmp_20k_buf` never shrinks
        back down and the postpone's own write already grew it. Fixed
        by growing the buffer as the slurp loop advances (tracking an
        offset instead of a raw pointer, since `realloc` can move the
        block) and again before `rfc822_parse_msg()`, sized off the
        already-known `strlen(file)`. Verified clean under `valgrind`
        afterward (no invalid-write errors, only pre-existing unrelated
        warnings). Added `test/cases/test_resume_postponed_many_recipients.py`
        (and a `postponed_mail=` param on `smoketest.Session` to seed
        `~/mail/postponed-mail` before a *fresh* pine process starts,
        which is what actually exercises this path) — confirmed this
        test fails without the fix and passes with it.
      - `pine/mailview.c`'s `display_parameters()` (used to display a
        MIME part's Content-Type parameters, e.g. when viewing a
        received message) walks a pointer through `tmp_20k_buf`
        appending one parameter's `attribute: value` per iteration,
        also with no bound — and unlike the postponed-mail case, this
        one is driven by *remote* message content (a crafted email with
        many/long Content-Type parameters), not just the user's own
        drafts. Fixed the same way as `pine_rfc822_header_size()`: sum
        the real attribute/value lengths up front and
        `grow_tmp_20k_buf()` before writing.
    - Full suite (8/8) now passes.
- [x] `s` (16KB, `pico/edef.h:69`) audit — found and fixed several real,
      exploitable bugs across ~15 pico files that use this global
      scratch buffer (filenames, search strings, prompts). Pico's own
      line-length convention (`NLINE`, `pico/estruct.h`) is only 256
      bytes, so `s` looked like a much safer candidate for a plain
      fixed-size shrink than `tmp_20k_buf` — but auditing real call
      sites turned up several genuinely unbounded (and exploitable)
      uses instead. (The buffer's size itself was deliberately left at
      16KB rather than shrunk in earlier rounds, pending sizing against
      `wkeyhelp()`'s `term.t_ncol`-scaled accumulation — closed out
      below.)
    - `pico/attach.c`'s `ParseAttach()` parses the composer's Attchmnt
      header field (`"N. filename (size) \"comment\""` syntax) by
      walking a pointer through `s` one character at a time
      (`*p++ = c`) with no bound of its own for the filename, size, and
      comment sub-fields, then `strcpy()`s each parsed field straight
      from `s` into **the caller's fixed-size stack arrays**
      (`SyncAttach()`'s `char file[NLINE], size[32], comment[1024]`)
      with no length check at all — the function's own header comment
      even said "Assumes the size of the buffers passed is large enough
      to hold what's there." Since the Attchmnt field is ordinary
      composer text with no length cap (pico's own line storage is
      dynamically sized), simply typing/pasting a filename, size, or
      comment longer than 256/32/1024 bytes respectively overflows the
      corresponding *stack* buffer in `SyncAttach()` — a classic stack
      smash, reachable with no remote/crafted message involved at all.
      Confirmed under `valgrind`: typing a ~4000-byte bogus filename and
      Postponing corrupted the stack badly enough that the backtrace
      itself showed frames literally made of the injected `'A'` (0x41)
      bytes, and pine's own crash handler (`panic`/`coredump`) caught
      the resulting SIGABRT.
      Fixed by giving `ParseAttach()` explicit `fnlen`/`szlen`/`cmntlen`
      capacity parameters (from `sizeof(file)`/`sizeof(size)`/
      `sizeof(comment)` at the one call site) and replacing every
      `strcpy()` into those output fields with `strlcpy()` — silently
      truncating an overlong filename/size/comment is an acceptable
      degradation here (unlike `tmp_20k_buf`'s recipient lists, where
      truncating would silently drop real addressees), so this stayed a
      fixed-size fix rather than a dynamic-allocation conversion.
      `strlcpy()` isn't available on the actual 2.11BSD/PDP-11 target
      (it postdates 4.3BSD-era libc) or on pre-2.38 glibc, so
      `pico/os_unix.c` now provides a fallback implementation, compiled
      in only when `__GLIBC_PREREQ(2, 38)` isn't satisfied (nested
      `#ifdef __GLIBC__`, since referencing `__GLIBC_PREREQ` at all
      when it isn't a macro is itself a preprocessor error — `&&`
      short-circuiting doesn't save you, because macro expansion of
      the whole `#if` line happens before any of it is evaluated).
      Also bounded all four `*p++ = c` accumulation sites in
      `ParseAttach()` against `sizeof(s)` itself (`s`'s extern
      declaration now carries its real size via a new `SLEN` constant
      in `pico/edef.h`, rather than the old incomplete `extern char
      s[]`) as defense in depth. Verified: rebuilt clean,
      re-ran the same valgrind repro afterward — no more invalid
      writes/stack corruption (some unrelated pre-existing invalid-*read*
      warnings remained in `composer.c`'s `packheader()`, since fixed —
      see below).
      Added `test/cases/test_attach_overflow.py`, which types a ~4000-byte
      bogus attachment filename and Postpones; confirmed this test
      reliably crashes pine before the fix and passes after it.
      Full suite (9/9) now passes.
    - Continuing the call-site survey turned up a second, more severe
      bug: `pico/composer.c`'s `InitMailHeader()` (~line 253) builds the
      Attchmnt field's display text by walking `mp->attachments` and,
      per entry, `sprintf()`ing `"N. filename (size) \"description\","`
      into the shared global `s` and then `strcat()`ing that into
      `addrbuf` — a fixed `malloc(1024)`, regardless of how many
      attachments there are or how long their fields are. Unlike the
      `ParseAttach()` case above (bounded by composer text-entry
      limits), `mp->attachments`' `filename`/`description` fields can
      come from a *resumed postponed message*: `pine/send.c`'s
      `parse_postponed_header()` reads `Attachment-file:`/
      `Attachment-description:` pseudo-header lines with `cpystr()`,
      with no length cap of its own at all. A postponed-mail file with
      one long `Attachment-description:` line therefore overflows both
      the shared 16KB `s` (silently — it's a static array, not a heap
      block, so nothing catches this one directly) and, confirmed under
      `valgrind`, the 1024-byte `addrbuf` heap block via `strcat()` —
      severely enough to corrupt valgrind's own heap metadata and abort
      it; reproduced without valgrind too (glibc's own allocator
      aborts with "malloc(): invalid size (unsorted)" /
      "free(): corrupted unsorted chunks"). Reachable just by resuming
      a postponed message with one oversized attachment description —
      no attachment file or user typing involved.
      Fixed by summing the real field lengths up front and `malloc`ing
      `addrbuf` exactly that size, then formatting each entry directly
      into `addrbuf` (via a running pointer) instead of through the
      shared, fixed-size `s`.
      Fixing this surfaced a third instance of the `LineEdit()`-class
      uninitialized-value bug (see above): `pico.c`'s main `pico()` loop
      declared `int i;` (~line 142) uninitialized and passed `&i` to
      `(*Pmaster->newmail)(&i, ...)`, which only sets `*cursor_moved` on
      paths returning `>= 0` — but `if(i || mpresf)` (~line 234) is
      unconditional, identical in shape to the already-fixed `j` bug.
      Fixed the same way: `i = 0` at declaration.
      Verified: rebuilt clean, re-ran a dedicated valgrind repro (a
      postponed-mail file with a 20000-byte `Attachment-description:`
      line, resumed via "Continue postponed composition") — reliably
      crashes before both fixes (heap corruption assertion in valgrind
      itself), 0 errors after. Added
      `test/cases/test_attach_resume_overflow.py`; confirmed it crashes
      pine (a real, non-valgrind glibc allocator abort) before the fix
      and passes after. Full suite (11/11) now passes.
    - Finished the file-by-file survey: `display.c`'s two remaining
      global-`s` writers (`mlyesno()`'s `sprintf(s, "%s...", prompt)`
      and `wkeyhelp()`'s char-by-char accumulation into `s`) are bounded
      by their callers' fixed/short literal prompts and by
      `term.t_ncol`, respectively — no unbounded input reaches either.
      `pico/os_unix.c`'s `gethomedir()`/`compresspath()` writers into
      `s` are bounded by `NLINE`-sized inputs. The remaining files
      either don't reference the global `s` at all (its name is reused
      as an unrelated local `int status` variable in `buffer.c`,
      `file.c`, `random.c`, `region.c`, and `search.c`) or only appear
      in comments (`bind.c`, `main.c`, `tcap.c`, `tinfo.c`, `word.c`).
      No further unbounded accumulation into `s` itself found.
    - While tracing `s`'s `fixpath()`/`gethomedir()` call chain (used by
      `os_unix.c`'s `gethomedir()`, `file.c`'s Read/Write/Insert-file
      prompts, and `attach.c`'s attachment filenames), found an
      unrelated but real stack-buffer overflow one level down:
      `pico/os_unix.c`'s `pfnexpand()` (~line 958) expands a leading
      `~`/`~username` into the matching home directory, and copies the
      username portion into a local `char name[20]` via
      `for(x = fn+1, y = name; *x != '/' && *x != '\0'; *y++ = *x++);`
      — no bound of its own. Typing an ordinary filename starting with
      `~` followed by more than 19 non-`/` characters (e.g. into the
      composer's Ctrl-R "Read File" prompt, or an attachment filename)
      overflows `name[20]` on the stack. Plain `valgrind`/Memcheck does
      **not** catch this class of bug — it only tracks heap allocations
      and select redzone-instrumented boundaries, not a plain local
      array overflowing into adjacent same-frame stack variables — so
      confirmed it a different way: extracted the vulnerable copy loop
      into a standalone reproduction compiled with
      `-fsanitize=address`, which reported a clean
      "stack-buffer-overflow" (`name` at offset 52 in its frame) on the
      original code and nothing after the fix. Also manually confirmed
      under the real binary that this exact input (Ctrl-R, then a
      filename starting with `~` and 40 `A`s) doesn't crash on this
      x86-64/glibc build (likely luck of stack layout/reordering, not a
      real mitigation) — a reminder that "doesn't crash under valgrind"
      isn't the same as "safe", especially porting to a target (the
      real PDP-11) with none of glibc/gcc's incidental hardening.
      Fixed by bounding the copy loop against `sizeof(name)` and
      rejecting (returning `NULL`, matching the function's existing
      failure mode for the "expanded path too long" case just below it)
      when the username portion doesn't fit — `fixpath()` (the only
      caller) already discards `pfnexpand()`'s return value and leaves
      the original string untouched on failure, so this degrades to a
      harmless "no such file" rather than a crash. Verified: rebuilt
      clean, full smoketest suite (11/11) still passes, and manually
      confirmed a normal `~/...`-relative filename still expands and
      opens correctly through the real composer.
    - The rest of `s`'s call sites (`sprintf`s of short/bounded content
      — paths, prompts, and `browse.c`'s file operations, which stay
      bounded because `mlreply()`/`NLINE`-sized buffers cap whatever
      reaches `s` there) didn't turn up another unbounded accumulation
      *except* one, found while trying to actually settle the "what's
      the real worst case" question the previous round left open
      (`resolve_niks()`'s call into pine's address builder). Chasing
      that down turned into its own round with four distinct real bugs,
      described next.
    - **New verification tool this round: a full local build with
      AddressSanitizer, not just a standalone-extracted repro.** The
      previous `pfnexpand()` bug (above) used a standalone C file
      compiled with `-fsanitize=address` because the vulnerable logic
      was a simple, self-contained loop. This round's bugs are deep in
      real call chains across pico/pine/c-client (curses state, c-client
      ADDRESS structures, the composer's line-list), too entangled to
      usefully extract. Instead: `git worktree add` a scratch checkout,
      rebuild `c-client.a`/`libpico.a`/`pine` there with
      `-fsanitize=address -fno-omit-frame-pointer` added to each
      makefile's `CFLAGS`/`PROFILE` override (same mechanism
      `run_smoketests.py --coverage` already uses for gcov), relink
      `bin/pine`, and drive it with the exact same `test/smoketest.py`
      harness (imported from the worktree's own copy) instead of a
      one-off script. This catches heap-, stack-, *and* global-buffer
      overflows and use-after-frees in the real binary, not just a
      hand-simulated fragment — at the cost of a several-minute rebuild
      per iteration instead of a one-file compile.
      One environment wrinkle: the ASan build crashed on startup before
      reaching the Main Menu at all (`SEGV`, backtrace through
      `gethostbyname` → `_nss_myhostname_gethostbyname3_r` → a symbol
      named `uname`) — `nm -D` showed `uname` exported as a *dynamic*
      symbol in the ASan build (but not the plain build), because
      `imap/ANSI/c-client/os_lnx.c:233` defines
      `char *uname = NIL;` (a per-file username cache used by
      `myusername()`) at file scope with external linkage, and
      AddressSanitizer's ODR-checking machinery makes the linker export
      globals dynamically where a plain build wouldn't. That export let
      the dynamic linker's symbol interposition resolve
      `libnss_myhostname.so.2`'s internal call to the real libc
      `uname()` to pine's *unrelated data variable* of the same name
      instead — a real, if obscure and ASan-tooling-specific, naming
      hazard. Fixed at the true source
      (`imap/ANSI/c-client/os_lnx.c`, not generated) by marking it
      `static` — it's never referenced outside this file. Confirmed
      `nm -D` no longer lists it and the ASan build starts normally.
      (The four sibling `os_*.c` files for other platforms have the
      exact same pattern but aren't part of this project's build, so
      were left alone.)
    - **Bug 1 — heap-use-after-free, `ods.top_l`/`ods.cur_l` after
      `resolve_niks()`.** `resolve_niks()` (composer.c) already carried
      a "BEWARE: this function can cause cur_l and top_l to get lost...
      This would usually result in a nice core dump" comment — a risk
      the original UW authors flagged but never fixed. When an
      entry's address(es) get rewritten, `resolve_niks()` builds an
      entirely new line list (`InitEntryText`) and frees the old one
      (`zotentry`) — but `ods.top_l` (and, outside the one `CTRL|'X'`
      call site that already patches `ods.cur_l` up per a
      "need to be careful here" comment, `ods.cur_l` too) can still
      point into the just-freed old list. Confirmed under
      AddressSanitizer: typing ~500 bytes of ordinary text into To:
      and pressing `CTRL|'N'` (next field) reliably produced a
      heap-use-after-free in `next_line()`, called from `entry_line()`
      from `InvertPrompt()` re-painting the *new* current field's
      prompt right after `resolve_niks()` ran on the old one — no huge
      address needed, no remote content, just ordinary composing.
      Fixed by having `resolve_niks()` itself repoint `ods.top_l`/
      `ods.cur_l` at the freshly built list whenever they referenced
      the entry being resolved, right before freeing the old one —
      this covers every caller (`CTRL|'N'`/`CTRL|'P'` field navigation,
      `CTRL|'O'` postpone, `CTRL|'X'` send) uniformly instead of
      patching each call site.
    - **Bug 2 — global-buffer-overflow, `pine/addrutil.c`'s
      `addr_string()`.** Formatted a single address into
      `static char string[MAX_ADDR_EXPN+1]` (1001 bytes) via
      `rfc822_write_address()`, which — like every c-client RFC822
      formatter in this file — does unbounded `strcat()`s
      (`rfc822_cat()`) trusting the caller's buffer is "big enough".
      `addr->mailbox`/`host`/`personal`/`adl` are `cpystr()`'d with no
      length cap anywhere upstream, so one address over ~1000 bytes
      overflows this global. Reachable simply by resuming a postponed
      message with a long To: address — fires in `outgoing2strings()`
      before the composer even redraws, confirmed under
      AddressSanitizer with a 20000-byte address. Fixed by sizing the
      buffer dynamically (grown on demand, same technique as
      `tmp_20k_buf`) instead of a fixed size — the function's
      "returns pointer to internal static buffer, caller doesn't own
      it" contract is unchanged, so no caller needed updating.
    - **Bug 3 — global-buffer-overflow, `resolve_niks()`'s call into
      `build_address()`.** This is the bug the whole round set out to
      chase down: `resolve_niks()` passed the shared 16KB global `s`
      directly to `(*Pmaster->buildaddr)()` (pine's `build_address()`)
      as its output buffer. Like `addr_string()` above,
      `build_address()`/`rfc822_write_address()` write with unbounded
      `strcat()`s. A single To:/Cc:/Bcc: field over 16KB (trivial to
      reach by pasting a large recipient list, or via a resumed
      postponed message) overflowed `s` itself — confirmed under
      AddressSanitizer, which identified the overflow landing exactly
      "0 bytes after global variable 's' ... 32 bytes before ...
      'mline_open'" (`edef.h`), i.e. corrupting the very globals
      declared right after `s` that the original call-site survey (two
      rounds ago) had flagged as the ones at risk if `s` were ever
      overflowed. Fixed by giving `build_address()` its own buffer,
      sized off the same upper bound used to size the input buffer
      (`sbuf`) instead of the shared fixed-size `s` — `s` is no longer
      touched by this function at all.
    - **Bug 4 — stack-buffer-overflow,
      `imap/ANSI/c-client/rfc822.c`'s `rfc822_address_line()`.**
      Formats each address in an envelope's To:/Cc:/etc. list into a
      fixed local `char tmp[MAILTMPLEN]` (1024 bytes) one at a time —
      same unbounded-`strcat()`-into-a-"should be big enough"-buffer
      pattern as the two bugs above, but this one is inside c-client
      itself rather than pine, and triggers on the *write* path
      (`pine_rfc822_header()`, called when Postponing or Sending
      actually writes the RFC822-formatted header out), not just the
      *read/resume* path. A single address over 1024 bytes overflows
      this stack buffer — confirmed under AddressSanitizer via
      Postponing a resumed message with a 20000-byte To: address.
      Fixed by sizing `tmp` per-address (`fs_get`/`fs_give`, matching
      this file's own existing allocation convention) instead of
      assuming `MAILTMPLEN` always fits one address.
    - This is likely not an exhaustive list — `rfc822.c` has several
      more address/text formatters (`rfc822_header_line`,
      `rfc822_write_body_header`, the group-address branches in
      `rfc822_address_line`/`rfc822_write_address` themselves) built on
      the same "caller's buffer is big enough" assumption, some
      relying on `pine_rfc822_header_size()`'s upper-bound accounting
      (itself only as good as its own estimate) rather than sizing
      themselves. Folded into the "Follow-up: buffer-overflow security
      audit" item below rather than re-auditing every c-client
      formatter right now.
    - Verified end-to-end: rebuilt the ASan worktree after each of the
      four fixes and re-ran the resume-postpone repro, confirming each
      fix individually cleared its bug (no crash, no new one uncovered
      until the next layer). Final combined run — resume a postponed
      message with a 20000-byte To:, tab up through the header fields,
      Postpone, quit — completes cleanly end-to-end with 0 ASan
      reports. Added `test/cases/test_resume_postponed_huge_address.py`
      covering all four. Full smoketest suite (12/12) passes on a
      normal (non-ASan) rebuild.
    - **Final round: `wkeyhelp()`'s `term.t_ncol`-scaled write, and the
      shrink itself.** `pico/display.c`'s `wkeyhelp()` (paints the
      composer/pico key-help footer) builds each row into `s` via
      `spaces = term.t_ncol/6` and then, per key slot, pads out to
      `spaces-extra` characters — completely unbounded against `s`'s
      actual size for a wide enough terminal. Extracted the accumulation
      loop into a standalone file compiled with `-fsanitize=address`
      (real terminals don't get anywhere near the ~16374-column
      threshold needed to overflow the original 16KB `s`, so a live
      repro wasn't the first port of call): confirmed a clean
      global-buffer-overflow at `term.t_ncol` a few hundred columns past
      that threshold, 0 errors below it.
      Trying to reproduce this through the real UI (resize the pty wide,
      SIGWINCH, redraw) kept silently *not* crashing — chasing that
      down surfaced a much bigger, unrelated, pre-existing bug: on this
      Linux/POSIX build, `pico/os_unix.c` only `#include`s
      `<sys/ioctl.h>` when `a32`/`cvx` (two obscure 1990s Unix variants)
      are defined, neither of which apply here. Without that include,
      `TIOCGWINSZ` is never defined, so *every* `#ifdef TIOCGWINSZ`
      block in the file — reading the real window size at startup,
      registering the `SIGWINCH` handler, `resize_pico()`'s two call
      sites — silently compiles out. Both `pico` and pine's composer
      were therefore permanently stuck at the compile-time
      `NROW`/`NCOL` default (`pico/os_bsd.h`, 24x80) on this build,
      never reading the real terminal size and never responding to a
      resize, no matter the pty's actual dimensions. Confirmed via a
      temporary debug print in `wkeyhelp()` (always `t_ncol=80`
      regardless of pty size) and via `/proc/<pid>/status`'s `SigCgt`
      bitmask (signal 28/`SIGWINCH` absent — never registered at all).
      Fixed by moving `#include <sys/ioctl.h>` out from under the
      `a32`/`cvx` guard to be unconditional (matching `pine/ttyout.c`'s
      own unconditional include of the same header for the same
      ioctl).
      That one-line fix immediately exposed a second, cascading,
      equally pre-existing bug: `winch_handler()` was declared/defined
      with old-style empty parens (`SIGTYPE winch_handler()`) in both
      its `#ifdef ANSI` prototype and its real definition, which modern
      C treats as "takes no arguments" — a hard type mismatch against
      `signal()`'s expected `void (*)(int)`, since this whole function
      body had never actually been compiled before (same dead
      `#ifdef TIOCGWINSZ` block) to surface the error. Fixed by giving
      it a real `int sig` parameter, matching the sibling
      `do_hup_signal(int)` right next to it. Also removed two now-
      redundant/conflicting local `extern int resize_pico();`
      forward-declarations (inside `winch_handler()` and the composer
      startup resize block) that only became visible as a *second*
      prototype-mismatch error once the first was fixed — `resize_pico`
      already has a correct `int resize_pico(int, int);` prototype in
      `efunc.h`, included earlier in the same file.
      With real resizing restored, re-ran the same wide-terminal repro
      end-to-end through the actual signal path (not just the
      standalone extraction) under a from-scratch `-fsanitize=address`
      rebuild of `libpico.a`: `winch_handler` → `resize_pico` →
      `update` → `wkeyhelp`, a genuine global-buffer-overflow into `s`,
      both in standalone `pico` (via its `sgarbk`-gated key-help
      redraw) and in pine's composer (confirmed pine's own
      `_columns`/`MAX_SCREEN_COLS` clamp does *not* protect pico's
      independent `term.t_ncol` — the composer's key-help footer is
      exposed to the real, unclamped terminal width too). Also manually
      confirmed, as a non-security side effect, that this restores
      real, correct behavior: pine's composer footer now properly
      re-flows (extra inter-key padding) when resized to 200 columns,
      instead of staying frozen at the old 80-column layout.
      Fixed `wkeyhelp()` itself by bounding every `*obufp++ = ...` write
      against `s + sizeof(s) - 1` (same `p < s + sizeof(s) - 1` idiom as
      `ParseAttach`'s fix above), truncating the rendered key-help line
      instead of overflowing — verified clean under the same ASan
      rebuild at `t_ncol` up to 65535 (the largest a `struct winsize`
      field can hold), and that ordinary 80-column rendering is
      byte-for-byte unchanged.
      With `wkeyhelp()` provably bounded regardless of `s`'s size, the
      remaining question was just the *other* call sites' real worst
      case. Re-examined `pico/composer.c`'s Ctrl-T Fcc-folder-picker
      (`(*Pmaster->folders)(s)` → pine's `folders_for_fcc()` →
      sometimes `context_apply()`) and found it assumes up to
      `MAXPATHLEN` (4096 on this Linux dev build, via glibc) plus a
      `MAILTMPLEN`-sized (1024) context-pattern buffer — not confirmed
      reachable in practice (would need an unusually long local
      folder-collection context, not remote/attacker content), but not
      disproven either, and out of scope to fully chase down in this
      round. Rather than leave it riding on an inflated `s`, sized the
      new `s` at 8192 bytes (2x that combined worst case, comfortable
      headroom) and filed the folders_for_fcc/context_apply path itself
      as a new candidate for the "Follow-up: buffer-overflow security
      audit" item in `TODO-pdp11.md`, alongside the already-open
      general sweep.
      Also flagged (not fixed, folded into the same TODO-pdp11.md
      `resize_pico()` item): now that real resizing works, its existing
      known `pscreen`-cell uninitialized-memory gap is reachable via a
      genuine resize too, not just latent/dead code as before.
      Verified: clean rebuild, full smoketest suite (13/13) passes.
      Added `test/cases/test_composer_wide_terminal.py` (resizes the
      pty to 20000 columns while composing and confirms pine survives)
      — confirmed via `git stash` that it reliably reproduces pine's own
      "Bug in Pine detected: Received abort signal" crash handler
      catching the old `wkeyhelp()` overflow when only the `display.c`
      fix is reverted (include/prototype fixes kept, so the resize path
      is genuinely live), and passes with the fix restored. `size`
      before/after: `bss` dropped from 30224 to 22032 bytes for
      `bin/pine` (491522+18776+22032 — the full 8192-byte `s` shrink,
      16384→8192) and from 18856 to 10664 for `bin/pico`.

- [x] `temp_format` (10KB, `pine/ttyout.c:1098`) — took the "audit every
      call site" branch rather than the `stdarg.h` rewrite (which
      remains the bigger lift, still not needed once the call sites are
      proven bounded). Found only two things:
    - `Stripe0()`/`Stripe3()` (one of the two named offenders) turned
      out to be entirely dead code — grepped the whole tree, zero
      callers anywhere outside their own definitions/declarations.
      Deleted both functions (`pine/ttyout.c`) and their prototypes
      (`pine/pine.h`, both the ANSI and old-style K&R declaration
      blocks) outright, closing off that half of the concern instead of
      auditing a function nothing calls.
    - Every remaining `PutLine1`..`PutLine5` call site (`pine.c`,
      `other.c`, `mailindx.c` x3, `screen.c` x5) formats only:
      fixed string literals; a single `%c` from a `char`; `%d`/`%3d`
      from an `int` (bounded by int range); `comatose()` (own
      `static char buf[100]`, never overflowed — the DOS branch's
      `buf[16]` is dead code, `DOS` isn't defined in this build);
      `BAR_STATUS()` (fixed 3-char-or-less literals); `int2string()`
      (own `static char string[20]`, `%d` of an `int` always fits); and
      `repeat_char()`, which self-clamps to its own `bb[512]` regardless
      of what's asked for. The one genuinely open-ended input was
      `other.c`'s `redraw_klocked_body()`: `klockame`/`klockin` were raw
      pointers into `VAR_PERSONAL_NAME`/`VAR_USER_ID`.
      `VAR_USER_ID` turns out to always be `get_system_login()`
      (`pw_name` from `getpwuid()`) on this non-DOS build — the
      `user-id` pinerc variable is DOS-only and never consulted here —
      so it's bounded by a trusted local `/etc/passwd` entry. But
      `VAR_PERSONAL_NAME` *can* come straight from the pinerc file
      (`cpystr()` of an `expand_variables()` result, no length cap) when
      a user sets `personal-name=...`, confirmed by testing with a
      pinerc containing a `personal-name` line and checking it rendered
      correctly on the keyboard-lock screen. Fixed by copying both into
      new fixed `klockin[128]`/`klockame[128]` buffers via `strlcpy()`
      (already available in this tree — see the `s`/`ParseAttach` writeup
      above) instead of storing raw pointers, closing off the only
      unbounded input reaching `temp_format` while leaving the
      "acceptable to silently truncate a locally-configured display
      name" behavior consistent with the `ParseAttach` precedent.
      With every input now audited/bounded, computed the true worst
      case across all call sites: `screen.c`'s
      `update_titlebar_message()`, `"%s of %s %3d%% %s%s"` with two
      `comatose()` results (~13 bytes each) + `%3d` (3) + `BAR_STATUS()`
      (3) + `repeat_char()`'s hard 511-byte cap + ~20 bytes of literal
      text — call it ~570 bytes, well clear of `redraw_klocked_body()`'s
      now-bounded ~290-byte worst case. Shrank `temp_format` from 10240
      to `MAILTMPLEN` (1024, matching the same constant already used
      throughout c-client for exactly this "one formatted line" use
      case) — comfortable headroom over the ~570-byte audited worst
      case without picking an arbitrary new number.
      Verified: rebuilt clean, full smoketest suite (12/12) still
      passes; manually drove the keyboard-lock screen (`k` from Main
      Menu) with a pinerc `personal-name` set, confirming the display
      still renders the name correctly through the new bounded buffers
      and that lock/unlock still works end-to-end. Confirmed the actual
      size win via `size bin/pine` before/after: `bss` dropped from
      39216 to 30224 bytes (the 9216-byte `temp_format` shrink, exactly
      10240 − 1024), `text` also dropped slightly from the two deleted
      functions.

## Build process issues
- [x] `pine/osdep/os-lnx.c` is a generated file (assembled by
      `pine/osdep/includer` from `os-lnx.ic` + fragment files, per the
      rule at `pine/makefile.lnx:140-149`), but the fixes needed to
      make it build with a modern glibc had been committed directly
      into the generated `os-lnx.c` instead of into the
      fragments/template it's generated from. A `./build clean &&
      ./build lnx` regenerated `os-lnx.c` from source and silently
      reintroduced the bugs, clobbering the checked-in fixes (this
      happened in an earlier session; `os-lnx.c` was reverted by hand
      rather than fixed properly at the time).
      Fixed the four generation inputs (confirmed by diffing a
      from-scratch regeneration against the previously-committed,
      working `os-lnx.c`, then iterating until the diff was empty
      modulo genuine cleanups):
        - `pine/osdep/os-lnx.ic:117` included `fgetpos.non` (a
          non-ANSI ftell/fseek-based emulation of fgetpos/fsetpos,
          whose fseek/ftell-as-long assumption doesn't hold for
          glibc's opaque `fpos_t`) — now includes the plain `fgetpos`
          fragment instead, which just calls the real ANSI
          `fgetpos()`/`fsetpos()` that glibc actually provides.
        - `pine/osdep/err_desc` referenced the removed-from-glibc
          `sys_errlist[]` array directly — now calls `strerror()`
          instead (same fix already used previously in the hand-patched
          generated file; this fragment is `include()`'d by most/all
          `os-*.ic` platform manifests, not just Linux's, but all other
          platforms are DOS/dead-Unix targets outside this project's
          scope so no regression risk in practice).
        - `pine/osdep/tempnam` and `pine/osdep/writ_dir` tested
          `st_mode & S_IFDIR`/`(st_mode & S_IFMT) == S_IFDIR` directly
          instead of using the portable `S_ISDIR()` macro, and
          `tempnam` declared a `u_int` (BSD-ism, not always defined)
          instead of `unsigned int` — both now fixed at the fragment
          source, matching the pattern already used in the previously
          hand-patched generated file.
        - `pine/osdep/mimedisp` still had the dead `#ifdef OLDWAY`
          two-charset-argument `match_charset()` block in
          `mime_can_display()` that had already been deleted from the
          generated `os-lnx.c`/`os.c` in an earlier session (commit
          `ace02e2`) but never from the fragment it's generated from —
          deleted here too, so regeneration doesn't resurrect it.
      Also found (not previously flagged) a redundant, already-provided
      `#include <sys/stat.h>` that had been added directly to the
      generated `os-lnx.c`'s header block; `sys/stat.h` is already
      included unconditionally by `pine/osdep/os-lnx.h:202` (`os.h` is
      a symlink to it, included via `headers.h`), so no fragment change
      was needed there — the regenerated file is simply correct without
      it. Also fixed two dependency-list references to
      `osdep/fgetpos.non` that should have said `osdep/fgetpos`
      (`pine/makefile.lnx:142` and `pine/osdep/makefile`'s `os-lnx.c:`
      line; left the latter file's other platforms' dependency lines
      and the rest of its generated content untouched by hand-editing
      only the one line, rather than rerunning
      `pine/osdep/makedep` wholesale — that pulled in unrelated churn
      for other platform targets, including one referencing
      `pine/osdep/os-dos.ic`, a fragment file that doesn't exist in
      this tree).
      Verified: `./build clean && ./build lnx` now regenerates
      `os-lnx.c` from scratch with no errors and no warnings related to
      `os.c`/`os-lnx.c`; diffing the regenerated file against the
      previously-committed hand-patched one shows only the redundant
      `#include` removal and a cosmetic fgetpos/fsetpos comment fix
      (now correctly says "just a call to" instead of the stale
      "emulation of", matching the fragment's actual, non-emulated
      behavior) — no functional difference. Full smoketest suite (9/9)
      passes. A clean rebuild is no longer destructive.
- [x] Hit the same class of bug in `pico/osdep.h`/`osdep.c`, which
      `pico/makefile.lnx:41-47` generates as a plain `cp` of
      `pico/os_unix.h`/`os_unix.c` (no fragment assembly, unlike
      `os-lnx.c`) — but those two files are gitignored, so the
      previously-working copies (evidently hand-patched at some point,
      same as `os-lnx.c`) had no git history and were unrecoverable
      once `./build clean` deleted them in an earlier session. Unlike
      the `os-lnx.c` case at the time, this one *was* fixed at the true
      source immediately:
      `pico/os_unix.h` had an unguarded `extern int kbseq();` that
      conflicted with `os_unix.c`'s `#ifdef ANSI` prototype
      `int kbseq(int *);` (now wrapped in the same `#ifdef ANSI` as the
      rest of the file), and an `extern char *getcwd(char *, int);`
      that conflicts with glibc's own `getcwd(char *, size_t)` on Linux
      (now excluded via `&& !defined(LINUX)`, matching the `-Dbsd
      -DLINUX` flags `makefile.lnx` builds pico with). A clean
      `./build clean && ./build lnx` now regenerates both `os-lnx.c`
      (also fixed at the source now, per the item above) and
      `pico/osdep.[ch]` (fixed at the source) and produces the same
      binary sizes as
      before.

### Follow-up: buffer-overflow security audit (raised alongside the above)
- [x] `pine/mailview.c`'s `type_desc()` (`static char type_d[200]`) had
      one branch — `TYPEAPPLICATION`/`octet-stream`, the "Attached file
      "%s"" (and sibling "File "%s"") case — that `sprintf`s a MIME
      parameter's `value` (the attachment's `name=` parameter, taken
      directly from a *received* message) into `type_d` **without** the
      `%.100s`-style precision cap every other branch in this function
      uses. Confirmed exploitable under valgrind: a message with an
      `application/octet-stream` part whose `name=` parameter is 500
      bytes crashed pine (SIGSEGV) as soon as the message view's
      `describe_mime()`/`part_desc()` formatted the "not shown"
      attachment description line — no attachment or user typing
      involved, just viewing an ordinary received message with an
      oversized parameter, same stack-smash signature (backtrace frames
      made of the injected `'A'` bytes) as the earlier `ParseAttach()`
      bug. Fixed by adding `%.100s` to both `sprintf`s, matching the
      convention already used by this function's other branches.
      Verified: rebuilt clean, confirmed under valgrind that the same
      repro (stashing the fix) reliably reproduces the invalid
      write/crash beforehand and is gone afterward (0 errors, exit
      status 0). Added `test/cases/test_mime_filename_overflow.py`
      (views a message with a 500-byte attachment `name=` parameter);
      confirmed it fails (crash) before the fix and passes after.
      Full suite (10/10) now passes.
- [x] `pico/composer.c`'s `packheader()` (~line 2487) did
      `if(line->text[strlen(line->text)-1] == ',')` with no check that
      `line->text` is non-empty first — when it is empty, `strlen()-1`
      underflowed (`size_t`) and wrapped around to read one byte *before*
      the heap block, which valgrind flagged as an invalid read every
      single time a composer session is packed for sending/postponing
      (reproduced with a completely ordinary compose+postpone, no
      attachments involved, so unrelated to the `ParseAttach()` fix
      above). Didn't appear to crash in testing (probably silently read
      adjacent heap metadata rather than unmapped memory) but was still a
      real out-of-bounds read, pre-existing and not introduced by the
      `ParseAttach()` work. Fixed by guarding with `*line->text &&` before
      the `strlen()-1` indexing.
      Fixing this and re-running valgrind surfaced a second instance of
      the identical pattern in the same file: `resolve_niks()`
      (~line 2092) did `line->text[i-1] == ','` (and again at ~line 2101,
      `line->text[i-1] != ' '`) where `i = strlen(line->text)`, same
      unguarded read one byte before an empty entry's heap block. Fixed
      both by requiring `i &&` before indexing at `i-1` (empty line now
      just skips the comma/space bookkeeping, matching the `packheader()`
      fix's treatment of empty text as "nothing to check"). Verified:
      rebuilt clean, re-ran the plain compose+postpone valgrind repro —
      both invalid reads are gone.
- [x] `pine/send.c:714` did `real_current_msgno =
      ps_global->sort[ps_global->current_sorted_msgno];` unconditionally,
      reading one element before/after `sort`'s allocated array in at
      least one path through a plain compose+send (surfaced by the same
      valgrind repro used to verify the `packheader()`/`resolve_niks()`
      fixes above, once their invalid reads stopped drowning it out).
      Different bug class — an index/allocation-size mismatch on
      `ps_global->sort`, not a missing-empty-string-check. Turns out
      `pine/newmail.c:208-209` already has the exact same
      `ps_global->sort[ps_global->current_sorted_msgno]` read guarded
      with `(ps_global->current_sorted_msgno <= 0L) ? 0L : ...` —
      `current_sorted_msgno` is `-1` before a folder has a current
      message selected (see `pine.c:261`, `mailindx.c`'s `< 0L` checks),
      which is out of bounds for `sort`. Fixed `send.c:714` with the
      identical guard, matching existing codebase convention rather than
      inventing a new one. Verified: rebuilt clean, re-ran the plain
      compose+postpone valgrind repro — no more invalid reads at all;
      full suite (9/9) still passes.
- [x] With all invalid reads/writes cleared, a further valgrind pass over
      the same plain compose+postpone flow surfaced three
      "uninitialised value" warnings and a small startup leak. Two fixed:
      - `pico/composer.c`'s `LineEdit()` declared `int j;` (~line 850)
        uninitialized and passed `&j` to `(*Pmaster->newmail)(&j, ...)`
        (~line 877), which only sets `*cursor_moved` (`pine/newmail.c`)
        on paths returning `>= 0` — its first check returns `-1` without
        touching it. But `if(j || mpresf)` (~line 883) is *unconditional*,
        outside that `>= 0` block, so `j` got read uninitialized whenever
        `new_mail()`'s earliest bail-out fired. Fixed by initializing
        `j = 0` at declaration.
      - `imap/ANSI/c-client/bezerk.c`'s `bezerk_ping()` (~line 838) called
        `stat(LOCAL->name,&sbuf)` and used `sbuf.st_size` without checking
        the return value — a failed `stat()` (mailbox deleted/inaccessible
        mid-session) left `sbuf` uninitialized. Fixed by guarding with
        `!stat(...) &&`, matching the same idiom already used a few
        hundred lines up in this file (~line 145).
      Verified both: rebuilt clean, re-ran the valgrind repro — those two
      warnings are gone, full suite (9/9) still passes.
- [x] Third warning found by the same pass: `pico/display.c`'s
      `wstripe()` (~line 2044) read `pscr(line,i)->c` from the physical
      screen buffer that `vtinit()` (~line 143) allocated with plain
      `malloc()` and never initialized (only `v_flag` was set to 0). This
      looked like a deliberate screen-diffing trick — uninitialized
      garbage almost certainly won't match the first real character,
      forcing an initial redraw — and the same pattern recurred for
      `vscreen`. Fixed by `memset`ing each freshly `malloc`'d `VIDEO`
      block (both `vscreen[i]` and `pscreen[i]` in `vtinit()`) to all
      zero instead of just clearing `v_flag`: NUL can never match a real
      printable/space screen character, so the "always differs, forces
      first redraw" property is now deterministic rather than relying on
      malloc's leftover bytes happening to differ. Verified: rebuilt
      clean, re-ran the valgrind repro — 0 errors, 0 contexts; full suite
      (9/9) still passes.
- [x] Picked back up via the "Follow-up: buffer-overflow security audit"
      standing item, with a broad grep-and-triage sweep (Explore agent)
      over every `sprintf`/`strcat`/`strcpy`/`rfc822_cat`/
      `rfc822_address_line` call into a fixed-size buffer across
      `pine/*.c`, `pico/*.c`, and the compiled c-client files
      (`mail.c`, `bezerk.c`, `mbox.c`, `dummy.c`, `rfc822.c`, `misc.c`,
      `os_lnx.c`, `sm_unix.c`). First bug fixed from that sweep:
      `pine/mailview.c`'s `describe_mime()` had three distinct
      unbounded-write bugs sharing one function, all reachable simply
      by *viewing* a crafted received message (no compose/forward/save
      needed):
      - `string[200]`: `sprintf(string, "%s%s%.*s%s", type_desc(...),
        ..., sizeof(string)-20, body->description, ...)` budgeted the
        `%.*s` precision as a flat "buffer size minus 20", implicitly
        assuming `type_desc()`'s own output is always short. It isn't:
        `type_desc()`'s `TYPEAPPLICATION` branches can return up to
        ~112 bytes (e.g. `"Application/"` + a 100-byte attacker-chosen
        subtype, uncapped anywhere in `rfc822_parse_content_header()`).
        A part with both a long subtype and a long `Content-Description:`
        (also uncapped upstream, straight `cpystr()`) overflows
        `string[]` by roughly 100 bytes. Confirmed: reliably aborts
        ("Bug in Pine detected: Received abort signal") on a plain
        (non-ASan, non-valgrind) build with a 100-byte subtype + a
        200-byte description — no valgrind/ASan needed to see this one.
        Fixed by computing the precision from `type_desc()`'s actual
        `strlen()` instead of a flat constant, so the total written
        (`type_desc` output + `", \""` + truncated description + `"\""`)
        can never exceed `sizeof(string)-1`.
      - `numx[50]`: `sprintf(numx, "%s%d.", prefix, n)` builds a
        dotted MIME part-number label (`"1.2.3."`) one nesting level at
        a time, growing `prefix` by a few bytes per level with no depth
        limit — the MIME nesting depth is entirely attacker-controlled.
        Did *not* crash on a plain build or under plain valgrind (matches
        this file's standing note that valgrind can miss a local array
        overflowing into an adjacent same-frame stack variable) but
        confirmed as a genuine `stack-buffer-overflow` (exact `sprintf`
        call site) under AddressSanitizer, using a hand-built message
        nested 40 levels deep in trivial (single-part-per-level)
        `multipart/mixed` wrappers. Fixed by only growing `numx`/the
        recursive `prefix` while `strlen(prefix) + 16 < sizeof(numx)`
        still holds; once too deep to safely grow, the last prefix that
        did fit is reused unchanged for all deeper levels (cosmetic
        degradation of the displayed part number, not a crash).
      - `alts[50]` (`MimeShow alts[50]`): indexed by a
        `multipart/alternative` body's part count with *zero* bounds
        check at all (the code's own comment flagged this: "BUG -- no
        bounds checking on this"). Confirmed as a genuine
        `stack-buffer-overflow` under AddressSanitizer using a crafted
        `multipart/alternative` message with 80 parts. Fixed by capping
        the indexing loop at `sizeof(alts)/sizeof(alts[0])` parts;
        alternative parts beyond the cap just aren't considered when
        picking which alternative to display.
      - Same-function bonus fix: `a->number[20]` (`struct attachment`,
        `pine/pine.h:519`) is written via `sprintf(a->number, "%s%d",
        prefix, num)` using the same unbounded `prefix` chain as
        `numx[50]` above, but `a->number` is only 20 bytes — tighter
        than `numx`'s already-fixed 50-byte guard, and lives in the
        `fs_get`'d `ps_global->atmts` array rather than the stack (so an
        overflow here would corrupt a *heap* attachment-list entry, not
        just a local). Fixed the same way: skip the `sprintf` and fall
        back to a bounds-safe `strncpy`-and-NUL-terminate once
        `strlen(prefix) + 16` would no longer fit in
        `sizeof(a->number)`. The one other `sprintf(numx, ...)` call
        site (the `TYPEMESSAGE`/`message/rfc822` recursion branch) got
        the identical `strlen(prefix) + 16 < sizeof(numx)` guard.
      Verified end-to-end: rebuilt clean (`./build lnx`); built a
      temporary ASan-instrumented `bin/pine` (`OPTIMIZE=-fsanitize=address`
      in `pine/makefile.lnx`, reverted afterward, not committed) to
      confirm all three bugs before the fix and their absence after,
      since this bug class isn't reliably caught by valgrind alone; full
      smoketest suite (14/14, then 15/15 with the new test) passes both
      before and after. Added `test/cases/test_describe_mime_overflow.py`
      covering the `string[200]` bug (the one that crashes on a plain
      build, so the standard harness can observe it without an ASan
      build); confirmed it fails before the fix and passes after. The
      `numx`/`alts` bugs aren't separately regression-tested since a
      plain build doesn't reliably crash on them either way — would need
      the harness to grow ASan-build support to usefully test, noted as
      a possible follow-up rather than done here.
      `text` grew 361079-360831 = 248 bytes (bounds-checking code, not a
      cut) — expected and acceptable for closing a real crash bug.
- [x] Picked up the sweep's `export_message()` candidate next (see the
      "Update" note above): `pine/mailcmd.c`'s `export_message()` did
      `sprintf(from, "%s%s%s", env->from->mailbox, ...)` into a fixed
      `char from[MAX_ADDRESS+1]` (101 bytes), with `env->from->mailbox`/
      `host` cpystr()'d from the message's own From: header with no
      length cap anywhere. Investigating this turned up two *more*
      instances of the identical bug class in the same call path,
      found while building a repro (a message with an oversized From:
      address kept crashing before the Export command was even
      reached):
      - `imap/ANSI/c-client/mail.c`'s `mail_fetchfrom()` (used by
        `pine/mailindx.c`'s `build_header_line()` to build the "From"
        column for every line of the FOLDER INDEX) did
        `char tmp[MAILTMPLEN]; sprintf (t = tmp,"%s@%s",adr->mailbox,
        adr->host);` -- reachable simply by opening the FOLDER INDEX,
        no Export/Forward/Save needed. Confirmed under valgrind: "Jump
        to the invalid address 0x4141414141414141" (return-address
        smash) with a 2000-byte From: local-part. Fixed by sizing the
        "mailbox@host" string with `fs_get()`/`fs_give()` instead of
        assuming `MAILTMPLEN` fits one address.
      - `pine/reply.c`'s `pretty_addr_string()` (formats each
        From/To/Cc/Bcc line when viewing a message, and also when
        quoting To/Cc in a reply) did `char buf[MAILTMPLEN];
        rfc822_write_address(buf, addr);` -- `rfc822_write_address()`/
        `rfc822_cat()` do unbounded `strcat()`s trusting the caller's
        buffer is "big enough", the same assumption already found
        wrong for `addrutil.c`'s `addr_string()` earlier in this audit
        (see the `s`/`tmp_20k_buf` follow-up entries above). Reachable
        simply by *viewing* any message with a long address --
        `format_envelope()` calls this for every message displayed, no
        Export needed either. Confirmed under valgrind: "Invalid read
        of size 8 ... Address 0x4141...69 is not stack'd, malloc'd or
        (recently) free'd" with a 2000-byte From: local-part. Fixed by
        sizing the buffer per-address with `fs_get()`/`fs_give()`,
        using the identical `need = 2*(personal+mailbox+host+adl) + 32`
        sizing formula already established by `addr_string()`.
      With both of those fixed, `export_message()`'s own bug could be
      isolated and confirmed: a moderate (300-byte) From: address only
      silently corrupted an adjacent same-frame stack local (confirmed
      under a temporary ASan build, not on a plain build or under
      valgrind -- same blind spot as the `describe_mime()` bugs above),
      but a larger one (5000 bytes) reliably aborts even on a plain
      build. Fixed the same way: `from` is now `fs_get()`'d to fit the
      actual address, and `grow_tmp_20k_buf()` is called before
      formatting it into the "From <address> <date>" mbox separator
      line written into the shared `tmp_20k_buf` (which was previously
      only ever handed short, `MAX_ADDRESS`-bounded content at this call
      site, so was never grown here before).
      Verified end-to-end: rebuilt clean; for `mail_fetchfrom()` and
      `pretty_addr_string()`, confirmed via `git stash` + valgrind that
      each crashes before its fix and is clean after; for
      `export_message()`'s own bug, confirmed via a temporary
      ASan-instrumented build (same technique as the `describe_mime()`
      entry above, reverted afterward) that it overflows before the fix
      and is clean after. Full smoketest suite passes throughout
      (18/18 with the three new tests). Added
      `test/cases/test_folder_index_from_overflow.py`,
      `test/cases/test_message_view_from_overflow.py`, and
      `test/cases/test_export_message_from_overflow.py` (distinct from
      the pre-existing, unrelated `test_export_message.py` ctime/
      time.h crash test) -- each uses a large enough address that the
      corresponding bug crashes even on a plain build, confirmed to
      fail before its respective fix and pass after.
      `text` grew 361079 (after the `describe_mime()` fix) to 361359 --
      280 bytes for three more bounds-checked call sites, again
      accepted as the cost of closing three more real crash bugs.
- [x] Picked up the sweep's last remaining named candidate,
      `pine/mailcmd.c`'s `save_attachment()`: it pre-filled the Save-as
      filename prompt with `strcpy(filename, param->value)` into a
      fixed `char filename[MAXPATH+1]` (4097 bytes), where
      `param->value` is an attachment's MIME `name=` parameter --
      `cpystr()`'d from the message with no length cap anywhere, and
      copied *before* the length-capped `optionally_enter()` prompt/edit
      step ever runs. Confirmed under valgrind with a 6000-byte `name`:
      the corrupted stack later manifests as "Jump to the invalid
      address 0x4141414141414141" (a smashed return address), reachable
      via the ordinary View/Save-attachment flow (`v` from MESSAGE TEXT,
      pick the attachment number, `s` to save) -- and it reliably aborts
      even on a plain (non-ASan, non-valgrind) build at that size.
      Fixing the repro turned up two more overflows in the same
      function once the (still long, if now bounded) `full_filename` gets
      echoed back to the user:
      - `sprintf(prompt_buf, "File \"%s\" already exists...",
        full_filename)` into a fixed `char prompt_buf[200]` when the
        destination file already exists -- `full_filename` can be up
        to `MAXPATH` (4096) bytes even after bounding `filename`
        itself, so this is its own overflow, only ~160 bytes needed to
        trigger rather than 4096+.
      - Every `q_status_message2()`/`q_status_message7()` call in this
        function reporting success or failure embeds `full_filename`,
        formatted into the shared `tmp_20k_buf` -- which starts at only
        a 2048-byte baseline (`TMP_20K_BUF_BASELINE`, `pine.c`) and was
        never grown at any of these three call sites, so a
        `full_filename` anywhere over ~2000 bytes overflows it even
        with `filename` itself now properly bounded to `MAXPATH`.
      Fixed by: truncating the initial `strcpy` to `MAXPATH` via
      `strncpy`+NUL (matching what `optionally_enter()` would enforce
      on user edits anyway -- unlike an address or description, a path
      is inherently bounded by a real length limit, so truncation is
      the semantically correct fix here, not dynamic sizing); capping
      `prompt_buf`'s `%s` precision at `%.150s`, matching this
      codebase's existing `%.100s`-style convention; and calling
      `grow_tmp_20k_buf(strlen(full_filename) + 128)` before each of the
      three status-message calls that embed it.
      Deliberately not expanded further: `os.c`'s `build_path()`
      (`pathbuf, first_part, second_part`, used to build
      `full_filename` from `ps_global->home_dir` + `filename`) has an
      explicit "No check is made for overflow" doc comment and does an
      unbounded `sprintf`/`strcat` of its own -- if `home_dir` were also
      close to `MAXPATH` long, `full_filename` could still overflow.
      Not fixed here: `home_dir` comes from the local system's
      `getpwuid()` entry, not remote/attacker-controlled content, so
      this is a self-inflicted local-config risk (same bucket as the
      `mail_valid_net_parse()`/`context_digest()`/`bezerk_file()`
      lower-priority findings in `TODO-pdp11.md`), not the remotely-
      reachable class of bug this sweep is chasing. `display_attachment()`
      (the sibling function right after `save_attachment()`, also
      declares `char filename[MAXPATH+1]`) was checked and is unrelated
      -- its `filename` comes from `temp_nam()`, not attacker content.
      Verified end-to-end: rebuilt clean; confirmed under valgrind that
      the original repro (6000-byte `name=` parameter) crashes before
      the fix (stack smash landing on a corrupted return address) and
      is clean after (only pre-existing, unrelated
      "uninitialised value" warnings in `ttyin.c`'s `line_paint()`
      remain, also seen in other unrelated tests). Full smoketest suite
      passes throughout (19/19 with the new test). Added
      `test/cases/test_save_attachment_overflow.py`; confirmed it fails
      (crashes) before the fix and passes after -- note the test can't
      assert the file was actually *written*, since a 6000-byte single
      path component exceeds any real filesystem's own `NAME_MAX`
      regardless of this fix, so it accepts either a clean
      "written to"/"MESSAGE TEXT" success or a clean
      "Error opening destination" failure -- either way, no crash.
      `text` grew 361359 (after the `export_message()`-chain fixes) to
      361431 -- 72 bytes for four more bounds-checked call sites in one
      function.
- [x] Picked up the sweep's original namesake candidate,
      `pine/send.c`'s `pine_rfc822_output_body()`/
      `pine_write_body_header()` (the "per-MIME-part header writer"):
      writing a multipart outgoing message builds each part's
      "--boundary" line plus a mini-header (Content-Type/subtype/
      parameters/-Transfer-Encoding/-ID/-Description) into
      `char tmp[MAILTMPLEN]` (1024 bytes), with *no* size accounting at
      all -- unlike the top-level header (`pine_rfc822_header()`, which
      writes into the dynamically-grown `tmp_20k_buf`, sized beforehand
      via `pine_rfc822_header_size()`). `subtype`/parameter values/
      `id`/`description` are all `cpystr()`'d from parsing a message
      with no length cap anywhere. Reachable via receive -> Forward
      (declining "as MIME attachment" -- off by default, so
      `copy_body()` recurses verbatim `cpystr()`s of the original
      part's fields automatically) -> Send of a multipart message with
      a long `Content-Description:` (a 3000-byte one used in testing).
      Confirmed under valgrind: an "Invalid read of size 1" inside
      `pine_write_body_header()` re-measuring `strlen()` of the
      already-corrupted buffer, landing squarely on the injected filler
      bytes; also reliably aborts on a plain (non-ASan, non-valgrind)
      build at this size, same as several of this audit's other finds.
      Verifying this needed an actual Send, not Postpone: Postpone
      strips all but the first text part before writing (`send.c`'s
      "Attachments won't be included in message" handling), so it never
      reaches this code path with the crafted part intact -- a dead
      end discovered the hard way when an initial Postpone-based repro
      attempt produced a suspiciously small, plain-text-only postponed
      message. Send works fine for this without a real `sendmail`
      installed: the vulnerable formatting happens in
      `append_message2()`/`pine_rfc822_output_body()`, which runs
      *before* `call_sendmail()` ever shells out, and that shell-out is
      itself backgrounded/fire-and-forget (`( ( ... ) < tmpfile & )`).
      Fixed by adding `pine_body_header_size()` (mirroring
      `pine_rfc822_header_size()`'s style: sum the field lengths, with
      a 2x factor on parameter values since `rfc822_cat()` can
      backslash-escape every character) and `fs_get()`/`fs_give()`-ing
      `tmp` per part instead of assuming `MAILTMPLEN` always fits one
      part's header; the trailing "--boundary--" line (after the
      per-part loop) got the same treatment, sized off the boundary
      cookie's own (also `cpystr()`'d, technically unbounded) length.
      Bonus fix found while writing `pine_body_header_size()`: a MIME
      parameter can legitimately have `param->value == NULL` (an empty
      `attr=;` value parses to no value at all --
      `mail_newbody_parameter()`'s default, left unset by
      `rfc822_parse_content_header()`'s `if (*s) param->value = ...`
      guard when the value word is empty) -- `pine_write_body_header()`
      passed this straight to `rfc822_cat()`, which has no NULL check
      of its own and would `strpbrk()` a NULL pointer. Guarded both the
      existing call site and the new size helper with
      `param->value ? param->value : ""`. Unlike this entry's other
      finds, not independently confirmed reachable via a hand-crafted
      repro (a few attempts at empty/quoted-empty parameter values all
      parsed to a non-NULL default instead) -- kept as a defensive fix
      since the code path is real and the guard has no behavior change
      for the non-NULL case, but flagged here as unconfirmed rather than
      overstated as a live crash.
      Second bonus fix, found after the fact: the first version of this
      entry's regression test drove Send with a real-looking recipient
      (`user@example.com`), on the assumption that `SENDMAIL`
      (`/usr/lib/sendmail`) wouldn't exist in a throwaway dev sandbox.
      That assumption was wrong -- this box has a live, running Postfix
      with `/usr/lib/sendmail` wired up for real, so every smoketest run
      would have hand it an actual outbound message addressed to a real
      domain. Caught before landing (the user asked what could be done
      about the bounce risk before running the suite repeatedly).
      Redesigned the test to leave To:/Cc: blank instead: pine's default
      `default-fcc` ("sent-mail") means Send still proceeds, but
      entirely through the local Fcc-only path
      (`call_mailer()`'s `else if(fcc_so)` branch calling
      `pine_rfc822_output()` with `f=NULL,s=NULL`) -- `call_sendmail()`
      is never reached, so this is safe regardless of what MTA (if any)
      is installed. Trying this exposed one more real, independent bug:
      `pine_rfc822_output_body()`'s multipart branch unconditionally
      called `(*f)(s, "...MIME format...")` (the "unsuspecting
      non-MIME-aware UA" warning banner) with no NULL check, unlike
      every other callback invocation in the same function (all guarded
      `f ? (*f)(...) : 1`). `f` is NULL precisely on this Fcc-only path,
      so it crashed immediately with "Jump to the invalid address 0x0"
      under valgrind -- a genuine, previously-undiscovered bug (sending
      any multipart message with no recipients and only the default Fcc
      has apparently never worked on this fork), not something
      introduced by this session's changes. Fixed with the same
      `f ? ... : 1`-style guard used elsewhere in the function.
      Verified end-to-end: rebuilt clean; confirmed under valgrind that
      the original repro (3000-byte `Content-Description:`) produces
      the invalid-read before the `pine_write_body_header()` fix and is
      clean after; separately confirmed the NULL-callback crash and fix
      the same way, with a blank-recipient multipart Send. Full
      smoketest suite passes throughout (21/21 with the two new tests).
      Ended up with two separate regression tests, kept both rather than
      picking one, because they exercise genuinely different code paths
      and independently caught different bugs during development:
      - `test/cases/test_send_body_header_overflow.py`: blank To:/Cc:,
        relying on `default-fcc` ("sent-mail") so Send still proceeds
        but entirely through the local Fcc-only path (`f=NULL,s=NULL`)
        -- never touches `sendmail`. This is the one that caught the
        NULL-callback bug (a real Send never hits it, since `f` is
        always non-NULL when there's an actual recipient).
      - `test/cases/test_send_body_header_overflow_via_sendmail.py`:
        a real-looking recipient, driven through `call_sendmail()`'s
        actual shell-out. Made safe via `smoketest.Session`'s new
        `sendmail_stub=True` option: runs pine inside a private,
        unprivileged `bwrap` mount namespace with a stub script
        bind-mounted over the fully-resolved real sendmail binary (the
        top-level `/usr/lib/sendmail` path is usually a symlink chain;
        `bwrap --bind` needs the real regular-file target, resolved via
        `os.path.realpath()`, or it fails outright). This matters on
        real hardware, not just in theory -- this dev box has a live,
        running Postfix genuinely wired up to `/usr/lib/sendmail`, so
        the first version of this test (before the stub existed) really
        would have hand it an outbound message addressed to a real
        domain on every run. Confirmed both before adding the stub
        (`mailq`/`readlink -f /usr/lib/sendmail` untouched afterward)
        and after (the stub's log file captures the full argv + piped
        message text, asserted on to confirm `call_sendmail()` actually
        ran rather than silently no-opping).
      `text` grew 361431 (after the `save_attachment()` fixes) to
      361863 -- 432 bytes, the largest single-item growth in this audit
      so far, for the new size-estimator helper plus its two call sites
      and the NULL-parameter guard.
      - [x] Reviewed the three remaining lower-priority findings from
        this sweep -- `imap/ANSI/c-client/mail.c`'s
        `mail_valid_net_parse()`, `pine/context.c`'s `context_digest()`,
        and `bezerk.c`'s `bezerk_file()`/`dummy.c`'s `dummy_file()`.
        The TODO note for all three assumed they needed "a contrived
        local config/folder name" to reach; that held for
        `context_digest()` (still config-only, see below) but not for
        the other two -- both turned out to be reachable just by typing
        a long string into the composer's **Fcc** field and hitting
        Send, no `.pinerc` editing required, because Send's Fcc-exists
        check (`call_mailer()` in `send.c`) runs the typed value through
        `folder_exists()` -> `mail_open()` -> each driver's `->valid()`
        regardless of whether the folder is later actually created.
        Chasing real repros for these two turned up **five** overflows
        total, not the one apiece the TODO note implied -- each
        confirmed with a standalone `cc -fsanitize=address` repro first
        (per CLAUDE.md's note that this class of same-frame stack
        overflow doesn't reliably trip under valgrind alone), then
        confirmed against the real binary (valgrind-clean before/after,
        and for the two most severe ones, a corrupted-return-address
        crash under `coredumpctl`/`valgrind` before the fix, gone after):
        - **`mail_valid_net_parse()`** (the item TODO actually named):
          parses a `"{host}mailbox"`-shaped name into a `NETMBX` struct
          with three unbounded copies, not just the one named --
          `mb->host` (`strncpy` with a length taken straight from the
          input, no cap), `mb->mailbox` (`strcpy`, the one TODO named),
          and `mb->service` (`strcpy` into `char[MAXSRV+1]`, only 21
          bytes -- from a `"/service=..."` switch value, the smallest
          and easiest of the three to overrun). Fixed by rejecting
          (`return NIL`) if any component would overflow its field,
          which is also just correct validator behavior -- a mailbox
          spec that long was never going to be valid anyway.
        - **`bezerk_isvalid()`** (not `bezerk_file()`, and not named in
          the original TODO note at all): its very first line
          unconditionally `strcpy()`s the *entire* candidate name into
          its caller's `char[MAILTMPLEN]` buffer before ever checking
          what the name starts with -- so a long name overflows here
          regardless of whether `bezerk_file()`'s own `~`-case is fixed.
          This is the one that actually produced the corrupted-
          return-address crash chased down below; found while tracing
          why the crash persisted after fixing `bezerk_file()` alone.
          Fixed by rejecting up front if the name is too long to fit.
        - **`bezerk_file()`/`dummy_file()`** (the TODO-named finding):
          the `case '~':` branch's `strcpy(tmp,name+1)` for a `~user`
          prefix, unbounded up to the next `/`. Fixed by truncating to
          fit (a truncated username just fails `getpwnam()` and falls
          through to the existing "/NOSUCHUSER" case, same as any other
          nonexistent user). While bounding this, noticed the `case
          '/':` and `default:` branches have the exact same
          unbounded-copy shape (a long absolute path, or a long
          "any other name", both `dst`-sized `MAILTMPLEN` by convention
          across every call site in both files) -- bounded those too
          with a single guard at the top of each function, generous
          enough (`MAILTMPLEN/2`) to leave headroom for the `~`
          branch's homedir-prepending `sprintf` afterward.
        - **`context_find()`/`context_find_all()`** (`pine/context.c`,
          not in the original TODO note -- found chasing the
          `bezerk_isvalid()` repro one level further out): after
          `bezerk_valid()` rejects a `~`-prefixed name outright (from
          the fix above) and the fallback `dummy` driver accepts it
          (it validates *any* nonempty name), `mail_open()`'s
          `OP_PROTOTYPE` path returns `NULL` for the `dummy` driver
          specifically (its `open()` callback only does anything for a
          real, non-`NULL` stream) -- so `folder_exists()` falls through
          to `context_find_all(NULL, NULL, fn)` with the raw, still
          unbounded name. Both functions build a `"Find of mailbox
          outside context: %s"` message *and* a follow-up `strcpy`, into
          the same `char[MAILTMPLEN]` `tmp`, whenever the name doesn't
          look "ambiguous" (true for anything starting with `~` or
          `{`). This is what was actually still crashing (with a
          `0x4141414141414141` corrupted return address, per
          `coredumpctl`) after the `bezerk_isvalid()` fix alone. Fixed
          by bounding both the log message (`%.900s`) and the copy
          (`strncpy`).
        - **`folder_exists()`** (`pine/folder.c`, also not in the
          original TODO note): its own `fn[MAXPATH+1]` gets a plain
          `strcpy(fn,file)` for any non-"ambiguous" (i.e. fully-
          qualified-looking) name, one step before any of the above --
          `MAXPATH` is 4096 here so it takes a much longer input to
          hit than the `MAILTMPLEN` (1024) buffers above, but it's the
          same unbounded-copy shape. Fixed by rejecting up front if the
          name doesn't fit.
        - **`context_digest()`** (`pine/context.c`): confirmed this one
          *is* config-only, as the original TODO note guessed --
          reachable only through `context_apply()`'s "ambiguous name"
          branch, which requires an actual configured folder-collection
          string (`.pinerc`'s `Folders=` or the Setup Collection List
          screen), not just a typed folder/Fcc name. Its `rcontext`
          write loop (writing into its own local `tmp[MAILTMPLEN]`
          when the caller doesn't supply an `rcontext` buffer) had the
          same missing bound as the others; fixed with the same
          "reject once too long" (returning a new `"Context too long"`
          error string, consistent with the function's existing
          `"Unbalanced '}'"`-style error returns). Left one related gap
          undone: the same function's `host` copy loop writes into a
          *caller-supplied* buffer with no length passed in to bound
          against -- every current caller happens to size that buffer
          at `MAXPATH` (4096), comfortably larger than the realistic
          `.pinerc` collection strings that reach it, so not fixed here
          (would need a signature change to pass the caller's buffer
          size through, out of scope for this pass).
        Two new regression tests, one per Fcc-field repro (both drive
        Compose -> reveal Fcc -> type the crafted value -> Send,
        matching the "type it into Fcc and hit Send" reachability
        these findings actually have): `test/cases/test_fcc_tilde_overflow.py`
        (the `bezerk_isvalid()`/`bezerk_file()`/`context_find_all()`
        chain) and `test/cases/test_fcc_netmbx_overflow.py` (the
        `mail_valid_net_parse()` service-buffer overflow). Full
        smoketest suite passes throughout (23/23 with the two new
        tests). `text` grew 361863 -> 362143, +280 bytes for the six
        added guards across `mail.c`, `bezerk.c`, `dummy.c`,
        `context.c` (x3), and `folder.c`.
      - Ctrl-T Fcc-folder-picker `s`-buffer concern (from the `s`-sizing
        write-up above): confirmed moot while surveying `pine/folder.c`
        for dead code. `pico/main.c` wires `Pmaster->folders` to
        `compose_folders()` (`pico/compose_stub.c`), not
        `folders_for_fcc()` -- the composer-subprocess redesign (see
        "Run the composer as a separate exec'd process" below) replaced
        the Ctrl-T Fcc picker's c-client `context_apply()` call with a
        flat `FCCLIST`-over-the-handoff-file protocol (`fcc_list`/
        `fcc_count` in `compose_stub.c`, populated by
        `fcc_candidate_list()` in `send.c`). `folders_for_fcc()` itself
        has zero remaining callers anywhere in the tree.
      - False positive, checked and ruled out: `bezerk.c`'s `LOCAL->buf`
        (the other site the original TODO note named) is `fs_get`'d
        dynamically (always >= `CHUNK` = 8192 bytes, never shrunk below
        it), and every `sprintf` into it writes only short `%.30s`-capped
        or small-integer diagnostic text -- safe as is.
- [x] Two more overflow instances turned up chasing later findings in
      this same sweep, each with its own full write-up and regression
      test elsewhere in this file: `pico/display.c`'s `resize_pico()`
      uninitialized-`pscreen`-cell gap (see "`pico/display.c`'s
      `resize_pico()` uninitialized `pscreen`-cell gap" elsewhere in
      this file) and `bfind()`/`readin()`'s `b_fname[NFILEN]` overflow
      in standalone `pico` (see "`bfind()`/`readin()`'s
      `b_fname[NFILEN]` overflow in standalone `pico`" elsewhere in
      this file).
- [x] Sweep closed out: every `sprintf`/`strcat`/`strcpy`/`rfc822_cat`/
      `rfc822_address_line` call into a fixed-size buffer across
      `pine/*.c`, `pico/*.c`, and the compiled c-client files has now
      been triaged. No further open instances of this bug class remain.

### Drop the image-viewer attachment feature (`VAR_IMAGE_VIEWER`)
- [x] Raised while surveying `pine/init.c`/`args.c` for one-shot startup
      code that could either be thrown away or moved into a separate
      bootstrap process (see the still-open composer-split item in
      `TODO-pdp11.md`). `image-viewer`'s only job is telling
      `display_attachment()` (`pine/mailcmd.c`) what external program to
      fork/exec to show a GIF/TIFF/etc. attachment -- a poor fit for a
      terminal-only PDP-11 mail station with no graphical display to hand
      an image to in the first place. Also already effectively dead on
      this very build: `mime_can_display()`'s `TYPEIMAGE` case
      (`pine/osdep/mimedisp`, assembled into `os-lnx.c`/`os.c`) only
      returned displayable if `getenv("DISPLAY")` was set -- true only
      under X11, never on a real target terminal.
      Removed:
      - `run_viewer()` (`pine/mailcmd.c`, the fork/exec of
        `$image-viewer` on a temp file) deleted outright, along with its
        two K&R-style forward declarations.
      - `display_attachment()`'s early "Can't display! Set image-viewer
        in pinerc" check and its `#ifndef DOS if(type==TYPEIMAGE)
        run_viewer(...)` call site removed; the temp-file-write
        scaffolding above it is now unconditional (still used by
        whatever non-text attachment types reach that point) and no
        longer needs the now-single-purpose `aux_filter` array, replaced
        with a plain `(filter_t *)NULL` argument to `detach()`.
      - `mime_can_display()`'s `TYPEIMAGE` case (`pine/osdep/mimedisp`,
        the actual fragment -- `pine/os.c` is a generated symlink, see
        `CLAUDE.md`) simplified to an unconditional `return(0)`, matching
        the already-`return(0)`-only `TYPEAUDIO`/`TYPEAPPLICATION` cases.
        Note `mimedisp` (unlike `os-lnx.c`-specific fragments) is shared
        by most of upstream Pine's other `os-*.ic` port manifests
        (aix/a32/aux/bsd/osf/nxt/ptx/hpp/bsi/pt1/dyn/cvx/sgi/isc/ult/
        gen/sv4) -- edited anyway since this fork only ever builds `lnx`
        and none of those other ports are maintained here, matching the
        precedent already set for `pico/os_unix.c` (also shared POSIX
        fragment, edited directly per `CLAUDE.md`).
      - `image-viewer` pinerc variable marked disabled via the
        established obsolete-slot pattern (same as `editor`/`printer`/
        `character-set`): `V_IMAGE_VIEWER`'s numeric index kept stable
        in `pine/pine.h` (old pinerc files naming it still parse without
        error) but its `VAR_IMAGE_VIEWER` current-value macro and
        `init.c`'s `set_current_val(vars,V_IMAGE_VIEWER,...)` call
        removed, and its `variables[]` help text changed to "Image-viewer
        is disabled, this build has no attachment display support".
      Bonus dead code found, deliberately left alone: `gf_play_sound()`
      (`pine/osdep/play_snd.non`) was `display_attachment()`'s TYPEAUDIO
      branch's only caller, which is now gone too -- but that branch was
      *already* unreachable before this change, since
      `mime_can_display()`'s `TYPEAUDIO` case has unconditionally
      returned 0 for a while (the check happens before the switch on
      body type ever reaches it). `gf_play_sound()`'s body is also
      already an empty no-op ("We don't know how to play audio."). Left
      the fragment file itself untouched rather than deleting it --
      unlike `mimedisp`, `play_snd.non` is the shared "no-op" stub for
      every non-Sun `os-*.ic` port (only `play_snd.sun` differs), and its
      cost is negligible (an empty function body), so it wasn't worth
      the wider blast radius of touching a fragment this broadly shared
      for a few bytes.
      Verified: clean rebuild (`./build lnx`), `nm bin/pine` confirms
      `run_viewer` is gone (`gf_play_sound` remains, as intended, still
      linked but unreferenced), full smoketest suite (13/13) passes.
      `text` dropped from 492010 to 490682 bytes (~1.3KB) -- small,
      since this was mostly UI/glue code around a temp-file-write path
      still needed for other attachment types, not a large standalone
      subsystem.

### Drop compose-side attachment support — DONE
- [x] Removed the ability to attach a local file when composing a
      message (the Ctrl-J picker and its Attchmnt header field), per the
      scoping in this file's former "Drop compose-side attachment
      support" entry. Receive-side MIME rendering (viewing/saving
      attachments other people send in) is completely untouched --
      `pine/mailview.c`'s `describe_mime`/`type_desc`/`part_desc`/
      `display_parameters`/`make_file_index`/`mime_show` and c-client's
      BODYSTRUCTURE parsing were never touched. Forwarding a received
      MIME message as an attachment on an outgoing message (reply.c's
      "Forward message as a MIME attachment" prompt, which builds a
      genuinely multipart outgoing body via `copy_body()`) also still
      works -- that path never went through the PATMT/attach-picker
      machinery being removed here, it only shares `create_message_body`/
      `outgoing2strings`'s generic multipart-body plumbing, which was
      kept (simplified, not deleted).
      What was removed:
      - `pico/attach.c` (871 lines, 100% attachment-specific: `AskAttach`
        Ctrl-J picker, `SyncAttach`, `ParseAttach`, `NewAttach`,
        `AttachError`, `ZotAttach`, `intag`, `sinserts`) deleted
        outright. Its one shared, non-attachment-specific function,
        `prettysz()` (byte-count pretty-printer also used by the plain
        file browser), was moved into `pico/browse.c` next to its one
        remaining caller (`getfcells()`) rather than left orphaned.
      - `pico/pico.h`: the `ATTACHMENTS` macro was not a real toggle
        (several `composer.c` call sites referenced `ATTCHDR` completely
        unguarded by `#ifdef`), so removing it meant deleting the
        `#ifdef`/`#else` scaffolding outright, not just undefining it.
        `ATTCHDR` enum value removed, `SUBJHDR`/`LASTHDR` collapsed to
        the non-ATTACHMENTS numbering (both now 4, one less header row).
        `PATMT` struct, its `A_FLIT`/`A_ERR` flags, and the `PICO`
        struct's `attachments`/`attachment_help` fields deleted --
        confirming the Pmaster-callback audit's prediction that this
        coupling "disappears entirely once the attachment-composition
        cut lands."
      - `pico/composer.c`: every `ATTCHDR`/`PATMT`/attach-picker call
        site removed -- `InitMailHeader()`'s attachment display-entry
        builder (including the buffer-overflow-audit fix from the `s`
        buffer sizing pass, which is now moot since the code it guarded
        is gone), the Ctrl-J handler, `SyncAttach()` calls on header-
        focus-change/postpone/send, the Ctrl-T Fcc-vs-attach-file-browse
        branch, `intag()` guards in char-insert/delete handlers,
        `wkeyhelp()`'s attach-specific key-help strings, and
        `packheader()`'s attachment special-case skip.
      - `pico/pico.c`: `wquit()`'s `AttachError()` pre-send-confirmation
        check removed (always shows the plain "Send message" prompt
        now).
      - `pine/send.c`: `parse_postponed_header()` no longer parses
        `Attachment-file:`/`Attachment-description:` pseudo-headers
        (still parses `Fcc:`, its other job) -- postponed messages that
        predate this build and reference attachments will just silently
        drop those references on resume rather than erroring.
        `outgoing2strings()`/`strings2outgoing()`/`create_message_body()`
        had their PATMT-list rebuild/filter/add logic stripped down to
        just the generic multipart body-encoding step they always did
        alongside it; `append_message2()` no longer writes
        `Attachment-file:`/`-description:` lines when postponing/saving
        an interrupted composition. `free_attachment_list()` and all its
        call sites deleted. `pine_send()`'s `att` parameter dropped
        (call sites in `send.c`, `reply.c` updated).
      - `pico/efunc.h`, `pine/pine.h`: forward declarations for all the
        above updated/removed to match.
      - `pico/makefile.lnx`: `attach.o`/`attach.c` dropped from
        `OFILES`/`CFILES`.
      - `pine/pine.hlp`: removed the (already-empty, per the earlier
        full-screen-help-removal cut) `h_composer_attachment` section
        header; `helptext.c`/`helptext.h` regenerate from this on build
        as usual (gitignored generated files).
      - Two now-obsolete regression tests deleted outright rather than
        adapted, since the vulnerable code they guarded no longer exists
        at all: `test_attach_overflow.py` (ParseAttach/SyncAttach stack
        overflow in the Ctrl-J picker) and `test_attach_resume_overflow.py`
        (parse_postponed_header's unbounded Attachment-description:
        parsing). Two other tests had stale "-> Attchmnt ->" mentions in
        header-navigation-count comments fixed after confirming
        `composer.c`'s `next_line()`/`prev_line()` clamp at the header
        list's ends (so the extra Ctrl-N/Ctrl-P keystrokes were already
        harmless no-ops before this change, just now one keystroke
        fewer is correct): `test_postpone_many_recipients.py`,
        `test_resume_postponed_huge_address.py`.
      Verified: clean rebuild (`./build lnx`) succeeds with no new
      warnings; grepped the whole `pico/`/`pine/` tree for `PATMT`,
      `ATTACHMENTS`, `ATTCHDR`, `AskAttach`/`SyncAttach`/`ParseAttach`/
      `NewAttach`/`ZotAttach`/`AttachError`/`sinserts` and confirmed zero
      remaining references outside the untouched DOS-only `pine.def`
      (a Windows/DOS-build export list, not part of the `lnx` target);
      full smoketest suite green (11/11, after retiring the two
      obsolete tests). `bin/pine` text+data+bss dropped from 531490 to
      517306 bytes (~13.9KB, matching the ~15-16KB estimate); `bin/pico`
      also shrank (136133 -> 127099 bytes, ~8.8KB) since it links the
      same `libpico.a`.

### Run the composer as a separate exec'd process — DONE
- [x] Implemented the fork/exec split scoped in this file's former
      "Consider: run the composer as a separate exec'd process" entry.
      `pine/send.c`'s `pine_send()` no longer calls `pico(&pbuf)`
      in-process; it writes a small handoff file, `fork()`+`execl()`s
      `pico -P <handoff> <result>`, waits, and reads an edited result
      back. `bin/pine`'s resident footprint dropped by **~112KB**
      (text+data+bss 517306 -> 405745, ~21.6%) -- close to the ~118KB
      `libpico.a`-size estimate this item was scoped against, and the
      single biggest cut this project has made.

      **The real blocker turned out to be linking, not design.** The
      approved plan's mechanism (handoff/result files, `pico -P` mode,
      callback stubs in a new `pico/compose_stub.c`) worked on the
      first build, but `bin/pine`'s size didn't move at all -- `nm`
      showed `pico()`, `HeaderEditor()`, `InitMailHeader()` still fully
      linked in. Root cause: `pine/filter.c`'s `PicoText` storage-object
      driver (used for the composed-message body, `so_get(PicoText,
      ...)` in `compose_mail()`/`reply()`/`forward()`) calls
      `pico_get()`/`pico_readc()`/`pico_writec()`/`pico_puts()`/
      `pico_seek()`/`pico_give()`, which lived in `pico/pico.c` --
      the *same translation unit* as `pico()` itself. A static linker
      pulls in a whole `.o` member or none of it, so any PicoText use
      anywhere in pine dragged in the entire composer regardless of
      whether `pine_send()` still called `pico()` directly. (A first
      attempt at fixing this by relocating just those functions to
      their own `pico/picotext.c` didn't work either -- pico's own
      basic-editing files, e.g. `line.c`/`basic.c`, have their own
      `if(Pmaster)`-gated calls into `composer.c`/`pico.c`, so even the
      *LINE-list allocator* pulls the composer back in transitively.
      Fully severing that would mean auditing `if(Pmaster)` call sites
      across some 11 pico source files -- untangling pico's own
      internals, a materially larger task than this one. Abandoned in
      favor of the option below.)

      **Fix: stop using `PicoText` in pine at all.** Switched all 5
      `so_get(PicoText, ...)` call sites (`pine/send.c` x3,
      `pine/reply.c` x2) to `so_get(CharStar, ...)` -- a plain
      in-memory buffer with no pico dependency whatsoever -- and
      deleted `PicoText`'s branches from `pine/filter.c`'s
      `so_get`/`so_give`/`so_readc`/`so_writec`/`so_puts`/`so_seek`,
      the `PicoText` enumerator itself (`pine/pine.h`'s `SourceType`),
      and (since nothing calls them anymore, `pico()`/`main.c`'s
      standalone mode never did) `pico/pico.c`'s
      `pico_get`/`pico_give`/`pico_readc`/`pico_writec`/`pico_puts`/
      `pico_seek`/`insline`/`breplace`/`PICOTEXT`/the unused
      already-dead `pico_replace()` stub, and the `PICO` struct's
      `msgtext` field (replaced by the new `bodyfile` field below).
      One real behavioral wrinkle this surfaced: `PicoText`'s
      `pico_writec()` silently dropped `\r` bytes and translated `\n`
      into a real line break, which `compose_mail()`'s postponed-
      message-resume decode comment called out as relied-upon ("we're
      writing into a PicoText storage object which treats CRLF and CR
      the same"). `CharStar` has no such translation, so decoded CRLF
      text would have landed with literal `\r`s in the buffer. Fixed by
      adding an explicit `gf_link_filter(gf_nvtnl_local)` after the
      decode filters in both `compose_mail()`'s resume path and
      `reply.c`'s `get_body_part_text()` null-body fallback (an
      ancient non-IMAP2bis-server compatibility path, essentially
      unreachable given this target's local-mbox-only setup, fixed
      anyway since it was cheap) -- the same "canonical model" already
      used by `detach()` (`pine/mailcmd.c`, called for MIME-part
      quoting/attachments) for exactly this reason. Confirmed no other
      decode path in `reply.c`/`send.c` needed the same treatment: all
      other `so_puts()`/`gf_puts()` calls write already-local-newline
      strings pine builds itself (headers, delimiters, signatures),
      and `detach()`'s own callers were already safe.

      **Handoff/result protocol**: a length-prefixed `TAG <len>\n
      <bytes>\n` format (`pine/send.c`'s and `pico/compose_stub.c`'s
      matching, independently-implemented `write_field()`/
      `read_field()` -- kept as separate small copies rather than a
      shared header, since pine and pico are different link units).
      Handoff carries `ANCHOR`/`VERSION`/`TO`/`CC`/`BCC`/`SUBJECT`/
      `FCC`/`BODYFILE` (a path, not inline -- the body crosses via its
      own plain-text temp file, read with `readin()` and written back
      with `writeout()`, both already existing pico primitives --
      `PICO.bodyfile` is a new field pico.c's `pico()` checks in place
      of the old `msgtext`/`breplace()` path) /`FLAGS` (the
      `P_SUSPEND`/`P_FKEYS`/`P_SECURE`/`P_BODY` bits) / an `FCCLIST`
      (see below). Result carries a `STATUS <int>` (the same
      `COMP_CANCEL`/`COMP_SUSPEND`/`COMP_GOTHUP`/0 bitmask `pico()`
      always returned, so `pine_send()`'s existing if/else-if chain
      testing it is unchanged) plus the same header fields back.
      `pine_send()`'s do-while loop structure is otherwise untouched;
      `outgoing2strings()`/`strings2outgoing()`/`create_message_body()`
      are byte-for-byte unchanged from before this cut.

      **Composer-side callback stubs** (new `pico/compose_stub.c`,
      wired up in `pico/main.c`'s new `-P <handoff> <result>` mode,
      which reuses the existing `bin/pico` binary rather than building
      a second one): `clearcur`/`keybinit` are no-ops (standalone
      `bin/pico` already does unconditional termios setup in
      `ttopen()`/`vtinit()` regardless of `Pmaster`); `raw_io` is a
      no-op for the same reason (only used by `bktoshell()`'s Ctrl-Z
      path, which already checks `Pmaster` itself); `helper` prints a
      fixed "no help available" message, consistent with the earlier
      full-screen-help-removal cut; `showmsg` is a no-op (no status-
      message queue to replay); `newmail` always reports no new mail
      (mid-compose notification is dropped -- new mail surfaces
      normally on return to the mail-reading process, an accepted
      trade under this project's less-flexible-is-fine principle);
      `buildaddr` is a passthrough copy with no address expansion (the
      other accepted trade -- full canonicalization still happens back
      in pine's `strings2outgoing()` when the child's result comes
      back, so addresses still resolve correctly on send/postpone,
      there's just no live on-screen feedback while typing); `folders`
      (Ctrl-T Fcc-picker) is an in-process numbered-menu prompt
      (`mlreply()`) over a **precomputed candidate list** -- since the
      child has no access to `ps_global->context_list`, `pine/folder.c`
      gained `fcc_candidate_list()`, which lists configured folder-
      collection nicknames (not a live per-collection folder browse
      like `folders_for_fcc()`/`context_apply()` did) -- a real,
      accepted scope reduction for this target's expected single/few-
      collection local-mbox setup; typing a folder name directly into
      the Fcc field still works exactly as before regardless.

      **Two real bugs found and fixed while testing against the
      existing smoketest suite** (both are exactly why this project's
      verification discipline runs the full suite after a change like
      this, not just a manual smoke check):
      - `FLAGS` was written with a bare `fprintf(fp, "FLAGS %u\n",
        flags)`, not through `write_field()` -- so whenever `flags`
        was nonzero (e.g. `P_BODY`, set when resuming a postponed
        message), `compose_stub.c`'s `read_field(fp, "FLAGS")`
        misparsed the value as a *byte count* and tried to read that
        many bytes as the field's content, desyncing the rest of the
        parse and failing the whole handoff read. Silent for a fresh
        compose (flags often 0 in a minimal test config) but broke
        every postponed-message-resume test. Fixed by routing `FLAGS`
        through the same `write_field()`/`read_field()` protocol as
        every other field (formatting it as a decimal string first).
      - The `wait()` loop was modeled on `call_system()`'s existing
        idiom (`while((r = wait(&status)) && r != -1 && r != pid);`),
        which treats *any* `wait()` return of `-1` as "done" --
        correct for `call_system()`'s short-lived, signal-quiet
        sendmail invocation, but wrong here: the composer subprocess
        can run for as long as the user takes to compose, during which
        `SIGWINCH` (terminal resize) routinely interrupts a blocked
        `wait()` with `EINTR`. The old idiom misread that interruption
        as the child exiting, read a result file that didn't exist yet
        ("Composer subprocess produced no result"), and abandoned the
        composition. Caught by `test_composer_wide_terminal.py`
        (resizes to 20000 columns mid-compose) failing specifically on
        the *quit* step afterward, not the resize itself. Fixed by
        retrying on `EINTR` instead of giving up
        (`while((r = wait(&status)) != pid) if(r == -1 && errno !=
        EINTR) break;`) -- `call_system()` itself is left alone, since
        it isn't subject to the same long-blocking-call-under-signals
        exposure and touching pre-existing working code beyond what
        this task needed wasn't warranted.

      **New regression test**: `test_compose_fcc_picker.py` -- drives
      Ctrl-T at the Fcc field, picks candidate 1, and confirms the Fcc
      header field actually gained the picked value (the shape a parse
      error, crash, or missing result field in the new mechanism would
      all otherwise silently take: an empty field, easy to miss without
      an explicit assertion).

      Verified: clean rebuild (`./build lnx`); `nm bin/pine | grep
      -E " T (pico|HeaderEditor|InitMailHeader|ArrangeHeader
      |ToggleHeader)$"` confirms all composer symbols are gone from
      `bin/pine` (still present in `bin/pico`, as expected); full
      smoketest suite green, 12/12 including the new test, stable
      across repeated runs; manual pty-driven checks of compose ->
      type headers/body -> postpone -> verify file content, and
      compose -> send -> reach MAIN MENU cleanly. `bin/pine`
      text+data+bss: 517306 -> 405745 bytes (~112KB, ~21.6%).
      `bin/pico` text+data+bss: 127099 -> 129074 bytes (+~2KB, the new
      `compose_stub.c` glue plus the `-P` mode in `main.c`).

### Fix real crash in (e)xport: missing `<time.h>` truncated `ctime()`'s return — DONE
- [x] User-reported: viewing a message and using `(e)xport` to save it
      to a file reliably crashed with `Bug in Pine detected: "Received
      abort signal"` (a `SIGSEGV` caught by `signals.c`'s
      `auger_in_signal()`). Not a buffer overflow -- a missing-
      prototype bug: `pine/osdep/os-lnx.h` explicitly commented out
      `#include <time.h>` ("plain time.h isn't enough on some
      systems"), relying on `<sys/time.h>` alone for time support. On
      this build that leaves `ctime()` (and `localtime()`) with no
      prototype in scope anywhere in `pine/*.c`, so the compiler
      implicitly declares them as returning `int` -- confirmed via the
      resulting "makes pointer from integer without a cast" warning at
      `mailcmd.c`'s `export_message()`:
      `strcpy(tmp_20k_buf, ctime(&now))`. That silently truncates
      `ctime()`'s real 64-bit `char*` return value to 32 bits;
      `strcpy()` then dereferences the truncated/garbage "pointer" and
      crashes immediately, every time, since `export_message()` calls
      `ctime()` unconditionally to build the exported mbox file's
      "From " separator line's date.
      This was systemic, not specific to `export_message()`:
      unprototyped `ctime()`/`localtime()` calls also exist in
      `signals.c`, `newmail.c`, `init.c`, `log.c`, `imap.c`, `reply.c`,
      `status.c` -- most inside `dprint()` debug diagnostics, so lower
      exposure by default (only crashes if that debug level is
      active), but the same latent bug. Also directly relevant to the
      eventual 2.11BSD/PDP-11 port this fork targets: an old K&R
      compiler with no prototype in scope has exactly this same
      failure mode, so this wasn't just a "modern glibc oddity."
      Fixed by restoring a real `#include <time.h>` in
      `pine/osdep/os-lnx.h` alongside the existing `<sys/time.h>`
      (kept for `struct timeval`).
      Verified: reproduced the crash first via a pty-driven script
      (view message -> `e` -> filename -> Enter -> "Bug in Pine
      detected"); confirmed the compiler warning pointed exactly at
      `export_message()`'s `ctime()` call; applied the fix; rebuilt
      clean with the warning gone; reproduced again -- exports cleanly
      now, confirmed the written file's content and mbox "From "
      separator line. Added `test/cases/test_export_message.py`;
      confirmed it fails with the crash (reverting the fix reproduces
      it) and passes with the fix. Full smoketest suite green, 13/13.

### Fix wrong timezone (+0100 regardless of system config) on sent/postponed mail Date header — DONE
- [x] User-reported: sent mail's `Date` header showed `+0100` no matter
      what `/etc/localtime` actually configured (e.g. `America/Chicago`,
      `-0500`/`-0600`). Every `Send` and `Postpone` stamps `outgoing->date`
      via `rfc822_date()` (`imap/ANSI/c-client/os_lnx.c`, called from
      `pine/send.c:1056`) unconditionally, so this hit every outgoing/
      postponed message, not just some.
      Root cause: `rfc822_date()` computed the UTC offset from the
      `struct timezone` optionally filled in by `gettimeofday()`:
      `zone = (t->tm_isdst ? 60 : 0) - tz.tz_minuteswest;`. That struct
      is obsolete on Linux -- glibc's `gettimeofday()` no longer
      populates it from `/etc/localtime`/`TZ` at all, it just always
      reports `tz_minuteswest=0, tz_dsttime=0`. So whenever
      `t->tm_isdst` was true (DST in effect), `zone` always came out to
      exactly 60 minutes -- `+0100` -- completely independent of the
      real configured timezone. Confirmed with a standalone test
      program comparing this computation against `struct tm`'s
      `tm_gmtoff` field (which glibc's `localtime()` *does* populate
      correctly from the real timezone) on a box configured for
      America/Chicago/CDT: old formula gave `+0100`, `tm_gmtoff` gave
      the correct `-0500`.
      Fixed by switching to `t->tm_gmtoff` directly, dropping the
      `struct timezone` from the `gettimeofday()` call entirely --
      matching the pattern already used by sibling os-dependent files
      in the same c-client tree (`os_bsi.c`, `os_osf.c`), which never
      had this bug. `os_lnx.c` was the outlier still using the
      obsolete approach.
      Verified: reproduced via a pty-driven postpone (Postpone runs the
      identical `rfc822_date()`-stamping code as Send, writing to
      `~/mail/postponed-mail` instead of invoking `sendmail` --
      confirmed `Date:` header was `+0100` before the fix, `-0500`
      after, on a machine configured for `America/Chicago`. Rebuilt
      clean with no new warnings. Added
      `test/cases/test_date_header_timezone.py`, which computes the
      expected offset from Python's own `time.timezone`/`time.altzone`
      (so it stays correct on any machine/timezone the suite runs on,
      not just this one) and confirmed it fails pre-fix (via
      `git stash` isolating just the one-line change) and passes
      post-fix. Full smoketest suite green, 14/14.

### Drop `init.c`'s convenience/self-documenting features — DONE
- [x] Removed all three pieces scoped in the original TODO item, plus
      their now-dead downstream state.
      `write_pinerc()` (`pine/init.c`, ~2.6KB) is gone outright, along
      with the copyright/trademark-banner detection in `read_pinerc()`
      that only fed it (`copyright_line_is_there`/
      `trademark_lines_are_there` statics, the `INITIAL_COMMENT1/2/3`
      macros) and the `EXTRA_BEFORE_*` pinerc-section-divider macros.
      `pine.c`'s `write_pinerc(pine_state)` startup call ("Temporary, to
      get new entries in .pinerc") is gone too. `set_variable()`/
      `set_variable_list()` still exist -- they're called from several
      places, not just the removed first-run/DOS prompts (see below) --
      but now only update the in-memory `struct variable` and always
      return 0; they no longer call `write_pinerc()`. Confirmed none of
      their callers (`pine/send.c`'s DOS-only From-address prompts,
      `pine/pine.c`'s DOS-only inbox-path prompt, and `init.c`'s own
      obsolete-variable-migration calls for `V_LAST_VERS_USED`/
      `V_SAVE_BY_SENDER`/`V_FOLDER_SPEC`) check the return value, so
      this is a pure behavior removal (pine no longer persists these to
      disk) with no dangling logic. Left `read_pinerc()`'s
      `pinerc_lines`/`struct pinerc_line` line-by-line bookkeeping
      alone -- it's now write-only dead weight (nothing reads it since
      `write_pinerc()` was its only consumer), but it's woven throughout
      a core, load-bearing parsing function; ripping it out safely is a
      separate, riskier job than this item scoped, so it's left as a
      known follow-up rather than done under this cut.
      `dump_global_conf()` (`pine/init.c`, ~1KB) and the `-conf`
      command-line flag are gone: removed the function, its
      `pine.h` prototypes, the `do_conf` variable and `"conf"` string
      check in `pine/args.c`'s flag parser, and the `-conf` line from
      `args_help()`'s usage text. `pine -conf` now falls through to the
      per-character flag parser, which rejects the unrecognized `c` in
      "conf" and exits nonzero -- confirmed via the updated
      `test/cases/test_conf_dump.py` (previously asserted the dump
      worked and omitted removed variables like `smtp-server`/
      `address-book`; now asserts `-conf` is rejected and produces no
      config dump at all, since the feature itself is gone).
      The sent-mail pruning/expiration subsystem (`expire_sent_mail`/
      `prune_folders`/`delete_old_mail`/`get_mail_list`/
      `compare_sm_files`/`init_save_defaults`, `struct sm_folder`,
      ~3.8KB) is gone from `pine/init.c`, along with its `pine.h`
      prototypes and its two `pine.c` call sites
      (`if(expire_sent_mail())` and the first-time-user/
      show-new-version `init_save_defaults()` call -- confirmed Save
      and Fcc both already lazily `context_create()` a missing folder
      on demand elsewhere, in `send.c` and `mailcmd.c`, so losing this
      startup pre-creation doesn't break either path, just defers folder
      creation to first use). Also removed the now-dead
      `V_LAST_TIME_PRUNE_QUESTION` migration/bugfix block in
      `init_vars()` (the "month value was 0-11, fixing it in January"
      workaround) and the `last_expire_year`/`last_expire_month` fields
      from `struct pine`, since nothing sets or reads them once
      `expire_sent_mail()` is gone. Marked the `last-time-prune-
      questioned` pinerc variable disabled via the same obsolete-slot
      pattern established for the `printer` cut (numeric `V_*` index
      kept stable, `is_obsolete` set, description replaced) -- no
      divider-anchor to move this time, since the whole
      `EXTRA_BEFORE_PRINTER`-anchor mechanism went away with
      `write_pinerc()` in the same session. Removed the now-orphaned
      `h_wt_expire`/`h_wt_delete_old` help topics from `pine/pine.hlp`
      and regenerated `helptext.c`/`helptext.h` via `cmplhelp.sh`/
      `cmplhlp2.sh` (gitignored build products, same as the printer cut).
- [x] Verified: clean rebuild (`./build lnx`) and a full `./build clean
      && ./build lnx` both succeed with no new warnings (the same six
      pre-existing warnings in `folder.c`/`screen.c`/`send.c`/
      `signals.c` persist, just at shifted line numbers). `nm bin/pine`
      shows no `write_pinerc`/`dump_global_conf`/`expire_sent_mail`/
      `prune_folders`/`delete_old_mail`/`get_mail_list`/
      `compare_sm_files`/`init_save_defaults` symbols. Updated
      `test/cases/test_conf_dump.py` to check `-conf` is rejected
      instead of asserting its old dump-and-succeed behavior. Full
      smoketest suite passes, 14/14. text+data+bss dropped from 405489
      to 394073 bytes (365321 + 13528 + 15224) -- an ~11.4KB reduction,
      a bit more than the original ~7.4KB estimate (the estimate didn't
      account for the copyright/divider-macro removal riding along with
      `write_pinerc()`, or the `helptext.c`/`.h` shrink from dropping the
      two orphaned help topics).

### c-client: dead TCP/`server_login` code in `osdep.o` — DONE
- [x] Removed the entire unreachable TCP client stack identified in the
      original audit from `imap/ANSI/c-client/os_lnx.c`: `tcp_open`,
      `tcp_aopen`, `tcp_getline`, `tcp_getbuffer`, `tcp_getdata`,
      `tcp_sout`, `tcp_soutr`, `tcp_close`, `tcp_abort`, `server_login`,
      plus `tcp_host`/`tcp_localhost` (confirmed to have zero callers
      anywhere, not even from the doomed TCP functions themselves) and
      the `TCPSTREAM`/`struct tcp_stream` type definition and `BUFLEN`
      macro that only existed to back them. Re-checked every symbol
      this touches against the whole repo (not just `smtp.c`/`phile.c`,
      which already aren't compiled) before deleting anything, since
      the original audit note's claim that `hdname`/`uname` were
      "entirely unreferenced" turned out to be imprecise -- both are
      file-scope statics that back `myhomedir()`/`myusername()`, which
      *are* still called (`mbox.c`, `bezerk.c`, `sm_unix.c`, `dummy.c`),
      so those two and `lockname()` (called from `bezerk.c`) were kept
      untouched. Also trimmed the includes/externs that existed only to
      support the removed code: `<sys/socket.h>`, `<netinet/in.h>`,
      `<netdb.h>`, `<ctype.h>`, `<errno.h>`/`extern int errno`,
      `<grp.h>`, `<arpa/inet.h>`, and the `extern char *crypt(...)`
      declaration. Updated the real tracked header source,
      `imap/ANSI/c-client/os_lnx.h` (`osdep.h` is a build-time hard link
      of it, per `makefile.lnx`'s `ln os_lnx.h osdep.h` rule, and is
      gitignored) to drop the matching prototypes; left the
      `#ifndef TCPSTREAM / #define TCPSTREAM void` dummy-type fallback
      in place since `rfc822.h` and `pine/send.c` both rely on that same
      fallback pattern for their `soutr_t` function-pointer signature,
      entirely decoupled from real TCP now.
- [x] Verified: `git stash`-isolated before/after `size bin/pine`
      comparison (clean `./build clean && ./build lnx` on each side) --
      text+data+bss dropped from 394073 to 389503 bytes (360831 + 13448
      + 15224), a 4570-byte reduction, somewhat more than the original
      ~2.9KB estimate (the estimate covered just the function bodies,
      not the header/include trimming that rode along). `nm
      imap/ANSI/c-client/osdep.o` confirms zero `tcp_*`/`server_login`
      symbols remain. Total warning count is unchanged at 180 (this
      code wasn't a warning source either way). Full smoketest suite
      stays green, 14/14 -- no regression test added, since this is
      pure removal of code with no caller anywhere in the tree (nothing
      observable changes for the harness to pin down).

### `pico/display.c`'s `resize_pico()` uninitialized `pscreen`-cell gap — DONE

`resize_pico()` (~line 1694) mirrors `vtinit()`'s per-row `malloc()`/
`realloc()` pattern for both `vscreen` and `pscreen` on a terminal
resize, but only `vscreen` got a "don't let any garbage in" fill
(`vteeol()`, gated on `old_ncol < term.t_ncol`) for newly-added rows
and the newly-widened tail of existing rows. `pscreen`'s equivalent
rows -- freshly `malloc()`'d for brand-new rows, or `realloc()`'d
(preserving only the old, narrower content) for existing ones -- were
left with whatever the allocator happened to return, with no
initialization at all.

As `TODO-pdp11.md` already noted, this was long-dead code on this
Linux build: a missing `#include <sys/ioctl.h>` (fixed during the `s`
buffer-sizing pass, see above) had silently compiled out every
`#ifdef TIOCGWINSZ` block, including both call sites of `resize_pico()`
itself, so no real terminal resize ever reached it. With that fixed,
a real resize (or the `test_composer_wide_terminal.py` regression test
above) does reach it, making this gap genuinely live.

**Reachability**: any real terminal resize while pico/pine's composer
is on screen. `winch_handler()` (`pico/os_unix.c`) calls
`resize_pico()` directly off `SIGWINCH`.

**Fix**: added a fill loop mirroring `vscreen`'s `vteeol()` approach,
right after each row's `pscreen[i] = vp;` assignment -- writes blank
`CELL{' ', 0}` values across the same `(old_ncol < term.t_ncol)`-gated
range `vscreen` already fills (`old_ncol` to `term.t_ncol` for
existing rows, `0` to `term.t_ncol` for brand-new ones). Deliberately
*not* a blanket `memset` like `vtinit()`'s fresh-init case uses,
because `resize_pico()` sets `v_flag = VFCHG` (not 0) just before this
-- a `memset` over the whole `VIDEO` struct would wipe that flag back
to 0 and this row would never get redrawn.

**Verification**: this class of bug (stale/uninitialized data read
into a redraw diff, not a bounds violation) doesn't crash in a plain
pty session, so it needed valgrind's uninitialized-value tracking
rather than a plain before/after run. Driving it through `bin/pine`
doesn't work for this, though -- since the composer redesign (see
above), composing runs as a separate exec'd `pico` subprocess, and
valgrind doesn't follow forked/exec'd children without
`--trace-children=yes`. Ran the repro directly against standalone
`bin/pico <file>` instead: `valgrind --track-origins=yes`, shrink the
pty (15x40) then grow well past the original 24x80 (45x160). Before
the fix: two "Conditional jump or move depends on uninitialised
value(s)" errors in `updateline()` (`display.c:783`/`:805`), both
`Uninitialised value was created by a heap allocation ... by
resize_pico (display.c:1762)` -- i.e. exactly the `pscreen`
malloc/realloc site. After the fix: both gone. (The same log also
showed a separate, unrelated, pre-existing overflow -- see the new
TODO item below -- present identically before and after this fix,
confirming it isn't something this change touches.)

Regression test added: `test/cases/test_composer_resize_grow.py`,
mirroring `test_composer_wide_terminal.py`'s resize-while-composing
pattern (shrink then grow past the original size). Since the harness
has no valgrind integration, this test can only check the coarser
regression (still alive, still responsive after the resize) -- the
actual uninitialized-value elimination was confirmed manually as
described above, not by this automated test.

Full smoketest suite passes throughout (24/24 with the new test).
`bin/pico`'s `text` grew 112078 -> 112160, +82 bytes for the fill
loop; `bin/pine`'s size is unchanged (it no longer statically
benefits or suffers from `display.c` size changes post-composer-
subprocess-split in the same way).

### `bfind()`/`readin()`'s `b_fname[NFILEN]` overflow in standalone `pico` — DONE

Found as a side effect of the `resize_pico()` valgrind repro above --
present in the log both before and after that fix, confirming it was
an unrelated, pre-existing bug. `pico/main.c`'s startup path (the
"open a file named on argv" case, *not* the `-P <handoff> <result>`
composer-subprocess mode pine itself uses) did:

```c
strcpy(bp->b_fname, argv[carg]);
```

directly from the command-line argument with no length check.
`b_fname` is `char[NFILEN]` (`pico/estruct.h`, `NFILEN` = 80) -- any
file path longer than 79 characters passed to standalone `bin/pico
<path>` overflowed it. (Confirmed incidentally: the test scratch
directory's own path in this environment is over 100 characters,
which is what surfaced this in the `resize_pico()` repro above by
accident -- not a contrived example.) `readin()` (`pico/file.c`,
called right after with the same `argv[carg]` string) had the
identical unbounded `strcpy(bp->b_fname, fname)` as its very first
operation. Every *other* `b_fname` write in `pico/file.c` goes through
`mlreply()`/`mlreplyd()`'s own caller-supplied max-length bound and
was already safe; these were the two places that bypassed that
convention, because they read directly from `argv`/an unbounded
parameter rather than an interactive prompt.

Not reachable from `pine`'s mail-composing flow at all -- pine invokes
the composer via `pico -P <handoff> <result>`, a separate dispatch
path (`pico/main.c`) that never touches `argv[carg]`/this code. Only
affects direct standalone use of `bin/pico <file>` with a long path.

**Why this one didn't need the ASan-standalone-extraction workaround**:
unlike `resize_pico()`'s stack-adjacent `CELL` gap, `b_fname` lives
inside a `BUFFER` struct that's a plain `malloc(sizeof(BUFFER))`
(`bfind()`, `pico/buffer.c`) with `b_fname` *not* the last field (16
more bytes of `b_bname` follow it, then the malloc'd block ends) --
so a long enough overflow runs straight off the end of the heap
allocation, which valgrind's memcheck (and glibc malloc itself) can
both detect directly. Confirmed via `valgrind` against standalone
`bin/pico -z <150-A's>` before the fix:

```
==...== Invalid write of size 1
==...==    at 0x484A924: strcpy (vg_replace_strmem.c:566)
==...==    by 0x400CD0: main (in bin/pico)
==...==  Address 0x4ac05e8 is 0 bytes after a block of size 152 alloc'd
==...==    at 0x4840AE6: malloc (vg_replace_malloc.c:447)
==...==    by 0x414B04: bfind (buffer.c:149)
==...==    by 0x40DD74: edinit (pico.c:278)
==...==    by 0x400C0E: main (in bin/pico)
```

(`152` matches `sizeof(BUFFER)` on this platform; a second, identical
pair of errors came from `readin()`'s own copy at `file.c:245`.) It
also reproduces as a bare, uninstrumented `SIGSEGV` -- no valgrind or
ASan needed to see it crash, unlike the `resize_pico()` case -- since
the overflow corrupts adjacent malloc chunk metadata that glibc's
allocator later trips over.

Fixed by bounding both copies to `NFILEN-1` with explicit NUL
termination:

```c
strncpy(bp->b_fname, argv[carg], NFILEN-1);
bp->b_fname[NFILEN-1] = '\0';
```

in `pico/main.c`, and the same pattern for `readin()`'s
`strcpy(bp->b_fname, fname)` in `pico/file.c`. Verified clean after
(`ERROR SUMMARY: 0 errors` under valgrind; no crash across argv
lengths up to 5000 characters in a plain, uninstrumented run). New
regression test `test/cases/test_pico_standalone_longpath.py`, which
spawns `bin/pico` directly (not via `smoketest.Session`, which always
drives `bin/pine`) with a 500-character argv path and asserts a clean
exit rather than `SIGSEGV` -- the coarse, harness-observable half of
the fix; the heap-overflow detail itself was verified manually via
valgrind as described above. Full smoketest suite: 25/25 passed.
`bin/pico`'s `text` grew 112160 -> 112260 (+100 bytes); `bin/pine`
unaffected (this code isn't linked into it).


### Don't block Compose on the "Folder opened" status message — DONE

Found while investigating why the smoketest suite felt slow (same
investigation that led to the `~/mail` pre-creation fix in
`test/smoketest.py` -- see that commit's message; this one is a real
source change so it gets a write-up here instead of just a commit
message). After fixing the `init_mail_dir()` sleep(4), `strace` still
showed a 2-second `clock_nanosleep` on every test that composes
shortly after startup. Traced with `bin/pine -d 9` (debug level 9)
against a scratch `$HOME`: the debug log showed

```
q_status_message, Count 1, "Opening "INBOX"..."
q_status_message, Count 1, "Folder "INBOX" opened with 0 messages"
```

right after startup. The second one is queued from
`pine/mailcmd.c:2038`:

```c
q_status_message7(0, 2, 4, "%s \"%s\" opened with %s message%s%s", ...);
```

with `min_display_time=2`. Pine's normal main loop honors
`min_display_time` non-blockingly (comparing wall-clock timestamps
across `GetKey()` cycles, no `sleep()` involved -- see the
`displayed_time`/`now`/`diff` bookkeeping in `status.c`). But
`flush_status_messages()` (`pine/status.c`), whose *only* callers are
the two sites in `pine_send()`'s composer loop
(`pine/send.c:972`,`:990`), forced a real `sleep(currently_displayed.
min_display_time)` for each queued message -- because curses is about
to be torn down for the composer subprocess handoff and there's no
more event loop to let that time pass naturally. Since a fresh
`Session` always opens INBOX at startup (queuing this message) and
nearly every test's first action is `send("c")` to compose, this hit
on every test that didn't happen to interact with something else
first.

This is real interactive UW Pine behavior (not a regression from this
fork's own changes), so fixing it was a product call rather than a
mechanical one: asked the user whether to leave it, flag it as a
future "deliberate simplification" candidate, or cut it now. Decision:
cut it -- composing a new message doesn't need to block on how many
messages are sitting in the folder just opened; that can be checked
later. Fixed by dropping the `sleep()` call from
`flush_status_messages()` (`pine/status.c`) entirely -- it still
dequeues and prints each pending message once (so nothing is silently
dropped), it just no longer blocks composing while waiting out each
message's minimum display time. Since `flush_status_messages()` has no
other caller, this is scoped exactly to the compose-entry path; it
doesn't touch the normal (non-blocking) per-loop status-message
display used everywhere else in the program.

Verified via `strace -f -tt -T -e trace=clock_nanosleep` against
`test_composer_wide_terminal`: the pine-side 2-second
`clock_nanosleep` is gone (the only remaining one is the test's own
`time.sleep(2)`, on `CLOCK_MONOTONIC` from Python itself, unrelated).
Full smoketest suite: 25/25 passed. `bin/pine`'s `text` shrank
362143 -> 362079 (-64 bytes) from the removed `sleep()` call site; no
new regression test added since the observable difference is timing
only (no distinct pass/fail condition beyond "compose still works",
already covered by the existing composer tests).

### `pine/send.c` dead/vestigial code sweep -- DONE (first file of the four-file survey)
- [x] First installment of the "`pine/` UI files: dead/vestigial code"
      survey item -- `pine/send.c` only; `mailcmd.c`/`folder.c`/
      `mailview.c` remain open in `TODO-pdp11.md`. Removed, all
      confirmed to have zero callers/reachability before deleting:
      - `file_type()` (~1652-1690) and the `FileTypes`/`IsText`/
        `IsText8`/`IsBinary` typedef (`pine.h:758`), plus its two
        K&R/ANSI forward declarations -- zero callers anywhere in the
        repo, a leftover from the since-removed compose-side
        attachment-typing code.
      - All three `#ifdef ENCODE_FROMS` blocks in `set_mime_types()`
        (the `froms`/`bmap` locals, the `Find_Froms()` call, and the
        trailing "if there were From_'s" encoding bump) -- confirmed
        `ENCODE_FROMS` is commented out in every `pine/osdep/os-*.h`,
        dead in any configured build.
      - Every `#ifdef DOS`/`#ifndef DOS` site in the file: the DOS
        "rb" vs "r" postponed-mail open mode; `dos_valid_from()`
        (~406-562, the interactive user-id/personal-name/domain
        prompt used only to build a DOS "From:" line) and its lone
        call site in `pine_send()`, plus the now-unused
        `extern char mm_login_user[]` declaration it needed (the
        variable itself is still defined and used elsewhere, in
        `pine/imap.c`); the `FCC_SOURCE` `FileStar`-vs-`CharStar`
        macro split (always `CharStar` now); the `dawz_string`/`d.fd`
        "hack! stolen from dawz.c" FCC-storage-object branch in
        `pine_send()` (always the `mail_string`/`so_text()` branch
        now); the DOS binary-detection branches in `set_mime_types()`
        (`lastchar`/`BREAKOUT` lone-CR/LF-detection logic); the
        DOS-only X-Sender/X-Warning header block in
        `pine_rfc822_header()` (required a `NETMBX netmbox` local only
        used there); `unix_puts()`'s DOS raw-`putc` branch (always
        does CRLF-to-LF translation now); `append_message2()`'s
        `OTYPE`/`OMODE` DOS-vs-Unix open-mode macro split (always
        `"w"`/`0600` now) and its `truncate()`-on-error-cleanup call
        (was `#ifndef DOS`, now unconditional); and the `#ifndef DOS`
        wrapper around `call_sendmail()`/`call_system()`'s forward
        declarations (the functions themselves were never
        DOS-conditional, only their prototypes were). Also removed a
        stale comment ("these functions provide for attachment
        handling in DOS...") that no longer describes the functions
        below it (`addr_list_size()`/`pine_rfc822_header_size()`,
        unrelated general-purpose helpers).
      - Left alone, deliberately: the magic-number MIME-type sniffing
        block in `set_mime_types()` (GIF/JPEG/TIFF/PostScript/`.snd`/
        AppleFile/ZIP detection) -- this is *not* DOS-only or dead, it
        runs on every build; it's listed separately under "Deliberate-
        simplification candidates" in `TODO-pdp11.md` since cutting it
        is a product decision, not a dead-code removal.
      Verified: `./build lnx` clean (no new warnings; the file's two
      pre-existing `union wait *`/`wait()` warnings, unrelated to this
      sweep, are the compiler-warnings TODO item's concern, not
      this one's); full smoketest suite 25/25, including the
      sendmail-stub-driven `test_send_body_header_overflow_via_sendmail`
      (exercises `call_sendmail()`, unaffected by removing its dead
      `#ifndef DOS` prototype wrapper) and the postponed-mail tests
      (exercise the "rb"/"r" open-mode site). No new regression test
      added -- this is dead-code removal with no behavior change to
      verify, not a bug fix. `bin/pine`'s `text` shrank
      362079 -> 361631, -448 bytes.

### `pine/mailcmd.c` dead/vestigial code sweep -- DONE (second file of the four-file survey)
- [x] Second installment of the "`pine/` UI files: dead/vestigial code"
      survey item -- `pine/mailcmd.c` only; `folder.c`/`mailview.c`
      remain open in `TODO-pdp11.md`. Removed, all confirmed to have
      zero callers/reachability or to be permanently dead given this
      build's macro configuration before deleting:
      - The Zoom (`'z'`/`OPF7`-already-in-index), Unexclude (`'&'`/
        `OOPF4`), Apply (`'a'`/`OOPF5`), Bounce (`'b'`/`OOPF10`), Flag
        (`'*'`/`OOPF11`), and Pipe (`'|'`/`OOPF12`, itself inside a dead
        `#ifndef DOS`) command-dispatch cases in `process_cmd()` --
        stock Pine 3.89 never implemented any of these; each case body
        only ever printed a "not implemented yet"/"not available"
        status message, sometimes gated behind an `F_ENABLE_*` pinerc
        flag that's still used elsewhere (`mailindx.c`/`mailview.c`'s
        own copies of the same dead commands, and `init.c`'s pinerc
        parsing) so those flag *definitions* were left alone -- only
        the now-deleted case bodies' `F_ON`/`F_OFF` checks went away.
        The `OPF7`-while-already-in-index path used to `goto do_zoom`
        (the Zoom case's label) to print the same stub message; with
        Zoom's case deleted this now `goto bogus`s instead, so it (and
        every deleted command's key) falls into the generic "Command
        ... not defined for this screen" message instead of a
        command-specific "not implemented yet" one -- a cosmetic
        behavior change accepted as part of fully cutting these dead
        stubs, consistent with the TODO's own instruction to delete
        the cases outright.
      - The `#ifdef X_NEVER` seen-flag-setting block in the Save
        Message case, and the `#ifdef X_FINISH` expunge-failure block
        in the Expunge case (whose body didn't even compile as valid
        C -- `q_status_message(1, 3, 5 "\007Expunge failed!")` is
        missing a comma between the `5` and the string literal,
        further evidence this branch has been dead and untested for a
        long time) -- both macros are never defined anywhere in this
        tree.
      - The `#ifdef OLDWAY`/`#else` variable declaration in
        `do_broach_folder()` (`MsgNo first_unseen;` vs `long i;`) --
        `OLDWAY` is never defined, and the surviving `long i;` itself
        turned out to be completely unused in the function (verified
        by grepping the whole function body), so the entire
        declaration -- both branches -- was dead weight.
      - The `#ifdef X_MAXMESSAGES` "80% of capacity" warning block in
        `do_broach_folder()` and the `#ifdef LATER` debug `dprint()` in
        `detach()` -- both macros never defined.
      - Every `#ifdef DOS`/`#ifndef DOS` site in the file: ~10 lone
        `flush_index_cache()`/"save room on PC" calls scattered through
        `process_cmd()`'s screen-switch cases; the `coreleft()`
        IMAP-memory-check guard on `OPF6`'s folder-open path (plus its
        now-dead `extern long coreleft()` declaration); `save()`'s
        `dawz_string`/`d.fd` "hack! stolen from dawz.c" FCC
        storage-object branch (always the `mail_string`/`so_text()`
        branch now) and its paired `mailgets`/`append_file` DOS setup
        and reset lines; `export_message()`'s DOS-specific prompt
        string and its `fixpath()`-vs-`fnexpand()` tilde-expansion
        branch (kept the Unix branch, which also runs the file-name
        validity checks the DOS branch skipped entirely); `save_msg()`'s
        equivalent tilde/`fixpath()` split in `save_attachment()`; the
        `do_broach_folder()` `OP_SHORTCACHE`-vs-`0L` `openmode` split;
        the doubling-vs-fixed-increment `sort_allocated` growth-rate
        split (kept the doubling growth); `detach()`'s DOS/`nntp`
        `MAX_MSG_INCORE`-check block (also contained one of the two
        `nntp` driver-name string comparisons the TODO flagged --
        confirmed double-dead, since no `nntp.c` exists in this tree
        *and* the whole block was `#ifdef DOS`-only) and its
        `mailgets`-reset/`append_file` ternary in `gf_set_readc()`/
        free-on-exit block; `display_attachment()`'s `temp_nam()`
        DOS-suffix-vs-prefix variant and its post-write `unlink()`;
        and `display_text_att()`'s DOS `src` source-type selection
        (the second `nntp` comparison the TODO flagged, same
        double-dead reasoning) and its `append_file`/`mailgets`
        full-header fetch path (kept the plain `so_puts()` branch).
      - A stale header comment ("capture address off incoming mail")
        that no longer describes any function in the file -- no
        `capture_addr`-style function exists here or anywhere else in
        the tree.
      - Left alone, deliberately, as out of this sweep's scope: the
        `#ifdef DEBUG` `OP_DEBUG` block in `do_broach_folder()` --
        `DEBUG` *is* defined in this build (`pine/makefile.lnx`'s
        `-DDEBUG`), so this is live code, not dead; `ps_global->restricted`
        demo-mode lockdown checks in `export_message()`/
        `save_attachment()` -- tracked separately under "Deliberate-
        simplification candidates" in `TODO-pdp11.md`; and the
        `F_ENABLE_*` flag *definitions* themselves (`pine.h`/`init.c`),
        since `mailindx.c`/`mailview.c` still reference them for their
        own copies of these dead commands (out of scope for this file).
      Verified: `./build clean && ./build lnx` clean both before and
      after (baseline confirmed via `git stash`) -- zero warnings on
      `mailcmd.c` in either build, so this removal didn't fix or
      introduce any compiler diagnostics; full smoketest suite 25/25,
      including `test_export_message`/`test_export_message_from_overflow`
      and `test_save_attachment_overflow` (exercise the tilde/`fixpath()`
      split and the DOS temp-file-naming removal) and
      `test_compose_fcc_picker` (exercises `save()`, unaffected by
      removing its dead `dawz_string` branch). No new regression test
      added -- this is dead-code removal with no behavior change to
      verify (aside from the accepted cosmetic status-message change
      on the six deleted stub commands, noted above), not a bug fix.
      `bin/pine`'s `text` shrank 361631 -> 360663, -968 bytes.

### `pine/folder.c` dead/vestigial code sweep -- DONE (third file of the four-file survey)
- [x] Third installment of the "`pine/` UI files: dead/vestigial code"
      survey item -- `pine/folder.c` only; `mailview.c` remains open in
      `TODO-pdp11.md`. Removed, all confirmed to have zero callers/
      reachability or to be permanently dead given this build's macro
      configuration before deleting:
      - `collection_lister`/`display_collections`/
        `create_collection_display`/`redraw_collection_screen` -- each
        forward-declared twice (once in the `#ifdef ANSI` prototype
        block, once in the `#else` K&R block) but never defined or
        called anywhere in the tree; an apparent stub for a
        multi-collection browse dialog that was never built (confirmed
        via grep across `pine/*.c`/`pico/*.c`). `redraw_folder_screen`,
        forward-declared right alongside them, is real and unrelated
        (defined and used later in the file) -- left untouched.
      - `compare_sizes()` -- zero callers (confirmed no `qsort()` call
        anywhere in the file; the real folder-list layout uses
        `create_folder_display()`'s manual width heuristic instead),
        plus its two now-dead forward declarations. Its misleading
        position directly under a header comment describing
        `create_folder_display()` (the *next* function down) was left
        alone -- that comment belongs to `create_folder_display()`, not
        to the deleted function.
      - `folders_for_fcc()` and the `GetFcc` `FolderFun` enum value it
        was the sole caller of -- confirmed dead once the composer
        became a separate process (superseded by
        `fcc_candidate_list()`/`compose_folders()`, see the "Run the
        composer as a separate exec'd process" write-up above). Removed
        the function itself, its two prototypes (`pine.h`'s ANSI/K&R
        blocks), the `GetFcc` value from `typedef enum {OpenFolder,
        SaveMessage, FolderMaint, GetFcc} FolderFun` (`pine.h`), and the
        two explicit `case GetFcc:` labels inside `folder_lister()`'s
        switches (the titlebar-text switch and the help-topic switch).
        The `GetFcc`-reachable "save message" branch inside
        `folder_lister()`'s big command-dispatch if/else-if/else chain
        (an `else` fallback shared with `SaveMessage`, not a distinct
        `case GetFcc:`) was left completely alone -- it doesn't
        special-case `GetFcc` at all, so it continues working
        unchanged for `SaveMessage` now that `GetFcc` can never occur.
        Noticed but left alone as out of scope: `pine/helptext.c`'s
        `h_folder_fcc` help-topic array (generated from `pine.hlp`) is
        now unreachable too, since its only caller was the deleted
        `case GetFcc:` help switch -- cutting it means editing the
        tracked `pine.hlp` source and regenerating, a separate
        generated-file workflow outside this sweep's scope.
      - Every `#ifdef DOS`/`#ifdef DOSXXX`/`#ifdef OLDWAY` site in the
        file (`DOS` is never defined anywhere in this tree, confirmed
        same as the `send.c`/`mailcmd.c` sweeps; `DOSXXX` and `OLDWAY`
        likewise never defined -- grepped the whole tree to confirm):
        `compare_names()`'s `STRCMP`/`CMPI` DOS-vs-Unix macro split
        (always `strcmp`/`((X)[0]-(Y)[0])` now); `pretty_fn()`'s
        DOS-backslash-vs-Unix-slash/tilde path-separator check (kept
        the Unix branch, which also handles `~` that the DOS branch
        didn't); two separate `OLDWAY`-gated `context_find()`-vs-
        `context_find_all()` choices (both always used
        `context_find_all()` already -- `OLDWAY` was never defined --
        so this was pure dead-branch removal, no `strlen`/behavior
        change); `folder_complete()`'s `STRUCMP` DOS-vs-Unix macro
        split (always `strncmp` now); the `FLIST` struct's
        `DOSXXX`-gated `FILE *folders` (disk-tmpfile) member, which was
        never compiled in favor of the real `FOLDER_S **folders` array
        member; `new_folder()`/`folder_entry()`/`free_folder_list()`/
        `folder_insert_index()`/`folder_delete()`'s paired `#ifdef
        DOSXXX` bodies (`new_folder()`'s was completely empty on the
        DOS side -- an unfinished stub, further evidence this path was
        long abandoned) -- each collapsed to its surviving `#else`
        array-based implementation; `folder_index()`/`folder_is_nick()`'s
        DOS case-insensitive (`strucmp`/`toupper`) vs. Unix
        case-sensitive (`strcmp`) comparison branches (kept the Unix,
        case-sensitive branch in both, since that's what every non-DOS
        build -- including this one -- has always actually run).
      Verified: `./build clean && ./build lnx` clean both before and
      after (baseline confirmed via `git stash`) -- `folder.c` itself
      compiles with zero new warnings; its one pre-existing
      `-Wpointer-compare` warning (`init_folders()` comparing
      `VAR_FOLDER_SPEC[0]` to `'\0'`) is unrelated to this sweep and
      simply shifted from line 2067 to 1998 as a result of the
      deletions above it -- the standing compiler-warnings TODO item's
      line reference was updated to match, not fixed here. Full
      smoketest suite 25/25, including `test_compose_fcc_picker` and
      `test_folder_index_view` (exercise `folder_lister()`/
      `create_folder_display()`/`folder_entry()`, all touched by this
      sweep). No new regression test added -- this is dead-code removal
      with no behavior change to verify, not a bug fix. `bin/pine`'s
      `text` shrank 360663 -> 360151, -512 bytes.

### `pine/mailview.c` routine dead-code sweep -- DONE (fourth file of the four-file survey, partial)
- [x] Fourth and final installment of the "`pine/` UI files: dead/
      vestigial code" survey item -- but only the routine, uncontroversial
      portion; the `FileStar` disk-paged scroll-buffer engine
      (`ScrollFile()`/`make_file_index()`/the `st.source == FileStar`
      branches in `format_scroll_text()`/`scroll_scroll_text()`/
      `search_scroll_text()`/`zero_scroll_text()`) was deliberately left
      untouched, per this item's own caveat in `TODO-pdp11.md`: cutting
      it is a design decision (repurpose for the PDP-11's own
      large-message case vs. cut outright), not a mechanical dead-code
      removal, so it remains open there. Removed in this pass, all
      confirmed to be `#ifdef DOS`-gated (`DOS` is never defined
      anywhere in this tree) or gated on the similarly-never-defined
      `X_NEW` macro:
      - `mail_view_screen()`'s two DOS blocks: the `mc->rfc822_size >
        MAX_MSG_INCORE`-or-`nntp`-driver check that would set
        `src = FileStar` (the *only* place in the file that ever set
        `src` to anything but `CharStar` -- confirming `st.source` can
        now never actually be `FileStar` at runtime, which is why the
        `FileStar` branches noted above are structurally dead too, not
        just theoretically), and the paired `dos_gets`/`append_file`
        full-header fetch path.
      - `format_message()`'s three DOS blocks in its no-BODY-structure
        fallback path: the `mailgets`/`append_file` DOS-fetch-to-disk
        setup, the `gf_set_readc()` DOS-`FileStar`-vs-Unix-`CharStar`
        source split (kept the `CharStar` branch), and the
        `append_file`-close/`mailgets`-reset cleanup pair.
      - The `#ifndef DOS` wrapper around the view-screen key-menu's
        `F_ENABLE_PIPE` check (same class of finding as the
        `mailindx.c`-style keymenu-bitmap pattern noticed during the
        `mailcmd.c` sweep -- `DOS` is never defined, so the check
        already always ran; only the wrapper was dead).
      - `zero_scroll_text()`'s `#ifdef DOS` block that unconditionally
        freed `st.text_lines`/`st.line_lengths` and zeroed
        `st.lines_allocated` on every call -- a DOS-only aggressive
        free-and-reallocate-every-view memory strategy; on this (and
        every other) build these arrays instead persist across views
        and are only freed once, at real shutdown, by `end_scroll_text()`.
      - `format_scroll_text()`'s `#ifdef X_NEW`/`#else` line-wrap-length
        calculation split -- `X_NEW` is never defined anywhere in this
        tree (confirmed by grep), so only the `#else` branch (the real
        word-wrap algorithm handling tabs, ISO-2022 escapes via
        `match_iso2022()`, and the bold/underline hack bytes) has ever
        compiled; removed the dead `#ifdef X_NEW` branch and its now-
        unused `max_line` local, keeping the `#else` branch's content
        verbatim, including its `match_iso2022()` call -- that's tracked
        separately as a low-priority "Deliberate-simplification
        candidate" in `TODO-pdp11.md`, not touched here.
      Verified: `./build clean && ./build lnx` clean both before and
      after (baseline confirmed via `git stash`) -- zero warnings on
      `mailview.c` in either build; full smoketest suite 25/25. No new
      regression test added -- this is dead-code removal with no
      behavior change to verify, not a bug fix. `bin/pine`'s `text`
      stayed exactly 360151 bytes, unchanged -- expected and correct,
      since every site removed here was inside an `#ifdef` branch that
      was never taken (the preprocessor strips untaken branches before
      the compiler ever sees them, so their presence or absence in the
      source has zero effect on the compiled binary); this is unlike
      the `send.c`/`mailcmd.c`/`folder.c` sweeps' savings, which came
      from deleting functions that *were* being compiled in but had no
      callers (dead by reachability, not by preprocessor gating).

### `pine/mailview.c` FileStar disk-paged scroll-buffer engine -- investigated and removed, closing out the four-file survey
- [x] The routine sweep above left the `FileStar` (disk-paged) branch
      of the scroll-buffer engine open as a design decision: cut it
      outright, or repurpose it for the PDP-11's own large-message
      case, since a PDP-11 has even less RAM than the DOS box this
      existed for. Investigated by reading `ScrollFile()`,
      `make_file_index()`, and every `st.source == FileStar` branch
      end to end before deciding. Findings, and why repurposing was
      rejected in favor of deleting outright:
      - The core idea -- an on-disk index of per-line byte offsets
        (`st.findex`) so a big message's lines can be paged in from
        disk a screenful at a time without an in-RAM array of offsets
        for the whole message -- is genuinely still relevant to a
        memory-constrained target. That part isn't why this was cut.
      - But the concrete implementation is explicitly unfinished by its
        own author's comments: `ScrollFile()`'s header comment reads
        "BOGUS: this is painfully crude right now, but I just want to
        get it going" and "NOTE: talk about massive potential for
        tuning... Goes without saying this is still under
        constuction" -- prototype-quality, not a shipped feature.
      - `format_scroll_text()`'s own comment on the `FileStar` branch
        says it "makes some simplifying assumptions: 1. Lines are on
        disk just the way we'll display them" -- i.e. it skips the
        word-wrap/tab-expansion/ISO-2022-escape-aware line-breaking
        that the `CharStar` branch does (`match_iso2022()`, tab
        columns, the bold/underline hack bytes), and nothing upstream
        of it pre-wraps text to screen width before writing the temp
        file `st.text` would point to. Reusing this as-is would have
        displayed raw, un-reflowed lines for any large message
        containing tabs, long lines, or ISO-2022 escapes -- a
        regression, not a memory-saving feature.
      - The same comment block adds "we get away with this mainly
        because the DOS display isn't going to be resized out from
        under us" -- no terminal-resize support, whereas this fork
        already handles `SIGWINCH` resize elsewhere (see the
        `resize_pico()` fix in this file's history) -- reusing it would
        reintroduce a resize bug class already fixed in the composer.
      - Conclusion: the mechanism's one conceptually-reusable idea (an
        on-disk line-offset index) is not separable from an
        implementation that's incomplete and would misformat text and
        break on resize. A PDP-11 large-message strategy, if ever
        needed, should be designed fresh against this codebase's real
        word-wrap logic rather than resurrecting this prototype.
      Removed, once the decision was made: `ScrollFile()` and
      `make_file_index()` (both functions, entirely) and their four
      forward declarations (ANSI/K&R prototype blocks); the
      `st.findex`/`st.fname` fields from the `scroll_text` struct (and
      their initializers in `push_scroll_state()`/the struct's static
      initializer) -- both were solely for `ScrollFile()`'s on-disk
      index bookkeeping; the `PGSIZE` macro (used only by the two
      deleted functions and `format_scroll_text()`'s deleted branch);
      `format_scroll_text()`'s entire `else` branch that built the
      file-backed line buffer (with its now-stale "Display text is in
      a file" comment) -- since `st.source` can only be `CharStarStar`
      or `CharStar` now, this was unreachable; the `st.source ==
      FileStar` conditionals in `redraw_scroll_text()`,
      `scroll_scroll_text()` (two sites), `zero_scroll_text()`'s
      per-line-vs-whole-array free logic, and `search_scroll_text()`'s
      `offset`/`dlines` windowing logic (both instances, plus the
      wrap-around `ScrollFile(offset = 0)` call) -- each collapsed to
      the surviving non-`FileStar` behavior, and `search_scroll_text()`
      simplified further since with `offset` always `0`, `l - offset`
      is just `l`. Also fixed `zero_scroll_text()`'s now-inaccurate
      header comment ("Free memory used as scrolling buffers for text
      on disk") to match what it actually does post-removal. The
      `FileStar` enum value itself (`pine.h`'s `SourceType`) was left
      alone -- it's still legitimately used elsewhere for real
      disk-backed storage objects (attachments, export, etc. via
      `so_get(FileStar, ...)`), a completely different mechanism from
      this scroll-buffer's now-removed disk-paging path.
      Verified: `./build clean && ./build lnx` clean both before and
      after (baseline confirmed via `git stash`) -- zero warnings on
      `mailview.c`; full smoketest suite 25/25, including
      `test_message_view_from_overflow` and `test_folder_index_view`
      (exercise the message-viewing scroll path touched by this
      removal). No new regression test added -- this is dead-code
      removal with no behavior change to verify (since `st.source` was
      already structurally always `CharStar`/`CharStarStar`), not a bug
      fix. `bin/pine`'s `text` shrank 360151 -> 358383 (-1768 bytes),
      `data` shrank 13448 -> 13416 (-32), `bss` shrank 15224 -> 15192
      (-32) -- unlike the routine sweep above, these functions *were*
      being compiled in (not preprocessor-gated), so deleting them
      produced a real size win. This closes out the "`pine/` UI files:
      dead/vestigial code" survey item across all four files.

### `{host}`-remote folder-collection syntax -- cut, first of the "Deliberate-simplification candidates" (product decisions, not dead-code deletion)
- [x] Decision made with the user: cut the `{host}`-remote collection
      syntax outright, since the target has no TCP/IP at all (per
      CLAUDE.md) -- not just make the four `pine/folder.c` sites the
      TODO originally named unreachable, but disallow the syntax at
      the config-parsing level. Investigation found the cut was more
      cross-cutting than the TODO's file list suggested: the `{host}`
      prefix isn't parsed only where it's acted on, it's baked into
      the *general* context-string grammar in
      `pine/context.c`'s `context_digest()` -- the single parser every
      context string (local or not) goes through -- so a full cut
      needed changes there too, not just in `folder.c`.
      **`pine/context.c`**: `context_digest()` -- removed the `host`
      output parameter and the `if(*p == '{' ...)` prefix-skip block
      that consumed a leading `{host[/proto][:port]}` before the rest
      of the context grammar; removed the trailing `if(host) *host =
      '\0';` reset and the `scontext`-building `if(*context == '{')`
      copy-through block (built a `{host}` prefix onto the
      sprintf-able context string) since a context can no longer begin
      with `{`. Grammar comment simplified from `[*] [{host
      ["/"<proto>] [:port]}] [<cntxt>] "[" [<view>] "]" [<cntxt>]` to
      `[<cntxt>] "[" [<view>] "]" [<cntxt>]`. Converted the function's
      own signature to ANSI style
      (`context_digest(char *context, char *scontext, char *rcontext,
      char *view)`) per CLAUDE.md's opportunistic K&R-to-ANSI
      guidance, since the signature was already being edited for the
      parameter removal. Updated both in-file callers
      (`context_fqn_to_an()`, `context_apply()`) to the new 4-arg form.
      **`pine/folder.c`**: removed `set_ftype()` entirely (the
      function that inspected a context string's `{host[/nntp]
      [/anonymous]}` prefix to set `FTYPE_REMOTE`/`FTYPE_OLDTECH`/
      `FTYPE_ANON`/`FTYPE_LOCAL` bits) and its call site in
      `new_context()`; `new_context()`'s label-building collapsed from
      the `FTYPE_REMOTE`-conditional `"%s%s%s", rcontext, " on ",
      host` sprintf down to plain `cpystr(rcontext)`, and its trailing
      `dprint()` no longer lists type flags since there's only one
      type now. `find_folders_in_context()`: removed the
      `(context->type)&FTYPE_REMOTE` branch that rebuilt a `{host[/nntp]
      [/anonymous]}` stream-open string and tried to reuse/open a
      remote `MAILSTREAM` (and the `local_open` bookkeeping that only
      that branch used), keeping just the local `else` branch's
      behavior (`find_folder_stream = NULL; search_context =
      rcontext;`) unconditionally; trimmed the stale comment table
      above it down to the two local-mail/local-news rows (the
      imap/nntp rows described behavior that no longer exists in this
      code path). `folder_list_entry()`: the `(Remote)`/`(Local)`
      label ternary (width 8 vs. 7, text `"(Remote)"` vs. `"(Local)"`)
      collapsed to always emit `"(Local)"`/7, since every collection
      is local now. `folder_exists()`: removed the
      `mail_valid_net()`-gated remote-stream-reuse-or-open block
      (`same_stream()` against the current/inbox stream, falling back
      to `mail_open(..., OP_HALFOPEN)` on a freshly-built `{host}`
      string) along with the `host`/`mbox`/`ourstream` locals it alone
      used, keeping just `find_string = fn` (what the existing `else`
      branch already did for any non-remote-looking `fn`). Note:
      `pine/pine.c`'s own two `mail_valid_net()` calls (checking an
      *already-open* live stream's driver, unrelated to parsing
      collection-config syntax) were left untouched -- out of scope
      for this item.
      **`pine/pine.h`**: removed the `FTYPE_LOCAL`/`FTYPE_REMOTE`/
      `FTYPE_SHARED`/`FTYPE_OLDTECH`/`FTYPE_ANON` defines and the
      `unsigned short type;` field from `CONTEXT_S` -- confirmed via
      grep that every remaining reference to `->type` or `FTYPE_*` in
      `pine/` was inside the sites just removed, so both the flags and
      the struct field they were stored in were dead in tandem.
      Not touched: the `{host}`-prefix handling implicit in
      `context_fqn_to_an()`'s prefix/suffix matching comment (mentions
      "`mail_find` on open stream returns `{host}context`") -- that's
      generic FQN-vs-context mismatch recovery logic for c-client's
      `mail_find`/`mail_find_all` callbacks, not itself a
      remote-collection-syntax parser, so left alone as out of scope.
      Verified: `./build clean && ./build lnx` clean (only the
      pre-existing, already-tracked `-Wpointer-compare` bug at
      `folder.c` remains, line number shifted from `:1998` to `:1979`
      by this change's deletions -- updated the standing
      compiler-warnings TODO entry accordingly); full smoketest suite
      25/25. Size delta via `git stash`-based before/after clean
      rebuild: `bin/pine`'s `text` shrank 358383 -> 356119 (-2264
      bytes), `data`/`bss` unchanged (13416/15192) -- a real win, since
      unlike the earlier `#ifdef DOS`-gated dead-code passes, this was
      live, always-compiled code with no callers left once the syntax
      was disallowed. No regression test added -- pure feature
      removal with no distinct pass/fail behavior for the harness to
      capture (a `.pinerc` `folder-collections` line starting with
      `{` will now be parsed as a literal, non-special context string
      instead of triggering remote handling, rather than being
      rejected outright -- CLAUDE.md's config-generality
      conventions don't require inventing new config validation for a
      syntax variant this build no longer looks for).

### `pine/folder.c`'s `-Wpointer-compare` warning -- fixed (standing compiler-warnings goal)
- [x] `VAR_FOLDER_SPEC[0] == '\0'` (line number moved to `:1979` after
      the `{host}`-remote-syntax cut just above, `:1998` before that,
      `:2067` before that) compares a `char *` (`VAR_FOLDER_SPEC` is
      the `.l` union member, documented in `pine.h` as "pointer to
      list of string values", i.e. a NULL-terminated `char **`) against
      the character constant `'\0'`. Confirmed this is legal C and not
      a functional bug: `'\0'` is an `int` constant with value 0, which
      the standard treats as a null pointer constant, so the comparison
      is identical to `VAR_FOLDER_SPEC[0] == NULL` -- i.e. it correctly
      checks "is the list's first slot the NULL terminator" (empty
      list), the same NULL-terminated-array convention this same
      function already relies on a few lines down (`for(i = 0;
      ps_global->VAR_FOLDER_SPEC[i]; i++)`). The `'\0'` spelling was
      just a misleading way to write a pointer-vs-NULL check (reads
      like an accidental string-emptiness test), which is exactly what
      triggered the warning. Fixed by writing the comparison as
      `== NULL` -- same generated code, no behavior change, warning
      gone since comparing a pointer against `NULL` (rather than a
      character constant) is unambiguous.
      Verified: `./build clean && ./build lnx` -- warning count dropped
      180 -> 179, `folder.c` no longer appears in the warnings list;
      full smoketest suite 25/25. No regression test added (no
      behavior changed to observe -- this was a style/clarity fix, not
      a bug fix).

### Magic-number MIME-type sniffing -- cut, second of the "Deliberate-simplification candidates"
- [x] Investigation before cutting: `pine/send.c`'s `set_mime_types()`
      is called from exactly two places, both inside
      `create_message_body()` -- `set_mime_types(*b)` for a plain
      `TYPETEXT` body, and `set_mime_types(&((*b)->contents.part->body))`
      for the text part of a MIME-forward-as-attachment `MULTIPART`
      body (`reply.c`'s `body->contents.part->body.type = TYPETEXT;`).
      Both call sites set `(*b)->encoding = ENCOTHER;` immediately
      before calling, and both always pass a body whose `type` is
      already `TYPETEXT`, never `TYPEOTHER`. Since the entire
      magic-number sniffing block (GIF/JPEG/TIFF/PostScript/`.snd`/
      AppleFile/ZIP detection, plus the `else` "PROBLEM" octet-stream
      fallback for an empty read) was gated behind `if(body->type ==
      TYPEOTHER)`, it was already fully dead code in every reachable
      call today -- not just "vestigial" as the TODO's original
      framing put it. Asked the user whether the function could be
      cut entirely given this; answer was no -- past the sniffing
      block, the function also determines the outgoing text's
      `Content-Transfer-Encoding` (7BIT/8BIT/BINARY, based on scanning
      for 8-bit characters/control chars/NULs/line length) and sets
      `subtype=PLAIN`/`charset=US-ASCII` -- real, necessary work
      for correctly encoding whatever the user actually typed or
      pasted (e.g. accented characters, stray control bytes), so a
      full removal would leave outgoing messages with no
      `Content-Transfer-Encoding` and no MIME subtype.
      Removed: the entire `if(n){ if(body->type==TYPEOTHER){
      ...magic numbers...} } else { if(body->type==TYPEOTHER){
      ...octet-stream fallback...} }` block. Then, since `body->type`
      is provably always `TYPETEXT` at every remaining point in the
      function, collapsed every downstream `if(body->type ==
      TYPEOTHER)` dead branch and `(body->type==TYPETEXT) ? X : Y`
      ternary down to its `TYPETEXT` arm: the NUL-byte handling in the
      byte-scanning loop (`new_encoding = (body->type==TYPETEXT) ?
      ENC8BIT : ENCBINARY; if(body->type==TYPETEXT) eight_bit_chars++;
      else break;` -> unconditional `new_encoding = ENC8BIT;
      eight_bit_chars++;`), the `max_line >= 1000L` branch's dead
      `TYPEAPPLICATION`/`octet-stream` fallback and its ternary, the
      `eight_bit_chars == 0L` branch's dead `body->type = TYPETEXT`
      no-op, the `< 30%` branch's same no-op, and the final `else`
      branch's dead `TYPEAPPLICATION`/`octet-stream` fallback. The
      outer `if(body->encoding == ENCOTHER || body->type ==
      TYPEOTHER)` guards (both call sites always pass `ENCOTHER`, so
      always true) and the final `if(body->type == TYPETEXT &&
      body->subtype == NULL)` condition were simplified the same way
      (dropped the now-always-true `type` half of each). Kept
      unchanged: the `!= ENCBINARY`/`== ENCOTHER` guards around
      `new_encoding` assignment (these aren't part of the
      type-guessing dead code -- they encode a real "don't downgrade
      an already-upgraded encoding" precedence rule), the
      `can_be_ascii`/control-character bookkeeping (unrelated to
      magic-number type sniffing; tracked separately by the still-open
      ISO-2022-handling TODO item), and the final `if(body->encoding
      == ENCOTHER) body->encoding = new_encoding;` guard (a distinct,
      still-meaningful "don't override a caller-supplied encoding"
      contract, not dependent on the type invariant). Updated the
      function's header comment to describe what it actually does now
      (encoding/subtype determination for typed text only) instead of
      the stale general-purpose "sniff the given storage object to
      determine its type, subtype and encoding" description.
      Verified: `./build clean && ./build lnx` clean, warning count
      unchanged at 179 (the removed code was already live/compiled,
      not preprocessor-gated, so no warnings were attached to it
      either way); full smoketest suite 25/25, including
      `test_base64_body` (exercises the encoding-selection logic this
      touched). No regression test added -- pure dead-branch removal
      with no reachable behavior change (the removed branches could
      never fire given the confirmed `TYPETEXT`/`ENCOTHER`-only
      callers), not a bug fix. Size delta via `git stash`-based
      before/after clean rebuild: `bin/pine`'s `text` shrank 356119 ->
      355023 (-1096 bytes), `data`/`bss` unchanged (13416/15192) -- a
      real win since, like the `folder.c` `{host}`-remote cut before
      it, this was live compiled-in code with no reachable callers
      left, not a preprocessor-gated `#ifdef`.

### UW Message-ID size-stats decoration -- cut, third of the "Deliberate-simplification candidates"
- [x] `pine/send.c`'s `call_mailer()` called
      `update_message_id(envelope, mime_stats(body))` right after
      generating the outgoing `Message-ID:`, splicing a 7-character
      stats token (log2 of total message size, plus clamped
      text/message/application/video/audio/image part counts, each
      base62-ish encoded via `encode_bits()`) into the id just before
      its `@`. A UW diagnostic convenience (support staff could
      eyeball rough message shape from the `Message-ID:` header
      alone) with no effect on mail delivery or correctness.
      Confirmed safe to remove outright: `generate_message_id()`
      (`pine/reply.c:1296`) -- called right before this, and from five
      other places in `reply.c` -- already builds a complete, unique
      `Message-ID:` on its own from hostname + pid + timestamp +
      counter character, with no dependency on the stats decoration.
      Grepped for any other reader of `message_id` in the tree
      (`mailindx.c`, `mailcmd.c`): both just print/compare it as an
      opaque string, neither parses the stats token, so nothing
      downstream depends on the decorated format either.
      Removed: `update_message_id()`, `mime_stats()`, `mime_recur()`,
      `encode_bits()`, `logbase2()` (all five functions, entirely),
      the static `struct mime_count { ... } mc;` and `mime_stats()`'s
      static `id[10]` buffer, the `mime_stats`/`mime_recur` forward
      declarations (both ANSI and K&R blocks), and the
      `update_message_id(envelope, mime_stats(body));` call site
      (plus its now-inaccurate "recompute message-id to encode body
      info stats" comment) in `call_mailer()`.
      Verified: `./build clean && ./build lnx` clean, warning count
      unchanged at 179; full smoketest suite 25/25. No regression test
      added -- pure removal of a cosmetic decoration with no reachable
      behavior to observe (the `Message-ID:` is still present, unique,
      and RFC-valid, just without the stats token). Size delta via
      `git stash`-based before/after clean rebuild: `bin/pine`'s `text`
      shrank 355023 -> 353671 (-1352 bytes), `bss` shrank 15192 ->
      15128 (-64 bytes, the removed static `mc`/`id` buffers),
      `data` unchanged (13416) -- live compiled-in code with no
      reachable callers left once removed, not a preprocessor-gated
      cut.

### Restricted ("demo") mode lockdown -- cut project-wide, fourth of the "Deliberate-simplification candidates"
- [x] Investigated the full reach of `ps_global->restricted` before
      cutting, since the TODO flagged this as needing a broader
      decision than the two `send.c`/`mailcmd.c` sites it originally
      named. Found the flag (settable only via the `-r` CLI flag --
      UW's public-kiosk-demo-account mode, `pine/args.c`) actually
      touches 7 files end to end: `pine/pine.h` (the `restricted:1`
      bitfield), `pine/send.c` (`check_addresses()`'s mail-to-self-only
      lockdown, and passing `P_SECURE` into the composer's
      `pine_flags`), `pine/mailcmd.c` (blocking "export message to
      file" and "save attachment"), `pine/pine.c` (blocking the
      keyboard-lock command, hiding its keymenu entry, blocking
      password-change), `pine/osdep/expnfldr` (blocking folder opens
      by local path `./`/`~`), and the composer's own file-browser
      lockdown fed through the `pine_flags`/`P_SECURE` handoff
      protocol into `pico/pico.h`'s `MDSCUR` mode bit, checked in 5
      sites across `pico/browse.c` (read directory, delete, copy,
      rename, visit-parent) and 2 sites in `pico/file.c` (insert
      file). Asked the user to confirm a project-wide cut given the
      scope (a single-user PDP-11 station never runs UW's public-kiosk
      demo configuration); confirmed.
      Removed, in order: the `-r` case and its `args_help()` usage
      line (`pine/args.c`); the `restricted:1` bitfield (`pine/pine.h`);
      the `P_SECURE` pass-through and the entire restricted-mode block
      in `check_addresses()` (`pine/send.c` -- kept the rest of that
      function, the mailer-daemon confirmation prompt, untouched);
      both restricted-mode early-returns in `mailcmd.c` (export
      message, save attachment); the keyboard-lock command's
      `if(ps_global->restricted) goto bleep;` guard, the
      password-change block's `if(restricted){...}else{...}` (kept
      just the `else` body -- `change_passwd()`/`ClearScreen()`/
      `mangled_screen=1` -- inside the existing, untouched
      `#ifdef DOS/#else/#endif` wrapper), and the keymenu-bit-clear
      site, which needed a small logic inversion rather than a
      straight deletion: it was `#ifndef NO_KEYBOARD_LOCK
      if(restricted) #endif clrbitn(...)` (hide the lock keymenu entry
      only in demo mode, given the keyboard-lock feature exists at
      all); since `NO_KEYBOARD_LOCK` is a separate, still-live
      compile-time knob unrelated to demo mode (confirmed via
      `pine/os.h`'s `/* #define NO_KEYBOARD_LOCK */`, currently
      commented out), the correct post-cut behavior is "hide the entry
      only if the keyboard-lock feature doesn't exist at all," i.e.
      `#ifdef NO_KEYBOARD_LOCK clrbitn(...) #endif` -- an inverted
      guard, not a deletion, to preserve that still-meaningful
      compile-time toggle (`pine/pine.c`); the local-path-open block
      in `pine/osdep/expnfldr` (the real source fragment for the
      generated `pine/osdep/os-lnx.c`/`pine/os.c` symlink target, per
      CLAUDE.md's generated-file rule -- editing `os.c`/`os-lnx.c`
      directly was refused by the write tooling since `os.c` is a
      symlink).
      `pico/pico.h`: removed the `P_SECURE`/`MDSCUR` flag definitions.
      `pico/browse.c`: removed all 5 `gmode&MDSCUR` guards (read
      directory, delete, copy, rename, visit-parent), 3 of which were
      combined with a `homeless()` check (is the target outside the
      user's home directory tree); `pico/file.c`: removed both
      `gmode&MDSCUR` guards (collapsing each `if(MDSCUR){...}
      else{...}` to just the `else` body -- normal file-read/insert
      behavior, unconditionally). Once all `MDSCUR`+`homeless()` sites
      were gone, `homeless()` itself (`pico/os_unix.c`) had zero
      remaining callers -- removed the function and its two `extern`
      declarations (ANSI and K&R blocks) in `pico/efunc.h`.
      Not touched: `address_is_us()` (`pine/addrutil.c`), still used
      by `mailindx.c`/`reply.c` for unrelated reply-address logic --
      confirmed via grep before leaving it alone, since
      `check_addresses()`'s deleted block was its only
      restricted-mode-specific caller.
      Verified: `./build clean && ./build lnx` clean (confirmed
      `pine/osdep/os-lnx.c` regenerates correctly from the fixed
      `expnfldr` fragment via `osdep/makedep`); warning count dropped
      179 -> 178 (one fewer K&R-style-definition warning, from the
      deleted `homeless()`); full smoketest suite 25/25. No regression
      test added -- pure feature removal with no distinct pass/fail
      behavior for the harness to capture (there's no CLI flag left to
      reach the removed code paths from at all). Size delta via `git
      stash`-based before/after clean rebuild: `bin/pine`'s `text`
      shrank 353671 -> 352767 (-904 bytes), `bin/pico`'s `text` shrank
      112260 -> 111366 (-894 bytes), `data` shrank 6380 -> 6372 (-8
      bytes); `bss` unchanged on both binaries -- live compiled-in
      code with no reachable callers left once the `-r` flag (the only
      way to ever set `restricted`) was gone, not a preprocessor-gated
      cut.

### Multi-collection navigation -- cut project-wide, fifth of the "Deliberate-simplification candidates"
- [x] The TODO flagged this item as needing a scope decision before
      cutting ("worth deciding whether multi-mailbox collections stay
      in scope at all"), so investigated the full reach of
      `CONTEXT_S`/`context_list` before touching anything (an
      `Explore` subagent surveyed it first, since the item was
      expected to span more files than the ones the TODO originally
      named). Findings: no Setup/Collection-List *editor* screen
      exists in this codebase (the usual UW Pine one was already
      stubbed out and deleted in an earlier pass -- collections are
      configured only via `.pinerc`, never interactively); no address
      book or per-collection signature file (both already removed/
      never per-collection); so the true footprint was `context_list`
      traversal/switching logic in `folder.c`, `mailcmd.c`, `send.c`,
      `screen.c`, `pine.c`, `pine.h`, and `pico/compose_stub.c` --
      roughly 190-210 lines out of ~14,600 across those files. Asked
      the user two scope questions and got "cut fully" both times: (1)
      cut the general `folder-collections` multi-collection machinery
      entirely, keeping only single-collection folder-management code,
      and (2) also cut `incoming-folders`/`CNTXT_INCMNG` (a *separate*
      `.pinerc` variable/feature from `folder-collections`, for
      TAB-cycling between several inbox-like mailbox files -- distinct
      enough from "named collections A/B/C" that it needed its own
      call, per the TODO's own note that it was "cleanly separable").
      **`pine/pine.h`**: `CONTEXT_S.next` (the list linkage) removed
      entirely -- a build now has exactly one `CONTEXT_S`, never a
      list. Removed the now-meaningless `CNTXT_PRIME`/`CNTXT_SECOND`
      flags (confirmed dead independent of this cut: set in
      `init_folders()` but never read anywhere), `CNTXT_INCMNG` (the
      incoming-folders marker), `CNTXT_SAVEDFLT` (redundant once
      there's only one collection -- it was always "the" default),
      and the `num` field (write-only, confirmed via grep: set but
      never read, likely already vestigial). Removed `struct pine`'s
      `context_last` (only ever existed to let `do_broach_folder()`
      remember "the collection you were in before switching to
      another one" -- meaningless with one collection) and
      `context_index` (already fully dead before this cut: only ever
      set to 0, never read anywhere, with `folder.c`'s own `/* BUG?
      eliminate context_index from pine struct??? */` comment flagging
      exactly this).
      **`pine/folder.c`** (the largest and riskiest file, since it
      both builds the collection and implements the Folder List
      screen that browses it): `init_folders()` rewritten from a
      ~160-line loop that built the synthetic "Incoming Message
      Folders" pseudo-collection from `incoming-folders` plus chained
      every `folder-collections` entry via `->next`, down to building
      exactly one `CONTEXT_S` from `VAR_FOLDER_SPEC[0]` (extra
      configured values are now silently ignored) and attaching INBOX
      to it directly (the code's own old comment called this "the good
      old pre-collection behavior" -- now simply *the* behavior).
      `folder_lister()`/`folder_list_entry()`/`create_folder_display()`:
      removed the Left/Right-arrow collection-boundary browsing, the
      "Now in collection <label>" status message (confirmed dead even
      before deletion -- `fs.context` never changes after the first
      draw once there's nothing to switch to, so the message could
      never fire), and the per-collection header-row-drawing branch in
      `folder_list_entry()` (also confirmed dead first: it was gated
      by a `label` counter that starts at 0 and is only ever set
      nonzero from inside that same branch or the `(*context)->next`
      branch -- both unreachable once there's one collection, so the
      whole thing was already provably a no-op, not just redundant).
      `search_folders()`: removed the cross-collection wrap-to-next-
      context walk (now wraps within the single collection's folder
      list only). `add_new_folder()`/`delete_folder()`: removed their
      `CNTXT_INCMNG` "can't add/delete in the incoming collection"
      guards. `free_folders_in_context()`: removed its `CNTXT_INCMNG`
      early-return special case. `default_save_context()` and
      `next_folder()` (the whole point of the "next folder" TAB
      feature) deleted outright -- both fully replaced by direct
      `ps_global->context_list` references at their call sites, since
      with one collection there's nothing left for either to compute.
      **`pine/mailcmd.c`**: removed the three `CNTXT_INCMNG`-gated
      "Press TAB for next folder" / "View next folder" prompts (^N at
      end of folder, delete-last-message, TAB-skip-to-next-unread);
      the ^P/^N collection-cycling key handlers and their "only show
      if >1 collection" gates in both `save_msg()`'s SAVE prompt and
      `broach_folder()`'s GOTO prompt (each had its own independently-
      implemented cycling loop -- both removed the same way); the
      `context_last` bookkeeping in `do_broach_folder()`; and the
      dead `(new_context->use & CNTXT_INCMNG) && ...` conjunct in the
      first-unread-message-on-open logic (folded down to just
      `open_inbox`, its other half).
      **`pine/send.c`**: `call_mailer()`'s Fcc handling simplified to
      use `ps_global->context_list` directly (was
      `default_save_context()` with a NULL fallback to the same
      thing); dropped the ">1 collection" message-wording gate for the
      "Folder doesn't exist, create?" prompt. Removed
      `fcc_candidate_list()`'s caller in `run_composer_subprocess()`
      and the `FCCLIST <n>` handoff-file block it wrote (see the
      composer side below).
      **`pine/screen.c`**: `set_titlebar()`'s ">1 collection" gate for
      showing the collection name removed -- the titlebar's
      `as.context_name` is now always `""`.
      **Composer side** (`pico/`): the ^T Fcc-collection picker
      (`pico/compose_stub.c`'s `compose_folders()`, added by the
      earlier "composer as a separate process" work specifically to
      let the subprocess offer a numbered menu of collection
      nicknames pine precomputed) is gone along with the whole
      `FCCLIST` handoff-file field and `fcc_list`/`fcc_count` statics
      it parsed -- with exactly one collection it degenerated to a
      useless 1-item menu. Removed the `PICO.folders` callback field
      (`pico/pico.h`) and its wiring (`pico/main.c`), the `CTRL|'T'`
      case in `pico/composer.c`'s composer command switch (falls
      through to the default "Unknown command: ^%c" beep now -- the
      Fcc field is still directly editable as plain text, it just has
      no picker), and `ShowPrompt()`'s `FCCHDR`-specific key-help
      string (folded into the same one every other header field
      shows, since there's no longer a `^T` to advertise). Updated the
      stale rationale comment on `pico/edef.h`'s `SLEN` (the global
      scratch-buffer size), which cited this exact ^T round-trip as
      its worst-case-size justification -- noted `SLEN` may now be a
      further shrink candidate, folded into the still-open general
      buffer-audit TODO item rather than acted on here (buffer
      right-sizing needs its own dedicated verification pass per
      CLAUDE.md, not a side effect of a feature cut).
      **`pine/init.c`**: `incoming-folders` converted to a disabled
      stub variable (`is_obsolete=1`, matching the established pattern
      for other removed features like `news-collections`/
      `address-book`), and its `set_current_val()` call at startup
      removed (also matching precedent -- none of the other disabled
      variables have one either). `folder-collections`' help text
      rewritten to document the single-collection reality instead of
      showing a 2-collection example.
      **Test suite**: deleted
      `test/cases/test_compose_fcc_picker.py` -- it was a regression
      test for the exact ^T Fcc-picker round-trip this cut removes,
      written for behavior that no longer exists rather than a bug
      that could recur.
      Verified: `./build clean && ./build lnx` clean; warning count
      177 (was 178 before this cut); full smoketest suite 24/24 (25
      minus the deleted picker test). Manually drove the Folder List
      screen (`L`), the GOTO folder prompt (`G`, confirmed no stray
      Prev/Next Collection keys), and the SAVE prompt (`s`, confirmed
      it now shows only `^T To Fldrs` with no collection-cycling keys)
      through a pty, since several of the touched code paths (the
      arrow-key collection browsing, the SAVE/GOTO cycling keys) had
      no existing smoketest coverage. Size delta via `git stash`-based
      before/after clean rebuild: `bin/pine`'s `text` shrank 352767 ->
      343999 (-8768 bytes, the largest single cut of this
      simplification pass so far), `data`/`bss` unchanged; `bin/pico`'s
      `text` shrank 111366 -> 110098 (-1268 bytes), `data` shrank 6372
      -> 6364 (-8 bytes), `bss` unchanged.

### ISO-2022 escape-sequence handling -- cut, sixth of the "Deliberate-simplification candidates"

- **What it was**: `pine/mailview.c` carried a table of X11R5-derived
  ISO-2022 charset-shift escape sequences (`iso2022_escapes[]`) and a
  `match_iso2022()` matcher, used by `format_scroll_text()`'s
  line-width accounting to avoid counting an ISO-2022 escape
  sequence's bytes toward the displayed line width -- a residual i18n
  accommodation with no purpose under this build's US-ASCII-only
  mandate (the TODO's original framing, ~25 lines, one file).
- **Scope grew once the shared symbol was traced** (same pattern as
  the multi-collection item): `match_iso2022()` is a plain global
  function (no header prototype, called via implicit K&R linkage), so
  it had two more call sites the TODO note didn't mention:
  - `pine/ttyout.c`'s `PutLine0n8b()` called it directly to compute an
    escape length, passed through to `Writechar(ch, esc_len)`'s
    caller-supplied-length parameter -- the *only* caller across the
    whole tree that ever passed a nonzero second argument; every other
    of the ~40 `Writechar()` call sites always passed `0`. `Writechar`
    threaded that length through a static `esc_len` counter used
    purely to suppress column-advance for however many subsequent
    characters `new_esc_len` said to skip.
  - `pine/filter.c`'s `gf_iso_filter()`/`gf_iso_filter_opt()`
    (`gf_iso_match` static), an ISO-2022-stripping stream filter --
    already dead code, wrapped in `#ifdef notdef /* not using this
    anymore */` since before this project forked pine, so it never
    compiled either way, but it's the same feature and referenced the
    function being deleted.
  - A real latent bug found in the process: `PutLine0n8b()`'s `len`
    local was set only inside the `*line == '\033'` branch, then
    reused **uninitialized** in the `else` branch for every other
    character on a line that never happened to start with an escape --
    i.e. `Writechar(c, len)` for ordinary text could pass an
    indeterminate stack value as an escape-skip length. Removing the
    ISO-2022 special case removes the bug along with it (both branches
    now just call the simplified single-argument `Writechar(c)`).
- **What changed**:
  - `pine/mailview.c`: deleted `iso2022_escapes[]` and
    `match_iso2022()`. `format_scroll_text()`'s `*p == '\033'` case in
    the line-wrap loop no longer branches on a (now nonexistent) match
    result -- it always takes what used to be the "plain escape"
    fallback: count the escape byte as one column and move on.
  - `pine/ttyout.c`: `Writechar()` loses its `new_esc_len` parameter
    and the `esc_len` static entirely; every call site across the tree
    (`mailbox.c`, `imap.c`, `screen.c`, `mailcmd.c`, `pine.c`,
    `ttyin.c`, `status.c`, `filter.c`, `ttyout.c` itself) updated to
    the one-argument form. Converted `Writechar`'s definition to ANSI
    style while touching it (per CLAUDE.md's opportunistic-conversion
    guidance). `PutLine0n8b()` drops its `\033`-special-case branch and
    the now-pointless `len` local (fixing the uninitialized-read bug
    above as a side effect); a raw ESC byte in displayed text now
    falls through to the same "unprintable control char -> `?`"
    handling every other control character already got, instead of
    being passed through to the terminal untouched.
  - `pine/filter.c`: deleted the entire dead `#ifdef notdef` block
    (`gf_iso_filter()`/`gf_iso_filter_opt()`/`gf_iso_match`), and fixed
    the one live `Writechar()` call site it neighbored
    (`gf_busy_filter`'s spinner) to the new one-argument form.
  - `pine/pine.h`: `Writechar()`'s ANSI prototype drops its second
    `int` parameter; `gf_iso_filter()`/`gf_iso_filter_opt()`'s
    prototypes (both ANSI and K&R blocks) removed.
  - Left alone: `pine/pine.def`'s `_match_iso2022`/`_gf_iso_filter`/
    `_gf_iso_filter_opt` export entries -- that file only feeds the
    unmaintained DOS/Windows `makefile.msc` target, out of scope per
    CLAUDE.md (same precedent as the stray `_default_save_context`
    entry noted in the multi-collection write-up above).
- **Observable behavior change** (accepted, not a bug): a raw ESC byte
  appearing in a displayed message body is no longer treated as
  possibly-zero-width -- it renders as a literal `?` and takes one
  column, exactly like any other control character. Under the
  US-ASCII mandate this can only happen from a stray/malformed byte in
  a message body, not from any charset pine still supports.
- **New regression test**:
  `test/cases/test_message_view_iso2022_escape.py` -- views a message
  whose body embeds a raw `\033(B` sequence between two marker words
  and confirms the escape renders as `?` (locking in the new
  behavior) with no crash/garbling of the surrounding text.
- **Verified**: `./build clean && ./build lnx` clean, 177 warnings
  (unchanged from before this cut -- `Writechar`'s K&R-style
  definition warning was already counted; converting it to ANSI style
  didn't change the total since it's still one function). Full
  smoketest suite 25/25 (24 existing + the new test). Size delta via
  `git stash`-based before/after clean rebuild: `bin/pine`'s `text`
  343999 -> 343415 (-584 bytes), `data` 13416 -> 13224 (-192 bytes),
  `bss` 15128 -> 15096 (-32 bytes), total -808 bytes; `bin/pico`
  unchanged (the cut code was all pine-side).

### `init_driver()` and `count_unseen()` -- first two gcov-flagged dead-code candidates, confirmed dead and removed

- **What they were**: the first two items pulled from the
  "gcov-flagged dead-code candidates" list added to `TODO-pdp11.md`
  after the `--coverage` audit.
  - `pine/init.c`'s `init_driver(ps, driver_list)`: its entire body was
    wrapped in `#ifdef FIRST_FOLDER_TRY`, a macro never defined
    anywhere in the tree (no `-D` build flag, no `#define`) -- so even
    on the rare chance something did call it, it would compile down to
    an empty function. Grep found exactly one match tree-wide (its own
    definition line) for both the parenthesized-call pattern and a
    bare-word pattern, so there's no function-pointer/callback-style
    reference hiding either (the pattern that produced 8 false
    positives in the original audit).
  - `pine/pine.c`'s `count_unseen(stream)`: a normal, fully-implemented
    function (counts unseen messages via `FETCH_ALL_FLAGS` +
    `mail_elt()`), not gated behind any `#ifdef`. Also exactly one
    tree-wide match (its own definition) under both grep patterns, and
    no header prototype anywhere.
- **Verification**: `./build lnx` confirmed clean before touching
  anything. Deleted both function bodies. `./build clean && ./build
  lnx` clean afterward, 177 warnings (unchanged). Full smoketest suite
  25/25 (unchanged -- neither function was reachable from any code
  path, so there's no observable behavior difference to lock in with a
  new regression test, unlike the fixed-size-buffer-overflow class of
  cuts).
- **Size delta**: `bin/pine`'s `text` 343415 -> 343015 (-400 bytes),
  `data`/`bss` unchanged; `bin/pico` unchanged (both cut functions were
  pine-side).

### `context_find()`, `context_subscribe()`, `context_unsubscribe()` -- confirmed dead and removed; `mm_bboard()` pulled back out as a false positive

- **What they were**: `pine/context.c`'s "Context Manager" layer wraps
  c-client's `mail_find`/`mail_find_all`/`mail_create`/`mail_rename`/
  `mail_delete`/`mail_append`/`mail_subscribe`/`mail_unsubscribe` with
  context-aware ambiguous-name resolution, one wrapper per c-client op,
  all prototyped together in `context.h`. Three of these wrappers had
  no caller anywhere: `context_find` (287; sibling `context_find_all`
  is the one pine's UI actually calls, from `folder.c`'s Find-folder
  and search-folders commands), `context_subscribe` (495) and
  `context_unsubscribe` (512) (no subscribe/unsubscribe UI exists in
  this build). Confirmed by checking their four siblings
  (`context_create`/`context_rename`/`context_delete`/
  `context_find_all`) all *do* have real call sites in `mailcmd.c`/
  `send.c`/`folder.c` -- these three are asymmetric, not part of some
  larger unused subsystem.
- **`mm_bboard` false positive caught during this investigation**: the
  original gcov audit grouped `mm_bboard` (context.c:59) with these
  three as "possibly leftover bboard/newsgroup-subscribe support," but
  `mm_bboard` is one of c-client's fixed-name mail callbacks (like
  `mm_searched`/`mm_expunged`/`mm_diskerror` in `mailbox.c`, already
  correctly excluded from the tier-1 list) -- `c-client/mail.h`
  declares it as an extern the application must define, and
  `c-client/dummy.c` calls it directly (twice, while pattern-matching
  bboard-style names during a folder scan). The original audit's
  cross-reference grep only covered `pine/`+`pico/`, never
  `c-client/`, so it missed this call site entirely. Re-checked all
  other remaining tier-1 candidates against `c-client/` too (none had
  a hit there) before trusting the rest of the list.
- **Verified**: `./build lnx` clean before touching anything. Deleted
  the three function bodies (`context.c`) and their `context.h`
  prototypes. `./build clean && ./build lnx` clean afterward, 177
  warnings (unchanged). Full smoketest suite 25/25 (unchanged -- no
  observable behavior difference, none of the three were reachable
  from any code path).
- **Size delta**: `bin/pine`'s `text` 343015 -> 342383 (-632 bytes),
  `data`/`bss` unchanged; `bin/pico` unchanged (context.c is pine-side
  only).

### `gf_set_writec()` and `gf_busy()` -- confirmed dead and removed

- **What they were**: `pine/filter.c`'s `gf_set_writec()` is the
  output-side twin of `gf_set_readc()` (both configure a `gf_io_t`
  function pointer to either the file-backed or string-backed
  primitive), but only `gf_set_readc` has real callers (4 call sites
  across `reply.c`/`mailcmd.c`/`mailview.c`/`send.c`) -- nothing in the
  tree ever calls `gf_set_writec`. `gf_busy()` is a filter matching the
  standard `void (FILTER_S *, int, int)` filter shape (a busy-spinner
  effect, cycling `/`/`-`/`\`/`|` in the corner every 2K characters),
  but unlike every other real filter in this file it's never actually
  installed into a filter chain -- checked every `gf_link_filter()`
  call site and every `filters[]`/`aux_filter[]` array assignment
  across `reply.c`/`mailview.c`/`send.c`/`mailcmd.c` (the same
  registration idioms that produced 8 false positives earlier in this
  audit) and `gf_busy` appears in none of them.
- **Verified**: both re-checked against `c-client/` too (given the
  `mm_bboard` false-positive lesson) -- no hits there either. `./build
  lnx` clean before touching anything. Deleted both function bodies
  from `filter.c` and their prototypes (ANSI + K&R blocks) from
  `pine.h`. `./build clean && ./build lnx` clean afterward, 177
  warnings (unchanged). Full smoketest suite 25/25 (unchanged -- no
  observable behavior difference, neither function was reachable from
  any code path).
- **Size delta**: `bin/pine`'s `text` 342383 -> 341855 (-528 bytes),
  `data`/`bss` unchanged; `bin/pico` unchanged (filter.c is pine-side
  only).

### Seven dead `pine/strings.c` functions -- confirmed dead and removed

- **What they were**: `sqzspaces()` (squeeze blanks out of a string),
  `strcat_nocr()` (concatenate skipping `\r`), `crlf2lf()`/`lf2crlf()`
  (CRLF<->LF conversion), `month_num()` (parse a `mmm-yyyy` string back
  to a month number -- the inverse of `month_abbrev()`, which *is*
  used), `enth_string()` (format a number as "1st"/"2nd"/"3rd"/"105th"),
  and `rfc822date_to_ctime()` (reformat an RFC822 date string). All
  seven had zero references anywhere in the tree beyond their own
  definition, header prototype, and (for most of them) a mention in
  `strings.c`'s own stale top-of-file function manifest comment --
  confirmed via bare-word grep across `pine/`+`pico/`+`c-client/`
  (the last one specifically because of the `mm_bboard` false-positive
  lesson from the context.c cut). `month_num`'s extra grep hit was a
  same-named but unrelated local parameter in the neighboring
  `month_abbrev()`, not a call.
- **What changed**: deleted all seven function bodies from
  `pine/strings.c` along with their doc comments (cleaning up one
  orphaned half-comment left over from deleting `strcat_nocr` --
  its comment header and `sstrcpy()`'s were back-to-back and had been
  split across the deletion boundary), removed their entries from the
  file's own top-of-file function manifest, and removed both the ANSI
  and K&R prototype declarations from `pine/pine.h`.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward, 177 warnings (unchanged).
  Full smoketest suite 25/25 (unchanged -- no observable behavior
  difference, none of the seven were reachable from any code path).
- **Size delta**: `bin/pine`'s `text` 341855 -> 340359 (-1496 bytes),
  `bss` 15096 -> 15032 (-64 bytes, from the two removed static return
  buffers `enth[10]`/`dout[30]`), `data` unchanged; `bin/pico`
  unchanged (strings.c is pine-side only).

### `pine/ttyout.c`: `InsertChar()`, `DeleteChar()`, `PutLine3()`, `CleartoEOS()` -- last of the gcov-flagged dead-code list, confirmed dead and removed

- **What they were**: `InsertChar()`/`DeleteChar()` are termcap
  insert-char/delete-char wrappers, `PutLine3()` is the 3-arg member of
  the `PutLine1`..`PutLine5` printf-style screen-output family, and
  `CleartoEOS()` clears from the cursor to end of screen. All four had
  zero references anywhere beyond their own definition and header
  prototype. `PutLine3`'s siblings `PutLine1`/`PutLine2`/`PutLine4`/
  `PutLine5` all have real callers (`pine.c`, `mailindx.c`, `screen.c`,
  `other.c`) -- `PutLine3` alone was never needed by any actual format
  string in the tree.
- **Cascading dead state found and removed alongside them**: with
  `InsertChar`/`DeleteChar`/`CleartoEOS` gone, the termcap-capability
  static pointers `_insertchar`/`_startinsert`/`_endinsert`/
  `_deletechar`/`_cleartoeos` (`ttyout.c`) became write-only -- assigned
  from `tigetstr()`/`tgetstr()` at terminal-init time but never read by
  anything else. Removed all five from the declaration line and both
  the terminfo (`tigetstr`) and termcap (`tgetstr`) init blocks. The
  `tgetstr(..., &ptr)` calls advance a shared buffer cursor rather than
  writing to fixed offsets, so dropping entries from the middle of that
  sequence doesn't disturb the capabilities still being read.
- **Verified**: all four re-checked against `c-client/` too (none
  found, as expected -- these are terminal-display functions, not
  c-client callbacks). `./build lnx` clean before touching anything.
  Deleted the four function bodies and the five now-write-only static
  pointers from `ttyout.c`, and their prototypes (ANSI + K&R) from
  `pine.h`. `./build clean && ./build lnx` clean afterward, 177
  warnings (unchanged). Full smoketest suite 25/25 (unchanged -- no
  observable behavior difference, none of the four were reachable from
  any code path).
- **Size delta**: `bin/pine`'s `text` 340359 -> 339607 (-752 bytes),
  `bss` 15032 -> 15000 (-32 bytes), `data` unchanged; `bin/pico`
  unchanged (ttyout.c is pine-side only).
- **This closes out the gcov-flagged dead-code list** added to
  `TODO-pdp11.md` after the `--coverage` audit: all 19 functions
  flagged after the first correction pass are now accounted for -- 18
  confirmed dead and removed across this and the four preceding
  entries (2 in `init.c`/`pine.c`, 3 in `context.c`, 2 in `filter.c`, 7
  in `strings.c`, 4 here in `ttyout.c`), and 1 (`mm_bboard`) turned out
  to be a further false positive (a live c-client callback) caught
  while verifying the `context.c` trio. Combined with the 8
  filter/callback-registration false positives caught before this list
  was first written to `TODO-pdp11.md`, the original gcov 0%-executed
  set had 9 false positives total out of 27 initial candidates.

### Shrink `c-client/dummy.c`'s unreachable `DRIVER` slots; fix up `bezerk.c`/`mbox.c` fallout

- **What it was**: prompted by "is dummy.c's fallback logic needed, don't
  we just need mbox+bezerk?" The TODO's original "Proposed shape" --
  delete `dummy.c`/`dummy.h` entirely and fold its load-bearing pieces
  into `bezerk.c` -- turned out to be unsafe once traced fully:
  `c-client/mail.c`'s own generic `mail_valid()` hardcodes the literal
  string `"dummy"` to exclude the catch-all driver whenever its `nopen`
  flag is set, which is how `mail_create()`/`mail_delete()`/
  `mail_rename()` resolve their "does this mailbox already exist"
  check. If `bezerk` became the sole catch-all driver (matching
  nonexistent files too), every folder-create attempt would wrongly
  report "already exists" -- a real regression. A separate driver
  literally named "dummy" has to keep existing; the cut had to be
  scoped down to what's actually safe.
- **Reachability, traced precisely via every dispatch path** (not just
  "looks like a fallback"): `dummy_open()` unconditionally fails, so no
  `MAILSTREAM` can ever persist with `dtb == &dummydriver`, which rules
  out `fetchfast`/`fetchflags`/`fetchstructure`/`fetchheader`/
  `fetchtext`/`fetchbody`/`setflag`/`clearflag`/`search`/`ping`/
  `check`/`expunge`/`copy`/`move`/`gc` (`mail_copy`/`mail_move` dispatch
  via the *source* stream's own `dtb`, which is always bezerk in
  practice). Separately, `mail_create()`/`mail_delete()`/
  `mail_rename()`/`mail_subscribe()`/`mail_unsubscribe()`/
  `mail_subscribe_bboard()`/`mail_unsubscribe_bboard()` all call
  `mail_valid()` with `nopen` set, excluding "dummy" from ever matching
  -- so `create`/`delete`/`rename`/`subscribe`/`unsubscribe`/
  `subscribe_bboard`/`unsubscribe_bboard` are unreachable too (`mail_create`'s
  local-mailbox branch always resolves through `bezerk`'s own
  `mailstd_proto`/opened-INBOX stream, never dummy's). Third, pine's own
  bboard/newsgroup UI is already gone (see this file's NNTP/news-strip
  item), so nothing calls `mail_find_bboards()`/`mail_find_all_bboards()`
  -- `find_bboards`/`find_all_bboards` are unreachable on top of that.
  Total: 24 of `dummydriver`'s 33 slots provably unreachable. The
  remaining 9 (`valid`/`parameters`/`find`/`find_all`/`open`/`close`/
  `append`/`fetchstructure`... plus `dummy_file()`'s helper) are genuinely
  load-bearing: `bezerk_find`/`bezerk_find_all` literally call
  `dummy_find`/`dummy_find_all` (the only implementation of the
  directory-scanning wildcard folder search), and `dummy_open`/
  `dummy_append` produce the correct user-facing error text and the
  `[TRYCREATE] Must create mailbox before append` hint pine's
  `mm_notify()` (`mailbox.c:361-362`) depends on for its
  create-on-append flow.
- **What changed**: collapsed the 24 unreachable slots into 6 shared
  stub functions in `dummy.c`, grouped by exact matching signature (no
  unsafe casts) -- `dummy_dead_mbx` (6 slots, `long(MAILSTREAM*,char*)`,
  quiet `return NIL` matching all 6 originals), `dummy_dead_mbx3` (3
  slots, `long(MAILSTREAM*,char*,char*)`, `fatal()` -- two of the three
  originals already used `fatal()`, the third (`rename`) was a quiet
  `return NIL`; since all three are equally unreachable, standardized on
  the louder/more-defensive behavior), `dummy_dead_find` (5 slots,
  `void(MAILSTREAM*,char*)`), `dummy_dead_fetch` (2 slots,
  `char*(MAILSTREAM*,long)`), `dummy_dead_setflag` (2 slots,
  `void(MAILSTREAM*,char*,char*)`), `dummy_dead_stream` (2 slots,
  `void(MAILSTREAM*)`). `fetchstructure`/`fetchbody`/`ping`/`gc` stay as
  individual functions (unique signatures, nothing to consolidate with).
  Updated `dummydriver`'s initializer and `dummy.h`'s prototypes to
  match. `bezerk.c`'s own dead bboard-forwarding slots got the same
  treatment: `bezerk_find_bboards`/`bezerk_find_all_bboards` (which
  called the now-deleted `dummy_find_bboards`/`dummy_find_all_bboards`)
  collapsed into one `bezerk_dead_find` no-op, and
  `bezerk_subscribe_bboard`/`bezerk_unsubscribe_bboard` (already
  identical `return NIL` one-liners, unreachable since
  `mail_subscribe_bboard`/`mail_unsubscribe_bboard` build a
  `"*"`-prefixed candidate name `bezerk_isvalid()` never matches to a
  real file) collapsed into one `bezerk_dead_mbx`. `c-client/mbox.c` --
  compiled into `c-client.a` but never `mail_link()`'d by pine (see the
  separate open TODO item questioning whether it's dead weight) --
  reuses several `bezerk_*` functions directly in its own driver table
  and needed its 4 references updated to the new shared names to keep
  the archive building; `c-client/phile.c` has the same pattern but
  isn't in `makefile.lnx`'s build list at all, so it was left untouched.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward, 177 warnings (unchanged). Full
  smoketest suite 25/25 (unchanged -- no observable behavior difference
  for any currently-tested path). Additionally drove a real pty session
  (via `bwrap --bind <scratch-dir> ~/mail`, since `myhomedir()`
  in `c-client/os_lnx.c:195-198` resolves the real passwd-database home
  directory via `getpwuid()` rather than honoring smoketest.py's `HOME`
  env override -- a pre-existing test-isolation gap, tracked as a new
  TODO item rather than fixed here) exercising the exact path this cut
  revolves around end-to-end: Save a message to a brand-new folder name,
  confirm the "doesn't exist... Create?" prompt (driven by
  `dummy_open()`'s failure -> `dummy_append()`'s TRYCREATE hint ->
  `mm_notify()` -> `want_to()`), answer yes, and confirmed the folder is
  actually created via `context_create()`/`bezerk_create()` with the
  message correctly appended -- the highest-risk path in this whole
  cut, and one no existing smoketest covers.
- **Size delta**: `bin/pine`'s `text` 339607 -> 338175 (-1432 bytes),
  `data`/`bss` unchanged; `bin/pico` unaffected (c-client is linked into
  `bin/pine` only).

### `myhomedir()` now honors `$HOME`, fixing smoketest.py's isolation gap

- **What it was**: `c-client/os_lnx.c:195-198`'s `myhomedir()` resolved
  the real passwd-database home directory via `getpwuid(geteuid())`
  unconditionally, never checking `getenv("HOME")`. Every default-
  relative mailbox path (`dummy_file()`/`bezerk_file()`'s "any other
  name" case, used for the default Fcc/Save folders) was built from
  this, so `test/smoketest.py`'s `Session` setting `env["HOME"]` to a
  scratch tempdir for isolation had no effect on those paths -- writes
  silently landed in the real developer's actual `~/mail/` instead.
  Discovered manually while verifying the dummy.c/bezerk.c cut above
  (tracked as its own TODO item first, then fixed here per the user's
  choice between "bwrap every test run" and "fix myhomedir()" -- the
  latter was simpler, fixed the root cause directly, and is a
  reasonable general correctness improvement given this project's
  single-user, no-multi-tenancy target).
- **What changed**: `myhomedir()` now checks `getenv("HOME")` first
  (non-empty), falling back to `getpwuid(geteuid())->pw_dir` only if
  unset -- same caching behavior as before (`hdname` static), just an
  extra check ahead of the passwd lookup.
- **Fallout in `test/smoketest.py`**: fixing the isolation gap exposed
  that several tests (anything driving a real Send/Save) were silently
  depending on the real `~/mail/sent-mail`/`~/mail/saved-messages`
  already existing to avoid pine's "Folder ... doesn't exist. Create?"
  prompt -- once writes stopped leaking into that already-populated
  real directory, a fresh scratch `~/mail/` had neither file, and 2
  tests failed hitting that unexpected prompt. Fixed by having
  `Session.__init__` pre-create both as empty files alongside its
  existing `~/mail` directory pre-creation, mirroring
  `DF_DEFAULT_FCC`/`DEFAULT_SAVE` (`pine/os.h`).
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward, 177 warnings (unchanged, `+31`
  bytes from the added `getenv()` check). Full smoketest suite:
  regressed to 23/25 immediately after the `myhomedir()` fix alone
  (exactly the newly-exposed "doesn't exist" prompt above), back to
  25/25 after the `smoketest.py` pre-create fix. Manually re-verified
  the exact save-to-new-folder flow from the previous cut's write-up,
  this time *without* any `bwrap` involvement -- confirmed the folder
  now correctly lands under the scratch `$HOME/mail/`, and separately
  drove a real Forward+Send (mirroring
  `test_send_body_header_overflow.py`'s Fcc-only pattern) and confirmed
  via an exact byte-for-byte `md5sum` before/after comparison that the
  real `~/mail/sent-mail` was completely untouched while the
  scratch copy correctly received the forwarded message.
- **Size delta**: `bin/pine`'s `text` 338175 -> 338206 (+31 bytes,
  `getenv()` check), `data`/`bss` unchanged; `bin/pico` unaffected.

### `dummy` is no longer a registered `DRIVER` -- just a plain helper `mail.c`/`bezerk.c` call

- **What it was**: even after shrinking `dummydriver`'s slots down to 6
  shared stubs (the previous cut), dummy still had to exist as a
  second, distinctly-named `DRIVER` in `maildrivers`, because
  `c-client/mail.c`'s own generic `mail_valid()` hardcoded the literal
  string `"dummy"` to exclude the catch-all driver from
  `mail_create()`/`mail_delete()`/`mail_rename()`'s "does this mailbox
  already exist" check (its `nopen` flag). Raised as a design question
  ("dummy doesn't seem like it should be a driver, but a consumer of
  drivers") and refined into a concrete plan: stop `mail_link()`-ing
  `dummydriver` entirely, delete the now-pointless name-string
  exclusion, and add explicit direct calls to dummy's helper functions
  only where `mail.c` actually needs them.
- **What changed**:
  - `pine/pine.c`: dropped the `mail_link((DRIVER *)&dummydriver);`
    call and `dummydriver` from the `extern DRIVER` declaration --
    `maildrivers` now only ever contains `bezerkdriver`.
  - `c-client/mail.c`: `mail_valid()` lost its `nopen` parameter
    entirely (its only use was the now-deleted `"dummy"` name check)
    and all 10 call sites updated to match. `mail_open()`/
    `mail_append()` now call `dummy_open_failure(name)`/
    `dummy_append_failure(stream,mailbox)` directly when `mail_valid()`
    returns NIL, replicating the exact same suppression conditions the
    old driver-dispatch path had (`mail_open`'s silent-probe/
    `OP_SILENT` cases stayed silent before; still do). `mail_find()`/
    `mail_find_all()`'s no-stream loop now calls
    `dummy_find(NIL,pat)`/`dummy_find_all(NIL,pat)` explicitly after
    iterating `maildrivers`, since `dummy_find_all` used to be the
    *only* thing in that loop that actually did anything when `stream`
    was NIL (`bezerk_find_all`'s forwarding to `dummy_find_all` is
    itself gated on `stream` being non-NIL) -- removing dummy from the
    chain without this would have silently broken "search all
    mailboxes with no open stream".
  - `c-client/dummy.c`/`dummy.h`: rewritten down to 5 plain functions,
    no `DRIVER` struct at all. `dummy_find()`/`dummy_find_all()`/
    `dummy_file()` kept verbatim (still the only implementation of the
    wildcard directory-scan search). `dummy_open()` -> renamed
    `dummy_open_failure(char *name)`, dropping its `MAILSTREAM*`
    parameter (it only ever used `stream->mailbox`/`stream->silent`,
    both now handled by the caller) and its own internal silent-check
    (moved to `mail_open()`, since that's the only caller now).
    `dummy_append()` -> renamed `dummy_append_failure(MAILSTREAM
    *stream,char *mailbox)`, dropping the unused `message` parameter.
    Deleted entirely (no longer needed once there's no `DRIVER` struct
    to populate): `dummy_valid`, `dummy_parameters`, `dummy_close`, and
    all 6 of the previous cut's shared "unreachable" stubs
    (`dummy_dead_mbx`/`dummy_dead_mbx3`/`dummy_dead_find`/
    `dummy_dead_fetch`/`dummy_dead_setflag`/`dummy_dead_stream`) plus
    the 4 individually-kept-but-equally-unreachable ones
    (`dummy_fetchstructure`/`dummy_fetchbody`/`dummy_ping`/`dummy_gc`)
    -- all of these were only ever load-bearing as `DRIVER`-struct
    filler, never as actually-called code.
  - `c-client/mbox.c`: unaffected (it references `bezerk_*` names, not
    `dummy_*`, and doesn't touch `dummydriver`).
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward, 177 warnings (unchanged). Full
  smoketest suite 25/25 (unchanged). Manually re-verified, via a real
  pty session (no `bwrap` needed now that `myhomedir()` respects
  `$HOME`), all three code paths this change touches: (1) saving a
  message to a brand-new folder name -- confirmed the "doesn't exist...
  Create?" prompt (now driven by `dummy_open_failure()`'s TRYCREATE
  path) and successful creation via `bezerk_create()`; (2) "Goto"-ing a
  genuinely nonexistent folder -- confirmed `mail_open()`'s new fallback
  produces the correct `strerror()`-based error text; (3) the Folder
  List screen's wildcard search -- confirmed all three real folders
  (`sent-mail`, `saved-messages`, a freshly-created one) appear,
  exercising `mail_find_all()`'s new explicit no-stream fallback call.
- **Size delta**: `bin/pine`'s `text` 338206 -> 337286 (-920 bytes),
  `data` 12936 (-288 bytes; the removed `DRIVER dummydriver` struct
  literal itself), `bss` unchanged; `bin/pico` unaffected.

### `c-client/mbox.c` was already dead weight -- confirmed and dropped from the build

- **What it was**: `c-client/makefile.lnx` compiled `mbox.o` into
  `c-client.a`, but pine never `mail_link()`s `mboxdriver` -- only
  `bezerkdriver` is linked (`pine/pine.c`) -- and grepping `pine/*.c`
  for `mboxdriver`/`mbox_valid` found nothing outside `mbox.c` itself.
- **Verified**: `nm bin/pine | grep -i "mbox_\|mboxdriver"` produced
  zero matches (confirmed `nm` itself was working correctly by also
  checking for a known-present symbol, `bezerk_valid`, which showed up
  fine) -- proving the static archive's unreferenced `mbox.o` member
  was never actually pulled into the final link, exactly as expected.
- **What changed**: removed `mbox.o` from `c-client.a`'s prerequisite
  list and the `ar rc` command line in `c-client/makefile.lnx`, and
  deleted the now-orphaned `mbox.o: mail.h mbox.h bezerk.h misc.h
  osdep.h` dependency rule.
- **Follow-up**: with `mbox.o` fully out of the build, deleted the
  source files themselves (`git rm imap/ANSI/c-client/mbox.c mbox.h`)
  rather than leaving known-uncompiled dead source sitting in the
  tree. `makefile.lnx` (the only makefile this project's dev workflow
  actually builds) had zero remaining references. The other-platform
  makefiles (`makefile.osf`/`.sco`/`.bsi`/`.a32`/`.aix`/`.nxt`) still
  mention `mbox.o`/`mbox.h` -- left alone, matching this project's
  existing precedent for out-of-scope/unmaintained build targets (the
  DOS/Windows `pine.def` stray-entry precedent from the ISO-2022 cut).
- **Verified**: `./build clean && ./build lnx` clean both before and
  after (makefile change, then file deletion), byte-for-byte identical
  `bin/pine` size both times (`337286`/`12936`/`15000`, matching the
  previous cut exactly) confirming `mbox.o` truly contributed zero
  bytes. 177 warnings (unchanged). Full smoketest suite 25/25
  (unchanged).
- **Size delta**: none (confirms the object file was already
  contributing nothing; this was a build-hygiene/dead-source-removal
  fix, not a size win).

### Removed subscribe/unsubscribe entirely; `dummy_find()` collapsed into `dummy_find_all()`

- **What it was**: `bezerk_subscribe()`/`bezerk_unsubscribe()`
  (`bezerk.c`) are real, non-IMAP-specific implementations -- they
  read/write a per-user `~/.mailboxlist` subscription database via
  `sm_subscribe()`/`sm_unsubscribe()` (`c-client/sm_unix.c`). But
  nothing in `pine/*.c` calls `mail_subscribe()`/`mail_unsubscribe()`
  anywhere (`context_subscribe()`/`context_unsubscribe()` were already
  confirmed dead and removed earlier in this same driver-shrinking
  effort), and `mail_subscribe()`/`mail_unsubscribe()` themselves (the
  generic `c-client/mail.c` dispatchers) have zero callers anywhere in
  `pine/*.c` or `c-client/*.c` either -- the whole mechanism is
  unreachable end to end, not just one driver's implementation of it.
  Since `sm_subscribe()` can never be called, `~/.mailboxlist` can
  never be created by this build, which means `dummy_find()`'s
  subscription-database check (`sm_read()`) always takes its "no
  database" branch and falls straight through to `dummy_find_all()` --
  `dummy_find()` was already, in every real invocation, just an
  elaborate way of calling `dummy_find_all()`.
- **What changed**:
  - `c-client/mail.h`: removed the `subscribe`/`unsubscribe` fields
    from the `DRIVER` struct definition itself (not just stubbed --
    confirmed nothing else references them), removed
    `mail_subscribe`/`mail_unsubscribe`'s prototypes and `#define`
    shortenings, and `sm_subscribe`/`sm_unsubscribe`/`sm_read`'s
    prototypes. Left `subscribe_bboard`/`unsubscribe_bboard` alone --
    out of scope for this cut (already-stubbed dead weight from an
    earlier pass, not touched here).
  - `c-client/mail.c`: deleted `mail_subscribe()`/`mail_unsubscribe()`
    entirely.
  - `c-client/bezerk.c`/`bezerk.h`: deleted `bezerk_subscribe()`/
    `bezerk_unsubscribe()` and their two `bezerkdriver` struct slots
    (the struct literal just has two fewer positional entries now,
    matching the two fewer struct fields). `bezerk_find()` -- which
    used to forward to `dummy_find()` -- now calls `dummy_find_all()`
    directly.
  - `c-client/dummy.c`/`dummy.h`: deleted `dummy_find()` entirely.
    `mail.c`'s `mail_find()` no-stream fallback (added in the previous
    cut) now calls `dummy_find_all()` directly instead of
    `dummy_find()`. `dummy.c` is down to 4 plain helpers now
    (`dummy_find_all`/`dummy_open_failure`/`dummy_append_failure`/
    `dummy_file`).
  - `c-client/sm_unix.c`: deleted entirely (`git rm`) -- its only three
    functions (`sm_subscribe`/`sm_unsubscribe`/`sm_read`) were all
    removed, leaving nothing else in the file. Removed from
    `c-client/makefile.lnx`'s `c-client.a` target and its standalone
    dependency rule. `sm_unix.c` is referenced by six other-platform
    makefiles (`.sco`/`.a32`/`.bsi`/`.nxt`/`.aix`/`.osf`) -- left alone,
    same out-of-scope precedent as `mbox.c`/`pine.def`.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean after the code changes (177 warnings,
  unchanged) and again after deleting `sm_unix.c` (byte-for-byte
  identical size both times, confirming the file deletion itself
  changed nothing beyond what the code edits already had). Full
  smoketest suite 25/25 (unchanged -- no observable behavior
  difference, nothing reachable was touched).
- **Size delta**: `bin/pine`'s `text` 337286 -> 334703 (-2583 bytes),
  `data` 12936 -> 12904 (-32 bytes; the two struct-field removals
  worth of `DRIVER` and `sm_unix.o`'s own static data), `bss`
  unchanged; `bin/pico` unaffected.

### `read_pinerc()` simplified to a bootstrap-only stub -- deliberate functionality loss, user-confirmed

- **What it was**: gcov flagged `pine/init.c`'s `read_pinerc()` as
  low-coverage -- every smoketest-driven invocation only ever exercised
  its first branch (`read_file(filename) == NULL` -> set
  `first_time_user`/return `-1`), since `test/smoketest.py`'s `Session`
  always points `-p` at a fresh, nonexistent scratch `.pinerc`. Unlike
  every other item in this project's cut list, the rest of the
  function -- the full line-by-line `.pinerc` parser (comment lines,
  variable lookup, obsolete-variable handling, quote stripping,
  comma-separated list parsing via `parse_list()`, storage into
  `v->user_val`/`v->global_val`) -- is real, working code with a real
  purpose; it just happens that nothing in this project's test suite
  exercises it. **This is a deliberate, user-confirmed capability cut,
  not proven-dead-code removal**: the user explicitly directed starting
  this item after being told plainly it drops real functionality.
- **What changed**: first collapsed `read_pinerc()` to just its
  original bootstrap branch (open the file; if it exists, `fs_give()`
  its contents back unread and return `0`; if it doesn't, set
  `first_time_user`/return `-1`, identical to before) -- then
  simplified further by hand to an unconditional `return(0);` with no
  body at all, dropping the file-open/existence-check entirely. Since
  `first_time_user` had no other setter anywhere in the tree (grepped
  to confirm), it's now permanently `NIL` -- the "Welcome to Pine"
  first-run screen (`pine.c`, gated on `first_time_user ||
  show_new_version`) can now only trigger via `show_new_version`.
  Deleted the now-fully-dead `static struct pinerc_line { ... }
  *pinerc_lines` file-scope declaration and type -- confirmed via grep
  it had no consumer anywhere outside `read_pinerc()` itself (no
  "write .pinerc back out" feature exists anywhere in this fork to
  have depended on it, despite the struct's shape suggesting that was
  its original purpose upstream). Left `parse_list()` alone --
  confirmed it has a real second caller (`pine/args.c:217`), so only
  its `read_pinerc()` call site went away, not the function itself.
- **Observable behavior change** (accepted, not a bug): a hand- or
  Setup-written `.pinerc`/system-wide pinerc is no longer opened or
  parsed at all. Every variable -- mail directory, feature list,
  signature file, personal name, everything -- silently stays at its
  compiled-in default on every run, for both `ParseGlobal` and
  `ParseLocal`. Manually verified end-to-end: wrote a real `.pinerc`
  setting `personal-name` to a distinctive value, sent a message via
  the Fcc-only path (mirroring `test_send_body_header_overflow.py`'s
  pattern), and confirmed the custom name never reaches the outgoing
  `From:` header -- it stays at the compiled-in default derived from
  the system username/hostname.
  `test/smoketest.py`'s `Session` gained a new `pinerc=<content>`
  parameter (mirroring the existing `postponed_mail=` pattern) to seed
  a real `.pinerc` before pine starts, for this and any future test
  needing one.
- **New regression test**: `test/cases/test_pinerc_not_parsed.py` --
  locks in the behavior change (asserts the custom `personal-name`
  never appears in the sent message), the inverse of what a working
  `.pinerc` parser would produce.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean after both the initial two-branch version
  and the further hand-simplified unconditional-`return(0)` version,
  177 warnings (unchanged) each time. Full smoketest suite 26/26 (25
  existing + the new test), reconfirmed after the final simplification.
- **Size delta**: `bin/pine`'s `text` 334703 -> 332455 -> 332159
  (-2544 bytes total: -2248 from the initial bootstrap-only collapse,
  another -296 from dropping the file-existence check entirely), `bss`
  15000 -> 14968 (-32 bytes, the removed `pinerc_lines` static
  pointer), `data` unchanged; `bin/pico` unaffected.
