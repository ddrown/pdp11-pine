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
