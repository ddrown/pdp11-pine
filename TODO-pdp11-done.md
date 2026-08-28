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

### 10 K&R-style function definitions converted to ANSI, fully clearing 4 `pico/` files

- **What it was**: opportunistic pull from the standing "eliminate
  compiler warnings" goal, per the user's direct request to fix a
  batch of 10. Picked all `-Wold-style-definition` warnings from 4
  small files so each one would end up fully cleared rather than
  partially touched: `pico/window.c` (`refresh`, 1), `pico/bind.c`
  (`wscrollw`/`normal`/`rebindfunc`, 3), `pico/region.c`
  (`getregion`/`markregion`, 2), `pico/buffer.c`
  (`zotbuf`/`bclear`/`packbuf`/`sgetline`, 4).
- **What changed**: each function's K&R-style definition (params
  declared below the signature line, e.g. `foo(a, b)\n  int a, b;\n{`)
  converted to ANSI style (`foo(int a, int b)\n{`), preserving
  `register` qualifiers and any trailing same-line comments (e.g.
  `zotbuf`'s `/* kill the buffer pointed to by bp */`). Pure
  signature-style rewrite, no logic changes.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward -- `bin/pine`/`bin/pico` both
  byte-for-byte identical in size (expected: ANSI vs. K&R style
  produces identical generated code), confirming this was purely
  cosmetic. Warning count 177 -> 167, and diffed the full warning list
  before/after to confirm exactly the 10 targeted
  `-Wold-style-definition` warnings disappeared and nothing new
  appeared. Full smoketest suite 26/26 (unchanged -- no observable
  behavior difference, pure style fix).
- **Size delta**: none (`bin/pine`/`bin/pico` both byte-identical).

### 20 more K&R-style function definitions converted to ANSI, fully clearing 6 more files

- **What it was**: continuation of the opportunistic warning-cleanup
  pull, per the user's direct request to fix another batch of 20.
  Picked all remaining `-Wold-style-definition` warnings from
  `pico/word.c` (`fillpara`, 1), `pico/tcap.c` (`tcapinsert`/
  `o_insert`/`tcaprev`, 3), `pico/search.c` (`eq`/`readpattern`/
  `forscan`/`expandp`, 4), `pico/fileio.c` (`ffropen`, 1),
  `pine/osdep/includer.c` (`readfile`, 1), and all 10 in `pico/pico.c`
  (`pico`/`edinit`/`execute`/`quickexit`/`abort_composer`/
  `suspend_composer`/`wquit`/`ctrlg`/`pico_help`/`zottree`) -- fully
  clearing all 6 files' `-Wold-style-definition` warnings (`pico.c`
  still has one unrelated `-Wint-to-pointer-cast` warning left, out of
  scope for this pass per the TODO's own risk note).
- **What changed**: each function's K&R-style definition converted to
  ANSI style, preserving `register` qualifiers and relocating
  trailing/inline comments to sensible spots (e.g. `forscan`'s and
  `expandp`'s multi-line per-parameter comments folded into an
  ANSI-style multi-line parameter list; `tcaprev`'s two comments split
  between a preceding doc-comment and a trailing one). Noticed and
  left alone (out of scope, pre-existing, and inert): `pico/efunc.h`
  has an `#ifdef ANSI ... #else ... #endif` split with a correct
  ANSI-style prototype in the first branch and a stale K&R-style
  fallback in the second (e.g. `o_insert`, `rebindfunc`) -- since this
  build always defines `ANSI` (`-DANSI` in `pico/makefile.lnx`), the
  stale branch is never actually compiled; clangd's "conflicting
  types" diagnostic on those functions is a static-analysis artifact
  from evaluating both `#ifdef` branches, not a real issue.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward -- `bin/pine` byte-identical;
  `bin/pico`'s `text` shrank slightly (110098 -> 110076, -22 bytes,
  presumably minor code-gen/register-allocation differences from the
  signature rewrites -- not byte-identical this time, unlike the
  previous 10-warning batch, but not a concern for what's still a pure
  signature-style change). Warning count 167 -> 147, diffed the full
  warning list before/after to confirm exactly 20
  `-Wold-style-definition` warnings disappeared and no other category
  changed. Full smoketest suite 26/26 (unchanged -- no observable
  behavior difference).
- **Size delta**: `bin/pico`'s `text` 110098 -> 110076 (-22 bytes),
  `data`/`bss` unchanged; `bin/pine` byte-identical.

### 40 more K&R-style function definitions converted to ANSI, fully clearing 4 more files

- **What it was**: continuation of the opportunistic warning-cleanup
  pull, per the user's direct request to fix another batch of 40.
  Picked all remaining `-Wold-style-definition` warnings from
  `pico/random.c` (5), `pico/file.c` (8), `pico/basic.c` (9),
  `pico/line.c` (9), and 9 of `pico/compose_stub.c`'s 10 -- fully
  clearing the first four files and leaving exactly one function
  (`compose_buildaddr`) in `compose_stub.c` for a future batch.
- **What changed**: each function's K&R-style definition converted to
  ANSI style, preserving `register` qualifiers and relocating inline
  comments. A few opportunistic extras beyond the exact 40 gcov-flagged
  ones, since they were sitting immediately adjacent to functions
  already being touched: `pico/line.c`'s `kdelete(void)` (empty-parens
  K&R form, not itself warning-flagged by gcc but cleaned up while
  converting its neighbors `kinsert`/`kremove`). Noted in passing:
  `pico/compose_stub.c` (new in this session, part of the composer-as-
  separate-process work referenced in `TODO-pdp11-done.md`) confirms
  that architecture is already implemented, not just planned.
- **Verified**: `./build lnx` clean before touching anything. `./build
  clean && ./build lnx` clean afterward -- `bin/pine` byte-identical;
  `bin/pico`'s `text` shrank further (110076 -> 110006, -70 bytes).
  Warning count 147 -> 107, diffed the full warning list before/after
  to confirm exactly 40 `-Wold-style-definition` warnings disappeared
  and no other category changed. Full smoketest suite 26/26 (unchanged
  -- `test_composer_resize_grow`/`test_composer_wide_terminal`/
  `test_compose_cancel`/`test_pico_standalone_longpath` all exercise
  the composer-subprocess path this batch touched most, giving good
  confidence beyond just the warning-count diff).
- **Size delta**: `bin/pico`'s `text` 110076 -> 110006 (-70 bytes),
  `data`/`bss` unchanged; `bin/pine` byte-identical.

### 80 more K&R-style function definitions converted to ANSI, fully clearing 5 more files

- **What it was**: continuation of the opportunistic warning-cleanup
  pull, per the user's direct request to fix another batch of 80.
  Picked all remaining `-Wold-style-definition` warnings from
  `pico/compose_stub.c` (1, the last function left from the previous
  batch -- `compose_buildaddr`), `pico/browse.c` (12), `pico/os_unix.c`
  (18, the real source behind the generated `pico/osdep.c` -- see
  CLAUDE.md's generated-files rule), `pico/composer.c` (26), and 23 of
  `pico/display.c`'s 26 -- fully clearing the first four files and
  leaving exactly 3 functions in `display.c` for a future batch.
- **What changed**: each function's K&R-style definition converted to
  ANSI style, preserving `register` qualifiers and relocating inline
  comments (multi-parameter functions with a trailing per-parameter
  comment got the comment moved to trail the whole signature line, one
  comment per parameter on its own continuation line, e.g.
  `updateline`/`FormatLines`/`PaintHeader`/`pputc`/`pputs`). One real
  bug surfaced and fixed in passing: `pico/os_unix.c`'s `fallowc` was
  K&R-defined with a `char c` parameter, but both its `efunc.h`
  prototypes declare `int fallowc(int)` -- under K&R rules the `char`
  argument is silently promoted to `int` at the call site so this never
  broke anything, but converting the definition straight to
  `fallowc(char c)` made it a strict ANSI prototype and gcc correctly
  rejected the mismatch (`conflicting types for 'fallowc'`); fixed by
  matching the definition's parameter type to the header's `int`
  instead. `browse.c`'s `PaintCell` needed matching parameters by name
  rather than K&R declaration order (the K&R block declared
  `struct fcell *cell;` before `int x, y, l, inverted;`, despite `cell`
  being 4th in the parameter list). Left 3 `-Wint-to-pointer-cast`
  warnings in `browse.c` (lines 433/1040/1042) and 2 in `composer.c`
  untouched -- different category, out of scope per
  `TODO-pdp11.md`'s own caution against batching cast warnings in with
  style-only K&R cleanups.
- **Verified**: `./build clean && ./build lnx` clean before and after.
  Warning count 107 -> 27, diffed the full categorized warning list
  before/after to confirm exactly 80 `-Wold-style-definition` warnings
  disappeared (83 -> 3) and no other category changed (12x
  `-Wint-to-pointer-cast`, 10x `-Wpointer-to-int-cast`, 2x
  `-Wincompatible-pointer-types` all unchanged). Full smoketest suite
  26/26.
- **Size delta**: `bin/pico`'s `text` 110006 -> 109994 (-12 bytes),
  `data` 6364/`bss` 10624 unchanged from prior measurement; `bin/pine`
  untouched by this batch (all 5 files are under `pico/`).

### Remaining 27 warnings cleared -- `./build clean && ./build lnx` is now 0 warnings

- **What it was**: per the user's direct request to finish off the
  warning-cleanup goal entirely ("do the last 27"), rather than
  stopping at the mechanical K&R/ANSI conversions. Cleared the last 3
  `-Wold-style-definition` warnings (`pico/display.c`'s `pinsert`,
  `wstripe`, `wkeyhelp` -- same mechanical conversion as every prior
  batch), then, unlike those prior batches, went through each of the
  remaining 22 `-Wint-to-pointer-cast`/`-Wpointer-to-int-cast` warnings
  and both `-Wincompatible-pointer-types` ones individually, per
  `TODO-pdp11.md`'s own explicit caution not to blanket-suppress these.
- **The `void *arg`-as-scalar family (16 warnings)**: `pico/display.c`'s
  `emlwrite()` and `pine/screen.c`'s `PutLine1()` both take a `void
  *arg` parameter that's really printf-style scalar data (an `int` or
  `long`) smuggled through a pointer-typed slot -- a 32-bit-era idiom
  that assumed `sizeof(void*) == sizeof(int)`. Every call site
  (`(void *)someint`) and every internal cast back
  (`(int)arg`/`(char)(int)arg`) direct-cast between `void *` and a
  narrower integer type, which gcc flags on this 64-bit dev host (LP64:
  pointers are 8 bytes, `int` is 4). Fixed by routing every such cast
  through an explicit `long` (this platform's pointer-width integer
  type, and already the pattern the code used for its one genuinely
  `long`-sized case, `%D`): `(void *)(long)intval` at call sites,
  `(int)(long)arg`/`(char)(long)arg` inside the callees. Purely a
  widening/narrowing-explicitness fix -- the actual values passed
  (loop counters, key codes masked to `& 0xff`, a `timeout` int) always
  fit in `int` at every call site, confirmed by reading each one
  (`pico/line.c:96`, `pico/pico.c:389`, `pico/main.c:225`,
  `pico/browse.c:432,1039,1041`, `pico/composer.c:627,731`,
  `pine/screen.c:861,913`, plus `pico/display.c`'s 7 internal
  `emlwrite()` casts). No behavior change on either this dev host or
  the eventual PDP-11 target (where `int`/`long`/pointer widths differ
  from here, but the explicit `long` step still round-trips correctly
  either way).
- **`pico/word.c:482`**: `dotflag = (int)strchr(...)` truncated a
  `char *` down to `int` just to use it as a boolean a few lines later
  (`if(dotflag && ...)`); nothing ever read it as a real pointer or
  address value. Changed to `dotflag = (strchr(...) != NULL)` -- an
  actual boolean instead of a truncated, meaningless integer, which is
  both correct and warning-free (no cast at all).
- **`imap/ANSI/c-client/misc.c:184`**: `search()`'s fast-path alignment
  check did `(int) s & 3` to find how many bytes `s` is short of a
  4-byte word boundary. Only the low 2 bits matter, which truncation to
  any narrower integer type preserves regardless -- so this was never
  an actual correctness bug, just an avoidable warning. Changed the
  cast to `(long) s & 3` to match pointer width and silence it, with no
  change in the computed value.
- **`pine/signals.c:153`**: `end_signals()` checked `(int)signal(SIGILL,
  SIG_DFL) < 0` to detect `signal(2)` failure -- casting the returned
  function pointer to `int` and comparing against 0 relies on `SIG_ERR`
  (`(void(*)(int))-1`) truncating to a negative `int`, which happens to
  hold on this platform but is exactly the kind of pointer/int-width
  assumption this whole audit exists to catch. Replaced with the
  portable, direct comparison `signal(SIGILL, SIG_DFL) == SIG_ERR`,
  which needs no cast and doesn't depend on `SIG_ERR`'s bit pattern.
- **`pine/send.c:619,1271` -- real bug, not just a style warning**: both
  sites conditionally call `wait((union wait *)&status)` under
  `#ifdef HAVE_WAIT_UNION`, which `pine/osdep/os-lnx.h:262` had
  defined. Checked this dev host's actual `<sys/wait.h>`: glibc's
  `wait()` takes `int *`, and `union wait` isn't even declared in
  glibc's headers anymore -- `HAVE_WAIT_UNION` was simply wrong for
  this target (per `TODO-pdp11.md`'s own note, "possibly a leftover
  from an older BSD `wait()` signature" -- confirmed). Disabled the
  `#define` in `os-lnx.h` (commented out, matching every other
  `os-*.h` platform header's convention for a disabled option, with a
  note explaining why), which routes both call sites to the `#else`
  branch's plain `wait(&status)` -- correct for this target and the
  reason the warning existed in the first place. `os-lnx.h` is a real
  tracked per-platform header (unlike the gitignored, build-generated
  `os-lnx.c` covered by CLAUDE.md's generated-files rule), so this was
  a direct, appropriate edit.
- **Verified**: `./build clean && ./build lnx` clean before and after
  every individual fix (rebuilt the specific `.o` after each site to
  confirm that warning's disappearance with accurate line numbers,
  same discipline as the K&R batches). Final full clean rebuild: **0
  warnings** (107 -> 27 from the prior batch's mechanical pass -> 0
  here). Full smoketest suite 26/26 throughout, including after the
  `wait()` fix (composer-subprocess tests --
  `test_compose_cancel`/`test_composer_resize_grow`/
  `test_composer_wide_terminal`/`test_pico_standalone_longpath` --
  exercise exactly the fork/exec/wait path that fix touches).
- **Size delta**: `bin/pico`'s `text` 109994 -> 110002 (+8 bytes, the
  extra `(long)` intermediate casts and `search()`'s widened
  alignment-mask expression add a handful of instructions); `bin/pine`
  876728 -> 876704 (-24 bytes, net effect of the `wait()`/`signal()`
  simplifications outweighing the `(long)` casts added elsewhere in
  `pine/`).

### Removed the `DRIVER` dispatch abstraction -- `bezerkdriver` was the only driver, now everything calls it directly

- **What it was**: `imap/ANSI/c-client/mail.h` defined `DRIVER`
  (`struct driver`), a ~29-slot function-pointer dispatch table that
  historically let multiple pluggable mailbox-format backends
  (imap/pop/nntp/mh/mbox/bezerk/...) register into a linked list
  `maildrivers` and be called polymorphically through
  `MAILSTREAM->dtb`. In this build exactly one driver was ever
  registered -- `bezerkdriver` (`imap/ANSI/c-client/bezerk.c`), linked
  once via `mail_link((DRIVER *)&bezerkdriver)` in `pine/pine.c`. Every
  `stream->dtb->foo(...)`/`factory->foo(...)`/`maildrivers`-walk in
  `imap/ANSI/c-client/mail.c` (~24 functions, ~27 raw call sites) was
  therefore single-implementation indirection, converted to direct
  `bezerk_foo(...)` calls.
- **Planning discipline**: this item was explicitly flagged in
  `TODO-pdp11.md` as needing "its own careful design/verification
  pass, not a quick follow-on" given its size (4 files, core c-client
  dispatch plumbing every mail operation flows through). Scoped with
  an Explore agent (full-tree sweep for any DRIVER/dtb reference not
  already found) and a Plan agent (worked out every exact rewrite),
  then independently re-verified by direct reading before executing
  -- including tracing `bezerkproto`'s real load-bearing callers
  (`default_driver()`/`create_proto()`, the Folder List "Add" and
  Fcc-to-new-folder paths) to make sure its positional-initializer
  rewrite couldn't silently break real folder creation.
- **The `dtb`-as-liveness-flag problem**: `bezerk.c` used `stream->dtb`
  for double duty -- dispatch pointer *and* an "is this stream's local
  data live" boolean (`bezerk_abort()` nilled it on teardown;
  `bezerk_open()`'s recycle path and `mail_open()` set it back to
  `&bezerkdriver`). Replaced with a real `unsigned int open : 1;`
  bitfield on `MAILSTREAM` (`mail.h`), set/cleared at exactly those
  same sites, and read wherever `dtb` was tested for liveness
  (`mail_free_handle`, `mail_stream`) or used to gate a dispatch call
  (all the mechanical conversions below).
- **`mail_valid`** retyped from `DRIVER *` to `long` (`T`/`NIL` --
  matches this codebase's own boolean idiom, and is all any caller
  ever used the return value for). The old `stream->dtb != factory`
  match check degenerates, with one driver, to exactly `!stream->open`
  (a stream can only differ from "the" factory by not having one open
  at all).
- **`mail_open` rework** surfaced one real, already-existing dead-code
  path, deleted rather than translated: the recycle branch's
  `if (stream->dtb) (*stream->dtb->close) (stream);` was unreachable
  even before this change -- inside `if (stream->dtb != factory)`,
  `stream->dtb` can only be NIL (no second driver exists for it to
  differ *to*), so the guarded call could never fire. Confirmed by
  direct reading, not just inferred.
- **Outright deletions** (zero remaining callers, confirmed via
  tree-wide grep before removal): `mail_link`, `mail_valid_net`,
  `mail_valid_net_parse` (dead beyond their one remaining caller,
  `pine/pine.c`'s `same_stream()`, itself simplified to always
  `return NIL` with an explanatory comment -- this build has no
  remote/TCP-IP mailbox support, so `stream->mailbox` can never hold a
  `"{host}mailbox"`-syntax name for it to match against; all 4 real
  callers already treat a NIL return as the normal "open a fresh
  stream" case), `mail_find_bboards`/`mail_find_all_bboard`/
  `mail_subscribe_bboard`/`mail_unsubscribe_bboard` (their only
  dispatch targets, `bezerk_dead_find`/`bezerk_dead_mbx`, were
  themselves already-documented unreachable stubs left over from the
  earlier bboard/newsgroup-UI removal), the `maildrivers`/
  `inboxdriver` globals (the latter had no reader anywhere in the
  tree, dead before this cut even started), and the `DRIVER`/
  `struct driver` type itself.
- **Out of scope, confirmed dead/unbuilt, left alone**:
  `imap/ANSI/c-client/phile.c` and `contrib/carmel/carmel.c`/`pod.c`
  each define their own `DRIVER` but aren't referenced by
  `makefile.lnx` or `./build` at all -- same orphaned-legacy-code
  precedent as the already-deleted `mbox.c`.
- **Verified**: `./build clean && ./build lnx` clean throughout (0
  warnings before and after). Full smoketest suite 26/26. `nm bin/pine
  | grep -i "bezerkdriver\|maildrivers\|inboxdriver\|mail_link\|mail_valid_net"`
  returns nothing -- confirmed fully gone from the binary. Manually
  re-verified (real pty session, not just smoketests) the two paths
  that route through `bezerkproto`: creating a new folder via the
  Folder List "Add" screen -- confirmed the file actually landed on
  disk (`mail/verifyme`, not just a UI success message) -- and traced
  that Fcc-to-a-new-folder's `context_isambig()` branch uses the same
  `fcc_cntxt->proto`/`bezerkproto`/`mail_create()` machinery, already
  covered by the existing `test_fcc_tilde_overflow`/
  `test_fcc_netmbx_overflow`/`test_resume_postponed_*` smoketests.
- **Size delta**: `bin/pine` `text` 332159 -> 329415 (-2744 bytes),
  `data` 12904 -> 12648 (-256 bytes), `bss` 14968 -> 14952 (-16 bytes);
  `bin/pico` untouched (this cut is entirely in `imap/ANSI/c-client/`
  and `pine/pine.c`).

### Removed `read_pinerc()` and `first_time_user` entirely

- **What it was**: follow-on to the earlier `read_pinerc()`
  bootstrap-only-stub cut. `read_pinerc()` (`pine/init.c`) had already
  been reduced to an unconditional `return(0);` with an empty body --
  this cut deleted the function itself, its 3 now-pointless call
  sites, and the `first_time_user` flag that had no setter left
  anywhere in the tree once `read_pinerc()`'s original bootstrap
  version (the only thing that ever set it) was gone.
- **What changed**: in `pine/init.c`, deleted `read_pinerc()`'s ANSI
  and K&R prototypes, the now-pointless `typedef enum {ParseLocal,
  ParseGlobal} ParsePinerc` (confirmed via grep it had no other
  consumer), the function definition itself, and its 3 call sites --
  the `if(ps_global->pine_conf) read_pinerc(...) else
  read_pinerc(...)` pair had no side effect beyond the no-op call, so
  the whole `if`/`else` was deleted outright; the third call
  (`read_pinerc(ps_global->pinerc, vars, ParseLocal)`) was deleted but
  the `if(!ps_global->pinerc){ build_path(...); ps_global->pinerc =
  cpystr(buf); }` block just above it was kept -- `ps_global->pinerc`
  is still read elsewhere (`imap.c`'s `passfile()` calls, `reply.c`'s
  signature-path derivation), so that assignment is real, live logic,
  not part of the dead read_pinerc plumbing. In `pine/pine.c`,
  simplified both `first_time_user`-checking conditions to just
  `show_new_version` and folded the now-dead
  `if(pine_state->first_time_user) ... else ...` branch straight down
  to its `else` (`new_user_or_version(new_version_message)`).
  Deleted `struct pine`'s `unsigned first_time_user:1` field
  (`pine.h`). One opportunistic extra found and cut in passing: once
  the `if(pine_state->first_time_user)` branch was gone, its
  `first_time_message` static string array (`pine.c`, the ~40-line
  "Welcome to Pine..." first-run banner) had no remaining reference
  anywhere -- confirmed via grep and deleted.
- **Verified**: `./build clean && ./build lnx` clean before touching
  anything (180 warnings, the accepted baseline after a separate,
  concurrent session's Tier-1-warning-flags rollout landed on `main`
  mid-session -- confirmed via `git log` and a `git stash`
  before/after rebuild that this baseline was pre-existing, not
  something this cut introduced). Clean afterward, still exactly 180
  warnings -- diffed the full categorized warning list line-by-line
  before/after and confirmed every difference was a pure line-number
  shift from the deleted lines, no new or removed warning categories.
  Full smoketest suite 26/26, including `test_pinerc_not_parsed`
  (still passes unchanged -- deleting the call sites preserves the
  exact same "never parses .pinerc" behavior the earlier stub cut
  established, just with the dead plumbing removed instead of stubbed).
- **Size delta**: `bin/pine` `text` 329551 -> 328303 (-1248 bytes),
  `data` 12656 -> 12496 (-160 bytes), `bss` unchanged; `bin/pico`
  confirmed byte-identical before/after (untouched by this cut --
  entirely `pine/`).

### Audited `-Wunused-parameter`/`-Wunused-function` -- one real fix, `-Wunused-function` now permanently enabled

- **What it was**: TODO-pdp11.md's own item cautioned not to
  blanket-enable either flag without auditing first, since this is a
  callback-heavy, function-pointer-dispatch codebase where most
  unused-parameter hits are expected, not bugs. Did the audit: trial
  runs via a real `./build clean && ./build lnx` (not per-file
  standalone `-fsyntax-only` compiles, which inflate counts with
  cross-translation-unit artifacts -- see below) with both flags
  temporarily added to `WARNFLAGS`.
- **`-Wunused-function` finding -- one real, fixable issue, not dead
  code**: all 38 hits (19 files x 2 functions) were the exact same
  warning -- `pico/efunc.h:117-118`/`:330-331` forward-declared
  `next_line()`/`prev_line()` as `static` in the *shared* header, so
  every `.c` file that includes it (all of them) sees a static
  prototype it never defines. `static` linkage means no file other
  than `composer.c` (where both are actually defined, at
  `composer.c:1455`/`:1483`, and used extensively above their
  definition point) could ever call them regardless -- putting them in
  the shared header served no purpose. Not a dead-code case (both
  functions are real, live, heavily used within `composer.c`) --
  fixed by moving their forward declarations out of `efunc.h`
  entirely and into `composer.c`'s own local ANSI/K&R prototype block
  (which already forward-declares the rest of that file's functions
  the same way), marked `static` there instead. Confirmed via grep
  these were the *only* two `static` declarations anywhere in
  `efunc.h` -- every other entry is a normal cross-file `extern`
  prototype, meaning this was a one-off oversight, not an established
  pattern to sweep further.
- **`-Wunused-parameter` finding -- confirmed noise, left disabled**:
  91 hits in the real build, sampled across `pico/basic.c`,
  `bind.c`, `random.c`, `region.c`, `window.c`, `compose_stub.c`, etc.
  -- overwhelmingly dominated by pico's uniform `int funcname(int f,
  int n)` key-bound-command signature (`f`/`n`, the "flag" and
  "numeric repeat count" arguments every editor command receives
  whether it uses them or not) plus `compose_stub.c`'s
  composer-as-subprocess `PICO` struct callback stubs -- both
  confirmed genuine function-pointer-dispatch requirements, not bugs.
  Per CLAUDE.md's standing guidance against cosmetic
  backwards-compatibility-style changes, did not rename ~90
  parameters purely to silence a linter with no bug-catching value
  here. Left `-Wunused-parameter` out of both makefiles' `CFLAGS`,
  matching the TODO item's own predicted conclusion.
- **Verified**: `./build clean && ./build lnx` clean before and after,
  with `-Wunused-function` now a permanent part of both
  `pine/makefile.lnx`'s and `pico/makefile.lnx`'s flag list (0
  warnings from it going forward -- a free standing dead-function
  tripwire for future cuts) and `-Wunused-parameter` confirmed *not*
  added to either. Warning count unchanged at 180 (the current
  Tier-1-flags baseline) before and after the `composer.c`/`efunc.h`
  fix. Full smoketest suite 26/26.
- **Size delta**: none -- purely a declaration relocation, no codegen
  change; `bin/pine`/`bin/pico` confirmed byte-identical before and
  after.

### Removed the dead `#ifdef ANSI`/K&R fallback branches entirely

- **What it was**: `ANSI` is unconditionally defined for this build
  (`-DANSI` in `pico/makefile.lnx`'s `CFLAGS`; `pine/osdep/os-lnx.h`'s
  `#define ANSI`, reached via `os.h` -> `headers.h` for everything
  under `pine/`), so every `#else`/K&R-style branch behind an
  `#ifdef ANSI` (or the one `#define const` behind an `#ifndef ANSI`)
  had never once compiled in this build. Removed all of it: 33 files
  (`pico/*.c`/`*.h`, `pine/*.c`/`*.h`), 34 blocks, ~670 dead lines --
  for each, kept the `#ifdef ANSI` branch's declarations
  unconditionally and deleted the wrapper plus the whole dead `#else`
  branch.
- **What changed**: mechanical for the ~31 small/medium files (one
  `Edit` per block, dropping the `#ifdef`/`#else`/`#endif` triplet).
  Nested, unrelated conditionals inside a handful of blocks (e.g.
  `pico/os_unix.h`'s `TIOCGWINSZ` guard around `winch_handler`,
  `pine/mailindx.c`'s `DOS`-only cache-hit declarations, `pine.h`'s
  `DEBUG`/`DOS` guards scattered through its block) were left in place
  untouched, only the outer `ANSI` wrapper came out -- verified each
  one individually rather than blindly deleting by line range, using a
  small Python depth-tracking script to find the *true* matching
  `#else`/`#endif` for the two big multi-hundred-line blocks
  (`pine/pine.h`'s actual K&R branch ran lines 1085-1381, not the
  nested `#ifdef DEBUG`'s `#endif` at line 1154 that a naive first
  grep suggested).
  Two files' `#else` branches turned out to be quietly out of sync
  with their `#ifdef ANSI` twin (further confirming both were long-
  abandoned dead code, not just harmlessly redundant): `pico/efunc.h`'s
  K&R branch was missing `getccol`, all of `tcap.c`'s two entries, and
  `unmarkbuffer` outright -- functions the ANSI branch had gained over
  time with no one keeping the K&R mirror in sync; `pine/mailview.c`'s
  local K&R block had *3 extra* stale declarations
  (`scroll_scroll_text`, `search_scroll_text`, `type_desc`) not in its
  own ANSI branch, traced and confirmed harmless to drop since all
  three are real, still-used functions already properly prototyped via
  the shared `pine.h` (kept). Neither discrepancy blocked anything --
  the K&R side was being deleted wholesale regardless -- but each was
  individually verified rather than assumed to be a clean mirror,
  per the TODO's own caution.
- **Side effect**: this was also the exact, previously-identified
  source of the clangd "Conflicting types for X" false positives seen
  repeatedly throughout this session's K&R-to-ANSI conversion work
  (`rebindfunc`, `o_insert`, `lfree`, `lchange`, `insspace`, `kdelete`,
  `next_line`, `prev_line`, etc.) -- clangd's editor-only analysis
  evaluated both `#ifdef` branches even though GCC only ever compiled
  the ANSI one. With the K&R branches gone, there's nothing left for
  clangd to see two conflicting declarations of.
- **Verified**: `./build clean && ./build lnx` clean throughout,
  after the first batch of ~26 small files, again after
  `pico/efunc.h`, and again after `pine/pine.h` (the two large/riskier
  files done last and separately, per the TODO's own recommendation).
  Warning count unchanged at 141 the whole way through (expected --
  the K&R branches were never compiled, so this is a pure source/text
  cut, not a warning cut). Full smoketest suite 26/26 after the full
  sweep. Final `grep -rl` for `#ifdef ANSI`/`#ifndef ANSI` across
  `pico/`, `pine/`, and `imap/` (excluding the gitignored, build-
  generated `osdep.c`/`osdep.h` copies) returns nothing.
- **Size delta**: none -- `bin/pine`/`bin/pico` confirmed byte-
  identical before and after, exactly as predicted (the deleted text
  was never compiled in the first place, so removing it can't change
  the binary). The win here is source maintainability (670 fewer dead
  lines to scroll past) and the clangd false-positive cleanup above,
  not size.

### Tier 1 warning flags enabled and fully cleared: 196 warnings -> 0

- **What it was**: TODO-pdp11.md's "Turn on Tier 1 warning flags"
  item -- add `-Wformat=2`, `-Wstringop-overflow=4`,
  `-Wstringop-truncation`, `-Wrestrict`, `-Wreturn-type`,
  `-Wdangling-else`, `-Wmisleading-indentation`, `-Wsequence-point`,
  `-Wtype-limits`, `-Waddress`, `-Wnonnull` to both `makefile.lnx`
  `CFLAGS`, then work the resulting warning list to 0. Landing this on
  top of a clean 0-warning baseline surfaced 196 warnings; worked
  through them in four batches (small high-signal warnings first,
  then format-security/extra-args, then the big
  format-overflow/nonliteral and format= chunks, then `-Wreturn-type`
  last since it was the largest and least bug-dense), rebuilding and
  running the full smoketest suite after each. Ended at a clean
  `./build clean && ./build lnx` with 0 warnings.
- **Real bugs found and fixed, not just warnings silenced** (the
  majority of sites across every category turned out to be genuine
  defects, not style noise):
  - `pine/folder.c`: `paint_folder_name()`'s NULL check on
    `folder_entry()`'s result was written backwards --
    `f->name == NULL` (an array, never NULL) instead of `f == NULL`
    -- dereferencing `f` *before* the guard could ever catch the
    out-of-range-index case it was written to protect against. Same
    file: a `max(0, unsigned-subtraction)` clamp in `display_folder()`
    was dead code (unsigned underflow can never compare `< 0`),
    silently defeating a column-clamp meant to keep centered text on
    screen.
  - `pine/mailview.c`: a dangling-`else` made PF-key remapping for
    keymap `which == 2` structurally unreachable (parsed nested inside
    the `which == 1` branch instead of parallel to it) -- an entire
    keymap's function-key handling was dead code.
  - `pico/os_unix.c` (source for generated `os.c`/`osdep.c`):
    `select()` called with the same `fd_set` for both `readfds` and
    `exceptfds` (a `-Wrestrict` aliasing violation); `pico_new_mail()`
    used the `$MAIL` environment variable directly as an `sprintf`
    format string (a format-string vulnerability) *and* copied it
    unbounded into a 256-byte buffer; `ttgetc()` (the core keyboard-
    read function, wired into `term.t_getchar`) could fall off the end
    without returning if `read()` failed for a reason other than EOF/
    EINTR and the resulting `SIGHUP` didn't actually terminate the
    process, leaving the caller with an undefined "character".
  - `pico/browse.c`: four identical `sprintf(child, "%s%c%s", dname,
    SEP, fname)` call sites could overflow `child[NLINE]` browsing a
    deep enough directory tree; replaced with one static
    `build_child_path()` helper that truncates rather than overflows.
  - `pine/newmail.c`: a `%.199s` precision bound on the mail-subject
    line didn't account for the surrounding `" %s "` preposition-word
    literal, actually overflowing `subject[200]` by up to 16 bytes;
    separately, `if(from == NULL)` tested a fixed array (always
    false) where `from[0] == '\0'` was clearly meant.
  - `pine/log.c`: `strncpy(string += 2, "Save", 4)` replaced the wrong
    substring of an "APPEND ..." message (produced `"APSave..."`
    instead of `"Save..."`); switched to the file's own `rplstr()`
    in-place-splice helper, used one block below for the same kind of
    substitution.
  - `pine/reply.c`: `a = a++;` was undefined behavior (two unsequenced
    modifications of `a`); fixed to plain `a++`.
  - Numerous `dprint()`/`sprintf()` format-vs-argument mismatches
    across `mailcmd.c`, `mailindx.c`, `newmail.c`, `send.c`,
    `status.c`, `strings.c`, `ttyin.c`, `mailbox.c`: pointers
    (`MAILSTREAM *`, `char *`, `int *`) printed with `%x` (silently
    truncated to 32 bits and technically UB via the varargs mechanism)
    switched to `%p`; `long` fields printed with `%d` (would misread
    the value and, on stack-passing ABIs, could misalign subsequent
    varargs) switched to `%ld`, including one literal stray `l` before
    a `%d` (`"cur:l%d"`) that was clearly meant to be `%ld`;
    `send.c`'s message-ID generator cast `random()`/`getpid()` to
    `(long)` to match its `%ld` format (`random()` has no prototype in
    scope under `-ansi`, so the compiler otherwise assumes `int`).
  - Several more `-Wformat-overflow=` sites where a scratch buffer was
    sized for the "realistic" case but not the worst case a `%ld` of a
    full-width `long` could produce (`pine/reply.c`'s message-ID
    buffer, `pine/send.c`'s sequence buffer, `pine/strings.c`'s
    `byte_string()`/`comatose()` buffers, `pine/mailcmd.c`'s two
    prompt buffers) -- widened with margin or bounded with a computed
    `%.Ns` precision matching this codebase's existing convention.
    `pine/osdep/fltrname` (source for generated `os.c`/`osdep.c`):
    the "illegal filename character" error message could overflow
    `error[100]` for a long enough filename -- resized off `MAXPATH`
    like the `ill_file` buffer it echoes back, rather than truncating
    the filename shown to the user.
- **The 17 `-Wformat-nonliteral` sites** all turned out to be the same
  established, intentional pattern -- a family of fixed-arity
  "printf-style" wrapper functions (`q_status_message1..7`,
  `PutLine1..5`, `panic1`, `context_apply`, plus two
  `char *fmt = "literal"` locals in `screen.c`) where every real call
  site passes a compile-time literal, verified by grep. Suppressed
  with scoped `#pragma GCC diagnostic ignored "-Wformat-nonliteral"`
  around each, with a comment explaining why, rather than restructure
  working code or convert them to true `<stdarg.h>` variadics (a
  bigger change than this pass warranted).
- **The 95 `-Wreturn-type` warnings** split cleanly into two kinds:
  55 were `-Wold-style-definition`'s sibling issue, K&R function
  *definitions* with no declared return type at all ("return type
  defaults to `int`") -- purely mechanical, verified each still
  actually returns plain `int` values before adding the keyword,
  including 12 in `pine/osdep/`-family fragments (`writ_dir`,
  `creatdir`, `rename`, `tempnam`, `hostname`, `jobcntrl` x2,
  `chnge_pw`, `mimedisp`, `fgetpos` x2) requiring edits to the real
  fragment source rather than generated `os.c`. The remaining 40 were
  genuine "control reaches end of non-void function" -- functions
  falling off the end without a `return`. Checked each one's callers
  before picking a fallthrough value: most (all of `pico/tcap.c`'s 13
  termcap-output wrappers, several `pico.c`/`browse.c`/`display.c`
  helpers) are pure side-effect functions whose `int` return has never
  been read by any caller, so a plain `return(0)` was correct and
  safe; a few needed real values -- `pico.c`'s `quickexit()` is
  registered via `rebindfunc(wquit, quickexit)` as a drop-in
  replacement for `wquit()` in the keybinding dispatch table (which
  *does* propagate command-function return values as the overall
  command status), so it now does `return(wquit(f, n))` instead of
  discarding wquit's result; `basic.c`/`bind.c`/`word.c`'s `gotobop()`/
  `whelp()`/`fillpara()` are keybound `int cmd(int f, int n)` commands
  whose return is propagated by `pico.c`'s `execute()` to the same
  dispatch mechanism, so they now return `TRUE` (matching this
  codebase's established command-success convention) instead of 0;
  `screen.c`'s `equalbits()` had a `default: panic(...)` case (`panic()`
  always calls `exit()`, confirmed by reading it) needing only a
  trailing `return(0)` to satisfy the compiler for the theoretically-
  unreachable fallthrough.
- **Verified**: full `./build clean && ./build lnx` after every batch,
  ending 0 warnings; full smoketest suite (26/26) green throughout,
  not just at the end.
- **Size delta**: not measured separately from the surrounding cuts in
  this session; the fixes are almost entirely logic corrections and a
  handful of widened stack buffers (tens of bytes each), not
  size-reduction work in themselves.

### Dropped `pico/ansi.c` -- permanently dead, never-enabled alternate terminal driver
- [x] Deleted `pico/ansi.c` outright (`git rm`) and dropped `ansi.o`/
      `ansi.c` from `pico/makefile.lnx`'s `OFILES`/`CFILES`. As
      described when this was found: the file's entire body (`#if
      ANSI_DRIVER` ... `#endif`, the alternate ANSI/VT100 terminal
      driver competing with `pico/tcap.c`'s termcap-based one) never
      compiled here since `ANSI_DRIVER` is never defined in this tree,
      and the one function outside that guard, `ansiopen()`, was an
      orphaned, always-unreferenced external-linkage function (its
      only reference was itself inside the dead block). Confirmed via
      grep across `pico/efunc.h`/`estruct.h`/`edef.h` and every other
      `pico/*.c` that nothing outside `ansi.c` itself referenced
      `ansiopen` or any `ansi*` symbol -- a clean, no-dependency
      deletion. Other platform makefiles (`pico/makefile.bsd`,
      `.sol`, `.sco`, etc.) still list `ansi.o`/`ansi.c`, but those
      aren't part of this project's active build (only `makefile.lnx`
      is used for the `lnx` dev target; there's no PDP-11
      cross-compiler makefile yet) and were left alone, matching the
      original TODO's scope.
  - **Correction to the original estimate**: the write-up expected a
    "small but real" binary-size win (~176 bytes text+data, per a
    standalone `nm` compile of `ansi.c` alone). The actual before/after
    `size bin/pico` was identical (text 110299, data 6364, both
    before and after). Root cause: `libpico.a` is a static archive, and
    the linker only pulls in archive members needed to resolve an
    undefined symbol when linking `pico` (`main.c` + `libpico.a`).
    Since nothing ever called `ansiopen()`, `ansi.o` was already being
    excluded from the final link even while `ansi.c` still existed and
    was being compiled into the archive -- confirmed by `nm bin/pico |
    grep -i ansi` finding nothing both before and after, and by `ar t
    libpico.a` no longer listing `ansi.o` post-deletion. So this cut is
    source-hygiene/dead-code removal, not a measurable size win; noted
    here so future size-accounting doesn't expect a delta that isn't
    there.
  - **Verified**: `./build clean && ./build lnx`, 0 warnings; full
    smoketest suite, 26/26 passing; confirmed no `ansi` symbols in
    `bin/pico` and no `ansi.o` member in `libpico.a` post-deletion.

### Removed the "This is a new version of Pine!" first-launch screen -- permanently broken by the earlier `read_pinerc()` cut, user-reported
- **What it was**: user reported seeing "This is a new version of Pine!"
  (`new_user_or_version()`/`new_version_message[]` in `pine/pine.c`)
  every single time they started pine, not just once. Root cause:
  this screen's "show once" logic (`pine/init.c`'s old `init_vars()`)
  compared the pinerc's `last-version-used` value against the
  compiled-in version string, and if they didn't match, set
  `ps->show_new_version = 1` and called `set_variable(V_LAST_VERS_USED,
  version_buff)` to "remember" it for next time. Two independent things
  make that permanently broken in this fork: `set_variable()`'s own
  header comment says it plainly -- "pinerc is never rewritten" -- it
  only updates the in-memory value, so the write-back back to disk
  never happened even before this session's changes; and separately,
  `read_pinerc()` was already cut down to an unconditional `return(0)`
  stub (see the "bootstrap-only stub" entry above), so the pinerc is
  never even read back in on the next run either. Net effect:
  `VAR_LAST_VERS_USED` is always `NULL` on every startup, so
  `show_new_version` was unconditionally `1` on every run -- this
  screen could never have worked as a once-only notice in this fork,
  with or without the `read_pinerc()` cut.
- **What changed**: removed the whole subsystem rather than patch the
  broken persistence, since there's no persistence mechanism left in
  this fork to make "show once" meaningful again:
  - `pine/pine.c`: deleted `new_user_or_version()`, the
    `new_version_message[]` array, its prototype, and the
    `if(pine_state->show_new_version){ ... }` call site in the startup
    sequence (the `ClearScreen()` calls immediately before/after it
    stay, unrelated to this screen). Simplified `warn_other_cmds()`'s
    condition (used for an unrelated "the O command is optional" hint)
    from `(ps_global->show_new_version && other_cmds % 3 == 0 &&
    other_cmds < 10) || other_cmds % 20 == 0` down to plain
    `other_cmds % 20 == 0` -- the more-frequent-hints-for-a-new-version
    behavior no longer has a "new version" concept to key off.
  - `pine/pine.h`: dropped the `show_new_version:1` bitfield from
    `struct pine`, and the now-fully-unreferenced `VAR_LAST_VERS_USED`
    macro. Left the `V_LAST_VERS_USED` index itself and its
    `variables[]` table entry in `pine/init.c` in place (renumbering
    `V_*` constants for one dead slot isn't worth the blast radius,
    and this fork already keeps several other disabled slots the same
    way -- `V_PRINTER`, `V_SMTP_SERVER`, etc. -- for the same reason);
    flipped its `is_obsolete` flag to 1 and reworded its description
    to "Last-version-used is disabled, the new-version welcome screen
    has been removed", matching the existing disabled-slot convention
    (confirmed via grep that `is_obsolete`/`is_used` aren't actually
    read by any code -- these fields, and the whole `variables[]`
    description text, are inert now that this fork has no Setup/Config
    screen left that displays them; kept purely for readability/
    consistency with neighboring entries).
  - `pine/init.c`: deleted the `set_current_val(vars, V_LAST_VERS_USED,
    TRUE)` / compare-and-set `show_new_version` / `set_variable(...)`
    block from `init_vars()` entirely.
  - `test/smoketest.py`: `Session.__init__`'s `dismiss_splash` handling
    used to `expect("MAIN MENU")` (matching the splash screen's
    underlying chrome), blindly `send("\r")` to dismiss it, then
    `expect("MAIN MENU")` again for the real menu. With the splash
    gone, that blind `\r` now lands as a real keystroke on the actual
    Main Menu -- caught by `test_pinerc_not_parsed.py` failing (it
    landed on the Folder List screen instead). Simplified to a single
    `expect("MAIN MENU")` with no keystroke; `dismiss_splash` kept as a
    no-op-if-true parameter so existing call sites don't need to
    change.
- **Verified**: `./build clean && ./build lnx`, 0 warnings (`bin/pine`
  text shrank 328383 -> 327351, data 12496 -> 12400, from the removed
  message array/function; `bin/pico` unaffected). Manually drove a
  fresh `Session(mbox=None, dismiss_splash=False)` and confirmed the
  very first screen is the real Main Menu with no interstitial splash
  or "Type any character to continue" prompt. Full smoketest suite,
  26/26 passing, run twice for stability (the `dismiss_splash` fix
  above was needed for `test_pinerc_not_parsed` specifically, since
  it's the one test that both relies on default `dismiss_splash=True`
  and does its own subsequent `To:`-field interaction that a
  misdirected keystroke could desync).

### Cut 22 of `pine/init.c`'s 36 `variables[]` pinerc entries -- dead now that `.pinerc` is never actually read or written
- **What it was**: `pine/init.c`'s `variables[]` table (`V_*`-indexed)
  models every `.pinerc`/`pine.conf` variable pine could ever have. But
  `read_pinerc()` is an unconditional `return(0)` stub (the
  "bootstrap-only stub" entry above) -- no `.pinerc` is ever actually
  read -- and `set_variable()`'s own header comment says plainly
  "pinerc is never rewritten," so nothing persists a value across runs
  either. Net effect: no entry in this table can ever hold anything but
  its compiled-in default, for the life of a run or across runs. Two
  tiers of entries fell out of grepping every `V_*`/`VAR_*` name
  against the whole tree (excluding the table/`#define` lines
  themselves):
  - **Tier 1 (17 entries, zero non-table references anywhere)**: the
    12 already-`is_obsolete`-flagged "disabled, X removed" slots from
    past cuts (`smtp-server`, `incoming-folders`, `news-collections`,
    `address-book`, `character-set`, `editor`, `image-viewer`,
    `printer`, `personal-print-command`, `standard-printer`,
    `last-time-prune-questioned`, `last-version-used`), 2 that were
    never wired into anything at all (`compose-mime`,
    `show-all-characters`), and 3 more caught in a second pass after
    the user spotted them (`bugs-nickname`/`bugs-fullname`/
    `bugs-address`): these had a `set_current_val()` call each (so an
    earlier grep for "any reference at all" undercounted them as
    live), but no `VAR_BUGS_*` macro was ever actually read anywhere,
    and no `DF_BUGS_*` default existed to populate them either --
    `set_current_val()` just copied an always-`NULL` `global_val`/
    `user_val` into an always-`NULL` `current_val`, a complete no-op.
    They're leftovers of the deleted address-book feature ("Nickname
    of bugs entry to be automatically placed in all address books").
    All 17 of these slots were previously kept (not deleted) purely so
    their `V_*` index numbers wouldn't shift and break re-parsing of
    an old `.pinerc` -- but since nothing is parsed by name anymore,
    that compatibility concern is itself moot, which is what unblocked
    full deletion instead of just marking them disabled.
  - **Tier 2 (5 entries, `elm-style-save`/`header-in-reply`/
    `feature-level`/`old-style-reply`/`save-by-sender`)**: these exist
    purely to let `read_pinerc()` translate pre-3.89 pinerc variable
    names into their modern `feature-list`/`saved-msg-name-rule`
    equivalents on the fly. Since they can only ever hold their
    compiled-in `DF_*` default (`pine/os.h`: all four "no"-defaulted
    ones are literally `"no"`, `DF_FEATURE_LEVEL` is `"sapling"`,
    identical across every platform variant in the tree), traced the
    resulting arithmetic by hand: `obs_header_in_reply`/
    `obs_old_style_reply`/`obs_save_by_sender` always resolve `0` and
    `obs_feature_level` always resolves `Sapling`, unconditionally, on
    every run. That made two further pieces of live-looking code
    provably dead: `process_feature_list()`'s three backward-compat
    parameters were always called `(0, 0, 0)`, so its `if(hir)`/
    `if(osr)`/`if(old_growth)` blocks and its entire ~35-line
    "build a new feature-list to fold the obsolete setting in" tail
    never executed; and `init_vars()`'s `obs_save_by_sender`-driven
    override of `saved-msg-name-rule` never fired either.
- **What changed**: deleted all 22 `variables[]` entries and their
  `V_*`/`VAR_*` `#define`s from `pine/pine.h`, renumbering the
  remaining 14 `V_*` constants sequentially from 0 (confirmed via grep
  that nothing in the tree hardcodes a literal index; the 3
  `V_BUGS_*` entries happened to already be last in the list, so their
  removal needed no renumbering of its own). Deleted the now-unused
  `FeatureLevel`/`Sapling`/`Seedling`/`Seasoned` enum. Deleted the
  Tier 2 migration block from `init_vars()`, its 3
  `set_current_val(vars, V_BUGS_*, TRUE)` calls, and simplified the
  `saved-msg-name-rule` "default-folder" branch (the dead
  `obs_save_by_sender` override that followed it is gone). Simplified
  `process_feature_list()`'s signature from `(ps, list, old_growth,
  hir, osr)` down to `(ps, list)`, deleting the three backward-compat
  parameters, their top-of-function `F_TURN_ON` blocks, and the
  "build a new feature-list" tail -- it now just walks the real
  `feature-list` value and sets bits, nothing else. The remaining 14
  variables (`personal-name`, `user-id`, `user-domain`, `inbox-path`,
  `folder-collections`, `default-fcc`, `mail-directory`,
  `read-message-folder`, `signature-file`, `feature-list`,
  `initial-keystroke-list`, `saved-msg-name-rule`, `sort-key`,
  `use-only-domain-name`) are untouched -- all confirmed real,
  live-read `VAR_*` values used throughout `pine/*.c` (re-checked
  after the `bugs-*` correction: each of these 14 has at least one
  `VAR_*` macro reference outside `init_vars()`'s bootstrap, unlike
  `bugs-*`). `sort-key` and `initial-keystroke-list` are also the only
  two ever touched by `pine/args.c`'s command-line-flag handling
  (`command_line_val`), an independent input path from `.pinerc` --
  unaffected by this cut. Left the never-read `struct variable`
  bitfields (`is_obsolete`/`is_used`/`been_written`/`is_global`) and
  the deeper 4-way `user_val`/`global_val`/`command_line_val`/
  `current_val` value model alone -- a bigger redesign of how
  variables work at all, separate from "which variables can be
  deleted."
- **Correction mid-cut**: the first pass classified `bugs-nickname`/
  `bugs-fullname`/`bugs-address` as live because they had a
  `set_current_val()` call each -- but the user pointed out these
  three were also removable, which prompted checking further and
  finding that call is the *only* thing that ever touches them:
  `VAR_BUGS_*` is never read anywhere, and no `DF_BUGS_*` default
  exists to give `set_current_val()` anything to actually propagate.
  Folded into this same entry rather than a separate write-up since
  it's a direct continuation of the same not-yet-committed cut.
- **Verified**: grepped all 22 removed `V_*`/`VAR_*` names (plus
  `FeatureLevel`/`Sapling`/`Seedling`/`Seasoned`) across the whole tree
  before and after each pass -- zero references either way beyond the
  lines being deleted. `./build clean && ./build lnx`, 0 warnings after
  both passes. Full smoketest suite, 26/26, run twice for stability
  after each pass. Manually confirmed default feature bits are
  unchanged: drove a fresh session and quitting still prompts "Really
  quit pine?" (i.e. `F_QUIT_WO_CONFIRM` still defaults off, matching
  pre-cut behavior since `old_growth` was already unconditionally `0`
  even before this cut).
- **Size delta**: `bin/pine` text 327351 -> 323943 (-3408 bytes), data
  12400 -> 11184 (-1216 bytes); `bin/pico` unaffected.

### Collapse dead `feature-list` (`F_*`) checks, phase 1 -- 28 call sites across 5 files

Same root cause as the `variables[]` cut above: `.pinerc` is never read
or written, so `process_feature_list()` (`pine/init.c`) -- the only code
that ever populates a feature bit from the `feature-list` variable -- is
a permanent no-op (its `list` argument, `VAR_FEATURE_LIST`, is always a
non-NULL pointer to a single `NULL` entry, so its `for` loop body never
runs). Grepped every `F_TURN_ON`/`F_TURN_OFF`/`F_SET` call site in the
tree: of pine's 19 feature bits, only `F_USE_FK`, `F_CAN_SUSPEND`, and
`F_EXPANDED_FOLDERS` have a second, real write path (`-k`/`-z`/`-l`
command-line flags in `pine/args.c`, the "pinef" argv0 check, and
`F_USE_FK`'s composer-subprocess toggle in `pine.c`/`send.c`/`ttyin.c`)
-- those three stay live and untouched. The other 16 can only ever be
set through the dead `process_feature_list()` path, so every
`F_ON()`/`F_OFF()` check on them is a provably-fixed-outcome dead
branch, using the same "trace the always-true/false condition by hand"
technique as the Tier 2 cut above.

Collapsed all 28 non-`F_USE_FK`/`F_CAN_SUSPEND`/`F_EXPANDED_FOLDERS`
call sites to their fixed outcome:
- `pine/mailindx.c` (`do_index_border()`) and `pine/mailview.c`
  (view-screen keymenu setup): 6 + 4 `if(F_OFF(F_ENABLE_PIPE/FULL_HDR/
  BOUNCE/FLAG/ZOOM/APPLY,...)) clrbitn(KEY, bitmap);` sites always
  fired -- replaced each with a bare unconditional `clrbitn(...)`.
- `pine/mailcmd.c` (13 sites, `process_cmd()`'s file -- mailcmd.o's
  largest symbol): delete-message handler's `F_DEL_SKIPS_DEL` 3-way
  branch collapsed to its permanent `F_OFF` outcome; the `'0'`-`'9'`
  digit-jump-to-message shortcut (gated by `F_ENABLE_JUMP`, always
  `goto bogus`) had its 10 `case` labels deleted outright -- they now
  fall through to `default: bogus` with identical behavior (the
  `'j'`/`'w'`/`OOPF7` Jump/WhereIs commands are untouched, still fully
  live); the `'h'` toggle-full-headers command (gated by
  `F_ENABLE_FULL_HDR`, always `goto bogus` before its body ran) had its
  entire `case` block deleted, same reasoning; both `F_AUTO_EXPUNGE`
  sites (`process_cmd()`'s expunge handler and `expunge_and_close()`)
  and the `F_AUTO_READ_MSGS` site collapsed from "prompt only if
  `F_OFF`, else auto-act" to always prompting via `want_to()`; the two
  `F_ENABLE_TAB_COMPLETE` extra-key-setup blocks in the save-folder
  prompt (`if(F_ON(...)) { ekey[rc].ch = ...; rc++; }`) were dead code
  and deleted outright.
- `pine/pine.c` (`quit_screen()`): `F_OFF(F_QUIT_WO_CONFIRM) &&` guard
  was always true -- dropped it, "Really quit pine" confirmation now
  unconditional (same as actual prior behavior).
- `pine/reply.c` (3 spots): `F_SIG_AT_BOTTOM`'s `F_OFF` guard on the
  "insert signature before included text" block in the reply composer
  was always true -- dropped it; two `if(F_ON(F_INCLUDE_HEADER,...))
  format_text_header(...);` lines in the reply composer were dead and
  deleted; `forward()`'s `F_MIME_FORWARD` block was the biggest single
  find -- `ret` is declared `int ret=0;` and never assigned before the
  `if(F_ON(F_MIME_FORWARD,...) && (ret=want_to(...))=='y')` line, and
  since `F_ON` is always false, `&&`-short-circuiting means
  `want_to()` is *never called*, so `ret` stays `0` forever, making the
  following `else if(ret == 'x')` branch dead too (`0 == 'x'` is always
  false). Execution always fell through to the final `else` (the plain
  non-MIME forward path). Deleted the entire dead
  `if(...){...} else if(ret=='x'){...}` (73 lines) and un-wrapped the
  final `else` block to run unconditionally; `ret` and the `BODY *b`
  local (only used inside the deleted MIME branch) became unused and
  were removed too.
- **What's deliberately left for phase 2**: `process_feature_list()`
  itself, `feat_list[]`, the `"feature-list"` variable/`V_FEATURE_LIST`
  entry, and the 16 now-fully-unreachable `F_*` `#define`s -- once
  every reader of the 16 dead bits is gone, nothing reads
  `process_feature_list()`'s output for them either, but that's a
  separate, independently-verifiable cut (different call sites, no
  overlap with this one).
- **Verified**: `./build clean && ./build lnx`, 0 warnings. Full
  smoketest suite, 26/26, run twice for stability. Manually drove a
  session and confirmed: quit still prompts "Really quit pine"
  (Y/n/^C) and a `n` answer correctly cancels; expunge still prompts
  "Expunge N message(s)..." and a `y` answer still expunges; forward
  still opens the plain (non-MIME) forward composer with no
  MIME-attachment prompt (matching pre-cut behavior, since that prompt
  was already unreachable).
- **Size delta**: `bin/pine` text 323943 -> 321543 (-2400 bytes), data
  unchanged at 11184 (expected -- this phase only touches code, not
  the `variables[]`/`struct variable` data). `bin/pico` unaffected.

### Collapse dead `feature-list` (`F_*`) checks, phase 2 -- remove the dead machinery

Follow-on to phase 1 above: with every reader of the 16 dead `F_*` bits
collapsed to its fixed outcome, nothing reads `process_feature_list()`'s
output for them either, so removed the whole now-fully-dead chain that
used to feed them, all in `pine/init.c` unless noted:
- The `"feature-list"` entry from the `variables[]` table (was
  `V_FEATURE_LIST` = 9 of 14), and its `FEATURE_LIST_TEXT` description
  string -- its only reference.
- The `set_feature_list_current_val(vars, V_FEATURE_LIST)` and
  `process_feature_list(ps, VAR_FEATURE_LIST)` call sites in
  `init_vars()`, plus the now-stale "this should come after
  process_feature_list because of use_fkeys" comment above them (moot
  once feature-list processing no longer exists -- `F_USE_FK` is set
  by `args.c` before `init_vars()` even runs).
- `process_feature_list()` itself, `feat_list[]`, and `struct
  features` -- confirmed via grep this was `process_feature_list()`'s
  only caller anywhere in the tree.
- `set_feature_list_current_val()` itself -- confirmed its only caller
  was the `init_vars()` call site just removed.
- Both functions' forward prototypes near the top of `pine/init.c`.
- In `pine/pine.h`: the `V_FEATURE_LIST`/`VAR_FEATURE_LIST` `#define`s,
  with the remaining 13 `V_*` constants renumbered sequentially (same
  safe mechanical renumbering as the `variables[]` cut above --
  confirmed via grep that nothing hardcodes a literal `V_*` index); and
  the 16 now-fully-unreachable `F_*` `#define`s (`F_OLD_GROWTH`,
  `F_ENABLE_FULL_HDR`, `F_ENABLE_PIPE`, `F_ENABLE_TAB_COMPLETE`,
  `F_QUIT_WO_CONFIRM`, `F_ENABLE_JUMP`, `F_ENABLE_BOUNCE`,
  `F_ENABLE_APPLY`, `F_ENABLE_FLAG`, `F_ENABLE_ZOOM`, `F_MIME_FORWARD`,
  `F_INCLUDE_HEADER`, `F_SIG_AT_BOTTOM`, `F_DEL_SKIPS_DEL`,
  `F_AUTO_EXPUNGE`, `F_AUTO_READ_MSGS`), keeping only `F_CAN_SUSPEND`,
  `F_EXPANDED_FOLDERS`, `F_USE_FK` -- renumbered 0/1/2. Safe for the
  same reason as the `V_*` renumbering: `F_ON`/`F_OFF`/`F_TURN_ON`/
  `F_TURN_OFF`/`F_SET` are the only way any code touches a feature
  bit, and every call site in the tree goes through the symbolic
  `F_*` name, never a literal bit number (confirmed via grep).
- **Verified**: grepped all 16 removed `F_*` names, `V_FEATURE_LIST`,
  `VAR_FEATURE_LIST`, and `FEATURE_LIST_TEXT` across the whole tree
  after the edit -- zero references remain anywhere. `./build clean &&
  ./build lnx`, 0 warnings. Full smoketest suite, 26/26, run twice for
  stability. Manually confirmed: a `.pinerc` seeded with
  `feature-list=quit-without-confirm` still has no effect (quit still
  prompts "Really quit pine", matching the pre-existing "pinerc is
  never parsed" behavior -- this cut removes dead code, it doesn't
  change what was already unreachable); the 3 surviving command-line
  feature flags (`-k`/`-z`/`-l`) are inherently safe under the
  renumbering since reader and writer both resolve through the same
  `F_*` symbol at compile time, with no literal bit values anywhere in
  the tree to go stale.
- **Size delta**: `bin/pine` text 321543 -> 317519 (-4024 bytes), data
  11184 -> 10768 (-416 bytes, from the removed `variables[]` entry).
  `bin/pico` unaffected. Combined phase 1 + phase 2 total against the
  pre-cut baseline: text -6424 bytes, data -416 bytes.

### Remove the keyboard-lock feature (`pine/other.c`)

Out-of-scope-feature cut, not a dead-code removal: `pine/other.c`
implemented "Keyboard Lock" (bound to `k`/PF10 on the main menu),
which password-locks the terminal until the same password is
re-entered so another user can't read the current user's mail while
they're away. Real, reachable code (0% smoketest coverage, but that
just meant it wasn't tested, not that it was unreachable) -- confirmed
with the user this doesn't belong on a UUCP-connected, single-user,
local-mbox PDP-11 mail station, and cut with their explicit go-ahead.

- **What changed**:
  - `git rm pine/other.c` -- deleted `lock_keyboard()`,
    `redraw_kl_body()`, `draw_klocked_body()`, `redraw_klocked_body()`
    entirely.
  - `pine/pine.c`: removed the `case PF10: case 'k': (void)lock_keyboard();
    ...` block from the main menu's command dispatch. Blanked the
    `main_keys[]` keymenu-footer entry that displayed `K KBLock`
    (index `KBLOCK_KEY` = 9) to `{NULL,NULL,0}`, matching the array's
    existing pattern for unused footer slots rather than
    renumbering/shrinking the array (confirmed via grep that
    `KBLOCK_KEY` itself had no other reader to worry about, unlike
    `HELP_KEY`/`COMPOSE_KEY`/etc., which are read elsewhere for
    keymenu highlighting). Removed the now-unused `#define KBLOCK_KEY
    9` itself.
  - `pine/pine.h`: removed the `lock_keyboard()` prototype.
  - `pine/makefile.lnx`: removed `other.c`/`other.o` from `obj=`,
    `cfiles=`, and the per-object dependency rule.
  - `pine/pine.hlp` (the real tracked source for generated
    `helptext.c`/`.h`): removed the `h_oe_keylock` help section, whose
    only reader was `lock_keyboard()`. Rebuilding regenerates
    `helptext.c`/`.h` (gitignored) without it -- confirmed via grep
    post-build.
  - `pine/ttyout.c`: fixed a now-stale comment above `temp_format`'s
    declaration that referenced `redraw_klocked_body()`'s
    `klockame`/`klockin` (previously the one call site the
    `PutLine1..PutLine5` buffer-size audit had to special-case as
    "unbounded, but capped by other.c's own 128-byte buffers") -- with
    `other.c` gone, that exception no longer exists, so simplified the
    comment to just "none pass unbounded content" instead of leaving a
    dangling reference to a deleted file.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25. Live pty check (`test/smoketest.py`'s
  `Session`, ad hoc script): pressing `k` at the main menu no longer
  shows the "Enter password to LOCK keyboard" prompt or the `K KBLock`
  footer label -- falls through as an unbound key with no crash, same
  as any other unmapped key at that screen. Final `grep -rn` sweep for
  `lock_keyboard|KBLock|KBLOCK_KEY|klockin|klockame|redraw_klocked_body|
  draw_klocked_body|redraw_kl_body|h_oe_keylock` across `pine/` turned
  up nothing left.
- **Size delta**: `bin/pine` text 280791 -> 278717 bytes (-2074 bytes).

### `pine/mailindx.c` and `pine/pine.c` dead-code sweep -- never-defined-macro cleanup, outside the earlier four-file survey

Neither file was part of the earlier `send.c`/`mailcmd.c`/`folder.c`/
`mailview.c` "UI files: dead/vestigial code" survey, so they still had
the same class of `#ifdef DOS` (and friends) dead weight that survey
already cleaned up elsewhere. Confirmed `DOS`, `MOUSE`, `DYN`,
`HAVE_SMALLOC`, `NXT` are never defined anywhere in the tree (grepped
every platform header and `makefile.lnx`), and that `USE_QUOTAS`/
`NO_KEYBOARD_LOCK` are always commented out in every `pine/osdep/os-*.h`
variant including the one this build actually uses (`os-lnx.h`) --
same "uniform-across-every-platform-variant" proof technique already
used for the `DF_*` migration defaults in the `variables[]` cut.

**`pine/mailindx.c`**: removed two dead mouse-support blocks
(`register_mfunc`/`mouse_in_index`/`clear_mfunc`) and a dead
`while_waiting = build_header_cache` pre-fetch hook from
`mail_index_screen()`'s command loop; unwrapped an always-true
`#ifndef DOS` guard around the "still sorting" status message (kept
the real `if(max_msgno > 500)` check it wrapped). The big find: the
entire **disk-paged index-line cache** was DOS-only --
`icread()`/`icwrite()`/`i_cache_hit()` (three whole functions), the
DOS half of `i_cache_size()`, and the `incore_cache`/`cache_block_s`/
`cache_base`/`ICC_SIZE`/`FUDGE` bookkeeping plus the `icache.name`
struct field they were the only users of -- same species of dead code
as the `FileStar` disk-paged scroll-buffer engine already cut from
`mailview.c`. Also deleted **`flush_index_cache()`** outright: its
entire body was `#ifdef DOS` (so it already compiled to a no-op) and
grep confirmed zero callers anywhere in the tree, plus its prototype
in `pine.h`.

**`pine/pine.c`**: `main()` lost a dead DYNIX stdio-buffering
workaround, dead `malloc_debug()` calls (`HAVE_SMALLOC`/`NXT`), a dead
`mailcache = dos_cache` hook, a dead disk-quota check
(`USE_QUOTAS`), and two dead terminal-init error-message suppressions
(`#ifdef DOS /* always succeeds */ #else <real error path> #endif` --
unwrapped to keep the real path unconditional). The biggest find:
`main()`'s entire "prompt for an inbox path if none is set" block (65
lines) was wrapped in `#ifdef DOS` -- meaning the
`set_variable(V_INBOX_PATH, int_mail)` call inside it, which an
earlier session's `variables[]`-removal scoping investigation had
flagged as "genuine interactive runtime mutation, can't just delete
`variables[]`", **never actually executes in this build**. That
earlier finding was looking at dead code. With this block gone,
`set_variable()` has zero callers left anywhere in the tree (confirmed
via grep) and was deleted outright too, along with its prototype in
`pine.h`. `main_menu_screen()` lost the same dead mouse/`while_waiting`
pre-fetch hook as `mailindx.c`, a whole dead `case 'h':` MS-DOS
heap-debugger command (`_heapwalk`/`_HEAPINFO`, self-labeled "while
we're testing DOS" / "END: for testing" in its own comments -- clearly
throwaway test code, not a shipped feature), a dead low-memory check
before opening an IMAP folder, and a dead `change_passwd()`-suppression
branch (unwrapped to keep the real `change_passwd()` call
unconditional). Two `NO_KEYBOARD_LOCK` sites: one `#ifndef` wrapper
that was always-true (unwrapped, kept the live keyboard-lock command)
and one `#ifdef` that was always-false (deleted). `default_driver()`
lost a dead `extern MAILSTREAM dawzproto` (the live branch already
declares `bezerkproto`, used via `DF_DRIVER_PROTO`).

Noticed but left alone, same as the `folder.c` sweep's `h_folder_fcc`
precedent: `pine.hlp`'s `h_sticky_inbox` help topic is now unreachable
too (it was only ever shown from the deleted inbox-path-prompt block),
but cutting it means editing the tracked `.hlp` source and
regenerating `helptext.c`/`.h` -- a separate generated-file workflow,
out of scope here.

Verified: `./build clean && ./build lnx`, 0 warnings both files. Full
smoketest suite, 26/26, run twice for stability. Manually drove a
session exercising every behavior-relevant (not just dead-branch) site
touched: sorting the index by subject (exercises
`build_header_cache()`/`get_index_cache()`, the surviving half of the
disk-cache code); the Main Menu's `'k'` keyboard-lock command still
prompts and cancels correctly; the Main Menu's `'g'` goto-folder
command (the now-unwrapped `broach_folder()` call site) still prompts
correctly. No new regression test added -- dead-code removal with no
intended behavior change, not a bug fix.

Size delta was smaller than the line count suggests, for the same
reason the `mailview.c` sweep's was: most of what was removed was
`#ifdef`-gated code that the preprocessor already stripped before the
compiler ever saw it, so its presence or absence in the source doesn't
move the compiled binary -- the real win is `set_variable()` and
`flush_index_cache()`, which *were* compiling in with zero callers.
`bin/pine` text 317519 -> 317247 (-272 bytes), data unchanged at
10768. `bin/pico` unaffected.

### Dead-code sweep, bucket 1 of 3: `reply.c`, `screen.c`, `args.c`, `strings.c`, `context.c`

First of three roughly-equal-sized buckets covering the rest of the
tree's `#ifdef DOS`/never-defined-macro dead weight, split out because
the leftover surface (~73 sites across 14 files) was too big for one
pass. User confirmed `PCTCP`/`PCNFS` (dead network-stack variants
guarding `reply.c`'s message-ID-generation code) don't apply to any
target platform this project cares about, same as `LWP` -- all three
confirmed never defined anywhere in the tree (not even commented out),
same standard as `DOS`/`MOUSE`/etc.

- **`reply.c`** (biggest file in this bucket, 16+ sites): removed a
  dead DOS `IN_RESERVE`/`malloc()` pre-allocation block from both
  `reply()` and `forward()` (paired with its `free()` at each
  function's exit) -- once removed, the `char *reserve;` locals became
  unused and were deleted too. Removed three near-identical dead
  "fetch part into storage object" `FileStar`-vs-`CharStar` branch
  pairs (`forward()` x2, `fetch_contents()` x2 more) -- each kept its
  `CharStar`/live half. `get_body_part_text()`'s DOS
  `mailgets`/`append_file`/`dos_gets` disk-paged-fetch setup and
  cleanup were dead; removing them left the local `MESSAGECACHE *mc`
  variable's only assignment (`mail_elt()`) with no reader, so that
  got removed too -- an actual (not just preprocessor-stripped) dead
  function call, the one real size win in this file.
  `generate_message_id()`'s `STACK` character macro (fed by the now-
  confirmed-dead `LWP`/`PCTCP`/`PCNFS`) was entirely inside `#ifdef
  DOS` and only used by the DOS half of an `#ifdef`/`#else` `sprintf()`
  format-string choice -- deleted the macro and the DOS format string,
  kept the real one. `get_signature()`'s DOS-vs-Unix signature-path
  logic (backslash/drive-letter paths vs. `/`/`~`-relative paths)
  collapsed to the Unix half.
- **`screen.c`** (4 sites, all `#if defined(DOS) && defined(MOUSE)`):
  dead mouse-registration calls in the key-menu-footer painter
  (`register_key()`/an unused `extern void register_key()` decl and
  `int j` counter) -- `sed` was used instead of the Edit tool for these
  specific deletions after the tool's literal-string matching kept
  missing on this function's mixed tab/space indentation.
- **`args.c`** (3 sites): the `-P <pine.conf>` command-line flag's
  `case` label, its handler, and its `-h` usage-text line were each
  wrapped in an always-true `#ifndef DOS` -- unwrapped all three (the
  flag was already fully functional, just needlessly conditional).
- **`context.c`** (1 site): `context_isambig()`'s DOS drive-letter
  (`C:`) ambiguity check was dead; removed, leaving the real dot/
  alphanumeric ambiguity check as the sole `else` branch.
- **`strings.c`** (1 site): `comatose()`'s entire DOS half (a 16-byte-
  buffer, non-locale-aware number-with-commas formatter, "no numbers >
  1 trillion!") was dead; kept the real 100-byte-buffer version with
  proper 3-digit grouping.
- **Verified**: `./build clean && ./build lnx`, 0 warnings across all
  five files. Full smoketest suite, 26/26, run twice for stability.
  Manually drove a session exercising every behavior-relevant site
  touched: Reply with original text included (confirms
  `get_signature()`'s path logic and the quoted-text/signature
  insertion still work); Forward (confirms `forward()`'s storage-object
  fetch path); `bin/pine -h`'s usage text still lists `-P <pine.conf>`.
  No new regression test added -- dead-code removal with no intended
  behavior change, not a bug fix.
- **Size delta**: `bin/pine` text 317247 -> 317183 (-64 bytes, all from
  `reply.c`'s `mail_elt()`/`mc` removal -- the only site in this bucket
  that was compiling in live code, not just preprocessor-stripped
  branches), data unchanged at 10768. `bin/pico` unaffected.
- **Remaining**: bucket 2 (`signals.c`/`newmail.c`/`imap.c`/`ttyin.c`/
  `other.c`) and bucket 3 (`init.c`/`filter.c`/`status.c`/`os.c`) still
  open in `TODO-pdp11.md`.

### Dead-code sweep, bucket 2 of 3: `signals.c`, `newmail.c`, `imap.c`, `ttyin.c`, `other.c`

Second of three buckets covering the DOS/never-defined-macro dead code
identified in the earlier tree-wide survey. `signals.c` has a mix of
the project's own dead `DOS` conditionals and *real* platform
conditionals (`SIGEMT`, `SIGTSTP`, `RESIZING`, `ULT`) that vary by
actual target OS -- confirmed via grep that `RESIZING` **is** defined
in `os-lnx.h` (the header this build actually uses), so that guard and
everything gated on `SIGEMT`/`SIGTSTP`/`ULT` were left untouched; only
the `DOS` sites were removed. Also confirmed `NO_KEYBOARD_LOCK` is
commented out in every `pine/osdep/os-*.h` variant including
`os-lnx.h` (same never-turned-on standard as `DOS`), justifying
`other.c`'s cut.

- **`signals.c`** (12 sites): `init_signals()`/`end_signals()` --
  unwrapped `#ifdef DOS x #else y #endif` pairs to their `#else` (real)
  bodies. `quit_signal()`/`fast_clean_up()`/`usr2_signal()` -- each
  wrapped in an always-true `#ifndef DOS`; unwrapped, keeping the
  bodies (`usr2_signal()` was gated as a whole function). `do_suspend()`
  -- removed a dead `int result;` local and its two `#ifdef DOS`
  read/write sites (`system("command")`/error check), unwrapped the
  `#ifdef DOS x #else y #endif` message-and-suspend-mechanism pairs to
  their real (`stop_process()`) halves. The `hold_int`/`hold_term` vs.
  `hold_hup`/`hold_int`/`hold_term`/`hold_usr2` `SigType` declaration
  pair collapsed to the latter (always-compiled) form.
  `mm_critical()`/`mm_nocritical()` -- unwrapped two more always-true
  `#ifndef DOS` guards around the `SIGHUP`/`SIGUSR2` hold/restore
  calls.
- **`newmail.c`** (6 sites, all in `new_mail()`): collapsed
  `old_sort`'s DOS-only `FILE *`/`old_sort_val` declaration pair to the
  real `long *old_sort` form; unwrapped the sort-array growth strategy,
  the `old_sort` populate step (DOS: temp-file-backed via
  `create_tmpfile()`; real: `fs_get()`-backed array copy), the
  "lowest changed position" scan (DOS: `fseek()`/`fread()`-based; real:
  direct array indexing), and the final `old_sort` cleanup
  (`fclose()` vs. `fs_give()`) to their real halves.
- **`imap.c`** (3 sites): deleted the entire `#ifdef DOS` password-
  cache block wholesale -- `PASSFILE`/`FIRSTCH`/`LASTCH`/`TABSZ`
  macros, `encrypt_key`, and the `xsum()`/`encrypt()`/`decrypt()`/
  `passfile()` functions (132 lines, a self-contained XOR-rotation
  password obfuscator writing to a `pine.pwd` file next to the
  `.pinerc` -- meaningless on this build since `.pinerc` itself is
  never read/written). `mm_login()`'s two remaining `passfile()` call
  sites (the "check for a leftover password from last session" read,
  and the post-login write) were both dead and removed; the local
  `char *p` this orphaned was removed from the declaration too.
- **`ttyin.c`** (2 sites, in `optionally_enter()`): the help-text-
  fetch DOS/real branch (`get_help_text()` vs. plain `help_text = help`
  assignment) collapsed to the real half; the paired DOS-only
  `free_help_text(help_text)` cleanup call removed.
- **`other.c`** (1 site): the entire file body past its forward decls
  (`lock_keyboard()` and its three `redraw_*`/`draw_klocked_body()`
  helpers -- the whole keyboard-lock screen) was wrapped in an
  always-true `#ifndef NO_KEYBOARD_LOCK`; unwrapped, keeping the body,
  matching the guard's dead status confirmed in `pine.c`'s prior
  `NO_KEYBOARD_LOCK` unwrap (see the `mailindx.c`/`pine.c` sweep
  entry above).
- **Verified**: `./build clean && ./build lnx`, 0 warnings across all
  five files. Full smoketest suite, 26/26. Manually drove a pty
  session through `other.c`'s restructured keyboard-lock flow end to
  end: `k` from Main Menu -> enter lock password -> confirm -> screen
  shows "This keyboard is locked" -> re-entering the same password
  unlocks back to Main Menu. (A mismatched-password re-prompt path was
  also spot-checked but hit a pexpect timeout waiting for the
  "doesn't match" status message under a scripted rapid-fire
  send -- not a regression, since that logic path wasn't touched by
  this sweep, just a timing/redraw quirk in the manual test harness.)
- **Size delta**: `bin/pine` text 317183 -> 317183 (0 bytes), data
  unchanged at 10768 -- confirmed by diffing against a clean build of
  the pre-bucket-2 tree. Every site in this bucket was inside an
  `#ifdef DOS`/`#ifndef NO_KEYBOARD_LOCK` branch the preprocessor
  already stripped before compilation, so none of it carried binary
  weight to begin with; the win here is source-size and readability,
  not code size.
- **Remaining**: bucket 3 (`init.c`/`filter.c`/`status.c`/`os.c`)
  still open in `TODO-pdp11.md`.

### Dead-code sweep, bucket 3 of 3: `init.c`, `filter.c`, `status.c`, `os.c`

Third and final bucket of the DOS/never-defined-macro dead-code survey.
`init.c` also had a `FIRST_FOLDER_TRY`-guarded site the TODO flagged for
the same never-defined confirmation given to `LWP`/`PCTCP`/`PCNFS`/
`NO_KEYBOARD_LOCK` earlier -- confirmed via tree-wide grep that
`FIRST_FOLDER_TRY` is never `#define`'d anywhere, and its guarded body
references `V_LOCAL_MAIL_FORMAT`/`DF_LOCAL_MAIL_FORMAT`, neither of
which exist anywhere else in the current tree (`V_LOCAL_MAIL_FORMAT`
was already cut in an earlier `variables[]` reduction session) -- the
block wouldn't even compile if the macro were ever turned on, so it
was doubly dead.

- **`init.c`** (12 sites): the `"user-id"` `variables[]` entry's
  `#ifdef DOS`/`#else` pair (`is_used` 1 vs. 0) collapsed to the Unix
  (`0`) half; the dead `FIRST_FOLDER_TRY` block removed outright;
  `init_vars()`'s 40-line DOS pinerc-location-search comment-and-block
  collapsed to the real `.pinerc`-in-`$HOME` `#else` half; a
  `sprintf()`'s DOS-vs-Unix path-separator character (`'\\'` vs `'/'`)
  collapsed to `/`; `expand_variables()`'s always-true `#ifndef DOS`
  wrapping its backslash-escape/`~`-expansion branches unwrapped,
  dropping the dead one-branch DOS `#else`; `init_username()`'s DOS/
  Unix `VAR_USER_ID` lookup pair collapsed to the real
  `get_system_login()` half, a dead `ps->blank_personal_name = 1`
  write removed, and a dead DOS drive-letter (`X:`) check ahead of
  `build_path()` removed; `init_hostname()`'s always-true `#ifndef
  DOS` local-lookup guard and DOS/Unix `maildomain`-selection pair
  both unwrapped/collapsed, and a dead `ps->blank_user_domain`-gated
  early return removed; `init_debug()`'s DOS/Unix debug-file-rotation
  pair (36 lines) collapsed to the real 4-generation
  `.pine-debug[1-4]`-rotation `#else` half. Chain reaction: once the
  `blank_user_id`/`blank_personal_name`/`blank_user_domain` write
  sites were all gone, grep confirmed zero readers or writers
  remained anywhere for those three `pine.h` struct bitfields (which
  were themselves already `#ifdef DOS`-guarded in the struct
  definition, so already dead-by-construction) -- removed all three
  fields.
- **`filter.c`** (10 sites): `so_get()`'s and `so_reaquire()`'s DOS/
  Unix `fopen()` mode-string pairs (`"a+b"`/`"rb"` vs. `"a+"`/`"r"`)
  each collapsed to the Unix half; `so_get()`'s DOS `TmpFileStar`-
  coercion block (working around an MSC `tmpfile()` quirk) removed,
  keeping the Unix `temp_nam()` half; `gf_pipe()`'s four DOS sites
  (a progress-spinner's static state/init, an error-path spinner
  clear, a per-1024-bytes spinner tick, and an end-of-pipe spinner
  clear) all removed as dead UI decoration with no Unix equivalent;
  `gf_nvtnl_local()`/`gf_local_nvtnl()`'s DOS-vs-Unix newline-
  convention `#ifdef`/`#else` pairs each collapsed to the Unix
  CRLF-conversion half; the entire trailing `#ifdef DOS` block
  (`gf_translate()`/`gf_translate_opt()`, a DOS-codepage-to-charset
  translation filter pair, 44 lines) deleted outright -- grep
  confirmed both functions had zero callers anywhere, only their
  own `#ifdef DOS`-guarded prototypes in `pine.h`, which were removed
  too.
- **`status.c`** (2 sites, both in `radio_help()`): the DOS/Unix
  help-text-fetch pair (`get_help_text()` vs. plain `text = help`)
  collapsed to the Unix half; the paired DOS-only
  `free_help_text(text)` cleanup call removed.
- **`os.c`** (1 site, via its real source fragment
  `pine/osdep/diskquot.non` -- `os.c` itself is a generated,
  fragment-assembled file per this project's build process, never
  hand-edited): `disk_quota()` was already wrapped in `#if
  defined(USE_QUOTAS)`, and `USE_QUOTAS` is confirmed commented out
  in every `pine/osdep/os-*.h` variant including `os-lnx.h` (same
  never-turned-on standard as `NO_KEYBOARD_LOCK`), and had zero
  callers anywhere in the tree besides its own now-removed `pine.h`
  prototype -- emptied the fragment, deleting the function entirely.
- **Verified**: `./build clean && ./build lnx`, exit 0. No new
  warnings attributable to this bucket's edits (the handful of
  `-Wsign-compare`/`-Wstrict-prototypes` warnings clang/gcc still
  report in `status.c`/`os.c` are pre-existing, in functions this
  bucket didn't touch -- part of the still-open Tier 2 warning-flag
  cleanup). Full smoketest suite, 26/26. Manually verified
  `init_debug()`'s restructured Unix debug-file-rotation logic by
  running `bin/pine -d 9` twice against an isolated `$HOME` and
  confirming `.pine-debug1` was created with the expected banner text
  and rotated to `.pine-debug2` on the second run.
- **Size delta**: `bin/pine` text 317183 -> 315839 (-1344 bytes), data
  unchanged at 10768 -- unlike buckets 1 and 2, most of this bucket's
  removed code (all of `filter.c`'s sites, most of `init.c`'s) was
  genuinely compiled, not preprocessor-stripped, so this is a real
  code-size win rather than just a source-readability one.
- **Remaining**: this closes out all three dead-code-sweep buckets
  from the original tree-wide survey. What's left open in
  `TODO-pdp11.md` is the Tier 2 warning-flag cleanup
  (`-Wsign-compare`/`-Wshadow`, `-Wstrict-prototypes`/
  `-Wmissing-prototypes` already mostly landed via the `two` remote's
  parallel work -- see the `two/main` merge commit) and the ASan/
  `-fanalyzer` build-target item.

### Dead-code survey and sweep of `pico/` and `c-client/`

After finishing the `pine/` dead-code-sweep buckets, surveyed the two
other source trees for the same `DOS`/never-defined-macro pattern.

**`c-client/` (`imap/ANSI/c-client/`)**: essentially nothing left --
already heavily swept in earlier sessions (SMTP client removed, phile
driver dropped from the build, the DRIVER dispatch abstraction
removed, `mbox.c`/`sm_unix.c` deleted). The only files this build's
`makefile.lnx` actually compiles into `c-client.a`
(`mail.c`/`bezerk.c`/`dummy.c`/`rfc822.c`/`misc.c`/`os_lnx.c`) have
exactly one preprocessor conditional between them: `bezerk.c`'s
`#ifdef NFSKLUDGE` NFS-lock-file workaround, and `NFSKLUDGE` **is**
defined in `os_lnx.h` (the header this build actually uses) -- a real,
live guard, not dead code, so left untouched. The other `os_*.c`
files, plus `phile.c`/`smtp.c`, aren't in `makefile.lnx` at all, but
per this project's own established precedent (matching
`pine/osdep/os-*.h`'s other-platform variants) those are intentionally
preserved portability/protocol reference source excluded from this
build, not dead code -- confirmed via their own removal commits
("drop phile driver from c-client build", "remove SMTP client") which
explicitly left the source files in place. Nothing to cut here.

**`pico/`**: also heavily swept already (DOS-only color/mouse code,
address book, compose-side attachments, `ansi.c` itself already
deleted as "permanently dead, never-enabled alternate terminal
driver"). Found one direct sibling that was missed plus two small
scraps:
- **`pico/tinfo.c`** (603 lines) -- an entire alternate,
  terminfo-based terminal driver (`tinfoopen`/`tinfoclose`/
  `tinfomove`/etc., paralleling `tcap.c`'s termcap-based one), never
  referenced in `pico/makefile.lnx`'s `OFILES` and never `#include`d
  or called from any other file in the tree -- the exact same
  situation as the already-deleted `ansi.c`, just missed in that
  earlier pass. Deleted outright (`git rm`).
- **`pico/estruct.h`**: an `#if MSDOS` block defining `SHFT`/`ALTD`
  (function-key shift/alt modifier bits) -- confirmed via grep that
  `MSDOS` (distinct spelling from the project's usual `DOS` knob) is
  never `#define`'d anywhere in the tree, and `SHFT`/`ALTD` have zero
  readers elsewhere. Removed.
- **`pico/display.c`** (2 sites): `wmsg()`'s dead `#if MSDOS
  vtmove(1,0); vteeol(); #endif` pair ahead of the real mode-line
  redraw call, removed; `wkeyhelp()`'s dead `#if defined(DOS) &&
  defined(MOUSE) register_keys(...) #endif` mouse-registration call
  (and its now-inapplicable "make hardware cursor position change
  known!!" comment) removed.
- **Verified**: `./build clean && ./build lnx`, exit 0, no new
  warnings (same pre-existing Tier 2 `-Wshadow`/`-Wsign-compare` set
  as before, none in the touched lines). Full smoketest suite, 26/26.
  Manually drove a composer session (`c` from Main Menu, typed body
  text, `^C`-cancelled) through a raw pty to confirm `wmsg()`'s
  mode-line and `wkeyhelp()`'s key-menu-footer painting -- both
  functions this change touched -- still render correctly.
- **Size delta**: `bin/pico` text 110299 -> 110299 (0 bytes) --
  confirmed by diffing against a clean build of the pre-change tree;
  every site removed here was preprocessor-stripped dead weight
  already, same as most of dead-code-sweep buckets 1 and 2. `bin/pine`
  unaffected (pico runs as a separate subprocess, not linked in).

### Finish Tier 2 warning-flag cleanup: `-Wsign-compare`, plus closing out `-Wshadow`/`-Wstrict-prototypes`/`-Wmissing-prototypes`

Closes out the Tier 2 warning-flag item (`-Wsign-compare`/`-Wshadow`/
`-Wstrict-prototypes`/`-Wmissing-prototypes`, all four now enabled in
both `makefile.lnx`s). `-Wstrict-prototypes`/`-Wmissing-prototypes`
(including `imap/ANSI/c-client`, which is in scope for this audit like
every other tree here) and `-Wshadow` were finished in an earlier
commit (`336bd1d`); this session cleared the last flag, `-Wsign-compare`
(52 raw warnings after merging `origin/main`'s bucket-3/`tinfo.c`
dead-code work), across `pine/`, `pico/`, and their generated-file
fragments. A clean `./build clean && ./build lnx` now reports **zero**
warnings of any kind across both Tier 1 and Tier 2 flag sets.

Nearly all 52 sites were `size_t`-vs-`int`/`long` or
`unsigned`-vs-`long` comparisons (`strlen()` against an `int` byte
budget, or c-client's `unsigned long nmsgs`/`msgno`/`size.bytes`
against pine's signed `MsgNo`/`long` message-number types) where the
values involved are non-negative in every real call path, so the fix
is a same-signedness cast at the comparison (not a suppression --
each site was checked for whether the "wrong" side could actually go
negative first; see the two real bugs below, which are exactly the
cases where it could). Representative fix:
`pine/pine.h`'s `FETCH_ALL_FLAGS` macro (used by `pine.c`/`mailcmd.c`)
compared a signed `faf_msgno` against `s->nmsgs` (c-client's
`unsigned long`) -- cast the latter to `long` once in the macro body
rather than at every call site. Similarly `pine/mailindx.c`'s
`compare_size()` had `size_a = mc != NULL ? mc->rfc822_size : -1;`
where the ternary's usual-arithmetic-conversions silently promoted the
whole expression (and thus the `-1` sentinel) to `unsigned long`
before it got assigned back into the signed `long size_a` -- harmless
on any two's-complement platform (round-trips back to `-1`) but worth
an explicit `(long)mc->rfc822_size` cast so the ternary's type doesn't
depend on that implementation detail.

**Two real bugs found and fixed along the way** (per CLAUDE.md's
standing note that finding genuine bugs during an audit like this is
expected, not scope creep):

- **`pine/strings.c`'s `repeat_char()`: off-by-one static-buffer
  overflow, plus no negative-`n` guard.** The original bound check was
  `if(n > sizeof(bb)) n = sizeof(bb) - 1;` with `static char bb[512]`.
  Two bugs: (1) it's `>` not `>=`, so `n == 512` exactly (a legal,
  in-range-looking value) sailed through unclamped, and the very next
  line, `bb[n--] = '\0'`, wrote `bb[512]` -- one byte past the end of
  the 512-byte static array; (2) `n` is `int` and the comparison's
  unsigned promotion (`sizeof` returns `size_t`) happened to clamp
  negative `n` up to 511 as a side effect, which is the wrong
  direction -- a negative `n` should never reach the fill loop at all.
  Reachable in practice via `pine/mailindx.c:985`'s
  `repeat_char(mess_count, '.')`, the "Still sorting.." progress-dots
  call in `sort_blip()` -- `mess_count` increments once per
  `DISPLAY_COUNT`-comparisons-and-15-real-seconds-elapsed tick during
  a qsort of the folder index, so it reaches exactly 512 only on a
  folder sort that runs uninterrupted for roughly 2+ hours. Essentially
  unreachable on this x86-64 dev box, but exactly the class of bug
  CLAUDE.md flags as invisible here and potentially fatal on the actual
  PDP-11 target, where sorting a large folder could plausibly take
  that long. Fixed to clamp both ends explicitly:
  `if(n < 0) n = 0; else if(n >= (int)sizeof(bb)) n = sizeof(bb) - 1;`.
  **Verified per the CLAUDE.md ASan-repro protocol**: extracted
  `repeat_char()` into a standalone file gated `#ifdef FIXED`, called
  with `n == 512`. Buggy version: `AddressSanitizer: global-buffer-
  overflow ... WRITE of size 1 ... 0 bytes after global variable 'bb'
  ... of size 512`. Fixed version: clean exit, `len=511`. (Valgrind
  would not have caught this one -- it's a global/static array, not a
  heap allocation, and per CLAUDE.md's note valgrind also can't reason
  about adjacent-variable overflows even for locals.) No regression
  test added under `test/cases/` -- reaching `n == 512` through the
  real code path needs ~2 hours of continuous folder-sort wall time,
  which isn't practical to drive through the pexpect-based pty
  harness; the ASan repro above is the verification record instead.
- **`pine/mailcmd.c`'s `expunge_and_close()`: narrow-terminal folder-
  name truncation silently skipped.** `max_folder = screen_cols - 50`
  is a signed `long` that goes negative on a terminal narrower than
  ~50 columns; the old `if(strlen(temp) > max_folder)` compared
  `size_t` against `long`, so a negative `max_folder` got promoted to
  a huge unsigned value and the "does the folder name need
  truncating" check always came back false exactly when truncation
  was most needed. Not a buffer overflow (`short_folder_name` is sized
  identically to the already-bounded `temp`), just a cosmetic
  wrong-branch bug on pathologically narrow terminals -- fixed with an
  explicit `(long)strlen(temp) > max_folder`, which now correctly
  treats "no room at all" (negative budget) as "truncate". Found the
  same pattern, same fix, in `pico/display.c`'s `modeline()`
  (`term.t_ncol-n-22` going negative on a narrow terminal skipped the
  `.../`-truncation of a long buffer filename in the mode line) and
  its pad-to-width loop a few lines down (`term.t_ncol - strlen(cp)`
  going negative would have made the `while(n < ...)` padding loop
  run far longer than intended, though `vtputc()`'s own internal
  `vtcol >= term.t_ncol` column clamp stops it from actually writing
  out of bounds -- a latent hang/wasted-cycles bug on a tiny terminal,
  not memory corruption). All three fixed the same way: cast the
  `strlen()` side down to `int`/`long` so the comparison stays signed
  and a negative budget compares as "no room" rather than wrapping to
  "unlimited room".

Also found and fixed one pre-existing `extern`/definition type
mismatch surfaced while chasing a `-Wsign-compare` site:
`pico/line.c` defines the kill-buffer-used counter as `unsigned kused`,
but `pico/random.c`'s `yank()` had a stale `extern int kused;` --
mismatched-type external declarations for the same object are
undefined behavior (harmless in practice here since `int`/`unsigned
int` share representation on every platform this project targets, but
still wrong). Grepped and confirmed the declaration was never actually
referenced by name inside `yank()` (it calls `kremove()`, which
internally reads the real `kused`) -- dead and wrong, so removed
outright rather than just correcting its type.

- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings of
  any kind (Tier 1 and all four Tier 2 flags). Full smoketest suite,
  26/26. Standalone ASan repro for the `repeat_char()` fix as
  described above.
- **Remaining**: Tier 2 warning-flag cleanup is now fully closed. What's
  left open in `TODO-pdp11.md` is the ASan/`-fanalyzer` build-target
  item (this session's `repeat_char()` repro was done ad hoc per
  CLAUDE.md's existing manual process, underscoring that a real build
  target would make this kind of check routine instead of one-off).

### Remove `-k`/`F_USE_FK`/`init_keyboard`/`end_keyboard`

This target is a UUCP-connected local station: real serial terminals
or a Linux console, never NCSA Telnet on a PC. `init_keyboard()`/
`end_keyboard()` (`ttyin.c`) sent `ESC[99h`/`ESC[99l` to put a vt100's
keypad in "function key" mode, but only when `termname` (from
`$TERM`/`tgetent()`) was exactly `"vt100"` or `"vt102"` *and*
`F_USE_FK` was on -- and `F_USE_FK` defaults off, only ever turned on
by the `-k` flag or a `pinef` argv0 alias. The function's own comment
self-describes the sequence as "a hack for NCSA telnet on an IBM PC,"
not a standard DEC keypad-mode escape (real DECKPAM/DECKPNM is
`ESC=`/`ESC>`). Asked by the user whether this machinery was needed
for real vt100 hardware or a Linux console; the answer was no for
either, so removed `-k`, `F_USE_FK`, and `init_keyboard`/
`end_keyboard` entirely, collapsing every reader to its permanent
`F_USE_FK`-off outcome.

- **`pine/ttyin.c`**: deleted `init_keyboard()`/`end_keyboard()` and
  the `extern char termname[]` used only by them. Collapsed
  `optionally_enter()`'s `PF2`/`PF3`/`^C` cases (PF2 now always
  bleeps instead of acting as Enter; PF3 now always bleeps instead of
  conditionally cancelling; `^C` cancels unless `disallow_cancel`) and
  `validatekeys()` (PF1-PF12 are now unconditionally `KEY_JUNK`, since
  function-key input mode can no longer be active). Removed the
  now-dead `F_SET(F_USE_FK,...)` restore in `read_char()`'s
  initial-command-sequence handling, and the `end_keyboard()` call in
  `read_with_timeout()`'s fatal-read-error path.
- **`pine/args.c`**: removed the `-k` flag's `case 'k'` handler, its
  `args_help()` line, the `pinef` argv0-suffix alias that also set
  `F_USE_FK`, and the now-unused `a0_len` local that alias needed.
- **`pine/pine.h`**: removed the `F_USE_FK` feature-bit `#define`
  (bit 2, numerically last, so no renumbering needed) and the
  `orig_use_fkeys:1` bitfield from `struct pine` -- it existed solely
  to save/restore `F_USE_FK` around the composer subprocess call and
  has no other reader once `F_USE_FK` is gone.
- **`pine/init.c`**: removed `process_init_cmds()`'s two
  `F_TURN_ON`/`F_TURN_OFF(F_USE_FK,...)` calls; `fkeys`/`not_fkeys`
  stay, still used by the "can't mix characters and function keys"
  error check above them.
- **`pine/imap.c`, `pine/mailbox.c`, `pine/signals.c`**: removed the
  paired `end_keyboard()`/`init_keyboard()` calls bracketing the
  disk-full "drop to a shell" escape hatch (`imap.c`/`mailbox.c`, two
  near-identical copies of the same retry function) and `do_suspend()`
  (`signals.c`).
- **`pine/pine.c`**: removed the startup `init_keyboard()` call and
  four more `end_keyboard()` sites (address-on-command-line exit,
  folder-open failure, `quit_screen()`, `panic()`); collapsed the
  default-command-key ternary (`ch = F_ON(F_USE_FK,...) ? f_key :
  key`) to always use `key`; collapsed the "command not defined" `"F1"
  : "?"` ternary to always `"?"`; collapsed `show_main_screen()`'s
  keymenu-name ternary (`f_key` vs `key`) to always `key`; removed
  `do_menu()`'s `use_fkeys` local and its `""  : pretty_command(key)`
  ternary, always showing the key name now. Since `mkeys[].f_key` had
  no readers left anywhere after these collapses, removed the
  `f_key` field from `struct menu_key` and its value from all 11
  `mkeys[]` initializer entries (chain-reaction orphaned-field
  cleanup, same pattern as prior buckets).
- **`pine/send.c`**: removed the dead `F_SET(F_USE_FK,...)` restore
  in the composer's initial-command-sequence-cancel block, and the
  `pine_flags |= F_ON(F_USE_FK,...) ? P_FKEYS : 0` line (pico's own
  `-k`/`MDFKEY` toggle in `main.c` is independent and untouched --
  this only removed pine's now-permanently-false contribution).
- **`pine/mailcmd.c`, `pine/mailview.c`**: collapsed three more
  `"F1" : "?"`/`PF4+rc : ctrl('T')` ternaries (bogus-command message
  in `mailcmd.c` and `mailview.c`; the save-to-folder `^T` extra-key
  setup in `mailcmd.c`, two sites) to their `F_USE_FK`-off half.
- **`pine/screen.c`**: `output_keymenu()` and `format_keymenu()` had
  the deepest `ufk` (`F_ON(F_USE_FK,...)`) entanglement -- column-width
  math throughout `format_keymenu()` branched between `strlen(name)`
  (alpha labels) and a fixed `2`/`3` width (`"F1"`.."F12"` labels).
  Removed the `ufk`/`fkey` locals in both functions and collapsed
  every branch to the alpha-label arithmetic; also collapsed
  `format_keymenu()`'s cache-invalidation condition -- previously
  `(F_ON(...) && orig_use_fkeys) || (F_OFF(...) && !orig_use_fkeys)`,
  which is unconditionally true once both operands are permanently
  false -- to just always refreshing the cached width/bitmap.
- **Verified**: `./build clean && ./build lnx`, exit 0, 164 warnings
  both before and after (the pre-existing Tier 2 `-Wshadow`/
  `-Wsign-compare` set -- none new, confirming no orphaned
  locals/params were left behind by the collapses). Full smoketest
  suite, 26/26. Manually drove a raw-pty session: Main Menu and its
  keymenu footer show plain alpha key names (`?`/`C`/`I`/`L`/`S`/`Q`),
  never `F1`-style labels; a bogus command (`9`) produces `Command
  "9" not defined for this screen. Use ? for help` (not `Use F1 for
  help`); `q` still raises the `Really quit pine` confirmation and
  `n` still cancels back to the Main Menu.
- **Size delta**: `bin/pine` text 818456 -> 816608 (-1848 bytes) --
  a real win, since most of this code (the `screen.c` keymenu-layout
  math especially) was genuinely compiled, not preprocessor-stripped.

### Remove `-l`/`-p`/`-P` and their dead `F_EXPANDED_FOLDERS`/`pinerc`/`pine_conf` backing fields

Follow-on survey of every remaining `pine/args.c` flag, asking of each
"does anything still read the state this sets?" Three didn't:

- **`-l`** (`F_EXPANDED_FOLDERS`): grepped the entire tree for any
  `F_ON(F_EXPANDED_FOLDERS,...)` reader -- there was none, anywhere,
  ever (unlike `F_USE_FK`, which at least had real readers before
  this project turned them all off). The surrounding `/* JUST
  WHILETESTINGFOLDERS */`/`/* END OF WHILETESTINGFOLDERS */` comments
  suggest this was UW-internal test scaffolding that never got wired
  to anything. Removed the `-l` case, its `args_help()` line, and the
  `F_EXPANDED_FOLDERS` `#define` (bit 1; `F_CAN_SUSPEND`, bit 0, is
  now the only feature bit left).
- **`-p <pinerc>`**/**`-P <pine.conf>`**: both just stored a path into
  `pine_state->pinerc`/`pine_conf`, but `read_pinerc()` -- the only
  function that would ever open either path -- was already deleted
  entirely in an earlier session ("Removed `read_pinerc()` and
  `first_time_user` entirely", above). Grepped for every `->pinerc`/
  `->pine_conf` reference: only the `-p`/`-P` writes themselves and
  `init.c`'s `if(!ps_global->pinerc) build_path(...)` default-path
  fallback (itself just populating a field nothing reads). Removed
  both flag cases, their `args_help()` lines, the `pinerc`/`pine_conf`
  fields from `struct pine` (`pine.h`), and `init_vars()`'s default-path
  block along with the now-unused `buf[MAXPATH+1]` local it needed.
- **Kept**: `-h`, `-z` (`F_CAN_SUSPEND`, real job-control suspend),
  `-i`/`-n` (`start_in_index`/`start_entry`, read in `pine.c`/
  `init.c`/`mailcmd.c`), `-o` (`open_readonly_on_startup`, read in
  `mailcmd.c`), `-f` (folder to open), `-sort`, `-I` (initial
  keystrokes, drives `process_init_cmds()`), and `-d` (already
  `#ifdef DEBUG`-gated, feeds `dprint()`) -- all confirmed to have
  real live consumers.
- **Test harness fallout**: `test/smoketest.py`'s `Session` was
  unconditionally passing `-p <tmp-path>` to every spawned `bin/pine`
  (to isolate `.pinerc` reads from a real one, back when
  `read_pinerc()` still existed) -- with `-p` gone this made every
  single test hit `args.c`'s `default: ... ++usage;` path and exit
  via `args_help()`, dropping the suite from 26/26 to 2/26. Fixed by
  no longer passing `-p` at all and just writing the seed `.pinerc` to
  the conventional `$HOME/.pinerc` path when a test asks for one --
  `test_pinerc_not_parsed.py` still writes there and still confirms
  the content is never consulted, since nothing reads that path by
  any mechanism now. Updated both files' stale comments (referencing
  the since-fully-removed `read_pinerc()`/`first_time_user`) to match.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  before and after (this merge's post-`two/main` baseline -- none
  new). Full smoketest suite, 26/26 (after the harness fix above;
  first run without it caught the `-p` breakage immediately).
- **Size delta**: `bin/pine` text 816328 -> 815600 (-728 bytes).

### Remove `test_pinerc_not_parsed`, its `.pinerc` seeding, and disable `-DDEBUG`

Follow-on to the `-p`/`-P` removal above. Two more loose ends:

- **`test/cases/test_pinerc_not_parsed.py`**: now that `-p` is gone
  and nothing reads `.pinerc` by any mechanism, this test's own
  premise (seed a `.pinerc`, confirm it's ignored) has nothing left
  to guard against -- there's no code path left that could regress
  into reading it. Deleted the test case, and removed
  `test/smoketest.py`'s `Session(pinerc=...)` parameter and its
  `.pinerc`-file-writing logic (its only caller).
- **`pine/makefile.lnx`**: `DEBUG=-g -DDEBUG` unconditionally compiled
  in the `-DDEBUG` preprocessor define, which is what makes `dprint()`
  (`pine.h`) expand to a real `fprintf`-to-`debugfile` call instead of
  its no-op `#else` branch -- so hundreds of `dprint(...)` call sites
  throughout the tree, plus the `-d`/debug-level argv handling and the
  `debug`/`debugfile` globals, were all being compiled into every dev
  build. Changed `DEBUG=` to just `-g` (keeping debug symbols for
  gdb, dropping the `DEBUG` macro itself). Every `#ifdef DEBUG` site
  checked beforehand was already cleanly gated (`headers.h`'s
  `debug`/`debugfile` externs, `pine.h`'s `init_debug()` prototype,
  `args.c`'s `-d` case), so no orphaned-reference fallout.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  (unchanged -- the dropped `dprint()` call sites weren't warning
  sources). Full smoketest suite, 25/25 (one fewer than before,
  matching the deleted test case).
- **Size delta**: `bin/pine` text 815600 -> 780448 (**-35152 bytes**)
  -- by far the largest single-change win in this project so far,
  confirming `dprint()`'s hundreds of call sites (format strings,
  `debugfile`/`debug` checks, the arguments computed just to log them)
  were real compiled weight, not preprocessor-stripped noise. Left
  the underlying `DEBUG`-gated machinery itself in place -- see the
  new TODO to investigate removing it outright rather than just
  disabling the build define.

### Remove `NFSKLUDGE` -- irrelevant for a local-mbox-only, no-NFS target

`imap/ANSI/c-client/bezerk.c`'s `bezerk_lock()` had an `#ifdef
NFSKLUDGE ... #else ... #endif` pair choosing between two mailbox-
lock-file creation strategies: `NFSKLUDGE`'s "hitching post" temp-file-
plus-`link()` dance (needed only because NFS's `O_EXCL` isn't reliably
atomic over the wire) versus a plain `open(...,O_EXCL,...)`, which is
atomic and sufficient on any real local filesystem. `NFSKLUDGE` was
`#define`'d in `os_lnx.h` (this build's dev target) but **not** in
`os_bsi.h` (the closest existing precedent among the other-platform
headers to the eventual 2.11BSD cross-build) -- confirming it was
wrong for this no-NFS, local-mbox-only, UUCP-connected target, dev
build included.

- **What changed**: removed `#define NFSKLUDGE` from `os_lnx.h`, then
  collapsed `bezerk_lock()`'s `#ifdef NFSKLUDGE`/`#else`/`#endif` down
  to just the `#else` (plain `O_EXCL`) body -- deleted the opening
  `#ifdef NFSKLUDGE` line through the `#else` line (the entire
  hitching-post branch, ~32 lines including its "SUN-OS had an NFS...
  kludgy as an albatross" comment), and the closing `#endif`. Left
  `j` declared (`int fd,ld,j;`) since it's reused later in the same
  function (`j = errno;` in the error-path cleanup) independent of
  either branch. `imap/ANSI/c-client/osdep.h` is a gitignored
  build-time hard link of `os_lnx.h` (`ln os_lnx.h osdep.h`, same
  pattern as `pine/os.c`), so it picked up the change automatically on
  rebuild -- no separate edit needed. The other `os_*.h` variants that
  still define `NFSKLUDGE` (`os_a32.h`/`os_aix.h`/`os_nxt.h`/
  `os_osf.h`) are other-platform portability reference material,
  untouched per this project's established precedent.
- **Verified**: unlike most `#ifdef` collapses here, this changes
  *live, exercised* locking behavior, not a dead branch -- so beyond
  `./build clean && ./build lnx` (exit 0, 52 warnings, unchanged) and
  the full smoketest suite (25/25), manually drove real lock
  contention: pre-created a fresh `<mbox>.lock` file, launched
  `bin/pine -f <mbox>` against it via a raw pty, and confirmed it hit
  the `EEXIST` branch and printed the periodic "Folder ... is locked,
  will override in N seconds..." status message (correctly timed to
  the retry loop's `i%15==0`/`LOCKTIMEOUT`-derived cadence, confirming
  the surrounding retry-loop bookkeeping the edit sits inside was
  untouched); removed the lock file mid-wait and confirmed the retry
  loop picked it up and opened the folder normally (reached MAIN
  MENU); confirmed pine's own lock file was cleaned up on quit.
- **Size delta**: `bin/pine` text 780448 -> 780168 (-280 bytes).

### Remove read-only mode (`-o` flag)

`-o` (`pine/args.c`) set `pine_state->open_readonly_on_startup`,
consumed exactly once (`mailcmd.c`, ~line 1513) to OR `OP_READONLY`
into the first startup folder-open's mode flags, then self-clear.
Grepped the whole tree for `OP_READONLY`: that one site was the
*only* place it was ever passed to `mail_open()`/`context_open()`
anywhere in this codebase -- no other caller, no pinerc-driven
default, nothing. On a single-user local mail station there's no
concurrent-access reason to defensively open read-only, so removed
the flag entirely.

- **What changed**: removed `args.c`'s `case 'o'` and its
  `args_help()` line, `pine.h`'s `open_readonly_on_startup:1`
  bitfield, and `mailcmd.c`'s `if(ps_global->open_readonly_on_startup)
  { openmode |= OP_READONLY; ... }` block. `openmode` itself stays --
  it's also OR'd with `OP_DEBUG` a few lines earlier, so it remains a
  live, meaningful local.
- **Left untouched (confirmed genuinely separate)**: `MAILSTREAM`'s
  general `->readonly` field has its own independent setters --
  `bezerk.c`'s `bezerk_open()` sets it itself when the lock-file
  couldn't be acquired (`stream->readonly = !LOCAL->ld;`, a real
  runtime permission/contention detection, nothing to do with `-o`)
  and when the mailbox name starts with `*`; `pine/signals.c` sets it
  directly during crash/dead-stream cleanup. All of `->readonly`'s
  readers (`mailcmd.c`, `mailview.c`, `newmail.c`, `screen.c`'s
  titlebar `ReadOnly`/`Normal` display) don't care why a stream is
  readonly, so they're unaffected either way. Verified via grep that
  `OP_READONLY` had exactly one setter in the entire tree before this
  change (the one just removed) and zero after.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  (unchanged). Full smoketest suite, 25/25 -- none of them exercised
  `-o`, so no manual live-behavior check was needed beyond confirming
  every test's normal (non-`-o`) folder-open still works, which the
  suite already covers extensively.
- **Size delta**: `bin/pine` text 780168 -> 779840 (-328 bytes).

### Make suspend (`^Z`) the default, remove the `-z` flag

`-z` (`pine/args.c`) turned on the `F_CAN_SUSPEND` feature bit,
checked at ~10 call sites almost always paired with a
`have_job_control()` check (`pine/os.c`, unconditionally returns 1 on
this Linux target) that already tests the real runtime capability.
2.11BSD has job control, so made `^Z` suspend always available
(equivalent to always passing `-z`) and removed the flag, as a
two-phase job like the `F_USE_FK` cut.

- **Phase 1** -- collapsed every `F_ON(F_CAN_SUSPEND,...)`/
  `F_OFF(F_CAN_SUSPEND,...)` site to its fixed-true outcome:
  - `pine/args.c`: removed the `-z` case and its `args_help()` line.
  - `pine/mailbox.c`, `pine/imap.c` (identical duplicated disk-full
    "drop to shell" recovery prompt): collapsed the `if(F_ON(...)){^Z
    prompt} else {! prompt}` to always show the `^Z` prompt. The
    second site, `while(!(ch == F_ON(F_CAN_SUSPEND,ps_global) ?
    ctrl('Z') : '!' || ch == '\r'))`, turned out to already be a
    mis-parenthesized ternary (see the new TODO) -- substituted the
    literal `1` for the macro call to preserve the exact existing
    (buggy) behavior rather than silently fixing an unrelated bug
    as a side effect of this collapse.
  - `pine/mailindx.c`, `pine/folder.c`, `pine/mailview.c`, `pine/pine.c`
    (four near-identical `case ctrl('Z'): if(!have_job_control())
    goto <bleep-label>; if(F_OFF(F_CAN_SUSPEND,...)){message; break;}
    else {do_suspend(...);}` sites): collapsed to unconditionally
    calling `do_suspend(...)`, dropping the "Pine suspension not
    enabled" message branch entirely.
  - `pine/status.c`: `if(!have_job_control() ||
    F_OFF(F_CAN_SUSPEND,ps_global))` collapsed to just
    `if(!have_job_control())`.
  - `pine/ttyin.c` (`optionally_enter()`'s `^Z` case): `if(F_ON(...))
    {suspend} else {goto bleep;}` collapsed to the suspend body
    unconditionally.
  - `pine/send.c`: `pine_flags |= (F_ON(F_CAN_SUSPEND,...)) ?
    P_SUSPEND : 0;` collapsed to `pine_flags |= P_SUSPEND;`.
- **Phase 2** -- with `F_CAN_SUSPEND` gone, grepped for any remaining
  `F_ON`/`F_OFF`/`F_TURN_ON`/`F_TURN_OFF`/`F_SET`/`feature_list`
  reference and found none, confirming it really was the last tenant
  of the feature-list framework. Deleted all five macros, the
  `F_CAN_SUSPEND` `#define`, and the `feature_list` `BITMAP` field
  from `struct pine` (`pine.h`) -- the entire abstraction is gone.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  (unchanged). Full smoketest suite, 25/25. Since this makes `^Z`
  suspend *live* behavior for the first time by default (previously
  only reachable via `-z`), drove a real pty session past a clean
  rebuild: sent `^Z`, confirmed `bin/pine`'s process actually entered
  the kernel's `T (stopped)` state (`/proc/<pid>/status`) right after
  the "Pine suspended. Give the \"fg\" command to come back." message
  -- i.e. `stop_process()`'s real `kill(0, SIGSTOP)` fired, not just
  the message -- then sent `SIGCONT` and confirmed the process
  returned to `S (sleeping)` and pine correctly redrew back to MAIN
  MENU (`init_screen()`/`init_tty_driver()` reinitialization intact).
- **Discovered but not fixed**: the mis-parenthesized ternary in
  `mailbox.c`/`imap.c` noted above -- added as its own TODO rather
  than fixed inline, since fixing it needs a dedicated repro (forcing
  a mailbox write failure) independent of this task.
- **Size delta**: `bin/pine` text 779840 -> 779024 (-816 bytes).

### Formalize an ASan build target (`ASAN=1`); evaluate `-fanalyzer` signal quality

Two-part task. Part 1 promoted CLAUDE.md's existing ad hoc ASan repro
process (extract-and-compile-standalone after a bug is already
suspected) into a real, standing build target so the whole tree --
`c-client`, `pico`, `pine` -- can be built and smoke-tested under
AddressSanitizer instrumentation directly. Part 2 ran GCC's `-fanalyzer`
as a one-off pass over all three trees and evaluated whether its signal
is good enough to justify a standing target.

**ASan build target**: added an `ASAN` make variable (default empty)
to `imap/ANSI/c-client/makefile.lnx`, `pico/makefile.lnx`, and
`pine/makefile.lnx`; `ifeq ($(ASAN),1)` appends `-fsanitize=address` to
each file's `CFLAGS`. No changes to the `./build` script itself were
needed -- its argument parser already forwards any non-3-character,
non-`clean` argument straight through to `make` as-is (it only
special-cases exactly-3-char strings as the target-platform selector,
per its `case $1 in ... clean|???)` glob), so `./build lnx ASAN=1`
Just Works: `makeargs` becomes `CC=cc ASAN=1`, forwarded verbatim to
all three sub-`make`s. Chose CFLAGS-only over touching LDFLAGS too --
`pico`'s single-line `$(CC) $(CFLAGS) main.c libpico.a ... -o pico`
and `pine`'s final link line (`$(CC) $(LDFLAGS) $(CFLAGS) -o pine
...`) both already include `$(CFLAGS)` at the link step, so appending
there alone instruments both compilation and linking; `c-client` only
ever produces a static `.a`, so `CFLAGS` alone covers it.

**Immediately found a real, reachable stack-buffer overflow**: running
the full smoketest suite under the new `ASAN=1` build, `test_save_
attachment_overflow` (an *existing* regression test, previously green)
crashed with `AddressSanitizer: stack-buffer-overflow ... in build_path
os.c:254`. `pine/os.c`'s `build_path(pathbuf, first_part, second_part)`
(real source: `pine/osdep/bld_path`) does `sprintf(pathbuf, "%s/%s",
first_part, second_part)` with, per its own docstring, "No check is
made for overflow." `pine/mailcmd.c`'s `save_attachment()` calls
`build_path(full_filename, ps_global->home_dir, filename)` where
`full_filename` is `char[MAXPATH+1]` (4097 bytes) and `filename` --
an attachment's MIME `name=` parameter, already capped to `MAXPATH`
(4096 bytes) by an earlier fix (see `test_save_attachment_overflow`'s
own header comment) -- can itself be nearly 4096 bytes. `home_dir +
"/" + filename` can therefore exceed `full_filename`'s capacity even
though `filename` alone fits, a gap the earlier fix didn't cover
since it never looked past `save_attachment()` into `build_path()`
itself. A second, identical call site exists at `mailcmd.c:1054`.
**Fixed at the root**: gave `build_path()` an explicit `pathbuf_size`
parameter and switched its body to `snprintf`/`strncpy` with explicit
truncation, rather than patching just the two currently-reachable
overflow sites -- `build_path()` had *no* bound checking at any of its
9 call sites tree-wide (`folder.c`, `init.c` x5, `pine.c`, `send.c` x2,
`mailcmd.c` x2, `reply.c`), all passing fixed-size stack/static
buffers. Updated the prototype in `pine.h` and every call site to pass
`sizeof(pathbuf)`. Verified with a standalone `#ifdef FIXED`-gated
ASan repro of just the buggy `sprintf` pattern (buggy: `global/stack-
buffer-overflow ... WRITE of size ...`; fixed: clean exit) in addition
to the full ASan smoketest run going 25/25 green (it had been 24/25
with this one crash before the fix). Full clean rebuild (both plain
and `ASAN=1`) stayed at 0 warnings throughout.

**`-fanalyzer` evaluation**: ran `cc -fanalyzer -c` (not
`-fsyntax-only` -- confirmed via a synthetic use-after-free test that
`-fanalyzer`'s actual analysis pass only runs on a real `-c` codegen
invocation on this GCC 16.1.1) over every `.c` file in `pine/`,
`pico/`, and `c-client`'s five source files. Fast (~5-9s per tree,
not the "slow" the TODO worried about) and modest volume (order of
30 raw warnings total across all three trees), but with a real,
identifiable false-positive pattern: **whenever an invariant is
threaded through a heap-allocated struct field across a function call
or loop iteration boundary, `-fanalyzer` loses track of it and flags a
false NULL-deref/use-after-free.** Confirmed by hand for the two
largest clusters:
  - `pine/folder.c`: 16 warnings, all "possible NULL deref of
    `folder_entry(...)`'s return." `folder_entry()`'s NULL return is
    conditioned on `i >= ((FLIST*)flist)->used`; every flagged call
    site is guarded by a `folder_total(flist)` check (which returns
    that exact same `->used` field) or a `*index < ftotal` loop bound
    -- `-fanalyzer` doesn't connect the two accessor functions'
    shared invariant, so it can't see the guard. Confirmed false
    positive at every site checked.
  - `pico/line.c`: "use after free of `dotp`" in `ldelete()`'s
    `while(n != 0)` loop. `dotp` is re-read fresh (`dotp =
    curwp->w_dotp`) at the top of every iteration; `lfree()` (called
    several stack frames down, via `ldelnewline()`) already redirects
    every window's `w_dotp` away from the line it's about to free
    *before* freeing it. `-fanalyzer` taints the *local* `dotp` once
    its target is freed but doesn't model that the next iteration's
    fresh read of `curwp->w_dotp` picks up `lfree()`'s own
    already-applied fixup. Confirmed false positive.
  - `pico/browse.c`'s `getfcells()` "use of uninitialized `tcp`" is
    the same shape one level simpler: `tcp` is unconditionally set at
    the end of every loop iteration, and the only read of it (in the
    `else` branch) is gated on `mp->head != NULL`, which is only true
    from the second iteration onward -- structurally provable safe,
    analyzer just can't see it because the guard is a heap struct
    field (`mp->head`), not a plain local.

  Despite that recurring blind spot, `-fanalyzer` also found several
  **genuine, fixed** bugs, roughly in ascending order of severity:
  - `pine/filter.c`'s `gf_link_filter()`: local `char c` passed
    by-value into every registered filter function on `GF_RESET`,
    uninitialized. Every filter's `GF_RESET` branch checked (9 of
    them) ignores the character argument, so harmless in every
    current filter, but initialized to `0` anyway as free insurance
    against a future filter that doesn't follow the convention.
  - `pine/reply.c`'s `fetch_contents()`: local `got_one` used at
    `if(!got_one) return(1);` without ever being initialized -- if
    every message part hit the error path (`*ep++ = part_count`)
    instead of the success path, `got_one` was read as indeterminate
    stack garbage instead of reliably triggering the "no parts
    fetched" bailout. Initialized to `0`.
  - `pico/random.c`'s `showcpos()`: `thisline`/`nbc` are only set
    inside a loop when the traversal reaches the cursor's exact
    position, which should always happen given the buffer/window
    invariants pico is supposed to maintain -- but initialized both to
    `0` anyway as defense-in-depth against those invariants ever being
    violated, matching the fix applied to the other two locals in the
    same function that already had bulletproof initialization.
  - `pico/composer.c`'s `resolve_niks()`: `sbuf_size = (size_t)i`
    where `i` stays `0` if `headents[entry].hd_text` is `NULL` (which
    it only would be if an earlier `InitEntryText()` call's `HALLOC()`
    failed) -- `malloc(0)` can return a non-NULL, zero-capacity
    allocation on glibc, and the following `*sbuf = '\0'` would then
    overflow it by one byte. Only reachable under an allocation
    failure during composer startup, but PDP-11-relevant given the
    project's tiny target memory budget makes allocation failure far
    more plausible there than on this dev box. Added an explicit
    `sbuf_size == 0` guard alongside the existing malloc-NULL check.
  - `pico/composer.c`'s `SaveHeaderLines()`: `while(--bp >= buf)`
    decrements-then-checks, so in the "every copied character was a
    trailing space" case `bp` is decremented one past `buf` *before*
    the bound check runs -- comparing a pointer before the start of an
    array with a relational operator is undefined behavior per the C
    standard even though it happens to evaluate as expected (false)
    on every mainstream target/compiler this project has ever run on.
    Rewrote as check-then-decrement (`while(bp > buf){ --bp; ... }`)
    so `bp` never forms an out-of-bounds pointer value at all,
    identical behavior in every case that hits `break`, and now
    bottoms out at `buf` instead of `buf-1` in the no-`break` case
    (a value nothing downstream reads anyway).
  - `pico/tcap.c`'s `kpinsert()`: two `newnode()` (a `malloc()`
    wrapper) call sites dereferenced the result without a NULL check,
    unlike every other allocation in this codebase. Added NULL checks
    that bail out (`return(0)`) on allocation failure -- consistent
    with the function's existing "return value carries no meaning,
    every path returns 0" convention (confirmed via grep: none of its
    ~30 call sites, all in terminal-capability init, check the
    return value).
  - `pico/display.c`'s `wstripe()`: called `pscr(line, i)->c` without
    checking `pscr()`'s own documented NULL return (out-of-range
    line/column) -- the `i` bound is checked by the caller's own loop
    guard, but `line` (the function's own parameter) is never
    validated before being handed to `pscr()`. Added a NULL check
    that returns early ("line out of range") rather than crashing.
  - **`pico/browse.c`'s `BrowserAnchor()` -- a real, verified crash,
    the most serious of this batch.** `while((p = strchr(p,
    C_FILESEP)) != NULL) if(...) break; else p++;` searches for a
    path separator positioned late enough in `dir` to fit the
    available display width; if none exists (a single path component
    too long to fit, e.g. a long final directory name on a narrow
    terminal), the loop exits with `p == NULL` by construction of its
    own condition. The very next line was `if(*p == '\0')` -- meant
    to detect exactly this "search exhausted" case (its own comment
    says `/* no suitable length! */`) but checking the wrong thing
    entirely: dereferencing `p` unconditionally, guaranteed-NULL in
    exactly the case it was trying to detect. **Verified with a
    standalone extracted repro**: buggy version segfaults
    (`AddressSanitizer: SEGV on unknown address 0x0`) with a long
    final path component and narrow terminal width; fixed version
    (`if(p == NULL)`) runs clean. This is a real, user-triggerable
    crash (browse a directory whose current path's last component
    alone is too long for the terminal width) that had nothing to do
    with signedness or any of this project's prior audit passes --
    purely a wrong-variable-checked logic bug, caught only because
    `-fanalyzer` happened to trace the NULL-producing path through
    `strchr()`'s loop.
  - `imap/ANSI/c-client/bezerk.c`'s `bezerk_search_string()`: the
    `default:` (atomic string) case of a `switch` handling local
    mbox-format SEARCH criteria parsing did `*n = strlen(*d = strtok
    (c," "))` with no NULL check, while the `case '"':` (quoted
    string) branch two lines above it *already* uses the correct
    defensive idiom (`if(!(strchr(...) && (*d = strtok(...)) && (*n =
    strlen(*d)))) return NIL;`) for the exact same `strtok`-can-
    return-NULL risk. Applied the sibling branch's own established
    pattern to the `default:` case. Since this build has no TCP/IP,
    the SEARCH criteria string this parses only ever originates from
    pine's own local "search messages" UI (never network input), so
    real-world severity is a crash-on-unusual-search-string rather
    than a network-facing vulnerability, but still a genuine
    unguarded-NULL-deref bug worth the one-line fix.
  - **Not fixed, deliberately**: `pine/signals.c`'s `hup_signal()`/
    `term_signal()` and `pico/os_unix.c`'s SIGINT handler all call
    `exit()` (not async-signal-safe) from within a signal handler,
    correctly flagged `CWE-479`. Already call `fast_clean_up()`/
    `printf()` (also not signal-safe) before `exit()`, so the handler
    was never async-signal-safe to begin with by the strict standard
    -- this is a decades-old, extremely common pattern in single-
    threaded interactive Unix programs of this vintage for
    intentional-termination-on-signal, not a new risk introduced
    here. Switching to `_exit()` would skip flushing the "Pine
    finished..." `printf()` output and any `atexit()`-registered
    cleanup, a real behavior-change risk not worth taking without a
    dedicated repro/verification cycle of its own -- left as a
    documented, low-priority follow-up rather than changed inline.
  - **Not fixed, too low-value to matter**: `pico/pico.c`'s
    `edinit()` leaks the `bp` allocation if the subsequent `wp`
    allocation fails (or vice versa) -- but this is a one-time
    startup path that either `exit(1)`s the whole process or aborts
    the composer-open attempt entirely on failure, so nothing
    accumulates and the OS reclaims everything moments later either
    way.
  - **Out of scope**: `pine/pine-use.c` (a separate, UW-internal
    usage-statistics utility with its own `pine-use:` makefile target,
    never part of the `pine` binary's dependency chain and compiled
    with no `$(CFLAGS)`/`$(WARNFLAGS)` at all) surfaced a handful of
    stale K&R-era implicit-declaration/int-to-pointer warnings --
    consistent with this project's established precedent of leaving
    never-built-by-`./build lnx` auxiliary tools alone.

  **Verdict**: keep the `ASAN=1` build target as a standing,
  routinely-used tool -- cheap to build, zero false positives
  observed, and it found a real, currently-shipping stack overflow on
  its very first full-suite run. Do **not** promote `-fanalyzer` to a
  standing/enforced CI gate in its current form: fast enough for an
  occasional manual pass and it did catch one serious, real,
  verified crash (`BrowserAnchor()`) plus several smaller genuine
  bugs, but roughly half its raw output on this codebase is a
  systematic false-positive pattern (heap-struct-field invariants
  it can't track) that would need per-site suppression annotations
  to gate on cleanly. Worth re-running by hand after significant
  changes to `folder.c`/`line.c`/`browse.c`-style linked-list/struct
  bookkeeping code, not worth wiring into every build.

- **Verified**: full clean rebuild (both plain and `ASAN=1`), 0
  warnings, 25/25 smoketests in both configurations at every
  checkpoint. Standalone ASan repros for both the `build_path()`
  overflow and the `BrowserAnchor()` NULL deref, confirming
  buggy-crashes/fixed-clean in both cases.
- **Remaining**: the mis-parenthesized-ternary and DEBUG/dprint-
  removal items above are still open. No further `-fanalyzer`
  findings from this pass were left unaddressed except the two
  explicitly deferred items (signal-handler `exit()`, `edinit()`'s
  startup-path leak) noted above.

### Remove the DEBUG/dprint() infrastructure entirely -- deliberate loss of a diagnostic tool, user-confirmed

Follow-on to disabling `-DDEBUG` by default. Asked the user how to
resolve the open TODO ("investigate whether this whole subsystem is
worth deleting"), laying out the tradeoff: ASan/`-fanalyzer` don't
actually substitute for it (they catch memory-safety bugs, not
program-logic tracing), and a PDP-11 with no source-level debugger may
realistically need `dprint()`-style textual tracing for real hardware
bring-up. Recommended keeping it. The user chose to delete it anyway
("any debug printing will be ad-hoc created") -- an explicit, informed
decision to trade away this diagnostic tool for source tidiness, not a
dead-code removal like the rest of this project's cuts.

- **Mechanical `dprint()` removal**: wrote a small comment/string-aware
  Python script (paren-balance tracking, so it doesn't get confused by
  `dprint(9, (debugfile, "foo(bar)", x))`-style nested parens/string
  literals) to find and delete every `dprint(...)` call statement.
  Handled both forms found in the wild: the common `dprint(...);` and
  a handful of sites relying on the macro's own trailing brace (no
  `;`) as the whole statement, e.g. `status.c`'s
  `q_status_message()`'s trailing `dprint(9, (...))\n}` with no
  semicolon before the function's closing brace. Ran it across all 19
  `pine/*.c` files with any `dprint()` call plus the one fragment
  source behind a generated file -- `pine/os.c` is `osdep/os-lnx.c`,
  itself generated from `osdep/os-lnx.ic` via `includer`, but its two
  `dprint()` calls (in `expand_foldername()`) live in the real,
  hand-maintained `osdep/expnfldr` fragment, so edited that instead of
  the generated file. 189 call sites removed total. Verified
  immediately with a clean rebuild (0 errors, 52 warnings unchanged)
  and 25/25 smoketests before touching anything else, to isolate any
  mechanical-script mistakes from the manual cleanup pass to follow.
- **Manual infrastructure cleanup** (everything the mechanical pass
  couldn't touch -- surrounding `#ifdef DEBUG` scaffolding, not just
  the `dprint()` calls themselves):
  - `pine/args.c`: removed the `-d` case (both the "take an argument"
    dispatch and its handler) and its `args_help()` line.
  - `pine/pine.h`: removed the `dprint(n,x)` macro definition (both
    the real and no-op halves) and `init_debug()`'s prototype.
  - `pine/headers.h`: removed the `debug`/`debugfile` extern
    declarations.
  - `pine/init.c`: removed `init_debug()` itself (opens/rotates
    `.pine-debug1-4`), the `int debug`/`FILE *debugfile` global
    definitions, and a `#ifdef DEBUG` block in `init_vars()` that
    dumped every `variables[]` entry (its `STRING`/`LIST` helper
    macros and the loop, now-empty after the `dprint()` inside it was
    already stripped, went too).
  - `pine/pine.c`: removed the `init_debug()` call site, a `case '#'`
    debug-only "test panic and abort" easter egg in the main-menu key
    switch, and the `fclose(debugfile)` in both the normal shutdown
    path and `panic()` (the `coredump()` call in `panic()` stays --
    real, non-DEBUG cleanup code, still called from `signals.c` too).
  - `pine/folder.c`: removed `dump_contexts()` (a whole DEBUG-only
    helper dumping folder-context state to the debug file) and its
    prototype and call site.
  - `pine/signals.c`: `init_signals()`'s signal-handler-installation
    guard was `#ifdef DEBUG #ifdef ULT if(debug<2) #else if(debug<7)
    #endif #endif { signal(...); ... }` -- with `DEBUG` (and thus the
    inner `ULT`, itself never `#define`'d anywhere either) gone, the
    guard disappears entirely and the block of `signal()` calls always
    runs unconditionally. Also removed two `fclose(debugfile)` sites
    (`quit_signal()`, `fast_clean_up()`).
  - `pine/mailcmd.c`: removed `broach_folder()`'s `if(debug>8) openmode
    |= OP_DEBUG;` (c-client's own internal stream-debug flag, now
    permanently off since nothing sets it).
  - `pine/ttyin.c`: removed `optionally_enter()`'s debug-only `^X`
    field-dump key (printed the edit buffer's raw contents).
  - `pine/newmail.c`: removed four now-vestigial `#ifdef DEBUG now =
    time(0); #endif` sites (leftover from stripping the `dprint()`
    calls that were their only reason to exist) and, in
    `check_point()`, a `time_t now;` local plus its two `#ifdef DEBUG
    now = time(0); #endif` sites -- unlike `new_mail()`'s `now` (a
    real parameter used elsewhere), this one was declared and used
    *only* inside the now-empty DEBUG blocks.
  - `pine/osdep/os-lnx.h`: removed the now-orphaned
    `DEBUGFILE`/`NUMDEBUGFILES`/`DEFAULT_DEBUG` `#define`s (only
    `init_debug()` read them). Left every other `os-*.h` variant's
    copies untouched -- other-platform reference material, same
    precedent as `NFSKLUDGE`.
- **Verified no orphaned locals**: since this build's `WARNFLAGS`
  doesn't include `-Wunused-variable`, ran a throwaway `cc
  -Wunused-variable -Wunused-but-set-variable -fsyntax-only` scan over
  every touched file, then repeated the same scan against a `git
  stash`ed pre-change checkout -- the two unused-variable lists were
  byte-for-byte identical (just shifted line numbers), confirming
  every one of those warnings already existed before this change and
  none were newly orphaned by the `dprint()`/DEBUG strip.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  (unchanged). Full smoketest suite, 25/25.
- **Size delta**: `bin/pine` text 779024 -> 778960 (-64 bytes) --
  intentionally small: `-DDEBUG` was already off by default (previous
  entry), so every bit of this code was already compiled out; this
  pass is a source-tidiness win, not a further binary-size one.

### Fix mis-parenthesized ternary in the disk-full "drop to shell" prompt

`pine/mailbox.c`'s and `pine/imap.c`'s identical `mm_diskerror()`
mailbox-write-failure recovery prompt had:
`while(!(ch == 1 ? ctrl('Z') : '!' || ch == '\r'))` -- flagged during
the `F_CAN_SUSPEND` collapse (the `1` used to be
`F_ON(F_CAN_SUSPEND,ps_global)`, itself the same bug). `?:` binds
looser than both `==` and `||`, so this parses as `(ch == 1) ?
ctrl('Z') : ('!' || (ch == '\r'))` -- and since `'!'` is a nonzero
constant, the `else` branch is always truthy, so the loop accepted
almost any keystroke immediately instead of re-bleeping until a real
`^Z`/Return.

- **Reproduction attempts**: tried to force a real disk-full write
  failure live, per CLAUDE.md's verification discipline, via
  `RLIMIT_FSIZE` (`ulimit -f`) with `SIGXFSZ` ignored (confirmed in
  isolation that this correctly turns an over-limit `write()`/`writev()`
  into a clean `errno==EFBIG` return with no signal) plus an mbox with
  enough messages that `bezerk_save()`'s rewrite spans multiple
  `writev()` batches (`bezerk_write_message()` uses 4 iovecs/message,
  so the 16-iovec batch cap is every 4 messages) so a later batch
  would start writing exactly at the rlimit boundary and fail cleanly.
  The rlimit mechanism itself checked out standalone (plain `write()`
  and `writev()` C programs both hit `EFBIG` exactly as expected,
  including when overwriting within an already-larger pre-existing
  file), but driving it through `bin/pine`'s actual expunge-on-quit
  path never tripped `mm_diskerror()` -- the rewritten mailbox came out
  fully intact past the limit every time, for a reason not tracked
  down (not a rabbit hole worth chasing further for a low-severity,
  cosmetic-input-validation bug). Abandoned the live repro as
  impractical to engineer deterministically within reasonable time.
- **Fallback verification** (per CLAUDE.md's own suggested technique
  for exactly this situation): extracted the loop condition into a
  standalone C file with `#ifdef FIXED` gating old vs. new, and fed it
  every case that matters -- five bogus keys (`a`, `x`, ' ', `9`, ESC)
  and the two real ones (`ctrl('Z')`, `'\r'`). The old form accepted
  all 5 bogus keys immediately (should re-bleep) and both real keys
  (correct by coincidence); the fixed form (`ch == ctrl('Z') || ch ==
  '\r'`) correctly rejected all 5 bogus keys and accepted both real
  ones. This is a deterministic, faithful demonstration of the exact
  defect and fix at the expression level.
- **What changed**: fixed the condition to `while(!(ch == ctrl('Z') ||
  ch == '\r'))` in both files. Since `F_CAN_SUSPEND` is now permanently
  on (see the collapse above) and the prompt text already only offers
  `^Z` (never `!`), the intended fixed logic reduces to dropping the
  `'!'`-vs-`^Z` choice entirely rather than restoring it -- consistent
  with that same collapse. As a direct, provable consequence, the
  `case '!':` "drop to a shell" block in both files' `switch(ch)` (the
  `system("csh")` escape hatch) became unreachable code, since the
  corrected loop condition can never let `ch` reach the switch as
  `'!'` -- removed both, chain-reaction style, matching this project's
  established practice.
- **Verified**: `./build clean && ./build lnx`, exit 0, 52 warnings
  (unchanged). Full smoketest suite, 25/25. No smoketest-suite
  regression test added -- the harness can't reliably force this
  specific disk-write-failure path either (same reproduction
  difficulty noted above), so a test would be flaky rather than a
  real guard; the standalone extraction above is the durable record
  of the fix's correctness instead.
- **Size delta**: `bin/pine` text 778960 -> 778784 (-176 bytes) --
  real removed code this time (the `case '!':` shell-escape blocks),
  not a preprocessor-stripped no-op.

### Collapse leftover `same_stream`/`IS_REMOTE`/`default_driver`/`create_proto` fallout

Fallout from an earlier cut: this build has no remote/TCP-IP mailbox
support, so `pine/pine.c`'s `same_stream()` was already collapsed to a
stub always returning `NIL` (its own comment explains why:
`stream->mailbox` can never hold a `{host}mailbox`-syntax name -- no
two streams can ever be "the same host"). But the `IS_REMOTE(X)`
macro that same file's `default_driver()`/`create_proto()` used to
decide between a local and remote driver was never collapsed at its
call sites, leaving several always-constant conditions still written
as live branches, plus every caller's dead fallback path built around
them.

- **Confirmed via grep + read, each site by hand**:
  - `default_driver()` (`pine.c`): `IS_REMOTE(name)` can never be
    true (the build supports exactly one, always-local, folder
    collection -- confirmed via `folder.c`'s `init_folders()`, which
    sets `tc->proto = default_driver(tc->context)` unconditionally
    for the single context it creates), so the function always
    returned `DF_DRIVER_PROTO`, never `NULL`.
  - `create_proto()` (`pine.c`): both ternaries
    (`IS_REMOTE(...) ? stream : ...`) always took the non-`stream`
    branch, making its `stream` parameter entirely unused.
  - `context_same_stream()` (`pine/context.c`): built a
    fully-qualified name and then called `same_stream()` on it, which
    always returns `NIL` regardless of the name -- so the FQN-building
    work was wasted and the function always returned `NIL`.
  - Every caller of `context_same_stream()`/`same_stream()` treated a
    `NIL` return as "no reusable stream, open fresh" -- so every one
    of these dead-branch call sites was already exercising the
    surviving behavior, just via a longer code path than necessary:
    `pine/send.c:1043-1051` (Fcc-folder auto-create),
    `pine/send.c:1102-1107` (Fcc-append stream reuse),
    `pine/mailcmd.c:909-917` (save-message stream reuse),
    `pine/folder.c` in `add_new_folder()` (`!cntxt->proto` dead branch
    -- `cntxt->proto` is always non-`NULL` per the `init_folders()`
    argument above), and in the folder-rename and folder-delete
    handlers (`ren_stream`/`del_stream`, same dead-reassignment shape).
- **What changed**: deleted `same_stream()` and the `IS_REMOTE` macro
  entirely from `pine.c`; simplified `default_driver()` to
  unconditionally `return(DF_DRIVER_PROTO)` (kept as a named function
  -- three real callers still want "give me the one local driver");
  dropped `create_proto()`'s now-unused `stream` parameter (2-arg
  signature: `context`, `folder`) and collapsed its body to a single
  ternary on `context_isambig()`. Deleted `context_same_stream()`
  from `context.c`/`context.h` entirely (zero remaining callers once
  its own callers were collapsed). At every call site, replaced the
  dead reassignment blocks with either a direct `NULL` argument (Fcc
  append, save-message append, folder rename, folder delete) or the
  single surviving branch made unconditional (`create_stream =
  cntxt->proto;` in `add_new_folder()`), and removed the
  now-single-assignment local variables that this made pointless
  (`save_stream`, `ren_stream`, `del_stream`, the `create_stream ==
  NULL` check in `send.c`).
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25. Since none of the affected code paths
  have a dedicated smoketest case (folder add/rename/delete, Fcc to a
  brand-new folder, save-message to a brand-new folder), drove all
  five live through a real pty session against the built binary:
  composed and sent a message with a nonexistent Fcc folder, confirmed
  creation, and confirmed the sent text actually landed in the new
  folder file; saved a message from the index to a brand-new folder
  name, confirmed creation, and confirmed the message landed there
  (with its `Delete` flag set, matching pine's normal save-and-mark-
  deleted behavior); added, renamed, and deleted a folder from the
  Folder List screen, confirming the on-disk file was created/renamed/
  removed at each step. All five matched pre-existing expected
  behavior exactly.
- **Size delta**: `bin/pine` text 287392 -> 285904 (-1488 bytes).

### Remove dead c-client stream options (`mail_debug`/`OP_HALFOPEN`/`OP_ANONYMOUS`)

Spotted during the same-stream/create_proto survey: `imap/ANSI/c-client/
mail.c`'s `mail_open()` sets three `MAILSTREAM` options bits
(`OP_DEBUG`/`OP_ANONYMOUS`/`OP_HALFOPEN`) that no code in the built
tree ever reads back, plus the `mail_debug()`/`mail_nodebug()`
function pair that has zero callers anywhere.

- **Confirmed via grep** across only the linked object set
  (mail/bezerk/dummy/rfc822/misc/osdep in c-client, plus all of
  pine/pico -- not the whole c-client source tree, which still carries
  unbuilt smtp.c/imap4.c etc. that do read some of these):
  - `stream->debug`/`OP_DEBUG`: only read by the never-built smtp.c;
    `mail_debug()`/`mail_nodebug()` (whose only job was toggling this
    field) had zero callers anywhere, built or not.
  - `stream->halfopen`/`OP_HALFOPEN`: zero readers anywhere, built or
    not -- IMAP half-open connect state has no meaning for a
    local-mbox-only build.
  - `stream->anonymous`/`OP_ANONYMOUS`: zero setters anywhere (nothing
    ever passes `OP_ANONYMOUS` to `mail_open()`), and its only reader
    in the whole tree was `pine/imap.c` (the stale unbuilt duplicate
    source targeted by the next TODO item) plus `pine/pine.h`'s
    `READONLY_FOLDER` macro's `|| ...->anonymous` disjunct -- always
    false, so dead weight on every check.
- **What changed**: removed `mail_debug()`/`mail_nodebug()` and their
  `mldbug`/`mlndbg` macro aliases and prototypes; removed the
  `debug`/`anonymous`/`halfopen` bitfields from `struct mail_stream`
  and their three setter lines in `mail_open()`; removed the
  `OP_DEBUG`/`OP_ANONYMOUS`/`OP_HALFOPEN` `#define`s. Collapsed
  `READONLY_FOLDER` (`pine/pine.h`) to drop its now-nonexistent
  `->anonymous` disjunct -- `->readonly` itself is untouched and still
  live via its own independent setters. Left `pine/imap.c`'s stale
  `ps_global->anonymous` reference alone (out of scope for this item;
  that whole unbuilt file is the next TODO item) and left `smtp.c`
  alone (already permanently unbuilt, same as every other never-linked
  c-client driver).
- **Spinoff finding, not fixed here**: `mail_open()` still has
  `stream->readonly = (options & OP_READONLY) ? T : NIL;`, and grep
  confirms no caller anywhere ever passes `OP_READONLY` (its only
  setter, the `-o` flag, was already removed). This line always
  evaluates to `NIL`, but `stream->readonly` itself stays genuinely
  live via other setters (`bezerk_open()`'s lock-failure path,
  `signals.c`'s crash cleanup) -- a dead *initializer*, not a dead
  field. Filed as its own small TODO item rather than folded in here,
  to keep this cut's verification scoped to what it actually touched.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25. This cut only removes writes that were
  never read (plus one always-false macro disjunct), so no new live
  behavior path exists to drive by hand -- the existing smoketest
  coverage of ordinary mail_open()/mailcmd.c flows is the relevant
  regression guard.
- **Size delta**: `bin/pine` text 285904 -> 285656 (-248 bytes).

### Fix a live, unpatched recurrence of CVE-2000-0909 -- stack overflow in the automatic new-mail-check message

Prompted by a web search for historical Pine security advisories to
check which still apply to this fork. Most didn't (see below), but one
did, essentially verbatim: **CVE-2000-0909**, "buffer overflow in the
automatic mail checking component of Pine 4.21 and earlier... via a
long From: header," was still present, unfixed, in `pine/newmail.c`'s
`new_mail_mess()` -- the function that formats the "New mail!"/"Most
recent from ..." status-line message pine's periodic background
new-mail check displays.

```c
char from[MAX_ADDRESS+20];   /* 120 bytes (MAX_ADDRESS == 100) */
...
sprintf(from, " Most recent from %s", e->from->personal);        /* no cap */
sprintf(from, " Most recent from %s@%s", e->from->mailbox, ...);  /* no cap */
...
sprintf(from, " From %s", e->from->personal);                    /* no cap */
sprintf(from, " From %s@%s", e->from->mailbox, e->from->host);    /* no cap */
```

`e->from->personal`/`mailbox`/`host` come straight from the message's
own `From:` header, copied by c-client's `rfc822_cpy()`
(`imap/ANSI/c-client/rfc822.c`) -- which is just `cpystr()`, a plain
heap duplicate with **no length cap**, bounded only by overall message
size. `new_mail_mess()` is called from `new_mail()` (`pine.c`,
`mailindx.c`, `mailview.c`, `folder.c` all poll it from their main
event loops), pine's periodic background check for newly-arrived mail
-- exactly the "automatic mail checking component" the original CVE
named. Any message delivered to the local mbox (trivial via local
`sendmail`/UUCP, this project's exact delivery model) with a long
enough `From:` personal name overflows this 120-byte stack buffer the
next time the background check fires, with zero user interaction.
Notably, this same function's `subject[200]` buffer two lines below
*does* already cap its `%s` (`%.183s`) -- the `from[]` buffer just
never got the same treatment.

- **Reproduced**: extracted the vulnerable `sprintf` pattern into a
  standalone file (`char from[120]`, unbounded `%s` of a >5000-byte
  string, gated `#ifdef FIXED`). Buggy: `AddressSanitizer: stack-
  buffer-overflow ... WRITE of size 6018 ... overflows this variable`.
  Fixed: clean exit.
- **Fixed**: capped all four `sprintf`s' `%s` precision to fit
  `from[]`'s fixed size -- `%.90s` for the personal-name-only case
  (19-char longest prefix + 90 + NUL = 110 <= 120) and `%.40s@%.40s`
  for the `mailbox@host` case (19 + 40 + 1 + 40 + NUL = 101 <= 120),
  both with comfortable margin.
- **No smoketest regression test added**: reaching this code path live
  requires mail to arrive in an already-open folder and pine's
  background check to actually fire, which is throttled to roughly
  every `NEW_MAIL_TIME` (150) seconds minus a couple -- not practical
  to wait out reliably in the smoketest suite (same call made for the
  mis-parenthesized-ternary and `repeat_char()` fixes above); the
  standalone ASan repro is the durable verification record instead.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25.

**Other historical Pine/pico CVEs checked against this fork, found not
to apply**:
- **Predictable `/tmp/pico.<pid>` temp-file name, no `O_EXCL`**
  (the classic Pico symlink-race file-overwrite advisory) -- the
  vulnerable code was still there verbatim (`tmpname()` in
  `pico/os_unix.c`), but its only caller, `writetmp()`
  (`pico/file.c`), had zero callers anywhere else in this tree --
  confirmed dead code, not reachable. Removed both outright (see the
  follow-on entry below) rather than leaving an unreachable-but-still-
  vulnerable pattern lying around.
- **URL-viewer shell-metacharacter injection** (`msg-view-url`,
  Bugtraq 1999/2001) -- the whole URL-viewer feature doesn't exist in
  this codebase (zero references to `url`/`http://` anywhere in
  `pine/`); this fork is based on Pine 3.89 (1993), which predates
  that feature.
- **`message/external-body` buffer overflow (pre-4.58)** -- this
  fork's handling (`mailview.c`, `reply.c`) only displays the
  external-body parameters as informational text via
  `display_parameters()`; no auto-fetch/exec mechanism exists to
  overflow.
- **c-client large-literal/mailbox-size integer-overflow bugs**
  (network IMAP4 driver) and **long `X-Keywords` header overflow** --
  both live in IMAP4 network-protocol driver code never included in
  this no-TCP/IP build; `bezerk.c` (the local-mbox driver actually
  compiled here) explicitly stubs out keyword support ("keywords not
  supported yet"). This whole CVE class is structurally moot -- no
  code path here parses untrusted network protocol input at all.
- **`rpdump` symlink race** (Pine 4.62) -- `rpdump` doesn't exist
  anywhere in this tree.
- **Pine 4.02 MIME-header buffer-overrun fixes** -- too generic to
  trace to a specific mechanism; very likely already covered by this
  project's own extensive prior buffer-overflow audit passes.

### Remove `writetmp()`/`tmpname()` -- dead code, confirmed while checking historical CVE applicability

Follow-on to the CVE-2000-0909 fix above. While researching historical
Pine/pico security advisories, the classic predictable-`/tmp/pico.<pid>`
-name symlink-race temp-file advisory turned up `pico/os_unix.c`'s
`tmpname()`, whose only caller, `pico/file.c`'s `writetmp()`, had zero
callers anywhere else in the tree -- confirmed dead, so removed both
outright rather than leaving an unreachable-but-still-vulnerable
pattern lying around.

- **`pico/file.c`**: deleted `writetmp()` entirely (write a temp file
  of the composed message text via a predictable `/tmp/pico.<pid>`
  name) -- confirmed via grep zero callers anywhere in `pico/`,
  `pine/`, or their headers besides its own prototype.
- **`pico/os_unix.c`** (real source; `pico/osdep.c` is its generated,
  gitignored `cp` copy, not hand-edited): deleted `tmpname()`, its
  only caller now gone.
- **`pico/efunc.h`**: removed both functions' now-orphaned prototypes.
- **Left alone**: `ffwopen()` (also called by `writetmp()`) stays --
  confirmed it has a second, very much live caller, `writeout()`
  (called from `pico.c`, `os_unix.c`, and `file.c` itself).
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings
  (confirming no other caller was missed). Full smoketest suite,
  25/25.
- **Size delta**: `bin/pico` text 109490 -> 109066 (**-424 bytes**) --
  a real win, this was genuinely compiled code, not preprocessor-
  stripped weight.

### Build libFuzzer harness 1 (parser-level) for incoming-mail parsing; fix a real bug it found

First of the two fuzzing harnesses from the TODO item added after
reviewing historical Pine CVEs. Targets `rfc822_parse_msg()`
(`imap/ANSI/c-client/rfc822.c`) -- the real trust boundary for this
project, since any bytes delivered to the local mbox via `sendmail`/
UUCP get parsed there before pine ever sees an `ENVELOPE`/`BODY`.

- **Harness** (`fuzz/rfc822_fuzz.c`, built via `fuzz/build.sh`):
  treats the whole fuzzer input as one raw RFC822 message, finds the
  header/body split at the first blank line (mirroring
  `bezerk_parse()`'s own `case '\n': /' end of header '/` convention),
  and calls `rfc822_parse_msg()` directly -- no filesystem or mbox
  setup needed, just a buffer and a call into the parser. Truncates
  the input at the first embedded NUL byte before doing anything else,
  matching `bezerk_parse()`'s own `case '\0': /' end of message '/`
  convention -- without this, the harness could "find" crashes that
  are really just feeding `rfc822_parse_msg()` byte sequences
  `bezerk_parse()` itself would never hand it in real operation. Sizes
  its scratch buffer as `max(headersize,bodysize)+1`, matching
  `bezerk_fetchstructure()`'s real `LOCAL->buf` sizing convention
  exactly -- undersizing it (first tried just `headersize+1`) produced
  a harness-only false-positive overflow, since
  `rfc822_parse_content()`'s multipart-recursion path reuses the same
  scratch buffer to collect each MIME part's own header lines out of
  the *body* stringstruct, not just the outer header. `fuzz/mm_stubs.c`
  provides no-op stubs for the ~13 `mm_*()` callbacks c-client expects
  its calling application (normally pine, see `pine/mailbox.c`) to
  supply; `mm_fatal()` calls `abort()` on purpose so libFuzzer catches
  and reports it. Seed corpus (`fuzz/corpus/rfc822/`) drawn from
  `test/cases/*.py`'s existing crafted messages (already encoding
  several historical overflow triggers) plus one plain valid message.

- **Found and fixed a real bug on the very first fuzzing run**:
  `rfc822_parse_word()` (`rfc822.c`), used throughout address/personal-
  name parsing. Its `case '\\':` branch (an unquoted backslash
  "escaping" the following character) did `str = st + 2` unconditionally
  to skip the backslash and the character after it -- but if the
  backslash is the very last character before the string's NUL
  terminator, `st + 2` skips *past* the NUL entirely, and the next loop
  iteration's `strpbrk()` call reads from that out-of-bounds pointer.
  Minimized by libFuzzer to a 4-byte reproducer: a `To:` header whose
  entire value is a single backslash (`To:\`). Verified with a
  standalone 2-line repro extracting the exact function body (gated
  `#ifdef FIXED`): buggy version is a clean
  `AddressSanitizer: heap-buffer-overflow ... READ of size 1` on a
  2-byte allocation; fixed version returns cleanly. Fixed by checking
  `st[1]` for NUL before advancing past it -- treat a trailing,
  nothing-following backslash the same as "no delimiter found" (return
  the end of the string) rather than blindly assuming there's always
  at least one more character after it. Reachable via any header value
  (address, personal name, etc.) parsed as an RFC822 "word" that ends
  in a bare, unescaped backslash -- plausible from a deliberately
  malformed or merely typo'd address header.
- **Verified**: standalone ASan repro (buggy crashes, fixed clean);
  full clean rebuild of the whole tree (plain and `ASAN=1`), 0
  warnings; 25/25 smoketests in both configurations. Added the
  minimized 4-byte crash as a permanent regression seed
  (`fuzz/corpus/rfc822/regress_trailing_backslash_address`).
- **A second, unconfirmed lead**: a longer fuzzing run (~3M execs)
  also hit an ASan allocator-internal consistency check failure
  (`CHECK failed: ... CHUNK_QUARANTINE`) during `mail_free_envelope()`
  /`mail_free_address()`'s recursive address-list teardown -- shaped
  like a double-free from a corrupted/cyclic `ADDRESS->next` chain.
  Did not reproduce on a standalone re-run of the saved crash input,
  nor across a 1,000,000-run deterministic replay of the full corpus
  -- couldn't confirm it's a real, reliably-reproducible bug (as
  opposed to some interaction with libFuzzer's persistent-process
  execution model) in the time available. Not fixed; flagged here as
  a lead worth revisiting with a longer/repeated fuzzing run rather
  than a confirmed defect.
- **Not committed to version control**: the corpus directory the
  fuzzer grows during a run (thousands of discovered inputs) -- only
  the small, curated seed set is tracked. `fuzz/build.sh`'s suggested
  invocation points libFuzzer's own growable output at a gitignored
  `fuzz/findings/` directory instead of the tracked seed corpus.

### Chase harness 1's "second lead": two more real integer-underflow bugs found and fixed, one lead still open

Follow-on to harness 1's initial run. Continued fuzzing turned up two
more real, confirmed bugs -- both the same root-cause pattern (an
`unsigned long i` byte-counter that gets decremented without first
checking it's still nonzero, so hitting exactly 0 makes it *wrap* to a
huge value instead of stopping the scan) -- plus a third, harder lead
that's documented but not yet fixed.

- **Bug 1: `rfc822_parse_content()`'s multipart-boundary scanner**
  (`rfc822.c`, the `TYPEMULTIPART` case's `while (i > j)` loop). The
  boundary-cookie-match loop, `for (k = j,s = t; i-- && ...)`, and the
  `--`-prefix check just above it, `if (i-- && ... && i-- && ...)`,
  both used bare `i--` with no guard. libFuzzer minimized this to a
  40-byte reproducer: `MIME-Version:\nContent-Type:multipart\n\n--` --
  a multipart body whose entire content is exactly the two-dash
  boundary prefix with nothing else, so `i` (the body's remaining byte
  count) hits exactly 0 right as the two dashes are consumed, and the
  very next decrement (matching the cookie's own content) underflows
  it. Once `i` wraps, the outer `while (i > j)` loop believes there's
  effectively unlimited data left, and `SNX()` (the STRING-reading
  macro, itself unbounded -- it trusts the caller's own counter
  entirely) keeps reading forward with no caller-side limit, eventually
  walking off the end of the allocated body buffer into unmapped
  memory. **Verified**: standalone repro extracting the exact function
  body (gated `#ifdef FIXED`) -- buggy version segfaults deep inside
  `strlen`/an out-of-bounds `strpbrk`-adjacent read (SEGV in
  `rfc822_parse_content` itself, confirmed via the real compiled
  library too, not just the extraction); fixed version runs clean.
  Also reproduced and confirmed via `git stash` (real code, buggy vs.
  fixed, same crash/clean result). Fixed every bare `i--` in this
  function to `i && i--` (matching the guard idiom the surrounding
  code already used correctly in several other spots in the very same
  function -- this bug was simply an inconsistency, not a novel
  pattern). Also guarded a `case '\015':`'s `i--` and the `default:`
  case's `i--` the same way, since both had the identical unguarded
  shape even though the fuzzer's specific minimized input didn't
  happen to hit them.
- **Bug 2: the same function's per-MIME-part header-collection loop**
  (a few lines further down, iterating `body->contents.part` to parse
  each part's own `Content-*` headers out of the body). `if (!--i)
  *s1++ = c = '\0';` runs unconditionally at the end of every
  inner-loop pass -- but a few lines earlier, `case '\012':`'s
  CR-eating branch (`if ((i > 0) && (CHR (bs) == '\015')) { SNX (bs);
  i--; }`) can *already* bring `i` to exactly 0 within the same pass,
  so the unconditional `--i` a few lines later underflows it a second
  time. Unlike bug 1, this manifested as heap-corruption-shaped
  crashes (a `bad-free`/allocator-magic-number-mismatch on an
  `ADDRESS` struct during `mail_free_envelope()`) rather than a direct
  SEGV -- consistent with the runaway loop overwriting past `t`, the
  scratch buffer shared across this whole parse (a *write* overflow,
  not just an over-read), landing on and corrupting an unrelated
  heap allocation's own bookkeeping. This is the "second lead" flagged
  in the previous entry as unconfirmed -- confirmed here. Traced by
  hand (the fuzzer's own crash inputs for this one never reproduced
  standalone, single-execution or even across a 1M-run deterministic
  corpus replay -- typical for corruption whose *effect* is detected
  far from its *cause*) rather than via a live repro; fixed with the
  same `i &&` guard. A subsequent ~150K-exec fuzzing run that
  previously hit this within under 2 minutes ran clean for the
  remainder of the session after the fix.
- **Verified (both fixes)**: full clean rebuild (plain and `ASAN=1`),
  0 warnings, 25/25 smoketests. Added the 40-byte bug-1 reproducer as
  a permanent regression seed
  (`fuzz/corpus/rfc822/regress_multipart_boundary_underflow`).
- **A third lead, still open**: continued fuzzing (both the full
  `rfc822_fuzz` harness and a new, narrower `fuzz/adrlist_fuzz.c` built
  specifically to isolate it -- calls `rfc822_parse_adrlist()`
  directly, skipping header/MIME parsing entirely) kept finding the
  same *class* of heap-corruption crash (allocator magic-number
  mismatches and bad-frees inside `mail_free_address()`'s recursive
  list teardown), reproducible only via extended fuzzing runs
  (anywhere from ~150K to ~3M execs), never via standalone replay of
  the saved crash input, not even with `ASAN_OPTIONS=quarantine_size_mb=0`
  forcing immediate chunk reuse. One instance's stack trace didn't
  even include any harness/target frames at all -- the corruption was
  purely detected inside libFuzzer's own unrelated `free()` calls,
  confirming this is a genuine delayed-effect wild write, not an
  artifact of the fuzzing harness itself. Every crash-triggering input
  seen so far is `:`/`;`-heavy, pointing at
  `rfc822_parse_adrlist()`'s RFC822 *group* address syntax
  (`group-name: addr1, addr2;`) as the likely area -- specifically,
  `n` (group nesting depth) is checked with `n >= 0` rather than `n >
  0` before treating a `;` as closing an open group and unconditionally
  appending a new marker address via `last->next = ...`, which is only
  safe if `last` is guaranteed non-NULL; a careful trace didn't
  conclusively prove this reachable with `last == NULL` (the "bad
  mailbox" error path seems to intercept the specific case tried by
  hand), but it wasn't ruled out either, and the byte patterns in every
  captured crash strongly implicate this function. Left unfixed pending
  a session with more time for either a debugger-assisted bisection or
  a much longer fuzzing run against `fuzz/adrlist_fuzz` (now checked
  in, corpus seeded with both well-formed and malformed group syntax)
  to produce a reliably-reproducing minimal case.
- **New tooling kept**: `fuzz/adrlist_fuzz.c` (builds via the updated
  `fuzz/build.sh`), seeded with `fuzz/corpus/adrlist/`'s well-formed
  and malformed address/group-syntax examples -- narrower and ~2-3x
  faster per-exec than the full `rfc822_fuzz` harness, worth reaching
  for first when chasing anything address/group-parsing-specific.

### Remove dead `OP_READONLY` bit-check in `mail_open()`

Spinoff from the `mail_debug`/`OP_HALFOPEN`/`OP_ANONYMOUS` cut: grep
confirmed no caller anywhere in the built tree ever passes
`OP_READONLY` in a `mail_open()`/`bezerk_open()` `options` argument
(its only setter, the `-o` flag, was already removed per the "Remove
read-only mode" writeup), so `imap/ANSI/c-client/mail.c`'s
`stream->readonly = (options & OP_READONLY) ? T : NIL;` always
evaluated to `NIL`.

- **Extra scrutiny before cutting**: `mail_open()`'s own doc comment
  advertises a real "recycling a stream" path (`else { ... }` when the
  caller passes a non-`NULL` candidate stream to reuse), and
  `bezerk_open()` has matching `if (LOCAL) { close old file ... }`
  reuse logic keyed off `stream->local` already being populated from a
  prior open. If that path were reachable, deleting this line outright
  (rather than collapsing it to its always-taken branch) would be a
  real bug, not a no-op: a stale `readonly=T` from an earlier open
  (e.g. a lock-contention failure) could leak into a later reopen of a
  perfectly normal, writable mailbox on the same recycled stream,
  since `bezerk_open()`'s own lock-acquisition attempt is gated on
  `if (!stream->readonly)`.
- **Confirmed unreachable, not just unlikely**: grepped every
  `mail_open()`/`context_open()` call site in the built tree. Every
  reachable one passes `NIL`/`NULL` for the recycling-candidate stream
  argument (`pine/context.c:343`, `pine/folder.c:1625`,
  `pine/mailcmd.c:1458,1480`, `imap/ANSI/c-client/mail.c`'s own two
  internal calls). The only two call sites that pass a real candidate
  stream are `pine/mailbox.c`'s `context_bboard()` -- inside the
  `#ifdef LATER` block from this file's own next TODO item, `LATER`
  never `#define`d, so never compiled -- and `pine/imap.c:491`, the
  stale unbuilt duplicate source (also a queued TODO item). So the
  "recycling" branch in `mail_open()`, and `bezerk_open()`'s matching
  reuse logic, are both dead code in this build: every real stream
  open goes through `mail_open()`'s fresh-stream path, which
  `memset()`s the new `MAILSTREAM` to all-zero before this line ever
  runs -- making the line a true no-op (redundant `NIL` write to an
  already-`NIL` field), not merely a dead condition. Safe to delete
  outright, matching this TODO item's original plan.
- **What changed**: deleted the `stream->readonly = (options &
  OP_READONLY) ? T : NIL;` line from `mail_open()` and the
  `OP_READONLY` `#define` from `mail.h`. Left `stream->readonly` the
  field, `READONLY_FOLDER` (`pine/pine.h`), and `->readonly`'s other
  independent setters (`bezerk_open()`'s lock-failure detection,
  `signals.c`'s crash cleanup) completely untouched -- all still
  genuinely live.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25.
- **Size delta**: `bin/pine` text unchanged at 285736 bytes -- a
  single redundant bitfield-store instruction is small enough to
  vanish into existing section/function alignment padding at this
  granularity; confirmed via `grep` that the line and `#define` are
  genuinely gone from the source and that `mail.o` was actually
  recompiled.

### Delete `pine/imap.c` -- stale unbuilt duplicate source

`pine/imap.c` (~544 lines) was never in `pine/makefile.lnx`'s `obj=`
or `cfiles=` lists (those build `mailbox.o`/`mailbox.c`, the real
successor -- `mailbox.c`'s own header comment says "extracted from
imap.c") and nothing `#include`d it.

- **Confirmed dead before deleting**: grepped for the file itself in
  the build (`obj=`/`cfiles=`, confirmed absent from both) and for its
  two functions that have no counterpart in `mailbox.c`
  (`mm_login()`/`mm_flags()`) anywhere else in the tree -- both were
  defined *only* in `imap.c` and referenced *nowhere*, not even by
  c-client itself (grepped `imap/ANSI/c-client/*.c` for callers, none
  found), meaning the build has always succeeded and run correctly
  without them existing at all. Zero risk to delete.
- **What changed**: `git rm pine/imap.c`. No other files touched --
  it wasn't referenced from any makefile, build script, or `#include`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25.
- **Size delta**: `bin/pine` text unchanged at 285736 bytes, exactly
  as expected -- the file was never linked into the binary to begin
  with; this cut is pure repo hygiene (removing a stale duplicate that
  could mislead a future grep), not a size win.

### Tiny: dead `#ifdef LATER` block in `mailbox.c`'s `context_bboard()`

`pine/mailbox.c`'s `context_bboard()` had an `#ifdef LATER ... #endif`
block rebuilding a bulletin-board's unread-count prefix via
`context_open()`. `LATER` is never `#define`d anywhere in the tree
(confirmed via grep -- the only other `LATER`-named macro anywhere is
pico's unrelated `MAYBELATER`), so the block was unreachable dead
code; the function itself stays live (called via `mm_bboard()` from
`imap/ANSI/c-client/dummy.c`'s local-folder-name pattern matching, per
the "mm_bboard pulled back out as a false positive" writeup elsewhere
in this file).

- **What changed**: deleted the `#ifdef LATER`/`#endif` block and its
  body from `context_bboard()`, leaving just `new_folder()` +
  `folder_insert()`. The block's removal orphaned the file-scope
  `extern MAILSTREAM *find_folder_stream;` declaration at the top of
  `mailbox.c` (its only use in this file was inside the dead block) --
  removed that too, chain-reaction style; `find_folder_stream` the
  global itself (defined in `folder.c`) is untouched and still used
  elsewhere. Left the sibling `extern void *find_folder_list;` alone,
  since `context_bboard()` and `context_mailbox()` both still use it
  outside the dead block.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25.
- **Size delta**: `bin/pine` text unchanged at 285736 bytes -- the
  block was already compiled out by the preprocessor before this cut
  (dead behind `#ifdef LATER`), so removing the source text has no
  runtime-code effect; this is source-hygiene only, same category as
  the `pine/imap.c` deletion above.

### Resolve the third fuzzing lead: heap-buffer-overflow in `rfc822_quote()` on a trailing backslash

The third lead from the previous entry (unconfirmed heap corruption in
`mail_free_address()`'s teardown, `:`/`;`-heavy triggering inputs)
turned out to have nothing to do with the `rfc822_parse_adrlist()`
group-syntax suspect flagged there -- it was a fourth instance of the
exact same *bug class* as the very first harness-1 find
(`rfc822_parse_word()`'s trailing-backslash overflow), this time in
`rfc822_quote()`, a much more central, widely-called function (called
by `rfc822_cpy()`, which is used throughout address parsing to copy
every mailbox/host/personal-name string).

- **Root cause**: `rfc822_quote()`'s in-place unquoting loop --
  `while (*src) { if (*src=='\"') src++; else { if (*src=='\\')
  src++; *dst++=*src++; } }` -- unconditionally does `*dst++=*src++`
  after (possibly) skipping a backslash. If the backslash is the last
  character before the string's NUL terminator, skipping it lands
  `src` *on* the NUL, and the unconditional copy both writes that NUL
  to `dst` (fine) and advances `src` *past* it (not fine) -- the next
  loop iteration's `*src` check reads one byte past the allocation,
  and if that byte happens to be nonzero (likely, uninitialized heap
  data), the loop keeps going, overflowing `dst` for real.
- **Why this eluded the default ASan build for so long**: the plain
  `-fsanitize=address` build kept hitting the *effect* of this bug
  (allocator magic-number corruption, bad-frees) many execs after the
  actual overflow, since a `heap-buffer-overflow` this shape doesn't
  always land squarely on ASan's redzone the instant it happens on
  every allocator/size-class combination -- and once it *does* corrupt
  an adjacent chunk's header, the failure only surfaces later, at
  whatever unrelated `free()` happens to touch that chunk next
  (sometimes inside libFuzzer's own bookkeeping, per the previous
  entry). Recompiling just for diagnosis with
  `-fsanitize-address-field-padding=1` (extra padding + stricter
  checking around struct fields) turned the exact same bug into a
  clean, standard `heap-buffer-overflow` report pointing straight at
  `rfc822_quote()`, on the very first fuzzing run (~5,000 execs,
  seconds instead of the many-minutes-to-hours the plain build needed
  to even manifest the corruption's effect).
- **Fixed**: after skipping a backslash, check whether `src` now
  points at the NUL terminator and `break` immediately instead of
  falling through to the copy -- mirrors the fix already applied to
  `rfc822_parse_word()` for the identical underlying mistake.
- **Verified**: standalone repro extracting the exact function body
  (gated `#ifdef FIXED`) -- buggy version is a clean `AddressSanitizer:
  heap-buffer-overflow ... READ of size 1` on a 2-byte allocation;
  fixed version runs clean. Full clean rebuild (plain and `ASAN=1`),
  0 warnings, 25/25 smoketests. Re-ran both fuzz harnesses at length
  to confirm the fix actually closed the lead rather than just this
  one input: `adrlist_fuzz` ran **22.4 million** executions in under 5
  minutes with zero crashes (previous crashes in this exact harness
  had shown up within 150K-1.7M execs); the full `rfc822_fuzz` ran
  **14.7 million** with zero crashes (only a separate, minor memory
  leak turned up, see the new TODO item). Added the 4-byte minimized
  trigger (`\xff\xff\xff\` followed by a backslash) as a permanent
  regression seed
  (`fuzz/corpus/adrlist/regress_trailing_backslash_quote`).
- **Diagnostic technique worth remembering**: `-fsanitize-address-
  field-padding=1` turned a many-hours-to-reproduce delayed-corruption
  bug into a first-run, five-second repro. Worth reaching for early
  the next time a fuzzing lead shows the "allocator-internal CHECK
  failure / bad-free, never reproduces standalone" signature, rather
  than after extensive plain-ASan chasing.

### Audit for other trailing-backslash instances of the same bug class: found a fourth in `rfc822_parse_word()`'s quoted-string case

With three confirmed instances of the "advance past a backslash
without checking it was the last character before the string's NUL"
bug (`rfc822_parse_word()`'s top-level word-delimiter handling,
`rfc822_quote()` -- see above two entries), swept the codebase for any
other place doing the same thing.

- **Found a fourth instance**, in `rfc822_parse_word()` itself but a
  *different* loop than the one already fixed: the embedded
  `case '"':` quoted-string scanner. `while (*++st != '"') switch
  (*st) { ... case '\\': ++st; ... }` -- the outer `while` does
  `*++st` on every iteration re-check, so if `case '\\':`'s own `++st`
  lands exactly on the NUL terminator (backslash was the last real
  character), the loop's *next* re-check does another `++st`, reading
  one byte past the allocation.
- **Fixed**: check `st[1]` for NUL before the `++st` and return `NIL`
  (unbalanced quoted string) if so -- same fix shape as the other
  three instances.
- **Verified**: standalone repro extracting the exact function body
  (gated `#ifdef FIXED`) -- buggy version is a clean `AddressSanitizer:
  heap-buffer-overflow ... READ of size 1` on a 3-byte allocation
  (`"\` + NUL); fixed version returns `NULL` cleanly, matching the
  intended "unbalanced quoted string" semantics. Full clean rebuild
  (plain and `ASAN=1`), 0 warnings, 25/25 smoketests both times.
- **Swept and confirmed safe** (no fix needed, same "peek/skip
  unconditionally" shape but already correctly guarded or provably
  bounded): `pico/search.c`'s `pat[0]=='\\'` (uses `strcmp`, safe
  regardless of what follows); `bezerk.c`'s `bezerk_getflags()`
  (backslash-then-`switch` falls to `default: i=0` on NUL, and the
  `if (i && ...)` short-circuits before any further dereference);
  `pine/folder.c` and `pine/init.c`'s `*p=='\\' && *(p+1)=='"'` sites
  (peek is always at most the NUL terminator itself, never past it);
  and `rfc822_skipws()`'s own two backslash sites in comment-parsing
  (both already explicitly check for NUL immediately after the
  escape-skip before continuing). These confirm the bug wasn't a
  systemic style issue in this codebase, just four genuine oversights
  in the address-parsing code specifically.
- Not added as a fifth harness-1 corpus regression seed under its own
  name since the input is a near-duplicate of the existing
  `regress_trailing_backslash_address`/`regress_trailing_backslash_quote`
  seeds' shape; covered instead by the extended re-fuzzing run
  described below, which exercises this code path directly.

### Bonus find while re-verifying the above: unsigned-underflow SEGV in `rfc822_parse_content()`'s multipart boundary scanner on a completely empty part

Found incidentally while re-running the fuzz harnesses to confirm the
backslash audit fix above hadn't regressed anything -- a new,
unrelated (non-backslash) crash surfaced in `rfc822_fuzz` after
~162,000 execs (needed `ASAN_OPTIONS=detect_leaks=0` first, since an
unrelated small leak was otherwise halting the run early).

- **Root cause**: the multipart boundary-scanning state machine tracks
  `m = GETPOS(bs); if (m) m--;` at the top of each scan iteration --
  "position just before the current line" -- and uses it in two places
  to compute a just-finished part's size: `part->body.size.bytes = m -
  part->offset`. Both are an unsigned subtraction with no check that
  `m >= part->offset`. For a completely empty part (two boundary
  markers back-to-back with nothing between them), `GETPOS(bs)` at the
  start of processing the *second* boundary line equals `part->offset`
  exactly, so `m` becomes `part->offset - 1` -- one less. The
  subtraction underflows to a value near `ULONG_MAX`, and that bogus
  size later drives a header-collection scan loop far past the real
  buffer, eventually segfaulting (observed at varying call depths --
  inside `strcrlflen()`, or directly in the scan loop -- depending on
  heap layout/ASLR).
- Minimized (via `-minimize_crash=1`) to a 71-byte, **deterministic,
  standalone-reproducible** input (unlike the earlier heap-corruption
  leads, this one crashes the same way every time):
  ```
  MIME-Version:
  Content-Type:multipart;boundary="BOUND"

  --BOUND
  --BOUND
  ```
  Confirmed via careful single-gdb-session tracing of the exact
  underflowing values: `i = 18446744073709551607`, `part->offset = 8`,
  `part->body.size.bytes = 18446744073709551615`.
- **Fixed** at both sites that compute a part's size from `m -
  part->offset` (the two-dashes/end-of-parts case, and the newline
  case): clamp to 0 when `m <= part->offset`, since a part that starts
  and ends at the same position is legitimately empty. Left a third,
  similar-looking site (`part->body.size.bytes = GETPOS (bs) -
  part->offset` after the whole scan loop ends) unchanged -- that one
  uses `GETPOS(bs)` directly with no `m--` adjustment, so it's always
  monotonically >= `part->offset` and can't underflow.
- **Verified**: standalone repro isolating the arithmetic and its
  consequence (gated `#ifdef FIXED`) -- buggy version aborts under
  ASan (redzone corruption from scanning past a 1-byte allocation with
  an underflowed byte count); fixed version computes 0 bytes and
  touches nothing out of bounds. Full clean rebuild (plain and
  `ASAN=1`), 0 warnings, 25/25 smoketests both times. Re-ran both fuzz
  harnesses at length with all of this session's fixes in place:
  `rfc822_fuzz` ran **58.2 million** execs in ~9 minutes with zero
  crashes; `adrlist_fuzz` ran **51.5 million** with zero crashes.
  Added the 71-byte minimized input as a permanent regression seed
  (`fuzz/corpus/rfc822/regress_multipart_empty_part_underflow`).

### Sweep for the general "walk a string pointer, unconditionally advance past a peeked/skipped character" bug shape outside email-parsing code

All of harness 1's finds so far shared one shape: a loop walks a
pointer over a NUL-terminated string, and on hitting some special
character (backslash, `%`, a boundary marker) unconditionally advances
the pointer again -- without checking whether that character was the
last one before the terminator -- reading or writing one byte past the
allocation. Swept the rest of the codebase (`pine/`, `pico/`, and the
in-scope `c-client` files -- confirmed via each `makefile.lnx`'s object
list which files are actually compiled here, since a few files like
`phile.c`/`smtp.c` and other-platform `os-*.c` variants aren't) for
the same shape, independent of the fuzz harnesses (which only cover
`rfc822_parse_msg()`), since command-line arguments and interactive
input reach a lot of code the fuzzers don't.

- **Found and fixed, in `pine/init.c`'s `expand_variables()`** (expands
  `$VAR`/`${VAR}` and a leading `~` in any pine variable's value --
  reached at startup for `-sort`'s argument via `init_vars()`'s
  unconditional `set_current_val(vars, V_SORT_KEY, TRUE)`, and more
  generally for `.pinerc`-sourced values before that parsing was
  dropped, see `TODO-pdp11-done.md`'s "simplify read_pinerc()" entry):
    - The `$VAR`/`${VAR}` case did `while (isalnum(*src)) *p++ =
      *src++;` copying into `char word[128]` with **no bound at all**
      -- a `-sort` argument containing `$` followed by 128+ alnum
      characters overflows `word[]`. Confirmed with a real
      `AddressSanitizer: stack-buffer-overflow` (`WRITE of size 1`,
      overflowing `word` at line 444) via `bin/pine -sort
      '$AAAA...(300 A's)...'`. Fixed by capping the copy at
      `sizeof(word)-1` and returning `NULL` if it doesn't fit -- an
      already-well-exercised "expansion failed" path, since every
      caller of `expand_variables()` already falls back cleanly (see
      `set_current_val()`, and `cpystr(NULL)` returning `NIL`).
    - The `~` case's own local `char buf[MAXPATH]` copy had the exact
      same missing bound (`for(i = 0; src[i] && src[i] != '/'; i++)
      buf[i] = src[i];`). Fixed the same way (bail out with `NULL` if
      it doesn't fit).
- **Found and fixed, in `pine/osdep/fnexpand`** (the source fragment
  for `fnexpand()`, assembled into `pine/os.c` -- see CLAUDE.md's
  "generated files" note; **do not edit `pine/os.c` directly**, it's a
  gitignored symlink to the real, tracked, generated
  `pine/osdep/os-lnx.c`): the `~username` expansion's own scratch copy
  -- `for(x = buf+1, y = name; *x != '/' && *x != '\0'; *y++ = *x++);`
  -- into `char name[20]` with no bound. This is reached one call
  further in from the `buf[MAXPATH]` fix above (`expand_variables()`'s
  `~` case calls `fnexpand(buf, MAXPATH)`), and is **much more easily
  triggered**: any `~`-prefixed value with 20+ non-slash characters
  (not 4095+, like `buf[MAXPATH]` needs) smashes `name[20]` first.
  Confirmed with a second real `AddressSanitizer: stack-buffer-
  overflow` (`name` at `os.c:361`, called from `expand_variables()`)
  via `bin/pine -sort '~BBBB...(2000 B's)...'`. Fixed by capping the
  write into `name[]` at `sizeof(name)-1` while still letting the scan
  pointer (`x`) advance to the *true* end of the username (needed
  correctness-wise: `x - buf` is used afterward to know how much of
  `buf` to replace, and `x == buf + 1` distinguishes the "no username,
  just `~/...`" case) -- an oversized username now just fails the
  `getpwnam()` lookup like any other unknown username, instead of
  overflowing.
- **Process note worth remembering**: this investigation started by
  trying to verify these fixes with `ASAN=1 ./build lnx` (as done for
  every earlier fix this session) and getting a clean-looking build
  that turned out to have **no ASan instrumentation at all** --
  `ASAN=1` as a shell *environment* variable is silently overridden by
  `pine/makefile.lnx`'s/`pico/makefile.lnx`'s own `ASAN=` assignment
  (make doesn't honor environment overrides of makefile-set variables
  without `-e`). The correct invocation, matching the makefile's own
  documented comment, is `./build lnx ASAN=1` (an argument *to*
  `./build`, forwarded as a `make` argument). Re-verified this
  session's earlier `rfc822.c` fixes are unaffected by this mixup --
  their real verification always went through `fuzz/build.sh` (which
  invokes `-fsanitize=address` directly, not through this makefile
  path) -- but any future "full-project ASAN=1 build" verification
  step should double-check `ldd bin/pine | grep asan` actually shows
  `libasan`, not just trust a clean-looking build log.
- **Also found, and fixed at the root rather than chasing every
  caller**: `pico/display.c`'s `mlwrite()` and `emlwrite()` (the
  message-line and centered-error-message writers, used from ~140 call
  sites across `pine/`/`pico/`) both scan their `fmt`/`message`
  argument for `%c`/`%d`/`%D`/`%o`/`%x`/`%s`/`%%` escapes, and both had
  the same trailing-`%` bug: on hitting `%`, each unconditionally reads
  one more character (`switch(*++bufp)` in `emlwrite()`; `c = *fmt++;`
  in `mlwrite()`) and then falls into an unconditional further advance
  (the shared loop's own `bufp++`/`*fmt++`) regardless of what that
  character was -- if `%` was the last character before the string's
  NUL terminator, the second advance reads past it. Checked all ~140
  call sites for whether any could plausibly pass a message/format
  ending in a bare trailing `%` (the *only* way to trigger this --
  `%s`-substituted `arg` content is inert data to these functions'
  own scanner, never rescanned for further `%` sequences): several
  compose a message dynamically via `sprintf()` first
  (`pico/random.c`, `pico/os_unix.c`, `pico/file.c`,
  `pico/composer.c`), but all of those have fixed trailing text after
  any embedded dynamic content -- except `pico/composer.c:1909`, whose
  `sprintf(s, "%s field: %s", ..., errmsg)` ends directly on `errmsg`.
  Traced that to `(*Pmaster->buildaddr)()`, which in this fork is
  `pico/compose_stub.c`'s `compose_buildaddr()` -- it unconditionally
  `return(0)` (success), so the `retval == -1` branch containing this
  call is dead code in the current build. **No currently-reachable
  call site was found**, so this is a proactive fix (matching the
  project's practice of hardening a shared function once the same
  bug shape turns up in it, rather than only patching demonstrated-
  reachable instances) rather than a demonstrated live crash. Fixed
  both by checking for a NUL immediately after consuming the `%`, and
  `break`-ing out of the scan loop if so (mirrors this session's
  other trailing-escape-character fixes).
- **Verified**: standalone repros extracting the vulnerable logic
  (gated `#ifdef FIXED`) for both the `expand_variables()`/`fnexpand()`
  family and the `mlwrite()`/`emlwrite()` family -- buggy versions are
  clean `AddressSanitizer: stack-buffer-overflow` reports; fixed
  versions run clean. The `expand_variables()`/`fnexpand()` fixes were
  additionally confirmed via a real `bin/pine -sort ...` invocation
  under a correctly-instrumented ASan build (see the process note
  above), both before (real crash, full stack trace) and after (clean,
  reaches `MAIN MENU` or exits normally). Full clean rebuild (plain and
  correctly-invoked `ASAN=1`), 0 warnings both times (one pre-existing,
  unrelated `-Wformat-overflow=` warning in `pico/random.c` -- a
  `sprintf` into a fixed `buffer[80]` that only surfaces under
  `ASAN=1`'s different codegen -- noted as a new TODO item rather than
  fixed here, since it's a buffer-sizing issue, not this sweep's
  pointer-walk shape), 26/26 smoketests both times. Added
  `test/cases/test_sort_key_expand_overflow.py`, exercising both the
  `word[128]` and `name[20]` overflows through a real `bin/pine -sort
  ...` invocation each.

### Remove dead c-client search infrastructure (`mail_search`/`bezerk_search`)

`mail_search()` (`imap/ANSI/c-client/mail.c`) is c-client's
IMAP-SEARCH-style criteria engine -- given a criteria string like
`"FROM foo SINCE 1-Jan-2000"`, it walks the mailbox marking messages
that match. Grepped every `pine/*.c` file: zero callers anywhere in
the built tree. Pine's own WhereIs feature is a separate, in-memory
text search over already-rendered screen content and never calls into
this c-client subsystem. That made the whole chain dead:
`mail_search()`/`mail_searched()` in `mail.c`; `bezerk_search()` and
its ~24 helper predicates in `bezerk.c` (`bezerk_search_all`,
`_answered/_deleted/_flagged/_keyword/_new/_old/_recent/_seen` and
their `un*` negations, `_before/_on/_since` date predicates,
`_body/_subject/_text/_bcc/_cc/_from/_to` string predicates, plus the
`bezerk_search_date`/`_flag`/`_string` helper-generator functions and
the `search_t` function-pointer typedef); and pine's own `mm_searched()`
callback stub in `pine/mailbox.c` (its doc comment already said
"Result: nothing, not used by pine" -- a leftover hint from the
original UW code that this had already gone unused).
`phile.c` -- the only other caller of `mail_searched()` in the c-client
source tree -- isn't built by `makefile.lnx` at all, so nothing outside
this now-deleted chain touched any of it. `pine/pine.def` (a DOS/OS2
`pcpine` module-definition file, unrelated to `makefile.lnx`) and
`imap/ANSI/c-client/Internal.DOC` still mention these symbols, but
neither is compiled or generated from source -- left untouched as
historical/foreign-platform artifacts, same treatment as other
not-built files elsewhere in the tree.

- **What changed**:
  - `imap/ANSI/c-client/bezerk.c`: deleted `bezerk_search()` (with its
    comment block and the page-break form-feed preceding it) and the
    entire trailing `bezerk_search_*` predicate block through end of
    file (342 lines total).
  - `imap/ANSI/c-client/bezerk.h`: removed the matching 27 prototypes
    and the `search_t` typedef.
  - `imap/ANSI/c-client/mail.c`: deleted `mail_search()` and
    `mail_searched()`.
  - `imap/ANSI/c-client/mail.h`: removed `mail_search`/`mail_searched`/
    `mm_searched` macro aliases and prototypes (the `mm_searched`
    prototype became dead too, once `mail_searched()` -- its only
    caller anywhere in the built tree -- was gone).
  - `pine/mailbox.c`: deleted the now-uncallable `mm_searched()`
    callback implementation.
  - `fuzz/mm_stubs.c`: dropped the matching `mm_searched()` stub (the
    fuzz harness links against `c-client.a`, which no longer has any
    undefined reference to it).
  - Several deletions landed on lines with literal form-feed (`^L`)
    page-break characters that broke the `Edit` tool's exact-string
    matching (a recurring friction point in this codebase's older
    files) -- used `sed` with explicit line ranges (verified via
    `cat -A`) instead, same fallback as past cuts.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite, 25/25. Final `grep -rn` sweep for
  `mail_search|bezerk_search|mm_searched|mail_searched` across
  `pine/`, `imap/ANSI/c-client/`, and `fuzz/` turned up nothing left
  in the built tree.
- **Size delta**: `bin/pine` text 285736 -> 280791 bytes, a 4945-byte
  reduction. `bezerk.o` had been the single largest c-client object
  file (25KB text, bigger even than `rfc822.o`'s 17KB) specifically
  because of this dead search engine; this cut is a real code-size win,
  not just source hygiene.

### `pine/folder.c` low-coverage investigation -- no dead code found, added Folder List screen coverage instead

- [x] `TODO-pdp11.md`'s "Investigation targets: low-smoketest-coverage
      files" flagged `folder.c` at 16.1% (132/822 lines) as the
      lowest-coverage, largest non-trivial source file in the tree, to
      be checked (not assumed) for removable dead code -- same
      per-site grep-confirm treatment as every other cut in this file.
      Ran `gcov -f` for a per-function breakdown: every 0%-covered
      function was part of the Folder List screen (`folder_lister()`,
      321 lines, plus `add_new_folder()`/`rename_folder()`/
      `delete_folder()`/`search_folders()`/`create_folder_display()`/
      `display_folder()`/`folder_list_entry()`/etc.) -- bound to `'l'`/
      `OPF5` from both the main menu (`pine.c`) and the message index
      (`mailcmd.c`), a real, reachable single-collection folder
      browser, not dead code.
      This file had already been through two dedicated size-reduction
      passes (a general dead/vestigial-code sweep, and the later
      multi-collection-navigation cut that removed cross-collection
      browsing, the incoming-folders pseudo-collection, and several
      branches those cuts made provably dead). Read `folder_lister()`
      in full plus its six largest helpers looking for anything either
      earlier pass missed -- found nothing: no unreachable branches,
      no leftover `CNTXT_*`/collection state, no IMAP/news remnants
      (already fully gone). Conclusion: the coverage gap here is
      exactly what the TODO predicted going in -- untested interactive
      UI, not a removal candidate.
- **What changed**: added `test/cases/test_folder_list_screen.py`,
  driving the Folder List screen end to end through a real pty:
  opening it from the main menu; both navigation-boundary messages
  (`ctrl('B')`/`'p'` and `ctrl('F')`/`'n'`'s "Already on first/last
  folder" cases); add/rename/delete a folder (`add_new_folder()`/
  `rename_folder()`/`delete_folder()`, including clearing
  `rename_folder()`'s prefilled-name field with `^A`/`^K` since typing
  over it appends rather than replacing); and selecting a folder to
  open it (the `do_broach_folder()` branch, landing in FOLDER INDEX).
- **Verified**: full smoketest suite 26/26 (was 25/25) with the new
  test added. `python3 test/run_smoketests.py --coverage`:
  `folder.c` 16.1% (132/822) -> 49.5% (407/822); whole-suite total
  42.5% (3683/8670) -> 46.2% (4006/8670). `./build clean && ./build
  lnx` afterwards (coverage instrumentation inflates the binary,
  per the harness's own reminder) confirms no size change from
  baseline -- `bin/pine` text still 278717 bytes, as expected for a
  test-only addition with no source changes to `pine/folder.c` itself.

### `pine/mailcmd.c` low-coverage investigation -- one dead function and one dead-fallout collapse found, plus Jump/Search coverage

- [x] Continuation of the same coverage-investigation TODO item, next
      file after `folder.c` above. `mailcmd.c` was flagged as worth a
      fresh look post-F_*-collapse since `process_cmd()` was that
      cut's biggest single beneficiary. `gcov -f` per-function
      breakdown found two real, confirmed-dead items (this file had
      already been through its own dead/vestigial sweep earlier, so
      these are fresh fallout from the *later* F_* collapse, not
      leftovers that sweep should have caught):
      - **`mcdate2ctime()`**: a small `MESSAGECACHE` -> ctime-style
        date-string formatter, 16 lines including its comment.
        `grep -rn "mcdate2ctime"` across `pine/` and `imap/` turned up
        only its own forward prototype and definition -- zero callers
        anywhere in the built tree. Deleted outright, prototype and
        all.
      - **`jump_to()`'s dead `first_num` parameter, and its call
        site's leftover digit/PF10 checks**: the F_* collapse (see
        this file's own earlier phase-1 entry) deleted the `case '0':`
        .. `case '9':` labels that used to fall into the shared
        `do_jump:` label so a typed digit could seed the Jump prompt
        with that digit already entered, and deleted `search_headers`'s
        `F_ENABLE_JUMP` guard along with it. What that cut *didn't*
        clean up: three residual checks at the `do_jump:` call site
        still tested `command >= '0' && command <= '9'` (twice) and
        `command == PF10` (once) to decide whether to call `jump_to()`
        or `search_headers()`, and what to seed `jump_to()`'s
        `first_num` with -- but `do_jump:` is now only ever reached by
        falling through from `case OPF8: case 'w': case OOPF7: case
        'j':`, and `PF10` is a *different*, non-fallthrough case block
        entirely (`case PF10: case 'u':`, Undelete, terminated by its
        own `break`). So `command` can never be a digit or `PF10` by
        the time either check runs -- both were provably-dead
        conditions, confirmed by reading every `case` label between
        the two points, not just guessed from the F_* history.
        Collapsed the three-way disjunction down to
        `command == 'j' || command == OOPF7` at both call-site checks,
        dropped `jump_to()`'s now-always-`'\0'` `first_num` parameter
        entirely (and its prototype in `pine.h`), and collapsed the
        function body's `if(first_num){...} else jump_num_string[0] =
        '\0';` to the unconditional else-branch. Converted `jump_to()`'s
        signature to ANSI style while touching it (`pine/CLAUDE.md`'s
        standing opportunistic-conversion guidance).
- **What changed**: `pine/mailcmd.c` (function deletion + the
  `jump_to()`/call-site collapse above), `pine/pine.h` (`jump_to()`
  prototype updated to match). Also added
  `test/cases/test_index_jump_and_search.py`, driving the FOLDER
  INDEX's Jump (`'j'`) and Search (`'w'`, "where is") commands live
  through a real pty -- both exercise the exact code just collapsed,
  and neither had any prior smoketest coverage.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 27/27 (was 26/26) with the new test added.
  Also drove Jump manually outside the harness (jump to message 3 of
  3, confirmed the real message text/subject/body came up) to double-
  check the collapsed logic before trusting the automated test alone.
  `python3 test/run_smoketests.py --coverage`: `mailcmd.c` 24.4%
  (265/1086) -> 29.1% (313/1075) -- note the *denominator* also
  dropped (1086 -> 1075 lines), confirming real code left the file,
  not just a numerator change from added tests; whole-suite total
  46.2% (4006/8670) -> 46.9% (4063/8659).
- **Size delta**: `bin/pine` text 278717 -> 278309 bytes (-408 bytes),
  `bss` 18888 -> 18856 (-32 bytes), `data` unchanged. Small but real,
  and -- unlike the `folder.c` entry above -- this one came from
  actual source deletion, not just added test coverage.
- Everything else `gcov -f` flagged as 0%-covered in `mailcmd.c`
  (`display_text_att`/`display_attachment`/`select_sort`/
  `search_headers`/`closest_not_deleted`/`broach_folder`/`save`/
  `save_msg`) has a real, live caller -- attachment viewing, the sort-
  order screen, header search, post-expunge cursor repositioning, the
  Goto-folder prompt, and Save-message-to-folder are all genuine
  reachable features, just not yet exercised by any smoketest. Left
  as-is for now; `TODO-pdp11.md` still lists `reply.c`/`signals.c`/
  `ttyin.c` as the remaining files to look at.

### Build harness 2: a libFuzzer harness for pine's own consuming-side (envelope/MIME-display) code

Harness 1 (`fuzz/rfc822_fuzz.c`) stops once `rfc822_parse_msg()` has
produced a valid `ENVELOPE`/`BODY` -- it never exercises the code that
actually *renders* one. That's exactly where CVE-2000-0909's
recurrence lived (`pine/newmail.c`'s `new_mail_mess()`, several calls
downstream of the parser -- see that entry above). This closes the gap
with a second harness, `fuzz/consumer_fuzz.c`, targeting:

- `pine/mailview.c`'s `format_envelope()` (Date/From/To/Cc/Bcc/Reply-
  to/Newsgroups/Subject formatting for the message view), which calls
  `pine/reply.c`'s `pretty_addr_string()` for every address field --
  the line-wrapping address formatter.
- `pine/mailview.c`'s `describe_mime()` (recursively walks the whole
  MIME part tree building the attachment list), which calls
  `type_desc()`/`format_mime_size()`/`mime_can_display()` on each leaf
  part.
- `pine/mailview.c`'s `display_parameters()` on the top-level body's
  Content-Type parameters.

**Design decision -- reuse the real parser instead of hand-building
structs**: the TODO item originally proposed hand-splitting fuzzer
bytes across `ENVELOPE` fields via a `FuzzedDataProvider`-style
splitter, to avoid parsing overhead and get "interesting" structures
directly. Went a different way instead: reuse `rfc822_parse_msg()`
(the exact same header/body-splitting convention as
`fuzz/rfc822_fuzz.c`) to build a *real*, parser-produced
`ENVELOPE`/`BODY`, then feed that straight into the functions above.
Reasoning: `rfc822_cpy()`/`cpystr()` never cap anything upstream, so a
real parsed message already gives arbitrarily long/weird field content
-- there's no "interesting state" a hand-rolled splitter reaches that
a crafted `From:`/`Content-Type:` header couldn't also reach, and
reusing the real parser sidesteps the risk (flagged in
`fuzz/rfc822_fuzz.c`'s own header comment) of a hand-built splitter
producing states the real parser could never actually produce --
"bugs" that would really just be harness artifacts. This also means
`consumer_fuzz` exercises the *combination* of parser and
display/consumer code on every input, closer to what pine's real
message-open path actually does end to end.

**None of the target functions touch a live `MAILSTREAM`** (unlike
e.g. `format_message()`, which calls `mail_fetchtext()` on a real
stream -- out of scope for this harness, would need a fake mailbox
driver), so they're callable directly against the hand-produced
structures.

**Linking**: `fuzz/consumer_fuzz` links the *real*
`pine/mailview.o`/`pine/reply.o`/`pine/strings.o`/`pine/filter.o`
(built by a normal `./build lnx`) plus `c-client.a`, exactly as they'd
be linked into the real `bin/pine`. Everything else those object files
reference but this harness's call graph never actually reaches at
runtime -- screen drawing (`ClearScreen`/`MoveCursor`/...), command
dispatch (`process_cmd`/`read_command`/...), the composer subprocess
(`pine_send`), and live-mailbox operations (`save_attachment`,
`new_mail`, ...) -- is stubbed out in `fuzz/pine_ui_stubs.c` (mirrors
`fuzz/mm_stubs.c`'s approach for c-client's `mm_*` callbacks). The one
exception is `grow_tmp_20k_buf()`, given a real, faithful
implementation (mirroring `pine/pine.c`'s own) since
`display_parameters()` genuinely depends on it growing `tmp_20k_buf`
correctly -- a no-op stub there would silently turn a real bug into a
harness artifact (an overflow of a buffer this harness itself failed
to grow, not one pine's real code would ever produce). `ps_global` is
a minimal `calloc`'d `struct pine`, with just `atmts`/`atmts_allocated`
set up (mirroring `pine/pine.c`'s own init) since `describe_mime()` is
the only target function that touches `ps_global` at all.

Two build-time snags along the way, both simple: `pretty_command()`
didn't need stubbing (it's a real function already provided once
`pine/strings.o` is linked -- a duplicate-definition link error caught
this immediately), and `read_file()` (called by `reply.c`'s
`get_signature()`, unreached by this harness but still in the same
object file) was missing from the initial stub list -- another
straightforward duplicate/missing-symbol link error caught it.

- **Verified**: builds cleanly via the updated `fuzz/build.sh`. Ran
  ~25 million executions total across two sessions (curated seed
  corpus covering a simple message, a multipart/mixed message, an
  unbalanced RFC822 comment in `From:`, multipart/alternative,
  a nested `message/rfc822` part, and an encoded-word `From:`) with
  **zero crashes** -- a clean baseline for this newly-built harness on
  already-hardened code (`describe_mime()`/`type_desc()`/`part_desc()`
  had three previous overflow fixes from a prior audit pass, and
  `pretty_addr_string()` had one; see their respective entries above).
  Coverage plateaus quickly (cov: 25, ft: 68) with this seed corpus --
  worth revisiting with a richer corpus (deeper MIME nesting, more
  encoded-word variety, RFC822 group syntax in more fields) in a
  future session. Full clean rebuild (`./build clean && ./build lnx`),
  0 warnings, 28/28 smoketests.
- Added `fuzz/consumer_fuzz.c`, `fuzz/pine_ui_stubs.c`, updated
  `fuzz/build.sh` to build all three harnesses, added
  `fuzz/corpus/consumer/` with six curated seeds, and added
  `fuzz/consumer_fuzz` to `.gitignore` (matching the other two harness
  binaries -- `fuzz/*.o` already covers its intermediate object file).

### `pine/reply.c` low-coverage investigation -- one dead function found, plus Reply coverage

- [x] Continuation of the same coverage-investigation TODO item, next
      file after `mailcmd.c` above. `reply.c` had already been through
      its own DOS/never-defined-macro dead-code sweep ("Dead-code
      sweep, bucket 1 of 3" above) and the F_* collapse (`F_SIG_AT_BOTTOM`/
      `F_INCLUDE_HEADER`/`F_MIME_FORWARD`, see this file's phase-1
      entry) well before this pass, so expected thin pickings -- read
      `reply()`, `forward()`, and every site those two F_* cuts touched
      looking for leftover fallout the way `mailcmd.c`'s `jump_to()`
      had; found none there (the signature-insertion and forwarded-
      text-header logic both read as fully collapsed already, single
      unconditional paths, no stray guard conditions). Did find one
      fresh item via `gcov -f`'s per-function breakdown:
      - **`copy_envelope()`**: a 27-line "deep-copy an `ENVELOPE`"
        helper. `grep -rn "copy_envelope"` across `pine/`, `pico/`, and
        `imap/` found only its own prototype and definition in the
        built tree -- its sole other mention anywhere is
        `pine/pine.def`, a DOS/OS2 module-definition file that isn't
        compiled or generated from source here (same "historical/
        foreign-platform artifact, left untouched" category as other
        `pine.def` mentions noted in this file's `mail_search`/
        `bezerk_search` removal entry above). Deleted the function and
        its prototype.
      - `reply()` itself (137 lines, the biggest function in the file)
        was 0%-covered along with everything only *it* calls
        (`reply_delimiter()`, `generate_reply_to()`, `first_addr()`) --
        no smoketest had ever pressed `'r'`. All confirmed live and
        reachable (`pine/mailcmd.c:372` calls `reply()` from the Reply
        command), not dead -- same "untested UI, not removable"
        conclusion as `folder.c`'s Folder List screen.
- **What changed**: `pine/reply.c` (`copy_envelope()` deletion and its
  prototype). Also added `test/cases/test_reply_screen.py`, driving
  Reply end to end through a real pty: the "Include original message"
  and "Reply to all recipients" prompts, landing in the composer with
  the quoted original text and `"On ... wrote:"` delimiter line
  correctly generated, then Cancels out (same pattern as
  `test_compose_cancel.py`) rather than actually sending.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 28/28 (was 27/27) with the new test added.
  `python3 test/run_smoketests.py --coverage`: `reply.c` 36.8%
  (181/492) -> 54.4% (256/471) -- denominator dropped 492 -> 471,
  confirming real code left the file; whole-suite total 46.9%
  (4063/8659) -> 48.1% (4152/8638).
- **Size delta**: `bin/pine` text 278309 -> 277813 bytes (-496 bytes),
  `bss`/`data` unchanged. All from `copy_envelope()` -- a real,
  compiled-in dead function, not a preprocessor-stripped branch.
- Everything else `gcov -f` flagged as 0%-covered in `reply.c`
  (`first_addr`/`generate_reply_to`/`copy_envelope` -- pre-deletion --
  were the only 0% entries besides `reply()` and `reply_delimiter()`,
  both now covered by the new test) is otherwise accounted for.
  `TODO-pdp11.md` still lists `signals.c`/`ttyin.c` as the remaining
  files to look at -- both already went through a DOS-macro sweep in
  "bucket 2 of 3" above, so likely the same "untested, not dead"
  outcome as `folder.c`/`reply.c`, but worth the same per-function
  check before assuming so.

### `pine/signals.c` low-coverage investigation -- no dead code found, added real-signal coverage

- [x] Continuation of the same coverage-investigation TODO item, next
      file after `reply.c` above. `signals.c` had already been through
      its own DOS-macro sweep ("bucket 2 of 3" above), so this pass
      was purely a hunt for reachable-but-untested code, not dead
      code. `gcov -f` showed only `init_signals()` (called at every
      startup) and `winch_signal()` (already exercised indirectly by
      the composer resize tests' `SIGWINCH`) with any coverage --
      every other handler was 0%, because no existing smoketest had
      ever sent the process a real signal. Confirmed each 0% function
      is a genuine, live signal handler (all wired up in
      `init_signals()`, all with real call sites) -- same "untested
      UI, not removable" conclusion as `folder.c`/`reply.c`, just for
      signal handlers instead of screens.
      Two handlers deliberately excluded from the new test since
      exercising them means deliberately crashing the test binary:
      `auger_in_signal()` (`SIGSEGV`/`SIGILL`/etc.) calls `panic()` ->
      `exit(-1)`; `end_signals()` is *only* ever reached from
      `panic()`, confirmed via `grep -rn "end_signals("` (its sole
      caller). `quit_signal()`'s abort branch (answering "y" to the
      "REALLY ABORT PINE" prompt) calls `coredump()` for the same
      reason -- only its decline branch is driven.
      Verified `do_suspend()`'s `stop_process()` (`kill(0, SIGSTOP)`,
      `pine/os.c`) is safe to test before writing it up: `pexpect`
      gives the spawned `pine` its own process group (confirmed via
      `os.getpgid()` differing from the test harness's own `os.getpgrp()`
      in a manual probe), so `kill(0, ...)` inside pine only ever
      touches pine's own group, never the harness process.
- **What changed**: added `test/cases/test_signal_handlers.py`, using
  `os.kill()`/`os.killpg()` against the real spawned `pine` process
  (exposed via `pexpect`'s `session.child.pid`) to drive: `SIGHUP` and
  `SIGTERM` (both graceful-exit paths, `hup_signal()`/`term_signal()`
  -> shared `fast_clean_up()` -> `exit(0)`, checked via each function's
  distinct "Received hang up/terminate signal" message and a `wait()`
  status of 0); `SIGUSR2` (`usr2_signal()`'s "Session now Read-Only"
  message, then confirms the session is still fully usable afterward);
  `SIGQUIT`'s decline path (`quit_signal()`'s emergency-exit prompt,
  answering "n"); and `^Z` suspend/resume (`do_suspend()`, sending
  `SIGCONT` to pine's process group and confirming it lands back at
  MAIN MENU).
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 30/30 (was 29/29) with the new test added, and
  the new test run standalone first to shake out timing issues before
  trusting it in the full run. `python3 test/run_smoketests.py
  --coverage`: `signals.c` 29.8% (34/114) -> 71.9% (82/114); whole-
  suite total 48.4% (4186/8641) -> 49.2% (4248/8641).
- **Size delta**: none -- `bin/pine` text unchanged at 277941 bytes
  (test-only addition, no source deletion this time; unlike
  `folder.c`'s and `mailcmd.c`'s/`reply.c`'s entries above, this
  file's earlier sweeps had already caught everything removable).
  `TODO-pdp11.md` still lists `ttyin.c` as the one remaining file.

### `pine/ttyin.c` low-coverage investigation -- one dead `#else` branch found, plus arrow-key coverage

- [x] Last file in the coverage-investigation TODO item, after
      `signals.c` above. `ttyin.c` had already had a small DOS-macro
      sweep ("bucket 2 of 3": two sites in `optionally_enter()`), but
      a fresh look at `read_char()` -- the standout at 8.5% of 153
      lines -- turned up one more dead macro that sweep didn't reach:
      `read_char()` itself is duplicated inside an `#ifndef
      OLD_READ_CHAR ... #else ... #endif` (lines 388-755), and
      `grep -rn "OLD_READ_CHAR"` across `pine/` and `imap/` found only
      that one `#ifndef` site -- never defined anywhere, not even
      commented out, same standard as the earlier `DOS`/`LWP`/`PCTCP`/
      `PCNFS` removals. Deleted the entire dead `#else` half (158
      lines, an older, structurally different escape-sequence decoder
      for the same key set) and un-wrapped the `#ifndef` around the
      live half. Since the preprocessor had already discarded the
      dead half in every build (this is a compile-time macro, not a
      runtime branch), this doesn't move `gcov`'s coverable-line count
      or `bin/pine`'s size -- pure source hygiene, same "0 byte
      delta" pattern as the other preprocessor-only DOS-macro cuts in
      this file's history.
      The surviving `read_char()`'s remaining low coverage is a real
      terminal-escape-sequence decoder (arrow keys, PF-keys, multiple
      terminal families -- vt100/vt200 CSI, vt100 application-mode
      SS3, Sun, DEC vt52, Zenith Z19) -- all genuinely reachable, just
      never exercised because every existing smoketest drives pine
      with plain ASCII keys, never a real terminal's multi-byte escape
      sequences for cursor keys.
- **What changed**: `pine/ttyin.c` (dead `#else`/`OLD_READ_CHAR`
  branch removed). Also added
  `test/cases/test_arrow_key_navigation.py`, sending the two escape
  sequences a vt100-class terminal (like this project's real WY-60,
  see `blog/`) actually uses for cursor keys -- CSI (`ESC [ A`/
  `ESC [ B`) and SS3 (`ESC O A`/`ESC O B`) -- and confirming they move
  the FOLDER INDEX cursor exactly like `'p'`/`'n'`
  (`pine/mailcmd.c:203/219` bind `KEY_UP`/`KEY_DOWN` to the same
  branches as those keys).
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 31/31 (was 30/30) with the new test added, run
  standalone first. `python3 test/run_smoketests.py --coverage`:
  `ttyin.c` 37.8% (186/492) -> 41.9% (206/492); whole-suite total
  49.2% (4248/8641) -> 49.5% (4277/8641).
- **Size delta**: none -- `bin/pine` text unchanged at 277941 bytes,
  as expected for a preprocessor-only source deletion plus a test-only
  addition.
- This closes out the "Investigation targets: low-smoketest-coverage
  files" TODO item -- all five flagged files (`folder.c`, `mailcmd.c`,
  `reply.c`, `signals.c`, `ttyin.c`) are done. Net across all five:
  three real dead-code removals (`mcdate2ctime()`, `copy_envelope()`,
  the `jump_to()` fallout, and this file's dead `#else` branch --
  `bin/pine` text -904 bytes total), five new smoketest files, and
  whole-suite coverage 42.5% -> 49.5%.

### Real 2.11BSD/PDP-11 `cc` build errors found bringing pine up on hardware

Compiling `imap/ANSI/c-client/` against the actual 2.11BSD `cc` (not the
gcc-based `lnx` dev build) surfaced two more gaps invisible on `lnx`,
same tier as the pico-side `bind.c`/`tcap.c`/`os_unix.c` fixes from the
prior hardware bring-up session.

- **`mail.c:104-154`: "Constant required" on every `case` label in
  `mm_cache()`'s switch** -- the `CH_INIT`/`CH_SIZE`/`CH_MAKELELT`/
  `CH_LELT`/`CH_MAKEELT`/`CH_ELT`/`CH_FREE`/`CH_EXPUNGE` cache-op codes
  are `#define`d in `mail.h` as `(long) N` (a cast expression), and
  2.11BSD's pcc-derived `cc` rejects a cast expression in a `case`
  label -- it wants a bare integer constant. Fixed by dropping the
  `(long)` cast from these eight macros' definitions in `mail.h` (left
  every other `(long) N` constant in the file alone -- only these eight
  are ever used as `case` labels, confirmed via
  `grep -rn "case CH_"`/`grep -rn "case (long)"` across `pine/`,
  `pico/`, `imap/`). Safe because every other use of these macros is an
  argument to `mm_cache()` or the `mailcache_t` function-pointer type
  (both in `mail.c`/`bezerk.c`), and both are fully ANSI-prototyped
  with a `long op` parameter -- so the now-bare `int` constant still
  widens to `long` correctly at the call site via the prototype. This
  is a different situation from the `%D`/`emlwrite()` varargs casts
  documented earlier in this file (search "already the pattern the
  code used for its one genuinely `long`-sized case") -- those casts
  are load-bearing because a varargs callee can't see a prototype;
  `mm_cache()` isn't varargs, so the cast here was only ever needed to
  satisfy the `case`-label constant-expression rule, not for actual
  type-correctness.
- **`bezerk.c:75`: "No field initialization" / "warning: union
  initialization non-portable" on `bezerkproto`'s aggregate
  initializer** -- `MAILSTREAM bezerkproto = {NIL,NIL,T};` was setting
  the struct's `open` member (an `unsigned int open : 1;` bit-field,
  added when the old `dtb`-as-liveness-flag was replaced -- see this
  file's "Collapse ... `default_driver`/`create_proto` fallout" entry)
  via a positional aggregate initializer, with the struct's trailing
  `cache` union left to implicit zero-fill. 2.11BSD's `cc` can't
  initialize a bit-field member through an aggregate initializer at
  all (a real, known pcc-era limitation, not just a style nit), and
  separately warns that eliding down to a union member's zero-fill is
  non-portable. Fixed by letting `bezerkproto` zero-initialize as a
  plain declaration (`MAILSTREAM bezerkproto;`, valid everywhere, no
  diagnostic) and setting `bezerkproto.open = T;` explicitly as the
  first statement in `pine.c`'s `main()`, before any code path that
  could reach the prototype stream (`default_driver()` via
  `DF_DRIVER_PROTO`, or `bezerk_open(NIL)`'s `OP_PROTOTYPE` return).
  Confirmed pico never links `bezerk.c`/c-client at all (`grep -n
  "c-client\|bezerk" pico/makefile.lnx pico/makefile.bsd` -- no
  matches), so this is entirely pine's concern and `main()` is a safe,
  single, unconditional place to set it.
- **`pine.h:627-630`: "Illegal #" / "GCC undefined; func. pragma" /
  "diagnostic undefined; func. pragma" on `#pragma GCC diagnostic
  push`/`ignored`/`pop` around the `gf_io_t` typedef, cascading into
  "gf_io_t redeclared" and further "Declaration syntax" errors at every
  later `gf_io_t`-using prototype in the file (`gf_pipe()`,
  `gf_set_so_readc()`, etc.)** -- these pragmas (added in `d76a936`,
  "enable Tier 2 warning flags", to silence gcc's
  `-Wstrict-prototypes`/`-Wformat-nonliteral` on the deliberately
  loose/varargs-style `gf_io_t` and `sprintf`-wrapper call sites) are a
  GCC-specific extension. 2.11BSD's pcc-derived `cc` doesn't recognize
  `#pragma GCC ...` at all -- it fails to parse the pragma's own text
  ("Illegal #"/"Expression syntax"), which on this compiler doesn't
  just warn and skip the line, it corrupts parsing of whatever
  declaration follows: here, the `gf_io_t` typedef itself never
  registers, so every subsequent use of `gf_io_t` as a parameter type
  errors out as an unknown/redeclared identifier. Same root cause
  and same fix applied at all nine other call sites across the tree
  using this GCC-only pragma pair to silence `-Wformat-nonliteral`
  around deliberately non-literal `sprintf()` format strings
  (`pine.c` x2, `status.c` x5, `context.c`, `screen.c`, `ttyout.c` x4)
  -- found via `grep -rn "pragma GCC"` once the first instance in
  `pine.h` was identified as the culprit, fixed proactively rather
  than waiting for each to surface one build error at a time as `make`
  reached each file. Fixed by wrapping each `push`/`ignored` pair and
  its matching `pop` in `#ifdef __GNUC__`/`#endif`, so 2.11BSD's `cc`
  (which never defines `__GNUC__`) has the pragma lines removed by the
  preprocessor before they ever reach pragma parsing, while gcc/clang
  (both define `__GNUC__`) see them exactly as before.
- **`filter.c:247,311,314,317,543,553`: "Illegal conditional"/"Lvalue
  required" on six `cond ? assignment, value : value` returns** --
  `so_readc()`, `so_seek()` (all three `switch` cases), `gf_sreadc()`,
  and `gf_swritec()` each packed an assignment plus a return value into
  a comma expression sitting in a conditional operator's true-branch,
  e.g. `return((so->txtp - ...) < so->used ? *c = *(so->txtp)++, 1 :
  0);`. Per the ANSI C grammar a conditional operator's middle operand
  is a full `expression` (comma-expressions included), so this is
  legal C -- but 2.11BSD's pcc-derived `cc` doesn't handle a comma
  expression there, misparsing the `:` and bailing with "Illegal
  conditional"/"Lvalue required" at the line the comma appears on.
  Fixed by rewriting all six as plain `if`/`else` with the assignment
  and `return` as separate statements -- functionally identical, just
  not relying on a compiler feature this target's `cc` doesn't
  support.
- **`filter.c:784,812,859,874`: "Illegal type of operand" on four
  `switch` statements selecting on `FILTER_S.n`** -- `gf_binary_b64()`
  and `gf_b64_binary()` (the BASE64 encode/decode filters) each
  `switch` on an expression involving `f->n`, which is declared `long`
  in `FILTER_S` (`pine.h`). 2.11BSD's `cc` only accepts an `int`-typed
  switch selector -- a real, known restriction of this compiler
  generation's `switch` code generator (it predates C's later
  broadening of `switch` to any integer type), not a style nit. Fixed
  by casting each selector expression to `(int)` at the `switch`
  (`switch((int)(f->n++ % 3))`, `switch((int)(f->n % 3))`,
  `switch((int)((f->n)++))` x2). Safe in all four cases: `f->n` here is
  always a small BASE64 quantum position (0-3, or the equivalent mod-3
  phase), explicitly reset to `0L` at multiple points in both
  functions and in their `GF_RESET` handlers -- it never approaches a
  value `int` can't represent, so truncation from `long` to `int`
  changes nothing observable.
- **`init.c:127-155`: "No field initialization" / "warning: union
  initialization non-portable" / "Too many structure initializers" /
  "illegal combination of pointer and integer, op =" on every entry of
  the `variables[]` config-variable table** -- same root cause as the
  `bezerkproto` fix above, on a bigger scale: `struct variable`
  (`pine.h`) declares `char *name`, then six `unsigned:1` bit-fields
  (`is_obsolete`/`is_used`/`been_written`/`is_user`/`is_global`/
  `is_list`), then `char *description`, then four value unions. Every
  one of `variables[]`'s ~14 entries positionally initialized the
  bit-fields with literal 0/1 values (`{"personal-name", 0, 1, 0, 1, 0,
  0, PERSONAL_NAME_TEXT}`, etc.) -- 2.11BSD's `cc` can't initialize a
  bit-field via an aggregate initializer at all, and once it chokes on
  the first one it appears to give up matching the rest of that
  entry's positional list against remaining struct members entirely,
  producing the pile of "Too many structure initializers"/"illegal
  combination of pointer and integer" follow-on errors on the same and
  next lines (the `description` string landing nowhere valid, a
  trailing union left to non-portable implicit zero-fill). Fixed at
  the root by reordering `struct variable` (`pine.h`) to put
  `description` right after `name`, ahead of the bit-fields (a pure
  declaration reorder -- every other reference to these members in the
  tree is by field name, confirmed via `grep -rn "struct variable"`
  across `pine/`, no positional access anywhere else), so each
  `variables[]` entry's initializer is now just `{name, description}`
  and every bit-field plus every value union zero-fills implicitly
  with no explicit bit-field initializer text ever appearing in the
  source (the actual trigger for "No field initialization", not
  implicit zero-fill, which 2.11BSD's `cc` accepts fine -- same
  distinction the `bezerkproto` fix relied on). The six flags, which
  previously varied per entry and can't be given back via the
  initializer, are now set explicitly once in `init_init_vars()`: a
  loop sets `is_user`/`is_used` for every real entry (`user-id`
  excepted -- `is_used` stays 0, intentionally disabled on Unix per
  its pre-existing comment), a second loop sets `is_global` for the
  contiguous `V_USER_DOMAIN`..`V_USE_ONLY_DOMAIN_NAME` range, and
  `is_list` is set explicitly on the two list-valued variables
  (`V_FOLDER_SPEC`, `V_INIT_CMD_LIST`) -- all three patterns transcribed
  directly from the original per-entry table so they're easy to audit
  against it line-for-line.
- **Verified**: the `mail.c`/`mail.h`, `bezerk.c`/`pine.c`, and
  `init.c`/`pine.h` fixes reproduce the reported `cc` diagnostics
  exactly against the line numbers in the real 2.11BSD build output
  (no PDP-11 cross-compiler available here to re-run `cc` directly, so
  this is inference from the diagnostic text plus direct reading of
  the relevant grammar limitation, not a re-run against real hardware
  `cc`); the `pine.h` pragma fix and the `filter.c` fixes likewise
  each match real 2.11BSD `cc` output pasted in by the user
  (`pine.h:628-692`; `filter.c:247-874`; `init.c:128-130`), same
  inference method. `./build clean && ./build lnx`, exit 0, 0 warnings,
  before and after each fix. Full smoketest suite 31/31 throughout.
  For the `init.c` fix specifically, also verified under `gdb` (break
  at `init_vars()`, print every `variables[]` entry) that every flag
  the runtime code now sets matches the original static table exactly,
  entry by entry (`is_used`/`is_user`/`is_global`/`is_list` spot-checked
  on `personal-name`, `user-id`, `user-domain`, `folder-collections`,
  `initial-keystroke-list`, `use-only-domain-name`, and the `NULL`
  terminator) -- this one changes actual runtime data, not just syntax,
  so build-clean and smoketest-green alone weren't enough confidence.
- **Size delta**: `bin/pine` `data` 10520 -> 10168 (-352 bytes, the
  `bezerkproto` initializer moving out of the initialized-data segment
  into BSS), `bss` 18856 -> 19208 (+352 bytes, symmetric move into
  BSS); `text`/overall size unchanged by those two. The `pine.h`
  pragma fix and the `filter.c` `if`/`else`-and-cast rewrites produced
  no further size change at all. The `init.c`/`pine.h` fix costs `text`
  277941 -> 278133 (+192 bytes, `init_init_vars()`'s new loops/
  assignments); `data`/`bss` unchanged (the struct reorder doesn't
  change its total size, just member order, and the array itself was
  already living in the same segment). `bin/pico` untouched throughout
  (no c-client link, and none of these fixes touch pico-only code).

### Fix `pico/random.c`'s `sprintf` into a too-small `buffer[80]`, flagged by `-Wformat-overflow=` under `ASAN=1`

- [x] Found incidentally while sweeping for the trailing-backslash/
      unbounded-pointer-walk bug class (see this file's "Sweep for the
      general ... bug shape" entry): `./build lnx ASAN=1` (the
      correctly-invoked form -- see the ASan-build-target entry's
      process note about the no-op `ASAN=1 ./build lnx` env-var form)
      surfaces `random.c:114:74: warning: 'sprintf' may write a
      terminating nul past the end of the destination
      [-Wformat-overflow=]`, with gcc's own note pinning the real
      range: "'sprintf' output between 42 and 120 bytes into a
      destination of size 80". Reproduced first, per this project's
      standing verification discipline, before touching anything.
      `showcpos()` (bound to `^X =`, "show cursor position") builds a
      status line like `"line 12 of 340 (3%), character 456 of 7890
      (5%)"` via `sprintf(buffer, "line %d of %d (%d%%%%), character
      %ld of %ld (%d%%%%)", ...)` into `char buffer[80]`. Worst-case
      argument widths on this dev platform (three `%d` ints at 11
      bytes each for `INT_MIN`, two `%ld` longs at 20 bytes each for
      `LONG_MIN` on a 64-bit `long`, ~31 bytes of literal text, plus
      the `%%%%` pairs each expanding to one literal `%`) sum to
      exactly the 120 bytes gcc reported -- confirmed by hand before
      picking a fix size, not just trusting the compiler's number
      blind. A real, if essentially unreachable in practice, buffer-
      sizing bug: `thisline`/`lines`/`nch`/`nbc` are derived from
      walking the actual open buffer's line/character counts, so
      hitting the true worst case would need billions of lines or
      exabytes of text open in the editor.
      `snprintf` (the TODO's other suggested fix) was considered and
      rejected: 2.11BSD (derived from 4.3BSD, 1986) predates
      `snprintf`'s standardization, and this is pico editor code that
      genuinely gets compiled for the real BSD target via
      `pico/makefile.bsd` -- unlike the `os-lnx.c`/`bld_path`
      `snprintf` calls elsewhere in the tree, which are Linux-dev-only
      fragments never built for the PDP-11 target. Grew the fixed
      buffer instead, sized for the worst case on whichever platform
      has the wider types (this dev/ASan target's 64-bit `long`, wider
      than the PDP-11 target's 32-bit `long`), so the fix is safe on
      both.
- **What changed**: `pico/random.c`'s `showcpos()`: `buffer[80]` ->
  `buffer[160]` (the 120-byte computed worst case plus headroom), with
  a short comment recording why 80 wasn't enough.
- **Verified**: `./build clean && ./build lnx ASAN=1` -- the
  `-Wformat-overflow=` warning on `random.c` is gone, 0 warnings
  across the whole ASan build. `./build clean && ./build lnx` (plain)
  also 0 warnings; full smoketest suite 31/31. No regression test
  added -- per this project's own framing of the bug going in
  ("impractical-to-hit-by-accident"), there's no way to construct a
  file in the test harness that pushes line/character counts anywhere
  near the actual overflow threshold, so there's nothing for an
  automated test to observably distinguish.
- **Size delta**: `bin/pico` text 109094 -> 109103 bytes (+9), the
  cost of the larger stack buffer's frame setup; `bin/pine` unaffected
  (`random.c` is pico-only). A tiny, expected cost for a genuine
  safety fix, consistent with how this project has treated similar
  small size costs from other overflow fixes.

### Strip stray form-feed (`^L`) page-break characters from older source files

- [x] `grep -rl $'\f' pine/ imap/ANSI/c-client/` found 33 files with
      literal form-feed characters used as printer page-break
      separators, mostly before function comment blocks -- the
      original UW c-client tree's convention, still present in every
      one of its per-OS driver files (`os_mac.c`/`os_aix.c`/
      `os_bsi.c`/`os_osf.c`/`os_a32.c`/`os_t20.c`/`os_sco.c` and
      their `.h` counterparts) even though only `os_lnx.c`/`os_lnx.h`
      (this dev target) and `os_bsd.c`/`os_bsd.h` (the `three`
      checkout's real-hardware target, see that memory) are ever
      actually compiled here -- plus the always-built `mail.c`/
      `bezerk.c`/`rfc822.c`/`misc.c` and their headers, and
      `pine/context.c`. Before stripping anything, checked *every*
      occurrence's surrounding bytes (`cat -A` across all 33 files)
      to confirm the character always sits alone on its own line
      (optionally with trailing spaces/tabs from period keyboard/
      terminal quirks, e.g. `os_lnx.c`'s `^L ` with a trailing space)
      -- never embedded mid-text -- so a mechanical strip-and-blank
      couldn't corrupt a comment or string literal.
      One file was deliberately excluded: `Internal.DOC`. Its 7 form
      feeds are genuine printed-page-break separators between major
      documentation sections (e.g. right before a "I. Public
      interface" heading) -- a legitimate use for a prose document,
      unlike the "leftover from a line printer" convention in the
      source files, and out of scope anyway since it isn't source
      code. `imap/ANSI/c-client/osdep.h` also matched the original
      sweep but needed no separate edit: it's `.gitignore`d (not a
      tracked file at all) and hardlinked to whichever `os_*.h` the
      last build symlinked it from (`ln os_lnx.h osdep.h` in
      `makefile.lnx`), so stripping `os_lnx.h` fixed the shared inode
      automatically.
- **What changed**: replaced every line whose content was solely a
  form-feed character (plus optional trailing whitespace) with a
  genuinely empty line, in all 33 non-`Internal.DOC` files -- 303
  lines total. Pure whitespace substitution: no line was added,
  removed, or reordered, so surrounding blank-line spacing is
  identical to before.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 31/31. Spot-checked several diffs by hand
  (`git diff imap/ANSI/c-client/bezerk.c`, etc.) to confirm every hunk
  is a single blank-line-for-blank-line-with-`^L` swap, nothing more.
  Final `grep -rl $'\f' pine/ imap/ANSI/c-client/` confirms only
  `Internal.DOC` remains, as intended. No regression test -- pure
  whitespace/comment change with no observable runtime behavior to
  distinguish.
- **Size delta**: none -- `bin/pine`/`bin/pico` byte-for-byte
  unchanged (277941/109103 bytes text, same as going in). Purely a
  tooling-friction fix: future `Edit`-tool-based cuts in these files
  (this sweep's own motivation was the friction hit during the
  `mail_search`/`bezerk_search` removal) no longer need the
  `sed`-with-explicit-line-ranges fallback these stray `^L`s used to
  force.

### Fix undefined `snprintf`/`clock`/`memmove` found linking pine on 2.11BSD hardware

While bringing up `pine-ovl` (the overlay build needed because pine,
like pico, is over the PDP-11's 64KB text-segment limit -- see
`pine/makefile.bsd`), the real 2.11BSD `ld` reported three undefined
symbols once compilation got far enough to actually link:

- **`_snprintf`** -- `pine/osdep/bld_path` (a shared fragment,
  assembled into both `os-lnx.c` and `os-bsd.c`, see
  `pine/makefile.bsd`'s `osdep/os-bsd.c:` dependency list) calls
  `snprintf()` unconditionally in `build_path()`. Same gap already
  found and documented for `pico/random.c`: 2.11BSD's libc predates
  `snprintf`'s standardization. `build_path()`'s own header comment
  stresses that its truncation-safety matters (`first_part`/
  `second_part` can be attacker-influenced MIME filenames), so this
  couldn't just be dropped -- rewrote it as an explicit truncating
  strncpy-based concatenation instead of reaching for a general
  `vsnprintf`-style compat shim, keeping the fix's surface area small
  for a security-relevant function. Verified behaviorally identical to
  the original `snprintf` version with a standalone comparison harness
  across 13 edge cases (empty parts, `first_part == NULL`, buffer
  exactly the string length, buffer one/two/three bytes, both parts
  individually overflowing the buffer) -- all byte-for-byte matches.
- **`_clock`** -- `imap/ANSI/c-client/os_bsd.h` had `#define gethostid
  clock`, carried over verbatim from `os_lnx.h` (identical line,
  identical stale comment: `/* non in the Linux C library?? */`) when
  `os_bsd.h` was created from it. `clock()` works fine on Linux (glibc
  has it) but doesn't exist on 2.11BSD either, so the substitution
  just traded one missing function for another. 4.3BSD-derived systems
  (2.11BSD's lineage) have a real `gethostid()`/`sethostid()` pair
  (added in 4.2BSD for NFS), so the fix was simply to delete the
  leftover `#define` and let the real libc function resolve normally
  -- no compat shim needed unless the real hardware link proves
  otherwise.
- **`_memmove`** -- `bezerk.c` calls `memmove()` directly at two sites
  (`LOCAL->buf` refill, header compaction), unconditionally on every
  platform. Other per-OS c-client files already carry their own
  `memmove()` for platforms lacking it (`os_sco.c`, `os_aix.c`), so
  `os_bsd.c` needed the same: added a small self-contained
  forward/backward byte-copy implementation (handles overlap in both
  directions, unlike `memcpy`), matching the shape already established
  by `os_sco.c`'s version.
- **Verified**: matches the exact three undefined symbols the user
  pasted from the real 2.11BSD `ld` output, name for name. No PDP-11
  cross-compiler available here to re-link and confirm resolution
  directly. `./build clean && ./build lnx`, exit 0, 0 warnings (the
  `bld_path` rewrite is shared with the `lnx` build too, so this is a
  real regression check, not just inference); full smoketest suite
  31/31. The `gethostid`/`memmove` fixes are BSD-only (`os_bsd.c`/
  `os_bsd.h` aren't part of the `lnx` build) and unverifiable without
  the real target.
- **Size delta**: `bin/pine` `text` 278133 -> 278178 (+45 bytes, the
  larger `build_path()`), `data` 10168 -> 10160 (-8 bytes, no longer
  needing whatever `snprintf`'s format-string handling cost in the
  initialized-data segment); `bss` unchanged. `bin/pico` untouched
  (doesn't link c-client, and `bld_path` isn't part of pico's own
  build). The `os_bsd.c`/`os_bsd.h` changes don't affect `bin/pine`'s
  `lnx` size at all -- that OS layer isn't compiled into it.

### Remove `struct variable`'s dead `description` field to fit pine-ovl's data+bss budget

`pine-ovl` (see prior entry) got far enough to actually attempt the
2.11BSD `ld` link, and the real diagnostic ("`ld: too big for type
431`") turned out to be a red herring at first glance -- `431` isn't a
computed size at all, it's `ld.c`'s `a_magic` constant for the `-i`
(separate I&D) executable format, printed with `%o` (octal), and
identical regardless of how `mkovmake` partitions text into base vs.
overlay groups. Reading the real 2.11BSD `ld` source (`ld.c`, supplied
by the user alongside `mkovmake`'s and `ld`'s own man pages,
`mkovmake.txt`/`ld.txt`) pinned the actual check for `-i` builds:
`round8k(base_text) + round8k(largest_overlay) > 64KB` **or**
`data + bss > 56KB`. Computing both from the user's real `size` output
across every pine/pico/c-client object (`size.txt`) showed the text
side has huge headroom (32KB of 64KB used, base 8030 -> rounds to
8192, largest of 10 `-Z` groups 24558 -> rounds to 24576), while
data+bss for pine+pico+c-client's own code alone already totals 54954
bytes against the 57344-byte (56KB) ceiling -- only 2390 bytes of
headroom, before even counting whatever `-ltermlib`/`-lc` themselves
add (almost certainly enough on their own to push it over, matching
the real `ld: .../libtermlib.a(termcap.o): text overflow` seen with
the archived-library `pine-ovl` attempt in the prior entry). Since
data+bss is never overlaid (always fully resident, regardless of
partitioning), this is a real "shrink the actual static footprint"
problem, not another `mkovmake` tuning question.

- **What was found**: `struct variable` (`pine.h`) has a `description`
  field, and `init.c`'s `variables[]` table populated it for all 13
  config variables with long human-readable help text (10 `#define
  ..._TEXT` macros plus 3 inline strings, ~2.3KB total) -- text meant
  for an interactive Setup/Config variable-editing screen. That screen
  was already cut in an earlier PDP-11 size-reduction pass (no
  `config_screen.c`-equivalent exists in this tree, and the top-level
  "Setup" menu entry in `pine.c` has no visible handler), and a
  repo-wide grep for `.description` usage confirmed the only hits left
  are `BODY`/`ATTACHMENT`'s unrelated same-named field -- `struct
  variable`'s `description` is never read anywhere. Pure dead weight,
  100% safe to remove: no behavior depends on it.
- **What changed**: removed the `description` field from `struct
  variable` (`pine.h`) entirely, removed the 10 now-unused `..._TEXT`
  macros and inline description strings from `init.c`, and shrank
  every `variables[]` entry back down to just `{"name"}` (the terminator
  to `{NULL}`). The bit-field/union zero-fill trick from the earlier
  aggregate-initializer fix (see "Fix `init.c` `variables[]` bit-field
  aggregate init" above) still applies unchanged -- now with only one
  positional value per entry instead of two, if anything simpler to
  reason about.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31. Re-ran the same `gdb` spot-check as the
  earlier `variables[]` fix (break at `init_vars()`, print several
  entries) to confirm every remaining flag/name still matches the
  original table exactly after the field removal -- `personal-name`,
  `user-id`, `folder-collections`, and the `NULL` terminator all
  checked.
- **Size delta**: `bin/pine` `text` 278178 -> 276250 (-1928 bytes --
  on this ELF/gcc dev build, string literals live in `.rodata`, which
  `size` folds into `text`, not `data`, so this dev build doesn't
  directly show the a.out-format `data`-segment savings the real
  target will see), `data` 10160 -> 10032 (-128 bytes, the 13 removed
  `char *description` pointers from the struct array itself); `bss`
  unchanged. On the real 2.11BSD/a.out target, where string literals
  have no separate read-only section and land directly in `.data`,
  the expected savings are the full ~2.3KB of description text plus
  the 13 pointer fields -- close to the entire 2390-byte data+bss
  shortfall computed above, on its own. `bin/pico` untouched (doesn't
  touch pico source at all).

### Fix missing `osdep.h` dependency on `rfc822.o` in c-client's `makefile.bsd`

While chasing the same `_clock` undefined-symbol chain as the prior
entry, one reference persisted on real hardware even after `os_bsd.h`/
`os_bsd.c` were fixed and c-client fully rebuilt: `rfc822.c:1275`
calls `gethostid()` directly (building a MIME boundary/uniqueness
string), and `imap/ANSI/c-client/makefile.bsd`'s dependency line for
it was missing `osdep.h`:
```
rfc822.o: mail.h rfc822.h misc.h
```
compare with every sibling object file's rule, which all correctly
list it (`mail.o: mail.h misc.h osdep.h`, `bezerk.o: ... osdep.h`,
`dummy.o: ... osdep.h`, `misc.o: ... osdep.h`) -- a real gap in the
original UW makefile, not something introduced this session. Without
that dependency edge, `make` has no way to know `rfc822.o` needs
recompiling when `osdep.h` changes, so it kept using the stale object
(compiled back when `osdep.h` still carried the leftover `#define
gethostid clock`, see prior entry) across every subsequent rebuild,
even a full `make -f makefile.bsd` run, since `rfc822.c` itself never
changed.
- **What changed**: added `osdep.h` to `rfc822.o`'s dependency line.
- **Verified**: matches the exact symptom reported from the real
  hardware build -- `nm -A *.o | grep clock` (after rebuilding
  everything else) singled out `rfc822.o`/`send.o` as the only two
  objects still referencing `_clock`, and `rfc822.c` was confirmed as
  the one other (besides `send.c`, already covered by the prior
  entry) direct caller of `gethostid()` in the whole tree. `./build
  clean && ./build lnx`, exit 0, 0 warnings (this Makefile isn't used
  by the `lnx` build at all, so this is a syntax/sanity check only,
  not a regression check); full smoketest suite 31/31. No PDP-11
  cross-compiler available here to confirm the rebuilt `rfc822.o`
  actually resolves clean -- the user still needs one manual `rm -f
  rfc822.o` on the target to clear the already-stale object, since the
  dependency fix only prevents the problem recurring, not retroactively.
- **Size delta**: none -- Makefile-only change, no compiled output
  from this repo's own build affected.

### Drop `gethostbyname()`/DNS resolution -- pulls in the entire BSD resolver+socket library on a no-TCP/IP target

With every undefined symbol resolved, `pine-ovl`'s real 2.11BSD `ld`
run got far enough to do its base+overlay/data+bss size check, and
still failed both halves of it (`text 48768` base + largest overlay
`24576`, rounding to 49152+24576=73728 > the 65536-byte ceiling;
`data 47518 + bss 21178 = 68696` > the 57344-byte ceiling) -- and by a
lot more than pine+pico+c-client's own code could account for (the
named modules alone only summed to ~52644 bytes of data+bss and
~8142 bytes of base text). Getting a real load map (`ld -M`, per
`ld.txt`) for the failed link named the actual cause: `/lib/libc.a`
pulled in `gethnamadr.o`, `res_query.o`, `res_mkquery.o`,
`res_comp.o`, `res_send.o`, `res_init.o`, `socket.o`, `connect.o`,
`send.o`, `sendto.o`, `recv.o`, `htons.o`, `htonl.o`, and
`inet_addr.o` -- the entire BSD resolver and a chunk of the sockets
API, on a project whose whole premise (README.md/CLAUDE.md) is a
target with **no TCP/IP**.

- **What was found**: three call sites, across two files, call
  `gethostbyname()` (a DNS/`/etc/hosts` lookup) to enrich a plain
  `gethostname()` result into a fully-qualified domain name:
  `pine/osdep/domnames`'s `getdomainnames()` (shared fragment,
  assembled into both `os-lnx.c` and `os-bsd.c` -- feeds
  `ps->hostname`/`ps->localdomain`, used throughout address-matching/
  From-line/Message-ID logic), `pine/osdep/canonicl`'s
  `canonical_name()` (assembled the same way, but a repo-wide grep
  found *zero* callers anywhere -- pure dead code, unlike the other
  two), and `imap/ANSI/c-client/bezerk.c`'s `bezerk_isvalid()` (caches
  the result in `lhostn`, used for the mbox `From ` separator line and
  as the default host when parsing headers with no explicit domain).
  On a machine with no working TCP/IP stack, `gethostbyname()` can
  never resolve anything (2.11BSD's UUCP-only target has no resolver
  to query) -- every call site's existing `NULL`-fallback branch
  (falling back to the plain `gethostname()` string) is unconditionally
  what would execute in practice, so the DNS-lookup capability was
  already 100% dead *behavior*, while its mere presence in the source
  forced the entire resolver+socket library into every link.
- **What changed**: `getdomainnames()` and `bezerk_isvalid()` were
  simplified to use `gethostname()`'s result directly, dropping the
  `gethostbyname()` call and its `struct hostent`/`host_name`
  bookkeeping entirely (behaviorally identical to what those functions
  already did whenever DNS lookup failed, which is unconditionally true
  here). `canonical_name()` -- the zero-caller one -- was deleted
  outright: its fragment file (`pine/osdep/canonicl`) stays on disk
  (many other never-built platforms' `.ic` files in `pine/osdep/
  makefile`'s own separate, more complete dependency list still
  reference it, same "keep vestigial platform cruft, don't build it"
  precedent as the other `os_*.c` files), but the `include(canonicl)`
  line was removed from the two `.ic` files that actually get built
  (`os-lnx.ic`, `os-bsd.ic`), plus its prototype in `pine.h` and its
  one-line doc entry in the `header` fragment.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31 (exercises `bezerk_isvalid()`'s `lhostn`
  path indirectly via every folder-open/mbox-append/send test in the
  suite). No PDP-11 cross-compiler available here to confirm the
  resolver members actually drop out of the real link -- that's the
  user's next real-hardware `size`/`ld -M` cycle to confirm.
- **Size delta**: `bin/pine` `text` 276250 -> 275746 (-504 bytes),
  `data` 10032 -> 10024 (-8 bytes); `bss` unchanged. Modest on this
  ELF/gcc dev build since `-lc`'s resolver code was never linked in
  here to begin with (glibc's static linking story differs a lot from
  2.11BSD's a.out archives) -- the real win is entirely on the actual
  2.11BSD target, where the load map showed 13 additional `libc.a`
  members (`gethnamadr.o`/`res_*.o` x5/`socket.o`/`connect.o`/
  `send.o`/`sendto.o`/`recv.o`/`htons.o`/`htonl.o`/`inet_addr.o`) no
  longer need to be pulled in at all. `bin/pico` untouched (doesn't
  link c-client, and none of these three functions are pico source).

### Fix `folder.c:2144`'s "mixed pointer assignment" warning on `compare_names()`

Found while explaining a real 2.11BSD `cc` warning the user asked
about (not a build blocker -- a warning, not an error -- but worth
fixing at the root while looking at it). `compare_names()` is declared
and defined with `QSType *x, *y` parameters (`QSType` is this
codebase's generic qsort-callback pointer type, `int *` on most
platforms -- see `pico/os_unix.h`) and immediately casts them to
`char **` internally (`char *a = *(char **)x, *b = *(char **)y;`) to
get at the actual strings it compares. That generic-callback signature
only makes sense if the function is ever passed to `qsort()` as a
comparator; a repo-wide grep found it never is (unlike
`mailindx.c`'s `compare_subjects`/`compare_from`/etc. or
`pico/browse.c`'s `qsort()` call, which genuinely use `QSType` this
way) -- `compare_names()`'s only call site, `folder_insert()`'s manual
sorted-insertion loop, calls it directly with real `char **`
arguments (`&sortname`, `&fsortname`). Passing `char **` where the
prototype says `QSType *` (`int *`) is exactly what 2.11BSD's `cc`
flags as "mixed pointer assignment" -- a real type mismatch, though a
survivable one on most architectures since pointer representations are
usually compatible, which is presumably why this was only ever a
warning and not caught as a hard error. Likely a leftover from when
this function was a genuine `qsort()` comparator in full upstream
pine, before this project's cuts changed folder-list insertion to the
current manual sorted-insert loop.
- **What changed**: `compare_names()`'s prototype (`folder.c:128`) and
  definition (`folder.c:1388-1389`) now declare `char **x, **y`
  directly, matching both its actual body logic and its one real call
  site; the now-redundant `*(char **)x`/`*(char **)y` casts inside the
  function became plain `*x`/`*y`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31 (folder-list-touching tests exercise
  `folder_insert()`/`compare_names()` indirectly). No PDP-11
  cross-compiler available here to confirm the warning is gone on real
  hardware, but the type mismatch this warning describes is now
  genuinely eliminated, not just silenced.
- **Size delta**: none -- `bin/pine`/`bin/pico` byte-for-byte
  unchanged (275746/109103 bytes text). Pure type-correctness fix, no
  behavioral or size change; the compiled code was already doing the
  right thing via the explicit cast, this just makes the declared
  types honest.

### Drop floating point from `pico/browse.c`'s `prettysz()` -- the only `%f`/float use in the whole tree

Continuing the `pine-ovl` link-size investigation (see the
`gethostbyname()` entry above): with the resolver gone, a fresh
`size` on the real 2.11BSD link attempt showed base text down to
41984 (from 48768) but still just short of clearing a segment
boundary (`rnd8k(41984)` still rounds to 6 segments same as before;
needed to drop below 40960, i.e. ~1KB more, to gain a segment back for
the text/overlay budget), and data+bss still ~9970 bytes over the 56K
ceiling. The refreshed `ld -M` load map's `/lib/libc.a` section still
carried a cluster of floating-point-conversion members: `atof.o`,
`ldexp.o`, `fltpr.o`, `doprnt.o`'s float path, `gcvt.o`, `ecvt.o`,
`modf.o`. A repo-wide grep for `atof`/`strtod` calls and for `%f`/`%e`/
`%g` format specifiers found exactly one hit, in two `sprintf()` calls:
`pico/browse.c`'s `prettysz()` (the file browser's human-readable
byte-count formatter), using `"%1.1f KB"`/`"%1.1f MB"` for sizes in
the 1-10KB and 1-10MB ranges. Nothing else in `pine/`, `pico/`, or
`imap/ANSI/c-client/` touches floating point at all -- this one
cosmetic display function is the sole reason the entire float-format
machinery gets linked into the program.
- **What changed**: rewrote `prettysz()` to always show a rounded
  whole-number KB/MB (no decimal place), using plain integer division
  with a rounding offset (`(l + 500) / 1000` for KB, `(l + 500000) /
  1000000` for MB) instead of `(float)l/1000` + `"%1.1f"`. (An
  intermediate version preserved the original one-decimal-place
  display via `(l+50)/100`-style tenths math with zero float
  involvement -- also fully correct and verified -- but the user
  preferred the simpler whole-number display once floating point was
  already off the table either way.)
- **Verified**: a standalone comparison harness checked the
  one-decimal-place intermediate version against the original
  `(float)l/1000` + `%1.1f` implementation across 31 values spanning
  every branch and boundary; the only mismatches were at exact `.x50`
  inputs (1050, 9950, 9950000), where the *original* float version's
  output is the less-correct one -- `0.05` isn't exactly representable
  in IEEE 754 binary, so `%1.1f` on values like `1.05` can round down
  to `"1.0"` instead of the mathematically-correct `"1.1"`, while
  plain-integer decimal rounding doesn't have that artifact. The final
  whole-number version was spot-checked separately for sane rounding
  across representative values. `./build clean && ./build lnx`, exit
  0, 0 warnings; full smoketest suite 31/31. No PDP-11 cross-compiler
  available here to confirm `doprnt`'s float path actually drops out
  of the real link -- that's the next real-hardware `ld -M`/`size`
  cycle to confirm.
- **Size delta**: `bin/pico` `text` 109103 -> 108932 (-171 bytes,
  even on this ELF/gcc dev build where the float-format machinery was
  never a large chunk of glibc's already-shared `libc.so` to begin
  with). `bin/pine` `text` unaffected (exactly 275746 before and
  after) -- `browse.c` is part of `libpico.a`, which `bin/pine` does
  link, but `prettysz()`/its file-browser callers are pico-standalone
  UI, unreachable from any code path `bin/pine` actually uses, so
  `browse.o` (or the relevant part of it) was never pulled into
  `bin/pine`'s link to begin with. The real win is on the 2.11BSD
  target, where the load map showed the whole `atof.o`/`ldexp.o`/
  `fltpr.o`/`gcvt.o`/`ecvt.o`/`modf.o` cluster (plus whatever of
  `doprnt.o`'s own size is float-format-specific) should no longer
  need to be pulled into *either* binary's link at all.

### Shrink `mailcmd.c`'s string literal footprint (`pine-ovl` data+bss)

Continuing the `pine-ovl` data+bss investigation: with the `description`
field gone and `s`/`SLEN` explained (see the two entries above/below),
the next-biggest lever left is string literal volume itself -- 2.11BSD's
`a.out` format has no separate read-only/`.rodata` section, so every
string literal lives in writable `.data`, unlike this repo's ELF/gcc
`lnx` dev build where they land in a read-only section merged into
`text` reporting (and get automatically deduplicated by gcc when
identical, which an old single-pass `cc` doesn't do). `mailcmd.c` alone
measured at ~4254 bytes of string literals earlier in this
investigation, the single biggest contributor of any one file. Per the
user's explicit direction ("I'd rather a cryptic error message than a
binary that doesn't run"), went through it and cut two ways:

- **Deduplication (zero behavior change)**: many `\007`-prefixed status
  messages were byte-for-byte identical at multiple call sites --
  `"\007No message in folder"` (7 call sites), `"\\DELETED"` (4, the
  IMAP flag name passed to `mail_setflag()`/`mail_clearflag()`),
  `"\007Error reading folder name"` (2), `"\007Error expanding file
  name: \"%s\" unknown user"` (2), `"Closing folder \"%s\". Keeping all
  %s message%s"` (2, both branches of an if/else that print the exact
  same message either way), `"\007%s in file name"` (2), and a cluster
  of six near-identical "cancelled" messages (`"\007Attachment
  cancelled"`, `"\007Save message cancelled"`, `"\007Export message
  cancelled"`, `"\007Open Folder cancelled"`/`"\007Open folder
  cancelled"` (differed only in capitalization), `"\007Save attachment
  cancelled"`/`"\007Save of attachment cancelled"`, `"\007Search
  cancelled"`, and `"\007No folder named; save message cancelled"` --
  12 call sites total) consolidated into one shared `"\007Cancelled"`,
  per the user's suggestion to combine same-meaning cancel messages
  rather than just shortening each independently. Each group became a
  single file-scope `static char *` holding the one copy of the text,
  referenced by pointer at every call site instead of repeating the
  literal -- C doesn't merge identical string constants across
  translation units on its own (and demonstrably doesn't merge them
  even *within* one file on an old single-pass compiler), so this is a
  real storage reduction, not a stylistic one, and changes *zero*
  user-visible text.
- **Shortening (real wording changes, deliberately terser)**: the
  longest remaining unique messages were cut down, e.g. `"\007Command
  \"%s\" not defined for this screen. Use %s for help"` ->
  `"\007\"%s\" undefined here (%s for help)"`; `"\007Problem creating
  space for message text."` -> `"\007Out of memory"`; `"\007Invalid
  number entered. Use only digits 0-9"` -> `"\007Invalid number"`;
  `"\007Message number (%s) must be greater than 0"` -> `"\007%s: must
  be > 0"`; `"\007Message number must be no more than %s, the number of
  messages"` -> `"\007Max is %s"`; `"\007No such attachment number
  \"%s\". Must be in displayed list"` -> `"\007No such attachment
  \"%s\""`; and the postponed/interrupted-mail pair (`"\007Can't open
  \"%s\". To continue postponed mail, give compose command"` /
  `"\007To continue interrupted mail, give compose command. (Can't open
  %s.)"`) collapsed into one shared `"\007Can't open \"%s\", use
  Compose"` -- the folder name argument (`POSTPONED_MAIL`/
  `INTERRUPTED_MAIL`, `pine/os.h`) already distinguishes which case it
  is, so this was both a shortening and a dedup. Every format-string
  argument count/order was double-checked against its call site during
  each edit (the classic risk with this kind of change). Left prompts
  the user has to read and act on alone (e.g. `"File (in home
  directory) to save message text in"`) untouched -- the user's
  "cryptic is fine" direction was specifically about error/status
  feedback, not input prompts, and one of those exact prompt strings is
  also asserted on verbatim by `test_export_message_from_overflow.py`.
- **Verified**: checked every touched message against
  `test/cases/*.py`'s `session.expect(...)` calls *before* editing --
  found and preserved the one real dependency (`"Error opening
  destination"`, `test_save_attachment_overflow.py`, left completely
  untouched) and confirmed none of the consolidated/shortened messages
  are asserted on anywhere else. `./build clean && ./build lnx`, exit
  0, 0 warnings; full smoketest suite 31/31. No PDP-11 cross-compiler
  available here to measure the real byte savings directly (and this
  dev build's own size numbers are a poor proxy for this specific
  change -- see below) -- that's the next real-hardware `size` cycle to
  confirm.
- **Size delta**: `bin/pine` `text` 275746 -> 275386 (-360 bytes) on
  this ELF/gcc dev build -- a real but *understated* proxy, since gcc
  already auto-merges identical string constants (the 7-way and 12-way
  duplicates were mostly free there already) and puts survivors in a
  read-only section reported under `text`, not `data`. Hand-totaling
  the actual source-text bytes removed/consolidated (old literal sizes
  minus the one shared copy plus one 2-byte pointer variable per
  group, plus the pure-shortening deltas) estimates roughly 840 bytes
  of real savings on the 2.11BSD target's `.data` segment, where none
  of this deduplication happens for free and every string literal
  costs full writable-segment space -- an estimate, not a measurement;
  real confirmation needs the next real-hardware `size` run. `bin/pico`
  untouched (`mailcmd.c` is pine-only).

  Confirmed on real hardware after this fix: `bin/pine` `data` 46746 ->
  45872 (-874 bytes, within 34 bytes of the ~840-byte hand estimate
  above); `bss`/`text`/overlays unchanged, as expected (string cuts
  only affect `.data`, not code size or overlay placement).

### Shrink `mailview.c`'s string literal footprint (`pine-ovl` data+bss)

Same investigation, next file down. Two kinds of win, one of them
bigger than a typical dedup:

- **Collapsed a repeated 4-way ternary into a helper function**: five
  separate call sites (`scrolltool()`'s page-up/page-down/line-up/
  line-down handlers) each spelled out the identical `style ==
  MessageText ? "Already at start of message" : style == HelpText ?
  "Already at start of help" : style == ComposerHelpText ? "Already at
  start of help" : "Already at start of text"` chain (or the "end of"
  equivalent) -- notably with the `"...of help"` string written out
  *twice* per chain, since `HelpText` and `ComposerHelpText` share the
  same word. That's ~20 duplicated string literals across 5 call
  sites. Added a small `style_word(TextType)` helper returning just
  `"message"`/`"help"`/`"text"`, and replaced each site with
  `q_status_message1(0, 0, 1, "Already at start of %s",
  style_word(style))` (or the "end" equivalent) -- two format strings
  plus one shared helper instead of ~20 literals *and* the repeated
  ternary logic itself (a real code-size win too, not just data,
  unlike the mailcmd.c dedup pass which only touched already-shared
  function-call arguments).
- **Deduplicated + shortened remaining candidates**: `"Error writing
  message: %s"` (2 call sites), `"us-ascii"`/`"richtext"` (MIME subtype
  comparisons, 2 sites each) -- all safe to share via a `static char *`
  since they're function-call arguments (confirmed this doesn't extend
  to the file's `KEYMENU_S` tables, e.g. `"PrevPage"`/`"NextPage"`/
  `"OTHER CMDS"` appearing 3-4 times each: those are static aggregate
  initializers, which C requires to be constant expressions -- a
  `static char *` variable's value isn't one, so the mailcmd.c-style
  trick doesn't apply there and was correctly left alone). Also
  shortened the file's own copy of the "not defined for this screen"
  message (mailcmd.c already had the identical pattern, fixed
  separately since they're different translation units) to match
  mailcmd.c's shorter wording, and condensed the three-line "text is in
  a different character set than your display" notice and the
  attachment placeholder text (`"Not Shown. Use the \"V\" command to
  view or save this part"` etc.) to shorter equivalents.
- **Verified**: checked every touched/candidate message against
  `test/cases/*.py` first, found no dependencies (the "Already at
  first/last folder" tests that came up in a keyword search are a
  different message, from `folder.c`, unrelated). `./build clean &&
  ./build lnx`, exit 0, 0 warnings; full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 275386 -> 274882 (-504 bytes) on
  the `lnx` dev build -- bigger than the pure-dedup mailcmd.c pass
  relative to its size, consistent with this file's win including
  real duplicated *code* (the ternary chains), not just data, which
  shows up in `text` on any platform including this ELF/gcc one, not
  only as an under-counted proxy the way string-only savings did.
  `data`/`bss` unchanged on this dev build (same gcc-already-dedupes-
  strings caveat as before -- the real `.data` savings need the next
  real-hardware measurement). `bin/pico` untouched (`mailview.c` is
  pine-only).

### Remove dead `find_rightmost_bit()` from `c-client/misc.c`

Pivoting the `pine-ovl` text/overlay investigation: with data+bss
steadily shrinking via string cuts but the base-text budget stuck
(short by a fixed 8192 bytes across several rounds, since string
edits don't move code size), audited the 11 specific files mkovmake
puts in the resident base (`pine/args.c`, `pine/help.c`, `pine/log.c`,
`pico/bind.c`, `pico/compose_stub.c`, `pico/fileio.c`, `pico/region.c`,
`pico/window.c`, `c-client/dummy.c`, `c-client/misc.c`,
`c-client/osdep.c`) for actual code (not string) bloat -- only code in
these specific files affects the base-text number at all.
`c-client/misc.c` was the biggest real-hardware contributor (1316
bytes). `find_rightmost_bit()` (aliased to `frtbit` for one ancient
compiler's 6-character symbol limit) turned out to have zero callers
anywhere in the tree -- confirmed the two functions it could plausibly
be confused with (`search()`/`ssrc()`, used by `rfc822.c`'s MIME
boundary detection, and `pmatch()`, used by `dummy.c`'s wildcard
mailbox matching) are both genuinely reachable, but `find_rightmost_bit`
itself is not, including from `phile.c` (the only other file that
might have called it) since `phile.c` isn't even a member of this
project's `c-client.a` (see its `makefile.bsd`).
- **What changed**: deleted `find_rightmost_bit()`'s definition (35
  lines, a non-trivial bit-scan loop) from `misc.c`, its prototype from
  `misc.h`, and the now-pointless `#ifdef __COMPILER_KCC__` macro alias
  above it.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274882 -> 274722 (-160 bytes).
  `bin/pico` untouched (doesn't link c-client).

### Stop linking pico's editor internals into `pine-ovl` at all

The single biggest finding of this whole investigation. Continuing the
base-file audit (previous entry): `pico/bind.c` turned out to hold
pico's ~60-line interactive help text (`helptext[]`, real hardware
`data` 2984 bytes) plus real, still-used editor code
(`wscrollw()`/`arraylen()`, called from `browse.c`/`search.c`/`file.c`
for their own help screens) -- not dead by itself. But asking *why*
`pine-ovl` needs any of `bind.c` at all, given `send.c`'s
`run_composer_subprocess()` already `fork()`s and `execl()`s a
*separate* `pico -P <handoff> <result>` child process for composing
(see `pico/compose_stub.c`'s own header comment, and the
`TODO-pdp11.md`/`-done.md` history of that redesign) rather than
calling into pico's editor in-process, led somewhere much bigger:
confirmed via `nm` that **zero** symbols defined anywhere across all
18 of pico's editor object files (`basic.o` through `word.o`) are
referenced by any undefined symbol in any of pine's own 24 object
files, and confirmed by directly relinking `bin/pine` on the `lnx` dev
build with `../pico/libpico.a` dropped from the link line entirely --
zero link errors, and the resulting binary is byte-for-byte identical
(`text`/`data`/`bss` all exactly the same) to the version linked with
it. Sanity-checked the relinked binary through a smoketest-style
session too (opens a folder, views a message, returns to Main Menu)
to make sure "links fine" wasn't hiding a real runtime gap.

The reason this was ever a problem despite selective archive linking
already pulling zero members "for free": `pine-ovl`'s overlay build
needs c-client.a's members *loose* (not archived) so `mkovmake` can
place them into overlays (see the earlier `pine-ovl` entries in this
file) -- and an earlier version of that same fix force-included
*pico's* members loose too, for the same reason. Loose object files on
a link command line are always linked in, unlike an archived library's
selective member-pulling, so that inadvertently forced ~59396 bytes of
text and ~14022 bytes of data+bss (pico's entire editor, per the
per-file `size` breakdown from `size.txt`/`size-lib.txt` earlier in
this investigation) into every `pine-ovl` link, none of which pine's
own code has needed since the fork/exec redesign.
- **What changed**: `pine/makefile.bsd`: removed the `PICOOFILES`
  variable and every reference to it from the `pine-ovl` target
  entirely (no replacement, loose or archived -- pico isn't part of
  this link at all any more). Also dropped `../pico/libpico.a` from
  `$(LOCLIBES)` (the plain, non-overlay `pine:` target) for clarity --
  it was already contributing exactly 0 bytes there too (selective
  archive linking), so this is a documentation fix, not a size fix,
  for that specific target.
- **Verified**: the `nm`/relink test described above (byte-identical
  binary, zero link errors, smoketest-style sanity pass) is the real
  verification here, done *before* touching the makefile, precisely
  because this is a bigger and more consequential change than a normal
  cut -- it touches the core link configuration of the build the user
  is actively testing on real hardware. After the makefile edit:
  `./build clean && ./build lnx`, exit 0, 0 warnings (the `lnx` build
  uses `makefile.lnx`, untouched by this fix, and still harmlessly
  links `libpico.a` there the same zero-cost way); full smoketest
  suite 31/31. `make -f makefile.bsd -n pine-ovl` and `-n pine` dry-run
  checked to confirm the generated `mkovmake`/`cc` command lines no
  longer mention any pico object file. No PDP-11 cross-compiler
  available here to confirm the real 2.11BSD `mkovmake`/`ld` run
  actually clears both budgets now -- that's the next real-hardware
  cycle, and given the scale of code removed (more than either
  remaining budget shortfall on its own), it's the most likely single
  fix to resolve both the text/overlay and data+bss overages at once.
- **Size delta**: not measurable on `bin/pine`'s own `lnx` build
  (`makefile.lnx` is untouched, and the `lnx` `pine-ovl`-equivalent
  doesn't exist as a separate target there) -- this fix is specific to
  `pine/makefile.bsd`'s real-hardware overlay link. Expected real
  savings: ~59396 bytes of text and ~14022 bytes of data+bss removed
  from every future `pine-ovl` link, based on the per-file sizes
  already measured earlier in this investigation for exactly these 17
  pico object files. `bin/pico` itself is completely unaffected (it's
  a different binary, built by `pico/makefile.bsd`, which still needs
  all of its own code).

### Fix `bezerk_open()` wasting 8KB on every open, crashing on a new user's empty INBOX

With `pine-ovl` finally linking clean (previous entry) and running on
real hardware for the first time, it booted to the Main Menu but
crashed opening INBOX: `Bug in Pine detected: "Out of free storage".
Exiting pine.` The account's mailbox file didn't exist yet
(`/usr/spool/mail/<user>` was absent -- a brand new user, the most
ordinary case there is), so this wasn't a large-mailbox memory
problem. No core dump was produced (this project's own signal
handling likely intercepts `abort()` before the default core-dump
action), so root-caused it by temporarily instrumenting `fs_get()`
(`os_bsd.c`) to report the failed allocation's size in the crash
message -- see below, since that turned out worth keeping permanently.
It reported `Out of free storage (8193 bytes)`, which is exactly
`CHUNK + 1` (`bezerk.h`'s `#define CHUNK 8192`) -- `bezerk_open()`
allocates this full read-buffer size *unconditionally*, before it even
knows whether the mailbox file exists.

A real ulimit-hunting detour happened first (`limit datasize`/`limit
-a` all reported "0 kbytes" for every resource) that turned out to be
a red herring: raising `datasize` to `unlimited` made the crash happen
*earlier* (at a smaller 2048-byte allocation, `pine`'s own
`tmp_20k_buf`'s initial size), the opposite of what raising a limit
should do -- strong evidence this ancient `tcsh`/kernel's handling of
`limit datasize unlimited` is itself buggy (plausibly a 16-bit
encoding issue turning "unlimited" into something small), not
something to route around by tuning limits. Left alone; the real fix
is not wasting memory in the first place.

`bezerk_parse()`'s buffer growth is already fully dynamic
(`fs_resize()` in its read loop, sized to exactly what's needed for
real message data), and every *other* use of `LOCAL->buf` in the file
is a short `sprintf()` (error messages, IMAP-ish date string
construction) that fits comfortably in far less than `CHUNK` bytes --
confirmed via a full `grep` of every `LOCAL->buf` reference in
`bezerk.c`. The unconditional `CHUNK`-sized allocation at open time
was pure headroom for a large-mailbox read that hadn't happened yet,
and on a PDP-11 where data+bss+heap+stack all share one 64KB address
space (separate I&D, per `ld.txt`), it was routinely enough to starve
the empty-mailbox case -- the *common* case for any new install --
of room to open at all.
- **What changed**: `bezerk_open()`'s initial `LOCAL->buf` allocation
  shrunk from `CHUNK` (8192) to `MAILTMPLEN` (1024, `mail.h`) bytes.
  The one other unconditional `CHUNK`-sized allocation in the file
  (`bezerk_parse()`, shrinking the buffer back down *after* a large
  read) is unaffected by this and doesn't need the same fix -- it's
  gated on `LOCAL->buflen > CHUNK`, which is never true unless a
  large-mailbox read already grew it past that point.

  Also kept (made permanent) the `fs_get()` diagnostic added to
  root-cause this: `os_bsd.c`'s "Out of free storage" `fatal()` call
  now reports the failed request size. Cheap (one `sprintf()`, failure
  path only) and valuable on a target this memory-constrained for
  diagnosing any future allocator exhaustion at a glance.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings
  (`bezerk.c` is shared between `lnx` and `bsd` builds, so this is a
  real regression check, not just inference); full smoketest suite
  31/31. No PDP-11 cross-compiler available here to confirm INBOX
  actually opens clean now -- that's the user's next real-hardware
  run to confirm, and the `fs_get()` diagnostic will immediately show
  whether some *other* allocation is now the next bottleneck if not.
- **Size delta**: `bin/pine` `text`/`data`/`bss` unchanged on the `lnx`
  dev build (274722/10120/19208) -- this changes a runtime allocation
  size argument, not code or static data size. `bin/pico` untouched
  (doesn't link c-client).

### Fix `read_with_timeout()`'s `select()` aliasing readfds/exceptfds -- keyboard input never worked

With `bezerk_open()` fixed (previous entry), `pine-ovl` ran far enough
on real hardware to boot to the Main Menu and open INBOX -- but then
didn't respond to *any* keyboard input at all, not even after pressing
Enter (ruling out "stuck in cooked/line-buffered mode waiting for a
newline", which would still process the first buffered key once Enter
flushed the line). `pine/ttyin.c`'s `read_with_timeout()` had:
```c
res = select(STDIN_FD+1, &readfds, 0, &readfds, &tmo);
```
passing the *same* `&readfds` pointer as both the read-set (2nd arg)
and the except-set (4th arg). `select()` writes its result back into
each set it's given, to say which descriptors actually became ready;
aliasing two different output roles into the same memory means
whichever set gets written last can silently clobber the other. Since
the code only ever checks `select()`'s numeric return value (`res ==
0` / `res < 0`) and never re-examines the bitmask contents, a
clobbered "fd 0 is readable" result could make `select()` report
nothing ready even right after a real keystroke -- so the caller kept
taking the `res == 0` (timeout) branch and returning
`NO_OP_COMMAND`/`NO_OP_IDLE` forever, never reaching the actual
`read()` that would have consumed the pending byte. This exact code
path is guarded by `#ifndef USE_POLL`, and `os-lnx.h` (unlike
`os-bsd.h`) defines `USE_POLL` -- meaning this `select()` branch has
never been compiled, let alone exercised, by any `lnx` dev build this
whole project has ever run. (Separately, the user also found `Ctrl-\`
triggers a real SIGQUIT -- unlike `Ctrl-C`'s `t_intrc`, `Raw()` never
sets `_raw_tchars.t_quitc = -1` to disable it the same way -- landing
in whatever `signals.c` does for SIGQUIT and producing a
space-printing redraw loop; left uninvestigated for now since it's a
separate, lower-priority issue from the "no keyboard input at all"
blocker.)
- **What changed**: `ttyin.c`'s `read_with_timeout()`: changed the
  `select()` call's except-set argument from `&readfds` (aliased) to
  `0` (`NULL`) -- the code never actually needed exception
  notifications, only read-readiness, so there's no reason to pass
  anything there at all.
- **Verified**: this specific branch is unreachable on the `lnx` dev
  build (`USE_POLL` excludes it via `#ifdef`/`#else`), so `./build
  clean && ./build lnx`, exit 0, 0 warnings and the full smoketest
  suite 31/31 confirm no regression to the `USE_POLL` path or the rest
  of the file, not this fix's actual behavior. Additionally compiled
  the corrected `select()` call in isolation (a minimal standalone
  file reproducing the exact types/arguments, `gcc -Wall`) to confirm
  it's syntactically sound C, since `<sgtty.h>` isn't available on
  this dev machine to compile the real BSD branch at all. The real
  verification is the user's next real-hardware run.
- **Size delta**: none measurable here (`lnx` never compiles this
  branch); expected to be a no-op or negligible on the real 2.11BSD
  target too (same argument count, one pointer expression replaced by
  a literal `0`).

### Fix `readfds` type -- a 16-bit `int` standing in for a 32-bit `fd_set`

The `exceptfds` aliasing fix above didn't resolve the keyboard-input
problem: real hardware still didn't respond to any keystroke. Traced
it with temporary `syslog(LOG_ALERT, ...)` calls added to
`read_char()`/`read_with_timeout()` (this file's own signal handling
prevents `abort()`-style core dumps, so `syslog` tracing stood in for
a debugger backtrace the same way it did for the `fs_get()` size
diagnostic in an earlier entry). The trace showed `read_char`/
`read_with_timeout` being entered correctly with sane `time_out`
values, and a normal `select res=0` (clean timeout, no error) on the
first 1-second-timeout call -- then a second call with `time_out=150`
that never logged a `select res=...` at all, i.e. `select()` was
genuinely sitting there rather than returning quickly once a key was
pressed (confirmed by the user declining to literally wait out 150
seconds -- correctly reasoning that a working `select()` should return
long before its timeout elapses once data arrives, not depend on
lasting the whole window).

That pointed past the earlier aliasing bug to something more
fundamental, confirmed by checking the target's actual header:
```
$ sed -n '54,56p' /usr/include/sys/select.h
typedef struct fd_set {
        fd_mask fds_bits[1];
} fd_set;
$ grep fd_mask /usr/include/sys/select.h
typedef long    fd_mask;
```
`fd_set` here is a struct wrapping a 32-bit `long`. But
`read_with_timeout()` declared `readfds` as a plain `int` (16 bits on
this target) and hand-set it as a bitmask (`readfds = 1 << STDIN_FD`,
the classic pre-`fd_set` BSD calling convention) before passing
`&readfds` to `select()` as if it were a real `fd_set *`. `select()`
reading/writing a 32-bit structure through a pointer to a 16-bit `int`
corrupts whatever stack memory follows `readfds`, and the "bitmask" it
reads back for input is half uninitialized garbage -- a real stack-
corruption bug, not just a semantic one, and enough on its own to
explain `select()` never correctly reporting fd 0 ready regardless of
the `exceptfds` aliasing (both needed fixing; the aliasing fix was
necessary but not sufficient).
- **What changed**: `read_with_timeout()`: `readfds` is now declared
  as a real `fd_set` (not `int`), initialized with the standard
  `FD_ZERO(&readfds)` / `FD_SET(STDIN_FD, &readfds)` macros instead of
  a hand-rolled bit shift, matching whatever the target's actual
  `fd_set`/`fd_mask` representation is instead of assuming it's
  `int`-sized.
- **Verified**: `FD_ZERO`/`FD_SET` are standard companions to `select()`
  on any BSD-derived system (this target's own `select.h` confirms
  their layout), so this is the portable, correct way to set up a
  descriptor set regardless of `fd_mask`'s actual size -- it can never
  have this class of bug again on any platform. Compiled a standalone
  test reproducing the target's exact `fd_set`/`fd_mask` layout (`long
  fds_bits[1]`) with `gcc -Wall` to confirm the `FD_ZERO`/`FD_SET`
  usage is correct C against that specific shape, since this branch
  is still unreachable on the `lnx` dev build (`USE_POLL` excludes it)
  and glibc's own `fd_set` representation differs from the target's
  anyway. `./build clean && ./build lnx`, exit 0, 0 warnings; full
  smoketest suite 31/31.

  Confirmed on real hardware: keyboard input works. Removed the
  temporary `syslog()` trace calls (and the `<syslog.h>` include added
  for them) from `read_char()`/`read_with_timeout()` afterward --
  unlike the `fs_get()` size-reporting diagnostic kept permanently in
  an earlier entry, these fired on every single keystroke and had no
  ongoing diagnostic value once the bug was found.
- **Size delta**: `bin/pine` `text`/`data`/`bss` back to
  274722/10120/19208 on the `lnx` dev build after removing the trace
  calls (matching the pre-instrumentation baseline exactly, as
  expected -- the `select()`-based branch these traces lived in was
  never compiled there in the first place). Real 2.11BSD target size
  impact not separately measured (this fix's actual value is
  behavioral -- keyboard input finally works -- not a size change).

### Shrink `send.c`'s string literal footprint, plus two cross-file shares

Continuing the string-cutting pass now that pine boots and runs on
real hardware (`mailcmd.c`/`mailview.c` covered in earlier entries):
`send.c` measured at ~2496 bytes across 141 string literals, the next
biggest file. Same two-part treatment as before:

- **Deduplication**: `"charset"` (4 sites), `"mailer-daemon"` (3),
  `"SUBJECT"`/`"BOUNDARY"` (2 each, all `write_field()`/`read_field()`/
  `cpystr()`/`strucmp()`/`strcmp()` arguments, confirmed none are in a
  static aggregate initializer this time), `"\015\012"` (the RFC822
  CRLF literal, 8 sites -- distinct from this codebase's `NEWLINE`
  macro, which is bare `"\n"`, so not interchangeable with it),
  `"\007Can't create composer temp file"` (2), `"\007Fcc of message
  rejected"` (2, kept the word "rejected" verbatim --
  `test_fcc_tilde_overflow.py`/`test_fcc_netmbx_overflow.py` assert on
  it), and `"Folder \"%.40s\" doesn't exist.  Create"` (2, shortened to
  `"\"%.40s\" doesn't exist. Create"` -- kept "doesn't exist" verbatim,
  same two tests assert on it) all consolidated into shared statics.
  `"\007Problem creating space for message text."` (2 sites) and a
  third near-duplicate `"\007Problem creating space for Fcc."` (1 site,
  different tail wording) were unified to the same shortened `"\007Out
  of memory"` wording already used in `mailcmd.c`, both deduping
  within the file and enabling the cross-file share below. The two
  `sprintf(tmp_20k_buf, fcc_doesnt_exist_fmt, fcc)` call sites needed
  the same `#ifdef __GNUC__` `-Wformat-nonliteral` pragma guard already
  established elsewhere in this codebase (`d76a936`), since the format
  string is a fixed constant but not a literal at the call site.
- **Cross-file sharing**: checked `mailcmd.c`/`mailview.c` (the two
  files already gone through this pass) for exact matches against
  `send.c`'s new shared strings, and found two: `"\007Out of memory"`
  (now also used once in `mailcmd.c`, from an earlier pass shortening
  a different original message to the same wording) and `"charset"`
  (also used twice more in `mailview.c`, as plain `strucmp()`
  arguments). A `static char *` can't be shared across translation
  units, so made `send.c` the single owner of each (dropped `static`,
  giving them external linkage) and added an `extern char *...;`
  declaration at each other call site instead of a second copy of the
  string.
- **Verified**: checked every touched/candidate message against
  `test/cases/*.py` first (same process as the `mailcmd.c`/`mailview.c`
  entries); preserved the two real dependencies found ("rejected",
  "doesn't exist"). `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274722 -> 274762 (+40 bytes) on
  this `lnx` dev build -- a net *increase* despite removing far more
  string bytes than that, because (as established in the `mailcmd.c`
  entry) gcc already auto-merges identical string constants for free
  here, so most of the within-file dedup work was already "free" on
  this platform; the two new `extern`-shared globals also cost a
  little visible symbol-table/relocation overhead here that a purely
  `static` variable wouldn't. The real win, as with the earlier string
  passes, is on the 2.11BSD target's `.data` segment, where none of
  gcc's deduplication happens and every one of the ~15 consolidated
  call sites plus the two cross-file shares was previously a full
  separate copy. `bin/pico` untouched (`send.c` is pine-only).

### Add an explicit `.c.o:` rule to `pine/makefile.bsd` -- default suffix rule mis-locates cross-directory output

Noticed while reviewing real hardware build output: a compile like
`cc -g -DDEBUG -c ../c-client/mail.c` (no `-o`) run from `pine/`
leaves the resulting object as bare `mail.o` in `pine/` itself, not at
`../c-client/mail.o` -- `cc -c path/to/file.c` conventionally strips
the source's directory prefix from its default output name. This
makefile had no explicit `.c.o:` rule at all, relying entirely on
`make`'s builtin default suffix rule (`$(CC) $(CFLAGS) -c $<`, no
`-o`), which is exactly what produces this. It matters specifically
for `pine-ovl`'s `$(CCLIENTOFILES)` prerequisites (e.g.
`../c-client/mail.o`), which do have a directory prefix -- if `make`
ever decides one of them looks stale (e.g. from timestamp skew after
a file transfer to the target, the same class of issue that bit the
`os_bsd.h`/`osdep.h` hardlink case in an earlier entry) and falls back
to the default suffix rule to rebuild it, the result lands in the
wrong place entirely.
- **What changed**: added an explicit `.c.o:` rule
  (`$(CC) $(CFLAGS) -c -o $@ $<`) to `pine/makefile.bsd`. `$@`/`$<`
  are original Make substitutions (target/first-prerequisite), not a
  GNU-ism -- fully supported by 2.11BSD's `make`. This is a no-op for
  same-directory compiles (pine's own `$(obj)`, which already had no
  prefix to strip) and fixes it for prefixed ones; an explicit rule
  always takes precedence over a builtin default in any standard
  `make`.
- **Verified**: `make -f makefile.bsd -n` dry-run (on this dev
  machine's GNU make, which also uses the explicit rule over its own
  builtin once one is defined, confirming the rule text itself takes
  effect) for both cases -- same-directory (`args.o`) produces the
  identical command as before; the cross-directory case
  (`../c-client/mail.o`) now correctly compiles to `-o
  ../c-client/mail.o ../c-client/mail.c` instead of mis-locating the
  output. This makefile isn't used by the `lnx` build at all, so no
  `./build lnx`/smoketest regression check applies here.
- **Size delta**: none -- Makefile-only change, no compiled output
  from this repo's own build affected.

### Shrink `folder.c`'s string literal footprint, plus a triple cross-file share

Continuing the string-cutting pass: `folder.c` measured at ~1992 bytes
across 109 string literals, next after `mailcmd.c`/`mailview.c`/
`send.c`.

- **Deduplication**: `"[Select]"` and `"Expand"` (2 sites each) --
  checked first that `"[Select]"`'s two sites are both runtime
  `folder_keys[SELECT_KEY].label = ...` assignments (not a static
  aggregate initializer), same as the `bezerkproto.open = T;`
  precedent, so sharing via a variable is safe; this file's `KEYMENU_S`
  table entries (`"OTHER CMDS"`, `"WhereIs"`, etc., also repeated
  2-3x) are aggregate initializers and were correctly left alone, same
  as the `mailview.c` entry's finding. `"\007Bad context, %s : %s"`
  (2 sites, exact format-string duplicate) shared the same way.
  Consolidated three near-duplicate "collection is empty" messages
  (`"\007Empty folder collection.  Nothing to select!"`,
  `"...  No folder to delete!"`, `"...  No folders to search!"`) into
  one shared `"\007No folders to %s"` format string with a `"select"`/
  `"delete"`/`"search"` argument, the same pattern as the
  `mailview.c` entry's "Already at start/end of %s" consolidation.
- **Shortening**: the file's own copy of the "not defined for this
  screen" message (already fixed the same way in `mailcmd.c`/
  `mailview.c`, a different translation unit each time) and
  `"\007Can't rename folder nicknames at this time!"` ->
  `"\007Can't rename nicknames"`. No test dependency found for either.
- **Cross-file sharing**: checked `folder.c`'s new candidate strings
  against `mailcmd.c`/`mailview.c`/`send.c` and found a genuine triple
  duplicate: `"\007Word not found"`, byte-for-byte identical in all
  three of `folder.c`, `mailview.c`, and `mailcmd.c` (all plain
  `q_status_message()` arguments). Made `mailcmd.c` the single owner
  (dropped `static`, external linkage) and added an `extern char
  *word_not_found_msg;` declaration in the other two instead of a
  second and third copy. (Also spotted `"To Fldrs"` matching between
  `folder.c` and `mailcmd.c` in an earlier grep -- turned out to be a
  false positive, a source comment in `folder.c` mentioning the
  feature by name, not an actual second string literal to share.)
- **Verified**: checked every touched/candidate message against
  `test/cases/*.py` first; none of this file's changes touch a tested
  string. `./build clean && ./build lnx`, exit 0, 0 warnings; full
  smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274762 -> 274650 (-112 bytes) on
  the `lnx` dev build (across this entry's two commits: the
  within-file dedup/shortening pass and the `word_not_found_msg`
  cross-file share) -- the real `.data` win on the 2.11BSD target is
  larger, per the same gcc-already-dedupes-strings caveat as every
  string-cutting entry in this file. `bin/pico` untouched (`folder.c`
  is pine-only).

### Revert the three cross-file `extern` string shares

The next real-hardware `pine-ovl` relink after the `word_not_found_msg`
share (on top of the earlier `charset_attr`/`out_of_mem_msg` shares)
and the `.c.o:` rule fix failed with `ld: too big for type 431`, and
`mkovmake`'s reported partition got noticeably worse than the
previously-working link: 6 overlay groups instead of 8, largest
overlay 32640 bytes instead of ~24320-24576, base 38016 -> would need
40960 rounded, `40960 + round8k(32640)=32768` = 73728, 8192 bytes over
the 65536 ceiling -- the same magnitude overage as the original
pre-pico-removal failure.

The three cross-file shares (`charset_attr` in `send.c`/`mailview.c`,
`out_of_mem_msg` in `send.c`/`mailcmd.c`, `word_not_found_msg` in
`mailcmd.c`/`folder.c`/`mailview.c`) were reverted back to independent
per-file `static` copies, as a diagnostic experiment to see if
undoing them restores the previous overlay partition.

**The suspected cause (extern data symbols creating `mkovmake` "ties"
that pull modules together into worse groupings) is NOT confirmed.**
`mkovmake.txt`'s own definition of a "tie" is specifically a match
between an undefined symbol in one module and a global *text* symbol
in another (for call-thunk purposes) -- data symbols don't need
thunking, since `.data`/`.bss` live in the one always-resident,
never-swapped segment on this `-i` (separate I&D) target, so an
`extern char *foo;` reference shouldn't register as a tie at all under
that definition. The real cause of the regression is still unknown;
plausible alternatives include general sensitivity of `mkovmake -O2`'s
greedy grouping heuristic to any change in individual module sizes
(even a few bytes can shift which modules greedily pack together), or
the `.c.o:` rule fix (see above) causing previously stale/mislocated
`../c-client/*.o` files to be correctly rebuilt with accurate sizes
for the first time in the same relink attempt, confounding the
comparison. This revert is a diagnostic step, not a confirmed fix --
next real-hardware relink will show whether it actually restores the
previous partition.

- **Changed**: `send.c`, `mailcmd.c`, `folder.c`, `mailview.c` --
  each of the three shared strings goes back to being declared
  `static` independently in every file that uses it (4 total `static`
  declarations restored across the 4 files, replacing 1 owner + 3
  `extern` declarations).
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31. Real-hardware relink re-test still
  pending as of this write-up.
- **Size delta**: `bin/pine` `text` 274650 -> 274706 (+56), `data`
  10216 -> 10248 (+32) on the `lnx` dev build -- reversing the earlier
  cross-file-share savings, as expected. Whether this is worth trading
  back for a working overlay link depends on the pending real-hardware
  result.

**Update -- reverted back to restoring the shares.** Real-hardware
relink after this revert produced the exact same failure and the
*identical* overlay partition (base `38016`, overlays
`32064,31040,32640,31616,25664,25600`) as before the revert -- only
`data` moved (the expected +64 bytes from the four independent
copies), which had zero bearing on the partition. This confirms the
"ties are text-only" correction: removing `extern` *data* references
could not have changed -- and did not change -- `mkovmake`'s grouping
at all. The three cross-file shares are exonerated and were restored
(back to the state described in the entries above); the real cause of
the "ld: too big for type 431" regression is still unidentified.
Remaining suspects, neither yet tested: (1) the `.c.o:` rule fix
elsewhere in this file's history causing previously
stale/mislocated `../c-client/*.o` files to be rebuilt at accurate
sizes for the first time in that same relink, or (2) ordinary
text-size/call-graph drift from the string-consolidation edits
themselves (e.g. `mailview.c`'s `style_word()` helper) shifting
`mkovmake -O2`'s greedy grouping, independent of any cross-file
sharing. Next step if resumed: bisect real-hardware relinks across
the commits between the last known-good partition (8 groups, largest
~24320-24576) and this one.

**Root cause found.** Suspect (1) above was it. The user ran the real
2.11BSD `make -f mkov` step twice, once with the `.c.o:` rule fix
(`pine/makefile.bsd`) present and once with it manually backed out:

- Without the fix: `text 37824`, overlays
  `23488,24320,24320,23680,23488,18176,23808,13568` (8 groups, largest
  `24320`) -- link succeeds.
- With the fix: `text 38016`, overlays
  `32064,31040,32640,31616,25664,25600` (6 groups, largest `32640`) --
  `ld: too big for type 431`.

`../c-client`'s own `makefile.bsd` builds with `CFLAGS = -g -O`
(real optimization); `pine/makefile.bsd`'s `CFLAGS = -g -DDEBUG` has
no `-O` at all. `pine-ovl`'s design has always assumed
`$(CCLIENTOFILES)` (`mail.o`/`bezerk.o`/`dummy.o`/`rfc822.o`/
`misc.o`/`osdep.o`) are prebuilt separately via c-client's own
optimized `all` target and only referenced loose here (see the
existing comment above `MKOVMAKE=`). Before the `.c.o: -o $@` fix,
the *mislocation bug was accidentally protective*: whenever make
decided one of these `../c-client/*.o` prerequisites looked stale, the
builtin (no `-o`) suffix rule fired `cc -c ../c-client/mail.c`, which
dropped a bogus `mail.o` into `pine/`'s cwd and left the real
`../c-client/mail.o` -- still c-client's smaller, `-O`-optimized build
-- untouched. Once the `-o $@` fix let that implicit rule resolve to
the *correct* output path, `pine-ovl` started silently **rebuilding
c-client's sources with pine's own unoptimized `-DDEBUG` flags**
whenever timestamps made them look stale (e.g. after uploading fresh
`.c` files to the target), inflating their text size and blowing the
`pine-ovl` link's budget -- exactly the ~3968-byte/6-vs-8-group
regression measured above. (Confirmed: `imap/ANSI/c-client/makefile.bsd`
has zero `#ifdef DEBUG`/`#ifndef DEBUG` anywhere, so `-DDEBUG` itself
is a no-op for c-client's code -- the size delta is purely
optimized-vs-unoptimized codegen.)

- **Fix**: added an explicit no-recipe rule in `pine/makefile.bsd`,
  `$(CCLIENTOFILES): ;`, right after the `CCLIENTOFILES=` definition.
  An explicit rule (even an empty one) takes precedence over make's
  implicit suffix-rule search, so `../c-client/mail.c` (etc.) is never
  considered as a way to (re)build these targets -- make just uses
  whatever `.o` is already there, exactly matching the documented
  "already built separately" assumption. This does not affect pine's
  own `$(obj)`, which still gets the `.c.o: -o $@` fix's correct
  same-directory recompilation.
- **Verified**: `make -f makefile.bsd -n pine-ovl` after `touch
  ../c-client/mail.c` (to force a stale-looking timestamp) -- before
  this fix, the dry run showed `cc ... -o ../c-client/mail.o
  ../c-client/mail.c`; after, that line is gone entirely, only pine's
  own `os.o`/`date.o` recompiles are listed. This makefile isn't part
  of the `lnx` dev build, so no `./build lnx`/smoketest cycle applies.
  Confirmed on real 2.11BSD hardware: `make -f makefile.bsd pine-ovl`
  linked clean, no `ld` error, partition back to `text 37824`, 8
  overlay groups, largest `24320` (`overlays:
  23488,24320,24320,23680,23488,18176,23808,13568`) -- exactly the
  previously-working shape, with `data 30840` showing all the
  string-cutting savings kept intact.
- **Size delta**: N/A on the `lnx` dev build (BSD-makefile-only fix).
  Confirmed on real hardware: restores the previously-working overlay
  partition while keeping every byte of the string-cutting savings --
  unlike the cross-file-share revert above, which cost bytes for
  nothing.

### Shrink `pine.c`'s string literal footprint

Continuing the string-cutting pass: `pine.c` measured at ~2146 bytes
across 91 string literals, next after `mailcmd.c`/`mailview.c`/
`send.c`/`folder.c` (the generated `helptext.c` was skipped -- edit
`pine.hlp` instead -- and `reply.c`/`init.c`/`mailindx.c`/`filter.c`
are smaller, still untouched).

- **Deduplication**: none of real substance -- `pine.c`'s intra-file
  duplicate literals are all tiny (`"TERM"`/`"HOME"` `getenv()` keys,
  `"?"` help-key strings, `KEYMENU_S` table entries like `"OTHER
  CMDS"`/`"Help"` -- the last correctly left alone, aggregate
  initializers per the established rule). Most of this file's byte
  volume is long, mostly-unique strings instead.
- **Cross-file find**: `pine.c`'s main-command-loop default case still
  had the *original*, long "not defined for this screen" message
  (`"\007Command \"%s\" not defined for this screen. Use %s for
  help"`) that `mailcmd.c`/`mailview.c`/`folder.c` already shortened
  in an earlier entry -- missed then because this file hadn't been
  measured yet. Brought in line: `"\007\"%s\" undefined here (%s for
  help)"`.
- **Shortening**: this file's error/status text falls into two very
  different categories, treated differently:
    - **Left untouched**: the six main-menu `mkeys[]` command
      descriptions (`"   %s     HELP               -  Get help using
      Pine"` etc.) -- these are core, constantly-user-read UI text
      (not error/status messages), live in a `static struct menu_key
      mkeys[] = {...}` aggregate initializer (can't share via a
      variable anyway), and a dozen-plus `test/cases/*.py` files
      pattern-match this exact wording to confirm the main menu
      painted (`test_folder_list_screen.py`, `test_signal_handlers.py`,
      etc.) -- bad risk/reward, skipped entirely. Also left untouched:
      the `"Copyright 1989-1993.  PINE is a trademark..."` notice (a
      real legal/trademark notice, not incidental text -- the
      surrounding source comment says as much) and the `"\n\nBug in
      Pine detected: \"%s\".\nExiting pine.\n"` crash-format string
      (not test-asserted, but this exact phrase is the standing
      diagnostic convention this whole project's crash-investigation
      workflow keys off of -- shortening it would cost more in lost
      convention than it'd save in bytes).
    - **Shortened**: the composer/postponed-mail startup notice
      (`"\007Use compose command to continue composing last message.
      It was interrupted."` -> `"\007Composing interrupted -- resume
      with Compose"`); the five pre-curses terminal/TERM startup
      errors in `main()` (`"Can't access terminal..."`/`"standard
      input is not allowed..."` -- two `fprintf()` calls -- merged into
      one `"\007Can't use terminal: stdin isn't a tty (e.g. \"pine <
      file\")\n"`; `"Terminal type (envronment variable TERM) not
      set.\n"` -> `"TERM not set.\n"`; `"Terminal type \"%s\", is
      unknown.\n"` -> `"Unknown TERM \"%s\".\n"`; `"Can't open termcap
      file; check TERMCAP variable and/or system manager.\n"` ->
      `"Can't open termcap file (check TERMCAP)\n"`; `"Your terminal,
      of type \"%s\", is lacking functions needed to run pine.\n"` ->
      `"Terminal \"%s\" lacks needed functions.\n"`); the
      folder-open-failure message (`"\n\nUnable to open %s \"%s\".
      Pine finished\n\n"` -> `"\n\nCan't open %s \"%s\"\n\n"`); and the
      `panic1()` message-too-long fallback (`"Pine paniced. (Reason
      for panic is too long to tell)"` -> `"Pine panic (message too
      long)"`, an extremely rare edge case -- only reachable when a
      panic message exceeds 1000 characters). All of these are
      one-shot diagnostic/status text, not something the user reads to
      know what to type next, matching this project's established
      "cryptic error over a binary that doesn't run" latitude.
- **Verified**: checked every touched/candidate string against
  `test/cases/*.py` first (`grep -rn "expect("` for the terminal/panic
  strings turned up nothing; the main-menu lines' widespread test
  matches are exactly why those were left alone). `./build clean &&
  ./build lnx`, exit 0, 0 warnings; full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274650 -> 274338 (-312 bytes) on
  the `lnx` dev build -- the real `.data` win on the 2.11BSD target is
  larger, per the same gcc-already-dedupes-strings caveat as every
  string-cutting entry in this file. `bin/pico` untouched (`pine.c` is
  pine-only).

### Shrink `reply.c`'s string literal footprint, plus a second cross-file share

Continuing the string-cutting pass: `reply.c` measured at ~1595 bytes
across 98 string literals, next after `pine.c`.

- **Deduplication**: `". Text not included"` (4 sites, all
  `get_body_part_text()` arguments -- safe, not aggregate
  initializers) and `"\007Error including all message parts"` (2
  sites, both `q_status_message()` arguments) each collapsed to one
  shared `static` string. `"\007Error allocating message text"` (2
  sites) likewise. `"1"` (6 sites, a literal part-number string arg to
  `get_body_part_text()`) was left alone -- 1 byte per site, not worth
  the pointer-variable overhead. Punctuation/formatting fragments used
  while building reply/quote text (`"@"`, `", "`, `"Re: "`, `"> "`,
  etc.) were also left alone for the same reason.
- **Cross-file sharing**: `"\007Message cancelled"` turned up 3x in
  this file and, via a fresh sweep for exact duplicates against every
  already-processed file (`mailcmd.c`/`mailview.c`/`send.c`/
  `folder.c`/`pine.c`), 1x more in `send.c` -- 4 copies total of
  what's functionally the same message `mailcmd.c` already
  consolidated its own instances of as `cancelled_msg = "\007Cancelled"`
  (a user suggestion from earlier in this pass: "one 'cancelled'
  message"). Unified wording to match and made `mailcmd.c`'s
  `cancelled_msg` the shared owner (dropped `static`); `reply.c` and
  `send.c` each get an `extern char *cancelled_msg;` instead of their
  own copies. This is the same *kind* of change as the cross-file
  shares that were suspected (then exonerated) in the "ld: too big for
  type 431" investigation above -- now confirmed safe to keep doing:
  that regression's real cause was the `$(CCLIENTOFILES)` makefile bug
  in the entry above, not cross-file data sharing, which `mkovmake`'s
  own tie definition (text-symbol-only) was never sensitive to in the
  first place.
  - The fresh cross-file sweep's other hits (`"headers.h"`, `"as is"`,
    `"pine-bugs@cac.washington.edu"`, etc.) were false positives from
    a naive text-based grep matching quoted substrings inside the
    shared copyright-header *comment* block and `#include` directives
    that are boilerplate at the top of every `.c` file in this tree --
    zero bytes in any compiled object, not a real duplication
    opportunity.
- **Verified**: checked every touched/candidate string against
  `test/cases/*.py` first; none of this file's changes touch a tested
  string. `./build clean && ./build lnx`, exit 0, 0 warnings; full
  smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274338 -> 274370 (+32), `data`
  10216 -> 10248 (+32) on the `lnx` dev build -- a net *increase* here,
  expected per the standing gcc-already-dedupes-strings caveat (gcc's
  own identical-string-constant merging on this ELF/gcc dev build
  already absorbed most of what these edits target, so the added
  pointer-variable overhead shows as a small net loss locally). The
  real win is on the 2.11BSD target, whose `a.out` toolchain has no
  such automatic merging -- every one of the now-4 `cancelled_msg`
  call sites and the 8 other collapsed sites keeps only one copy of
  its string in `.data` instead of one per call site. `bin/pico`
  untouched (`reply.c` is pine-only).

### Shrink `init.c`'s string literal footprint

Continuing the string-cutting pass: `init.c` measured at ~1495 bytes
across 66 string literals, next after `reply.c`. Unlike the previous
several files, intra-file duplication here was minimal (`"user-domain"`
x3, `"variable"`/`"reverse"` x2 -- all tiny and not worth a shared
variable), and a cross-file sweep against every already-processed
file turned up nothing real (`"cc"`/`"HOME"` matches were incidental,
unrelated 2-4 byte tokens). All the size here is one-shot startup/
config diagnostic text printed via `printf`/`fprintf(stderr, ...)`
before the main screen exists (mail-directory setup, `TERM`/sort-key/
keystroke-list validation, login-name lookup) -- same category as the
`pine.c` startup errors shortened in an earlier entry, and confirmed
via `grep -rn "expect("` to have zero `test/cases/*.py` dependencies.
- **Shortened**: the `init_mail_dir()` cluster (3 cases, originally
  5 separate `printf()` calls) -- `"The \"mail\" subdirectory already
  exists, but it is not writable by pine\n"` + `"so pine cannot run.
  Please correct the permissions and restart pine.\n"` merged into one
  `"\007\"mail\" dir exists but isn't writable; fix permissions\n"`;
  `"Pine requires a directory called \"mail\" and usualy creates
  it.\n"` + `"You already have a regular file called \"mail\" which is
  in the way.\n"` + `"Please move or remove \"mail\" and start pine
  again.\n"` merged into one `"\007\"mail\" is a file, not a
  directory; move or remove it\n"`; `"Creating subdirectory \"%s\"
  where pine will store\nits mail folders.\n"` -> `"Creating mail
  directory \"%s\"\n"`; `"\007Error creating subdirectory \"%s\" :
  %s\n"` -> `"\007Can't create \"%s\": %s\n"`. Also: the
  `initial-keystroke-list` char/function-key-mixing warning (2
  `fprintf()` calls merged into 1); the login-name lookup failure
  (`"Who are you? (Unable to look up login name)\n"` ->
  `"\007Can't look up login name\n"`); the folders-directory-too-long
  error (2 `printf()` calls merged into 1, now including the offending
  path in the same message); the two sort-key validation messages; and
  the `BOTCH` internal-error `panic1()` string (only reachable via a
  programmer error in this codebase, never end-user-visible in
  practice, so shortened aggressively).
- **Verified**: `grep -rn "expect("` against every candidate string
  first (zero hits). `./build clean && ./build lnx`, exit 0, 0
  warnings; full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 274370 -> 273906 (-464 bytes) on
  the `lnx` dev build -- larger than most single-file entries in this
  pass since several messages here were merged from multiple `printf`/
  `fprintf` calls into one (removing a real function-call site, not
  just shortening a literal). The real `.data` win on the 2.11BSD
  target is separately larger still, per the standing
  gcc-already-dedupes-strings caveat. `bin/pico` untouched (`init.c`
  is pine-only).

### Shrink `mailindx.c`'s string literal footprint, plus a fourth cross-file share

Continuing the string-cutting pass: `mailindx.c` (the folder index
screen) measured at ~1112 bytes across 111 string literals, next
after `init.c`.

- **Noteworthy non-finding**: the cross-file sweep against every
  already-processed file turned up a *huge* pile of exact matches
  against `folder.c`/`mailview.c`/`pine.c` -- single-letter command
  keys and labels (`"C"`/`"Compose"`, `"D"`/`"Delete"`,
  `"GotoFldr"`, `"OTHER CMDS"`, etc.). These are not a real
  opportunity, though: every one of them lives in a `static struct key
  foo_keys[] = {{"?","Help",0}, ...}` compile-time aggregate
  initializer (one per screen -- index/folder-list/view/main-menu each
  define their own), and C requires constant expressions there, so
  they can't be pulled into a shared `char *` variable without
  restructuring these tables to populate at runtime instead -- a much
  bigger, riskier refactor touching every screen's key-menu table, out
  of scope for this opportunistic pass. Noted in `TODO-pdp11.md` as a
  real but structural future opportunity, not attempted here.
- **Real cross-file find**: `"\007Already on first page."` matched
  `folder.c` byte-for-byte; `"\007Already on last page"` was one
  trailing period away from matching `folder.c`'s copy. Unified
  wording (dropped the trailing periods) and shared both as
  `already_first_page`/`already_last_page`, owned by `mailindx.c`
  (non-`static`), with `extern` declarations in `folder.c`.
- **Shortening**: `"No folder is currently open"` ->
  `"No folder open"`; `panic("Error calculating index position")` ->
  `panic("Bad index position")` (an internal-error path, not routine
  UI text); the `"[ No Message Text Available ]"` index-line
  placeholder (shown when a message's envelope has literally no
  displayable fields, a rare edge case, not routine content) ->
  `"[No Text]"`; `"get_index_cache failed!"` ->
  `"\007Index cache error"`. Left `"Sorting folder by %s"` alone
  (routine, already concise status text).
- **Verified**: `grep -rn "expect("` against every candidate string
  first (zero hits). `./build clean && ./build lnx`, exit 0, 0
  warnings; full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 273906 -> 273810 (-96 bytes) on
  the `lnx` dev build. Real 2.11BSD `.data` savings are larger, per
  the standing gcc-already-dedupes-strings caveat. `bin/pico`
  untouched (`mailindx.c` is pine-only).

### Shrink `filter.c`'s string literal footprint

Continuing the string-cutting pass: `filter.c` (the MIME/richtext/
QP/base64 stream-filter engine) measured at ~993 bytes across 49
string literals, next after `mailindx.c`.

- **False positives filtered out first**: several apparent duplicates
  (`"Illegal type of operand"`, `"Illegal conditional"`, `"cond ? a =
  b, n-- : 0"`) turned out to live inside *comments* documenting past
  `pcc`/2.11BSD `cc` portability bugs fixed earlier in this project,
  not real string literals -- the naive text-based grep this pass uses
  doesn't distinguish code from comments. `b64v`, the base64 alphabet
  table (`"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"`,
  66 bytes, the single longest string in the file) is essential data,
  not incidental text -- left untouched.
- **Deduplication**: `"Non-hexadecimal character in QP encoding"` (2
  sites, both `gf_error()` arguments) collapsed to one shared `static`
  string, also shortened in the process (see below).
- **Shortening**: all internal filter-engine error paths (`gf_error()`/
  `q_status_message()` calls for malformed MIME/base64/richtext input
  or storage-object I/O failures) -- `"Non-hexadecimal character in QP
  encoding"` -> `"Bad hex char in QP encoding"`; `"ERROR creating
  message file"` -> `"Can't create msg file"`; `"ERROR creating
  message temp file"` -> `"Can't create temp file"`; `"ERROR reopening
  %s : %s"` -> `"Can't reopen %s: %s"`; `"ERROR positioning in %s :
  %s"` -> `"Seek error in %s: %s"`; `"Error writing out pipe"` ->
  `"Pipe write error"`; `"Internal error in base64 encoder"` ->
  `"Base64 encoder error"`; `"Illegal '=' in base64 text"` -> `"Bad
  '=' in base64"`; `"Internal base64 decoder error"` -> `"Base64
  decoder error"`; `"Richtext token over 40 characters"` -> `"Richtext
  token too long"`; `"Incomplete token in richtext"` -> `"Incomplete
  richtext token"`. None of these had a `\007` bell prefix originally
  and none were added -- that would be a real (if minor) behavior
  change (an audible terminal bell), not just shorter text, so left
  as-is either way.
- **Verified**: `grep -rn "expect("` against every candidate string
  first (only one unrelated hit, matching decoded plaintext in
  `test_base64_body.py`, not any of these error strings). `./build
  clean && ./build lnx`, exit 0, 0 warnings; full smoketest suite
  31/31.
- **Size delta**: `bin/pine` `text` 273810 -> 273706 (-104 bytes) on
  the `lnx` dev build. Real 2.11BSD `.data` savings are larger, per
  the standing gcc-already-dedupes-strings caveat. `bin/pico`
  untouched (`filter.c` is pine-only).

### Shrink `args.c`'s string literal footprint

Continuing the string-cutting pass: `args.c` (command-line argument
parsing) measured at ~849 bytes across 23 string literals, next after
`filter.c`.

- **Left untouched**: the `args_help()` usage/`-h` screen (roughly
  half this file's string bytes) -- real documentation the user reads
  to learn how to invoke this stripped-down build's command-line
  flags, the same category as the `pine.c` main-menu text left alone
  in an earlier entry, and (unlike that case) not even test-covered,
  just judged not worth the clarity risk for the size.
- **Shortened**: the four `argv`-parsing error messages instead --
  `"Error: missing argument for flag \"%c\"\n"` -> `"Missing arg for
  -%c\n"`; `"Error: Non numeric argument for flag \"%c\"\n"` ->
  `"Non-numeric arg for -%c\n"`; `"Error: unknown flag \"%c\"\n"` ->
  `"unknown flag -%c\n"`; `"Error in -I argument \"%s\": %s\n"` ->
  `"Bad -I arg \"%s\": %s\n"`.
- **Near-miss, caught by the smoketest suite**: shortening the
  "unknown flag" message to `"Unknown flag -%c\n"` (capitalized, matching
  this entry's other three messages) broke `test_conf_dump.py`, which
  does a direct `assert "unknown flag" in result.stderr` -- a
  case-sensitive substring check bypassing `session.expect()` entirely,
  so the pre-edit `grep -rn "expect("` sweep this pass has relied on
  to find test dependencies never saw it. Fixed by keeping that one
  message lowercase (`"unknown flag -%c\n"`) rather than touching the
  test. **Methodology note for future entries in this pass**: the test
  dependency check needs to also grep for direct `assert ... in
  result.std(out|err)` patterns, not just `session.expect(`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite caught the regression above (30/31) and
  confirmed the fix (31/31).
- **Size delta**: `bin/pine` `text` 273706 -> 273642 (-64 bytes) on
  the `lnx` dev build. Real 2.11BSD `.data` savings are larger, per
  the standing gcc-already-dedupes-strings caveat. `bin/pico`
  untouched (`args.c` is pine-only).

### Shrink `signals.c`'s string literal footprint

Continuing the string-cutting pass: `signals.c` measured at ~750
bytes across 16 string literals, next after `args.c`. No intra-file
duplicates and no real cross-file matches this time.

- **Left untouched, applying the by-now-established prompt/
  diagnostic-convention exceptions**: `panic("Received abort
  signal")` (feeds the "Bug in Pine detected: \"%s\"" diagnostic
  format left alone in the `pine.c` entry); `"\007Pine emergency exit.
  REALLY ABORT PINE RIGHT NOW"` (a `want_to()` confirmation prompt for
  an irreversible action -- and `test_signal_handlers.py` checks this
  exact substring); `"Type ^L (control-L) to repaint the screen and
  continue with Pine"` (tells the user what key to press --
  `test_signal_handlers.py` checks `"repaint the screen"`); and the
  two `"Pine finished. Received %s signal"` hangup/terminate messages
  (`test_signal_handlers.py` checks this exact format string with
  `%s` filled in as `"hang up"`/`"terminate"`).
- **Shortened, preserving tested substrings**: the two "another pine
  has this open" messages keep their tested `"Session now Read-Only"`
  suffix verbatim (`test_signal_handlers.py` checks that substring)
  but shortened the lead-in (`"Another pine access to Inbox.  Session
  now Read-Only."` -> `"Inbox open elsewhere.  Session now
  Read-Only."`, same treatment for the folder variant); the suspend
  notice keeps its tested `"Pine suspended"` lead-in
  (`"\nPine suspended. Give the \"fg\" command to come back.\n"` ->
  `"\nPine suspended; \"fg\" to resume.\n"`). Also shortened (no test
  dependency): the IMAP-suspend-timeout warning and the
  connection-broken status message (see the `do_suspend()` dead-code
  question logged as a new open item in `TODO-pdp11.md` -- these two
  strings may be unreachable on this fork, not confirmed, left as
  code either way); the signal-reset error; and the abort/coredump
  banner.
- **Verified**: `grep -rln` against every candidate string across
  `test/cases/*.py` first (this is the file where that check actually
  mattered -- `test_signal_handlers.py` exercises real
  SIGHUP/SIGTERM/SIGUSR2/SIGQUIT/SIGTSTP delivery and checks several
  of these exact strings). `./build clean && ./build lnx`, exit 0, 0
  warnings; full smoketest suite 31/31 including
  `test_signal_handlers.py`.
- **Size delta**: `bin/pine` `text` 273642 -> 273546 (-96 bytes) on
  the `lnx` dev build. Real 2.11BSD `.data` savings are larger, per
  the standing gcc-already-dedupes-strings caveat. `bin/pico`
  untouched (`signals.c` is pine-only).

### Shrink `screen.c`'s string literal footprint

Continuing the string-cutting pass: `screen.c` (the titlebar/status-
line layout engine) measured at ~711 bytes across 48 string literals,
next after `signals.c`. This file had little real opportunity --
apparent duplicates (`"<folder>"` x3, `"DEL"` x3, `"Title"` x2)
were all comment mentions or a single macro definition
(`BAR_STATUS()`), not repeated code strings, and most of the rest of
the file's bytes are tightly parameterized `sprintf()` format strings
for the titlebar's location/percent/status display -- e.g. `"%s %s of
%s %3d%% %s"` -- that a careful column-width computation elsewhere in
the file depends on; not touched.
- **Shortened**: the file's three internal `panic()` "can't happen"
  assertions (unreachable except via a real programmer bug, not
  routine UI) -- `"Update_titlebar_page with wrong style"` -> `"Bad
  titlebar style"`; `"Can't happen in savebits()\n"` -> `"BOTCH:
  savebits()\n"`; `"Can't happen in equalbits()\n"` -> `"BOTCH:
  equalbits()\n"` (matching the `BOTCH:` internal-error convention
  already established in the `init.c` entry).
- **Verified**: `grep -rln` against every candidate string across
  `test/cases/*.py` first (zero hits). `./build clean && ./build
  lnx`, exit 0, 0 warnings; full smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 273546 -> 273506 (-40 bytes) on
  the `lnx` dev build -- modest, since this file had little real
  duplication to begin with. `bin/pico` untouched (`screen.c` is
  pine-only).

### Shrink `newmail.c`'s string literal footprint, drop the `carray[]` synonym whimsy

Continuing the string-cutting pass: `newmail.c` measured at ~580
bytes across 36 string literals, next after `screen.c`.

- **Feature-vs-size tradeoff, asked rather than assumed**:
  `new_mail_mess()` had a `static char *carray[] = {"regarding",
  "concerning", "about", "as to", "as regards", "as respects", "in
  re", "re", "respecting", "in point of", "with regard to",
  "subject:"}` -- 12 synonyms, randomly picked (`carray[(unsigned)
  random()%12]`) to vary the wording of the new-mail notification's
  subject clause purely for cosmetic variety (e.g. "...concerning
  Lunch Plans" vs "...as regards Lunch Plans"). This was the single
  biggest chunk of the file's string data (~100+ bytes of literal
  text, plus the 12-pointer array itself) but a genuine behavior
  change to remove, not just a shortening -- out of scope for this
  pass's normal "check tests, shorten in place" pattern, so the user
  was asked directly rather than the call being made unilaterally.
  Chosen: collapse to the single shortest entry, `"re"`, and drop the
  `random()` call and the array entirely (`sprintf(subject, " %s
  %.183s", carray[(unsigned)random()%12], e->subject)` -> `sprintf
  (subject, " re %.183s", e->subject)`). The `subject[1] =
  toupper(subject[1])` capitalization logic right after (for when this
  clause opens the whole message) is index-based, not
  content-specific, so it still works correctly against `"re"`'s `'r'`
  as it did against every other entry.
- **Shortened**: `"\007MAIL FOLDER \"%s\" CLOSED DUE TO ACCESS
  ERROR"` -> `"\007FOLDER \"%s\" CLOSED: ACCESS ERROR"` (kept the
  all-caps urgency styling, just trimmed the wording).
- **Verified**: `grep -rln` against every candidate string across
  `test/cases/*.py` first (zero hits, including for the `carray[]`
  entries). `./build clean && ./build lnx`, exit 0, 0 warnings; full
  smoketest suite 31/31.
- **Size delta**: `bin/pine` `text` 273506 -> 273322 (-184 bytes),
  `data` 10280 -> 10184 (-96 bytes) on the `lnx` dev build -- a real
  win on *both* segments here (unlike most entries in this pass),
  since removing `carray[]` and the `random()` call cut actual code
  and a pointer array, not just literal text gcc might already
  dedupe. `bin/pico` untouched (`newmail.c` is pine-only).

### `reload_body_text()` allocates the new composer body before freeing the old one

Real-hardware crash report: composing a message works, but choosing
either Send or Cancel immediately fails with `Bug in Pine detected:
"Out of free storage (8192 bytes)". Exiting pine.` -- the same
`fatal()`-via-`fs_get()` diagnostic pattern (kept permanently in
`os_bsd.c`, see this file's `bezerk_open()` entry) that found the
`CHUNK`-vs-`MAILTMPLEN` bug earlier this session, this time pointing
at `MSIZE_INIT` (8192, `filter.c`) instead of `CHUNK`.

- **Root cause**: `send.c`'s `pine_send()` writes the composer body to
  a temp file, forks/execs the `pico` composer subprocess, then --
  *unconditionally, before checking whether the user chose Send,
  Cancel, or Postpone* -- calls `reload_body_text()` to read the
  (possibly edited) temp file back into a fresh in-memory `STORE_S`.
  That function allocated the new `CharStar` storage (`so_get()` ->
  `fs_get(MSIZE_INIT)`, unconditionally 8192 bytes) *before* freeing
  the pre-edit body's old storage (also a `CharStar` object, itself at
  least `MSIZE_INIT` bytes), so for one moment both the ~8KB+ old
  buffer and the ~8KB+ new buffer had to be live simultaneously. On
  this target's tight combined data+heap+stack budget (the whole
  point of this project's size-cutting work), that transient doubled
  requirement was enough to exhaust available memory -- explaining why
  *both* Send and Cancel failed identically: this code runs before the
  branch between them is ever reached.
- **Fix**: reordered `reload_body_text()` (`send.c`) to free the old
  `STORE_S` (`so_give(&oldso)`) *before* calling `so_get()` for the
  new one, removing the transient doubled-memory window entirely.
  Checked for a dangling-pointer risk from freeing `oldso` before
  confirming the new allocation succeeds: `so_get()`'s `== NULL` check
  right after can't actually fire on this codebase -- `fs_get()`
  (`os_bsd.c`) never returns NULL on failure, it calls `fatal()` and
  the process exits immediately -- so there's no path where `bod`'s
  storage pointer would be left dangling and then dereferenced.
- **Verified**: traced the call graph precisely (`pine_send()` ->
  `reload_body_text()` -> `so_get(CharStar,...)` -> `fs_get()`) to
  confirm this is the exact site the crash's reported byte count
  (`8192` = `MSIZE_INIT`, `filter.c`) points at, and that it's reached
  on every composer exit regardless of Send/Cancel/Postpone, matching
  the report. Attempted a `ulimit -d`-constrained repro on the `lnx`
  dev build (a small wrapper spawning `pine` under a shrunk data-
  segment limit, composing and cancelling a message) to reproduce the
  crash directly and confirm the fix moves the failure threshold --
  this did **not** work: glibc's dynamic-linking/malloc-arena startup
  overhead on this dev build dominates the same narrow `ulimit -d`
  range this bug would need to isolate, so the observed failure
  threshold (successful at 240KB, EOF-crash at 235KB, identical before
  and after the fix) is a generic startup-memory cliff, not this
  bug -- a real limitation of trying to model 2.11BSD's small,
  `brk`-based allocator with glibc's much heavier one, not a
  disproof of the fix. `./build clean && ./build lnx`, exit 0, 0
  warnings; full smoketest suite 31/31 (including
  `test_compose_cancel.py`, which exercises this exact `reload_body_
  text()` path and would already have failed if the reordering were
  wrong). No regression test added -- the fix changes allocation
  *timing* under memory pressure, not any output the `lnx` harness can
  observe at its effectively-unconstrained heap; the real confirmation
  is the pending real-hardware retest.
- **Size delta**: none -- pure reordering, no bytes added or removed.
  `bin/pine` `text`/`data` unchanged on the `lnx` dev build.

**Update -- the reload_body_text() reorder above wasn't the whole
story.** Real-hardware retest still failed, but the reported size
moved from `8192` (`MSIZE_INIT`, `filter.c`) to `8193` (`CHUNK + 1`,
`bezerk.c`'s two unconditional-looking allocations at lines 486/1169)
-- the reorder fixed the first bottleneck and immediately exposed a
second one. Rather than keep guessing at call sites from a byte count
alone, went after better tooling: see the next entry.

### `panic()` never actually produced a core dump, despite its own doc comment saying it would

`panic()` (`pine.c`) -- reached by every `fatal()`/`mm_fatal()` call,
including `fs_get()`'s out-of-storage path above -- calls
`end_signals()` (resetting every caught signal, including
`SIGABRT`/`SIGIOT`, back to `SIG_DFL`) and then just `exit(-1)`. `exit()`
never produces a core file. `signals.c`'s own top-of-file comment
describes the intended design explicitly: *"trap the signal, reset
the tty modes... and then call abort to get a useful core dump"* --
and `panic()`'s own header comment says *"If debugging is active a
core dump will be generated"* -- but neither `panic()` nor
`auger_in_signal()` (the real `SIGSEGV`/`SIGBUS`/`SIGILL`/etc handler,
which itself just calls `panic()`) ever actually raised a signal or
called `abort()`. This meant *no* crash in this codebase, real signal
or `fs_get()`-detected allocation failure alike, could ever produce a
core file, contradicting both comments' documented intent -- likely
lost somewhere during this fork's cutting, not something ever
deliberately decided against.

- **Fix**: `panic()`'s final `exit(-1)` changed to `abort()`. Since
  `end_signals()` runs immediately before this point and already
  restored `SIGABRT`/`SIGIOT` to `SIG_DFL`, `abort()` gets the normal
  default action (core dump) with a clean, already-restored tty state
  -- exactly what both comments describe.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings
  (including no implicit-declaration warning for `abort()` despite no
  explicit `#include <stdlib.h>` in `pine.c` -- picked up transitively
  via `headers.h`, same as the already-present `exit()` call). Full
  smoketest suite 31/31. Confirmed the underlying mechanism actually
  works in this environment with a minimal standalone `abort()`
  reproduction (`signal(SIGABRT, SIG_DFL); abort();`) -- the kernel
  reported "Aborted (core dumped)", confirming a real core dump was
  generated; no `core` file appeared in the working directory only
  because this dev machine's `/proc/sys/kernel/core_pattern` routes
  dumps through `systemd-coredump` instead of a plain file -- a local
  dev-machine config detail with no bearing on the real 2.11BSD
  target, which writes a plain `core` file to the current directory
  per classic Unix/`adb` convention. Reproducing the *specific*
  `fs_get()` 8192/8193-byte failures directly on the `lnx` dev build
  (to confirm `abort()` fires from that exact path) was attempted via
  `ulimit -v`/`ulimit -d` constraints and abandoned as impractical --
  see the `reload_body_text()` entry above and its own repro-attempt
  note; the gap between "just barely runs" and "dynamic linker can't
  even map libc" is too narrow on this glibc-linked build to isolate
  pine's own small allocations from process-startup overhead.
- **Size delta**: negligible (`exit(-1)` -> `abort()` is a same-size
  call substitution). Not separately measured.
- **Next step**: real-hardware retest, with `limit coredumpsize
  unlimited` set first (the target account's default was seen at `0
  kbytes` in an earlier investigation this session, which would
  silently suppress the dump even with this fix in place) -- then
  reproduce the compose/Send-or-Cancel crash again and pull a stack
  trace via `adb pine core` + `$c` to identify the actual `CHUNK+1`
  allocation site precisely instead of guessing from the byte count
  alone.

### `set_mime_types()`'s unconditional 8193-byte scratch buffer (found via a real adb stack trace)

The `panic()`/`abort()` fix above paid off immediately: the user
raised `coredumpsize`, reproduced the compose-then-Send-or-Cancel
crash again, and pulled a real call stack with `adb pine core` + `$c`:

```
0175114: ~abort() from ~panic+0104
0175132: _panic(0175174) from ~mm_fatal+016
0175150: ~mm_fatal(0175174) from ~fatal+014
0175166: ~fatal(0175174) from ~fs_get+070
0175306: ~fs_get(020001) from ~set_mime_types+0112
0175364: _set_mime_types(0134270) from ~create_message_body+040
0175402: _create_message_body(0176756) from ~strings2outgoing+0270
0175422: _strings2outgoing(...) from ~pine_send+01730
0176140: _pine_send(...) from ~compose_mail+02314
```

`020001` octal = `8193` decimal (`CHUNK + 1`) -- but this isn't
`bezerk.c` at all, it's `send.c`'s `set_mime_types()`, called from
`create_message_body()` -> `strings2outgoing()`, right after the
composer subprocess returns (same "runs before Send/Cancel branch"
shape as the `reload_body_text()` bug above, explaining why both
choices failed identically again).

- **Root cause**: `set_mime_types()` scans the entire outgoing body in
  `RBUFSZ`-sized chunks (`#define RBUFSZ (8193)`, matching `bezerk.c`'s
  `CHUNK`-sized buffers) via `so_readc()`, to decide 7BIT/8BIT/BINARY
  encoding and PLAIN/US-ASCII subtype -- an unconditional 8193-byte
  `fs_get()`, same class of oversized fixed buffer as the
  `CHUNK`-vs-`MAILTMPLEN` `bezerk_open()` bug fixed earlier this
  session.
- **Fix**: shrank `RBUFSZ` to `MAILTMPLEN + 1` (1025 bytes, matching
  the `bezerk_open()` precedent) -- an 8x reduction. Confirmed this
  is purely a chunking-granularity change, not a correctness one: the
  function processes `buf` one byte at a time via a persistent `p`
  cursor and accumulates cross-chunk line length via `line_so_far`,
  so a smaller buffer just means more, smaller refills.
- **Independent bug found auditing the same buffer**: tracing the
  refill logic to confirm the above turned up a real, separate defect
  -- on a mid-line refill, the code re-pointed `p` at the new chunk's
  start (`p = buf;`) but left `bol` (marks the current line's start)
  at its old, now-stale offset into the same reused buffer, so the
  next `line_so_far + (p - bol)` computation would measure a bogus
  (generally too-small) distance instead of the true in-progress line
  length. At the old `RBUFSZ` (8193) this needed a single unbroken
  8KB+ line to ever trigger -- essentially unreachable. At the new,
  much smaller `RBUFSZ` this looked like it would become easily
  reachable by any ordinary long line, so fixed it in the same change:
  `p = bol = buf;` on refill, matching this project's standing
  practice of fixing real bugs found while auditing a buffer even
  when they're outside the nominal task.
- **Verified, and a claim retracted**: built a script-driven A/B test
  (send a message with a long line via the interactive composer, then
  via a seeded postponed-mail file, checking for the
  `Content-Transfer-Encoding: 8BIT` header an RFC821-over-1000-byte
  line should force) and initially believed it confirmed the `bol` bug
  reproducibly. It didn't, and re-checking caught this before it went
  into the log: **pico's own line editor hard-wraps any line -- typed
  interactively or loaded from a file -- to roughly 255 characters**,
  confirmed by inspecting the actual bytes `sendmail_stub`'s capture
  showed being sent (a 3000-byte unbroken line came back chopped into
  ~255-byte physical lines with real embedded newlines, identically
  whether typed live or preloaded via `postponed_mail=`, ruling out a
  pty-input-queue artifact). Since `create_message_body()` /
  `set_mime_types()` is only ever reached *after* `run_composer_
  subprocess()` returns (`dump_body_text()`/`reload_body_text()`
  round-trip every body -- new compose, reply, forward, or resumed
  postpone alike -- through that same subprocess), no line reaching
  `set_mime_types()` can exceed pico's own cap in this codebase as it
  stands, which stays far below even the new, shrunk `RBUFSZ` --
  meaning the exact scenario needed to observe the `bol` bug's effect
  never occurs through any composer-driven path. The two test variants
  "failing" identically with and without the fix wasn't confirmation
  of the bug; it was both runs failing for the same unrelated reason
  (no line ever got remotely close to the 1000-byte RFC821 threshold).
  Deleted the misleading test rather than keep something that doesn't
  actually exercise what its own docstring claims. The `bol = buf`
  fix itself is still correct and kept -- cheap, safe, and a real
  latent-correctness improvement if this ever changes (a higher pico
  line cap, or a future caller that doesn't route through the
  composer) -- just not something this test harness can currently
  demonstrate mattering. `./build clean && ./build lnx`, exit 0, 0
  warnings; full smoketest suite 31/31 (unaffected either way, as
  expected given the above).
- **Size delta**: `bin/pine` `text`/`data` unchanged on the `lnx` dev
  build (the `RBUFSZ` shrink only affects a local stack/heap
  allocation size at runtime, not the compiled binary's own
  footprint; the `bol` fix is a same-size pointer-assignment
  substitution).
- **Next step**: real-hardware retest of compose -> Send/Cancel.

**Update -- RBUFSZ shrink wasn't enough either.** Real-hardware retest
still failed, this time `"Out of free storage (1025 bytes)"` -- a
second `adb pine core` + `$c` trace confirmed the *same* call site
(`fs_get(02001)` = 1025 decimal, from `~set_mime_types+0112`), just
now failing even at the new, already-shrunk `RBUFSZ`. Since `fs_get()`
and `fs_resize()` have distinguishable failure messages (`fs_resize()`
-> plain `"Can't resize free storage"`, no size; `fs_get()` ->
`"Out of free storage (%lu bytes)"`), this confirmed it's a *fresh*
1025-byte request failing, not a resize -- so something else is
saturating the heap by this point in the call chain, not `RBUFSZ`
itself being too big.

### `filter.c`'s `MSIZE_INIT`/`MSIZE_INC` (backing every `CharStar` storage object) were still 8192/4096

Rather than another slow upload-build-reproduce-adb round trip to
chase one buffer at a time, audited the rest of the same call path
for other oversized fixed allocations before asking for another
retest. `reload_body_text()` (fixed above) calls `so_get(CharStar,
...)`, which is backed by `filter.c`'s `so_get()` -- still allocating
an unconditional `MSIZE_INIT` (8192) bytes for `so->txt` regardless of
how much text the composer body actually holds. That buffer (`newso`)
is still alive and assigned to `bod->contents.binary` by the time
`set_mime_types()` runs its own `fs_get(RBUFSZ)` a few calls later, so
the two have to coexist -- and `MSIZE_INIT` alone (8192) is already
over 8x the *entire* new `RBUFSZ` (1025), making it by far the more
likely dominant consumer squeezing out that smaller, later request.

- **Fix**: `MSIZE_INIT` 8192 -> `MAILTMPLEN` (1024), `MSIZE_INC` 4096
  -> `MAILTMPLEN` (1024) -- matching the established "small buffer +
  demand-driven growth" pattern from `bezerk_open()`/`RBUFSZ`. This is
  the shared constant behind *every* `CharStar` object in the tree
  (new-compose body, reply/forward body -- `reply.c`'s several
  `so_get(CharStar,...)` calls found in an earlier entry -- fcc
  storage in `send.c`), so unlike `RBUFSZ` (one call site) this is a
  systemic reduction across the whole compose/send/reply path at
  once.
- **Verified safe**: checked `so_writec()`/`so_puts()`'s growth logic
  in `filter.c` -- both resize via `fs_resize()` (which does a
  straight `realloc()`) on demand, and `so_puts()`'s growth is a loop
  (`while(so->size <= needed) so->size += MSIZE_INC;`) that already
  correctly handles needing more than one `MSIZE_INC` step in a single
  call, so a smaller increment only changes resize *frequency*, not
  correctness. Grepped for any other code assuming a CharStar object's
  `so->size` starts at some minimum -- none found; every other
  reference is part of the same growth/write logic.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31, including the tests most likely to
  stress the growth path (`test_resume_postponed_huge_address.py`,
  `test_send_body_header_overflow*.py`).
- **Size delta**: none on the `lnx` dev build (runtime allocation size
  change only, not a compiled-binary size change).
- **Next step**: real-hardware retest of compose -> Send/Cancel, a
  third time.

**Update -- the memory crash is fixed.** Real-hardware retest got past
compose/Send/Cancel cleanly this time (no more `fs_get()` failures) --
confirming the `reload_body_text()` reorder, the `RBUFSZ` shrink, and
the `MSIZE_INIT`/`MSIZE_INC` shrink together actually resolved the
original crash report. Next issue hit was unrelated: `call_sendmail()`
couldn't find `sendmail` at the hardcoded path.

### `SENDMAIL` pointed at the wrong path for this target's actual layout

`pine/osdep/os-bsd.h` (the fragment `pine/os.h` symlinks to for the
real 2.11BSD `makefile.bsd` build) hardcoded `SENDMAIL
"/usr/lib/sendmail"`, matching every other `os-*.h` variant in the
tree (all identical, presumably copied from a common template). On
this target, `sendmail` actually lives at `/usr/sbin/sendmail`
instead.

- **Fix**: `SENDMAIL` in `osdep/os-bsd.h` changed to
  `"/usr/sbin/sendmail"`.
- **Verified**: `osdep/os-bsd.h` isn't part of the `lnx` dev build at
  all (only reached via `pine/os.h`'s symlink for the real-hardware
  `makefile.bsd` target -- the `lnx` build uses `osdep/os-lnx.h`'s own
  separate `SENDMAIL`, left untouched and still correct for this dev
  machine, which has both `/usr/lib/sendmail` and `/usr/sbin/sendmail`
  as valid symlinks), so no `./build lnx`/smoketest cycle applies;
  verification is the pending real-hardware retest of an actual Send.
- **Size delta**: none (one string literal, same length, no compiled
  object from this repo's own `lnx` build affected).

**Update -- the memory crash is confirmed fixed; a new, different crash
appeared past it.** Real-hardware retest got all the way through
composing, Sending (correct `SENDMAIL` path now used), through to
`call_mailer()`'s Fcc/folder-management code -- then hit `"Illegal
instruction (core dumped)"` twice, via `tmux capture-pane` on the
user's PDP-11 session (rather than a pasted transcript, since the
crash and its `adb` session were already sitting in a live window):

```
adb> $c
0150764: _bezerk_file(...) from ~bezerk_append+0210
0173122: _bezerk_append(...) from ~mail_append+052
0173144: _mail_append(...) from ~context_append+0252
0175170: _context_append(...) from ~call_mailer+01450
...
```
and, on a second attempt:
```
adb> $c
0161770: _context_digest(...) from ~context_fqn_to_an+0110
0166034: _context_fqn_to_an(...) from ~mm_mailbox+026
0166056: _mm_mailbox(...) from ~dummy_find_all+0330
...
0175174: _find_folders_in_context(...) from ~call_mailer+0236
...
```

Neither trace passes through `fs_get()`/`fatal()`/`panic()`/`abort()`
at all -- this is a *raw* `SIGILL`, not our controlled crash path.
That's itself a strong signal: `init_signals()` installs
`auger_in_signal()` (which calls `panic()` -> `abort()`, printing "Bug
in Pine detected" first) for `SIGILL` specifically, so a *clean* SIGILL
should have gone through that handler and died with "IOT trap", not
"Illegal instruction", with no "Bug in Pine detected" message at all.
Getting the raw default action instead suggests the stack was already
corrupted badly enough that the kernel couldn't safely dispatch to the
handler -- pointing at real memory corruption, not another allocation
failure.

### Two project-owned functions callable with no prototype in scope at their call site

Both crash traces run through `call_mailer()` (`send.c`)'s Fcc/folder
code. Recognized a pattern here from a diagnostic seen (and dismissed
as clang-only IDE noise, since `./build lnx`'s actual `gcc` invocation
showed 0 warnings throughout this whole session) repeatedly while
editing `send.c` earlier: *"Call to undeclared function
'free_folders_in_context'; ISO C99 and later do not support implicit
function declarations"*. Re-examined it property this time --
`free_folders_in_context()` (called from `send.c:1061`, inside
`call_mailer()`, right before the code path that leads into both crash
traces) has a prototype declared *only* locally inside `folder.c`
(line 134), never exposed in any shared header, unlike its sibling
`find_folders_in_context()` (properly declared in `pine.h:730`) --
so `send.c`'s call site compiles it as an old-style implicit/K&R
declaration with no visible signature.

Re-ran the search properly (matching `./build lnx`'s actual
`-DLNX -g -ansi -I/usr/include/bsd` flags via `clang
-Wimplicit-function-declaration`, since a naive per-file `clang
-fsyntax-only` without the real include path produced a pile of false
positives for `strcmp`/`strlen`/etc.) across every `pine/*.c` file and
found one more of the same shape: `save_attachment()` (`mailcmd.c`,
called from `mailview.c:1417`), prototype declared only locally in
`mailcmd.c` (line 71), invisible to `mailview.c`. This one carries
materially higher real risk than `free_folders_in_context()`'s (whose
one argument is a same-representation pointer, and whose return value
is discarded at its one implicit call site, both low-risk shapes):
`save_attachment(int, long, struct attachment *)` takes a `long` as
its *second* argument -- on the PDP-11's 16-bit `int`/32-bit `long`
split, a `long` argument passed through a call site with no visible
prototype is a textbook stack-argument-width mismatch hazard, capable
of misaligning every argument after it (here, the `struct attachment
*`) and the stack itself. A third instance, `folder_total()` (`void
*` argument, `int` return matching the implicit default, called from
`mailbox.c:94`), carries much lower risk by the same reasoning, but
was fixed anyway -- cheap, and removes any doubt.

- **Fix**: added proper prototypes for all three to `pine.h`, matching
  their real definitions exactly, right alongside their sibling
  functions' existing entries (`free_folders_in_context`/
  `find_folders_in_context`, `save_attachment` in the `mailcmd.c`
  block, `folder_total` in the `folder.c` block).
- **Deliberately not chased further**: the same properly-scoped sweep
  also found `vfork`/`truncate`/`random` (`send.c`) and `tputs`/
  `tgetnum` (`ttyout.c`, `gethostname`/`kill`/`srandom` (`os.c`/
  `pine.c`) implicitly declared -- but these are all *external
  library* functions with one single, stable, globally-consistent ABI
  (the real implementation is already compiled into libc/libtermcap,
  unaffected by whether this translation unit's own compile happens to
  see the right header), a fundamentally different risk shape than a
  same-codebase function whose prototype-checked definition and
  prototype-blind call site could silently diverge. Left as pre-
  existing, lower-risk gaps rather than turned into the broader
  "convert every K&R-ish declaration in the tree" pass `CLAUDE.md`
  explicitly says not to do opportunistically.
- **Verified**: re-ran the `clang -Wimplicit-function-declaration`
  sweep after the fix -- `folder_total`/`save_attachment` no longer
  flagged. `./build clean && ./build lnx`, exit 0, 0 warnings, byte-
  identical `text`/`data` (pure declaration additions, no codegen
  change on this dev build's ABI, where these mismatches are
  harmless). Full smoketest suite 31/31, including
  `test_save_attachment_overflow.py`, which directly exercises
  `save_attachment()`.
- **Honesty check**: this fix is well-motivated and clearly correct to
  make regardless, but **not proven** to be the actual cause of the
  illegal-instruction crash -- the PDP-11's real calling-convention
  behavior for an implicit- vs. prototyped-declaration call to the
  same function isn't something verifiable from this dev machine, and
  the crash could instead (or additionally) be a genuine buffer
  overflow somewhere in the `context_append()`/`bezerk_append()`/
  `find_folders_in_context()` machinery both traces pass through --
  exactly the class of bug this project's own standing audit already
  watches for. Framed to the user as "worth trying first, cheap, and
  correct either way" rather than a confirmed fix.
- **Size delta**: none (header-only prototype additions).
- **Next step**: real-hardware retest of an actual Send through to
  completion. If the same illegal-instruction crash persists, the
  next move is auditing `context_append()`/`bezerk_append()`/
  `find_folders_in_context()`'s own buffer handling directly, the same
  way every other overflow in this project's audit was found.

**Update -- the prototype fix wasn't the cause either; found the real
one via live adb.** Real-hardware retest hit the identical
`find_folders_in_context()` -> ... -> `context_digest()` crash again,
unchanged. Rather than another slow upload/build/reproduce cycle, drove
the user's already-open `adb` session directly via `tmux send-keys`
against the existing core file:

```
adb> 0174564/s
0174564:        mail/[]
adb> 0162004/s
0162004:
adb> $r
ps  0170004
pc  0121010    ~context_digest+016
sp  0157750
r5  0161770
...
~context_digest+016:        clr  0175762(r5)
```

Two things this revealed: (1) the actual context string being
processed is `mail/[]` -- 7 bytes, trivially short, ruling out any
"string longer than some buffer" theory; (2) the fault is `clr
0175762(r5)` at `context_digest+016` -- 14 bytes into the function,
still in its own prologue (allocating/touching its local `tmp
[MAILTMPLEN]`), and the target address (`r5 + -1038` = decimal 57322)
lands **2 bytes above the live stack pointer** (`sp` = 57320 decimal).
That's not a data-content bug at all -- it's the stack itself running
out of room, right as `context_digest()` tries to touch its own local
buffer.

### Stack exhaustion: four functions in one call chain each carry a 1024+-byte local buffer

The crash's full call chain (`main` -> ... -> `call_mailer` ->
`find_folders_in_context` -> `context_find_all` -> `mail_find_all` ->
(c-client's dummy driver) -> `mm_mailbox` -> `context_fqn_to_an` ->
`context_digest`) is 12 frames deep, and four of those frames each
declare one or two `MAILTMPLEN`/`MAXPATH`-sized (both effectively
~1024 bytes on the real target) local arrays:
`find_folders_in_context()` (`rcontext`, `view`), `context_find_all()`
(`tmp`), `context_fqn_to_an()` (`tmp`, `context`), and
`context_digest()` (`tmp`) -- roughly 6KB of stack from just these
four frames, stacked on top of everything main()/compose_mail()/
pine_send()/call_mailer() already had live. On a target where the
whole combined data+bss+heap+stack budget is 64KB (and `data`+`bss`
alone already claim the majority of it), 6KB in one deep, one-shot
call chain is enough to run the stack pointer into unmapped space.

- **Fix**: converted all six of these local arrays from stack
  (automatic) to `static` storage. Verified non-reentrancy first --
  the only thing downstream of `context_digest()` in this chain is
  `context_mailbox()`/`context_bboard()` (`mailbox.c`), which just
  call `new_folder()`/`folder_insert()` and never call back into any
  of `find_folders_in_context()`/`context_find_all()`/
  `context_fqn_to_an()`/`context_digest()` -- so none of these
  functions can be re-entered while an outer call is still on the
  stack, making `static` safe (no data race possible either, this
  program is single-threaded). `context_digest()`'s `tmp` is also
  called from `context_apply()`'s own separate, non-nested call site --
  safe for the same reason: calls are sequential, never concurrent.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  `bss` grew +12320 bytes on the `lnx` dev build (moving six buffers
  off the stack into `.bss`) -- much larger than the real target will
  see, since this dev machine's `MAXPATH`/`MAXPATHLEN` is ~4096
  (Linux's `PATH_MAX`) vs. the real 2.11BSD target's much smaller
  value; the real `data`+`bss` growth will be well under half this.
  Full smoketest suite 31/31, including `test_send_body_header_
  overflow_via_sendmail.py`, which drives a real `call_mailer()` send
  through this exact machinery.
- **Size delta**: `bin/pine` `bss` 19208 -> 31528 (+12320) on the
  `lnx` dev build; real 2.11BSD delta will be substantially smaller
  given the `MAXPATH` difference above. `data`/`text` unchanged.
- **Next step**: real-hardware retest of an actual Send through to
  completion -- if this is the real fix, `call_mailer()` should now
  get past the Fcc/folder-search step without crashing.

**Update -- the stack-exhaustion fix worked (no more SIGILL in
`context_digest()`), but the `static` conversion's own footprint
pushed a *later* `fs_get()` in the same `call_mailer()` flow over the
edge instead.** Real-hardware retest reached "Sending mail....." and
got a fresh crash: `"Out of free storage (1024 bytes)"`, this time
`abort()`-triggered as designed, with `adb` confirming the site --
`fs_get(02000)` (1024 decimal = `MSIZE_INIT`) from `so_get()`, called
from `call_mailer()` building the Fcc storage object (`fcc_so =
so_get(FCC_SOURCE, ...)`, `send.c`). The stack fix's six `static`
buffers cost `data`+`bss` ~4608 bytes (real hardware `size` output
confirmed this exactly: `bss` `11436` -> `16044`, and `(16044-11436-
4096)/2 = 256`, giving this target's real `MAXPATH` value for the
first time), eating directly into the *same* shared 64KB budget the
heap needs -- fixing the stack problem by consuming space the heap
then didn't have.

### Right-sizing the newly-static context.c buffers from MAILTMPLEN (1024) to MAXPATH (256), plus two real pre-existing overflow bugs found in the process

All four of the newly-`static` `context.c` buffers (`context_fqn_to_an()`'s
`tmp`/`context`, `context_digest()`'s `tmp`, `context_find_all()`'s
`tmp`) were sized `MAILTMPLEN` out of generic convention, but they all
hold folder/context *paths* -- the exact domain `MAXPATH` ("longest
pathname we ever expect") already exists for, and which
`find_folders_in_context()`'s own `rcontext`/`view` buffers (never
touched, already correctly `MAXPATH`-sized) were already using.
Re-sized all four `MAILTMPLEN` -> `MAXPATH`, recovering roughly
`4 * (1024 - 256) = 3072` bytes of the `data`+`bss` this session's
stack fix had just spent -- while keeping them `static` (still off
the stack, still fixing the original SIGILL). Also re-sized
`context_apply()`'s own (already-stack, not part of the deep chain,
left as a regular local) `MAILTMPLEN` buffer to `MAXPATH` for
contract consistency with `context_digest()`'s `scontext` parameter,
and trimmed `context_find_all()`'s `"...outside context: %.900s"`
truncation width down to `%.200s` to fit its own now-smaller buffer.

**Found while re-sizing, not part of the original stack-exhaustion
hunt**: `context_digest()`'s `rcontext` bounds check already compared
against `MAILTMPLEN`, even though the caller-supplied buffer it
writes into (e.g. `find_folders_in_context()`'s own `rcontext
[MAXPATH]`) was always sized `MAXPATH` -- since `MAILTMPLEN` (1024) is
*larger* than this target's real `MAXPATH` (256), that check let
writes overflow the caller's actual buffer by up to 768 bytes. This
bug predates this whole session; it was just never exercised by input
long enough to hit it. Fixed both `rcontext` bounds checks (the
context-parsing loop and the trailing-context loop) to compare against
`MAXPATH` instead. Also found the `view` parameter's write loop
(`if(viewp) *viewp++ = *p;`) had **no bounds check of any kind** --
completely unguarded, not just checked against the wrong constant.
Added one (`if(viewp - view >= MAXPATH - 1) return("Context too
long");`), matching the pattern already used for `rcontext`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings.
  Full smoketest suite 31/31. Real-`MAXPATH` size delta not
  meaningfully measurable on the `lnx` dev build -- this dev machine's
  own `MAXPATH` (`MAXPATHLEN`, Linux's `PATH_MAX` = 4096) is *larger*
  than `MAILTMPLEN`, so switching to it here makes these buffers
  bigger, not smaller, the opposite of the real target's effect;
  `bss` delta only makes sense measured on real hardware.
- **Size delta**: not meaningful on `lnx` (see above); real 2.11BSD
  delta expected to recover ~3072 of the ~4608 bytes the stack fix
  spent, net cost roughly +1536 bytes of `data`+`bss` versus before
  either fix, in exchange for fixing both the stack-exhaustion crash
  and two real overflow bugs.
- **Next step**: real-hardware retest -- confirm `size pine` shows
  the expected `bss` reduction from this entry, and confirm the
  "Out of free storage (1024 bytes)" crash at `call_mailer()`'s Fcc
  `so_get()` is gone.

**Update -- confirmed fixed, and further diagnostics on a new,
separate crash.** Real-hardware retest confirmed the MAXPATH resize
worked (`bss` `16044` -> `12972`, matching the predicted 3072-byte
recovery) and the "Out of free storage (1024 bytes)" crash at Fcc's
`so_get()` is gone. Real Send progress reached much further -- but hit
two *different*, harder-to-pin-down issues along the way: an
apparently unresponsive hang (no crash message, `ps` showed genuine
CPU-bound spinning, `STAT R`, unresponsive to `SIGILL`/`SIGSEGV`/
`SIGQUIT` for 90+ seconds, only `SIGKILL` worked -- no core, so no
direct diagnosis possible), and, on a later attempt, a real crash:
`"Can't resize free storage"` (`fs_resize()`, not `fs_get()`).

Added temporary `syslog(LOG_ALERT, ...)` tracing to `dummy_find_all()`
(c-client's directory-scan loop, imap/ANSI/c-client/dummy.c) and
`context_fqn_to_an()` to pin down where either failure mode was
happening (same proven technique as the `ttyin.c`/`bezerk_open()`
bugs earlier this session). The trace showed the *entire*
`find_folders_in_context()` -> ... -> `context_fqn_to_an()` ->
`context_digest()` chain completing cleanly this time (all directory
entries processed, callback returns) -- ruling out this whole chain
as the hang/crash site. The crash instead happens later, inside
`call_sendmail()`'s real message-body formatting for the sendmail
temp file (`append_message2()` -> `pine_rfc822_output_body()` ->
`gf_pipe()` -> the character-filter chain), a code path this session
had never reached before (every earlier attempt crashed upstream of
it). Removed the temporary tracing once this was established.

An `adb` stack trace on the "Can't resize free storage" crash's core
file showed `fs_resize` called with what looked like garbage
arguments (`fs_resize(010,0141300)`, i.e. a near-null block pointer
and an implausible ~49KB size) from a caller (`folder_insert_index`)
that makes no sense in the real call graph at that point (a
folder-list function inside a text-encoding filter's frame). Directly
inspecting the core file confirmed this is an `adb` symbol-resolution
artifact, not real corruption: the real character-output path for a
`TYPETEXT` body is `gf_local_nvtnl` -> `gf_terminal` -> `l_putc()`
(`send.c`) -- `l_putc` is `static`, so `adb` falls back to the nearest
*exported* symbol when it can't resolve the real target, which
happened to be `folder_insert_index`. Directly examined `fcc_so`
(the global Fcc `STORE_S`, `send.c`) and its `l_out_buf` batch buffer
in the core file: `l_out_buf` was empty (not implicated) and
`fcc_so->size` was still exactly `1024` (`MSIZE_INIT`, unchanged) --
ruling out the initial "`fcc_so` growing" theory. `49856` also isn't
a clean multiple of `MSIZE_INC` from any real starting size, further
confirming the trace's `fs_resize` argument values themselves are
unreliable past this point, not a real recoverable data point.

### Extended fs_get()'s permanent size-reporting diagnostic to fs_resize()

Given the `adb` trace's precise arguments proved unreliable for this
call site, applied the same fix as `fs_get()`'s existing diagnostic
(`os_bsd.c`, kept permanently per earlier explicit instruction): make
`fs_resize()`'s failure message report the actual requested size
directly, instead of relying on a debugger to recover it after the
fact.
- **Fix**: `fs_resize()` (`imap/ANSI/c-client/os_bsd.c`) now captures
  `realloc()`'s result into a local, and on failure reports `"Can't
  resize free storage (%lu bytes)"` before calling `fatal()`, matching
  `fs_get()`'s message format exactly.
- **Also fixed while re-reading `context_fqn_to_an()` for the tracing
  above**: two more unbounded copies into the now-`MAXPATH`-sized
  `tmp` buffer, both writing a filesystem-derived `fqn` (no length cap
  upstream) with no bound of their own -- `sprintf(tmp, "Found mailbox
  outside context: %s", fqn)` (only reachable on a mismatch between a
  found mailbox and its context, an unusual/rare path) and the
  immediately following `strcpy(tmp, fqn+pfxlen)`. Bounded the first
  to `%.220s` and the second via `strncpy` with `anlen` capped to
  `MAXPATH - 1`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31.
- **Size delta**: negligible (a few instructions in `fs_resize()`, two
  bounded-copy changes with no size-constant change). Not separately
  measured.
- **Next step**: real-hardware retest of the same Send. The `fs_
  resize()` diagnostic will report the real failing size directly in
  the crash message this time, with no `adb` archaeology needed --
  and, separately, the still-unexplained hang (no core available from
  that attempt) remains open if it recurs.

### Tracked down the small memory leak left open from the third fuzzing lead

Resolved the leak documented in the TODO item of the same name: a
2-byte `cpystr()` allocation not reachable from `env->from` (or
`sender`/`reply_to`) by the time `mail_free_envelope()` runs, on
input `From:<a;I<EYE(N)`.

- **Root cause**: `rfc822_parse_address()`'s "phrase <route-addr>"
  branch (`imap/ANSI/c-client/rfc822.c`, handles the `I<EYE(N)` shape
  -- a leading phrase `I` followed by a route-addr `<EYE(N)`). This
  branch treats the leading phrase as the address's personal name,
  which is meant to take priority over any personal name
  `rfc822_parse_addrspec()` may already have derived on its own --
  `rfc822_parse_addrspec()` has a fallback that pulls a personal name
  out of a trailing `(comment)` inside the route-addr itself (used
  only when the addr-spec had no `@host` part, which is exactly this
  input's shape: `EYE(N)` parses as local-part `EYE` with comment
  `N`, no host). So by the time control reaches the "phrase
  <route-addr>" branch, `adr->personal` is already a live, separately
  `cpystr()`-allocated `"N"` -- and the branch's `adr->personal =
  rfc822_cpy (s)` unconditionally overwrites the pointer with the
  phrase's own personal name (`"I"`) *without freeing the old value
  first*, leaking it.
- **Confirmed via `gdb`-assisted tracing** (single, non-libFuzzer-
  wrapped process run -- LeakSanitizer's own report can't run under a
  debugger, "LeakSanitizer does not work under ptrace", so this had to
  be a separate pass from the actual leak-detection confirmation):
  planted breakpoints at the addrspec-fallback allocation site and at
  the phrase-branch overwrite site, confirmed `adr->personal` held a
  valid, non-NULL "N" allocation immediately before the overwrite, and
  that no `free()`/`fs_give()` call anywhere in the run's lifetime
  ever touched that address. `ASAN_OPTIONS=malloc_context_size=30:
  fast_unwind_on_malloc=0` was needed to get LeakSanitizer's full
  allocation-site stack trace (the default is shallow, only ~2
  frames) -- with it, the trace ran cleanly down through `fs_get` ->
  `cpystr` -> `rfc822_cpy` -> `rfc822_parse_addrspec` -> ... to
  `LLVMFuzzerTestOneInput`. Also had to account for libFuzzer's own
  `TryDetectingAMemoryLeak` re-executing the single input a second
  time internally -- an initial "duplicate allocation" reading of two
  identical-looking breakpoint hits was actually just this internal
  re-run, confirmed by checking the second hit's backtrace for that
  frame.
- **Fixed**: free `adr->personal` (if already set) immediately before
  the phrase-branch's overwrite, in `rfc822_parse_address()`.
- **Verified**: `From:<a;I<EYE(N)` (saved permanently as
  `fuzz/corpus/rfc822/regress_overwritten_personal_leak`) now runs
  clean under `ASAN_OPTIONS=detect_leaks=1`. Re-ran both `rfc822_fuzz`
  and `adrlist_fuzz` at length with leak detection on (39.2M and
  35.1M execs respectively) with zero leaks or crashes, then again
  after merging a large batch of upstream changes (17.1M more execs
  on `rfc822_fuzz`, 12.9M on `adrlist_fuzz`, 8.6M on `consumer_fuzz`)
  to confirm the fix holds on the final tree. Full clean rebuild, 0
  warnings; 31/31 smoketests.
- **Size delta**: negligible (one `if`/`fs_give()` call added to a
  rarely-taken parsing branch). Not separately measured.

### Fixed a fuzz-harness-only false positive found while re-verifying the leak fix: header buffer needs a valid trailing byte

The same extended leak-detection fuzzing run that confirmed the fix
above also turned up a new SEGV in `rfc822.c`'s header-folding scan
(`if (s[1] != ' ' && s[1] != '\t') *t = c = '\0';`) -- investigated
and determined to be a harness artifact, not a real bug reachable
from `bezerk.c`.

- **Root cause**: `rfc822_parse_msg()`'s header-folding scan, on
  finding a `'\n'`, peeks one byte past it (`s[1]`) to check for a
  continuation line's leading whitespace. Real callers
  (`bezerk_fetchstructure()`, `imap/ANSI/c-client/bezerk.c`) always
  keep the header inside a larger, always-NUL-terminated buffer
  (`m->internal`) with the body (or a NUL, if there's no body)
  immediately following it in the same allocation -- so that peek is
  always safe in real operation. `fuzz/rfc822_fuzz.c` and
  `fuzz/consumer_fuzz.c` both allocated the header as an exactly-
  `size`-byte standalone buffer for their "no blank line found in
  this input" case, with nothing valid immediately after it -- a
  buffer-layout mismatch from the real caller, not a parser bug.
  Confirmed the two harnesses' scratch-buffer *sizing* formula
  (`max(headersize, bodysize) + 1`) already matched
  `bezerk_fetchstructure()`'s exactly; only the trailing-byte
  guarantee was missing.
- **Fixed**: both harnesses now allocate `size + 1` bytes for the
  message buffer and NUL-terminate it, matching the guarantee real
  `bezerk.c`-delivered messages always provide.
- **Verified**: the crashing input no longer reproduces; re-ran
  extended fuzzing (figures above) with zero further crashes on any
  of the three harnesses.
- **Size delta**: none (fuzz harness only, not shipped code).

### Cross-overlay indirect function pointer: `gf_terminal()`'s network-output terminal function could go stale mid-Send

The hang/crash/missing-body saga above continued across many more
real-hardware retest cycles (canary buffers added to every `STORE_S`
allocation and to `gf_out_buf`, syslog tracing added to `gf_pipe()`,
`gf_terminal()`, and `gf_local_nvtnl()`) before the actual root cause
was found. Symptoms seen across different attempts, all from the same
underlying bug: a message body silently missing from the sent copy, a
genuine infinite loop unresponsive to every signal but `SIGKILL`, a
crash with nonsensical `adb` backtrace arguments, and a "TERM not
set." exit mid-Send.

- **Root cause**: `pine-ovl` links with real 2.11BSD `ld -i -X`
  (auto-overlay). `mkovmake` partitions object files into a resident
  BASE plus swapped OVERLAY segments purely by *static, name-based
  symbol matching* ("ties" between an undefined global symbol in one
  module and a global text symbol in another) -- per its own manual,
  "there is no way to tell mkovmake to put a certain group of modules
  together." This scoring has zero visibility into runtime function
  pointers. `filter.c`'s `gf_terminal()` invokes a registered
  `gf_io_t` (a deliberately untyped K&R function pointer) via
  `(*last_filter)(c)`; the pointer is set at runtime by
  `gf_set_terminal()`, called from `gf_pipe()`, which `send.c`'s
  `pine_rfc822_output_body()` calls passing its own local `l_putc()`
  as the terminal. `mkovmake` happened to place `filter.o` in overlay
  `OV1` and `send.o` in `OV5` (confirmed by `cat`-ing the generated
  `mkov` makefile on real hardware) -- with no "tie" between them
  visible to it, since the only link is a runtime pointer. Calling
  `gf_pipe()` from `send.c` (in `OV5`) swaps `OV5` out of memory to
  load `OV1`, invalidating the raw pointer to `l_putc()` taken
  *before* the swap -- `l_putc()`'s actual compiled code is no longer
  resident. `gf_terminal()`'s subsequent indirect call then jumps into
  whatever instruction bytes `OV1` happened to have loaded at that
  address instead, explaining every symptom above depending on what
  garbage code happened to run.
- **Fix**: relocated the entire "network output" terminal function and
  its state (`l_f`/`l_stream`/`l_out_buf`/`l_putc`/`l_flush_net` in
  `send.c`) into `filter.c` itself, renamed `gf_out_f`/`gf_out_stream`/
  `gf_out_buf`/`gf_out_putc`/`gf_flush_net`, with a new
  `gf_set_output()` to register the destination and
  `gf_output_putc_fn()` to hand back the terminal function pointer.
  This guarantees the indirect call from `gf_terminal()` never crosses
  a module/overlay boundary, regardless of how `mkovmake` partitions
  `filter.o` vs `send.o` -- more robust than fighting `mkovmake`'s
  partitioning directly (see below). `send.c`'s call sites now do
  `gf_set_output(f, s, fcc_so); pc = gf_output_putc_fn();` in place of
  the old direct `l_f`/`l_stream` assignment. New prototypes added to
  `pine.h`.
- **Approach tried and abandoned**: forcing `filter.o` into
  `mkovmake`'s BASE segment via `pine/makefile.bsd`'s `pine-ovl`
  recipe (`filter.o -Z $(obj) ...`), with several `MKOVFLAGS` basesize
  hints (20k, 17k, 11k, no hint) -- all four attempts failed to link
  with `ld: too big for type 431`, despite `round8k(base) +
  round8k(largest overlay) <= 64KB` arithmetic suggesting each should
  fit; the exact mechanism behind these specific failures was not
  fully diagnosed, and the approach was abandoned once the
  file-relocation fix (above) proved simpler and more robust.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: `gf_out_putc` (via syslog tracing, since
  removed) confirmed executing reliably with correct `c_in_buf`
  progression across multiple Sends; sends no longer hang, crash, or
  silently drop the body. `pine-ovl` still links cleanly under
  `mkovmake`'s completely unmodified, automatic partitioning.
- **Size delta**: negligible -- this is a relocation of existing code
  between two files already linked together, not new logic.

### `so_text()`'s callers used `strlen()` instead of `STORE_S->used` to size the Fcc/save-to-folder write

Once the overlay bug (above) was fixed, sends stopped hanging/dropping
the body, but a *new* symptom appeared: the saved Fcc copy
(`~/mail/sent-mail`) sometimes contained literal binary garbage
spliced into already-written header content (e.g. `Content-Type:
TEXT/PLAIN; c` followed by NUL/garbage bytes in place of
`charset=US-ASCII`, or a `To:` line trailing off into unrelated binary
data ending in a recognizable leftover pathname fragment like
`/home/abob/mail`). Confirmed via `cat -v` over a real command
connection (`dz-exec.py`), ruling out terminal-rendering artifacts.

Added a full-buffer dump helper (`dump_so_full()`, temporary, since
removed) logging a `STORE_S`'s entire `used` region in 60-byte chunks
from offset 0 -- the existing tail-only dump (`dump_so_tail()`, last
60 bytes) had never actually covered early header fields like
`Subject:`/`To:` in a 200+ byte header, which is why earlier checks
kept reporting "clean" while the actual saved file was corrupt. The
full dump showed the in-memory `STORE_S` buffer *itself* was correct
end-to-end at every checkpoint, including immediately before the
final disk write -- meaning the corruption was happening downstream
of the buffer, in however its content got copied out.

- **Root cause**: `append_message2()` (`send.c`) and `save_procedures()`
  /`save_msg()`-style folder-save code (`mailcmd.c`) both compute the
  byte length to hand to `context_append()`/`mail_string` via
  `strlen((char *)so_text(fcc_so))` instead of the `STORE_S`'s own
  accurate `->used` counter. `so_puts()`/`so_writec()` never write a
  trailing NUL after copying content into the buffer -- the buffer
  only reads as a clean, correctly-terminated C string because
  `so_get()` does a one-time `memset(so->txt, 0, size)` over the whole
  allocation at creation time. That zeroed tail is not a durable
  guarantee: on real hardware, with a long-running process and
  `malloc`/`free` churn across many prior allocations, the unused tail
  of a freshly `malloc()`'d 1024-byte block is not reliably still
  zero. `strlen()` then walks straight past the real end of the
  written content into whatever stale bytes happen to be there,
  continuing until it happens to hit some unrelated `\0` deep in
  adjacent heap memory -- explaining both the garbage itself (leftover
  heap content, e.g. a fragment of a previously-freed pathname string)
  and its varying length (however far away the next stray zero byte
  happens to be) between different runs. `so->used` is tracked
  exactly by `so_puts()`/`so_writec()` on every write and is immune to
  this failure mode.
- **Fix**: both call sites (`send.c`'s `pine_send()`, right before its
  `context_append()` call, and `mailcmd.c`'s save-to-folder path, same
  pattern) now pass `(unsigned long) so->used`/`fcc_so->used` instead
  of `strlen((char *)so_text(...))`.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: added a full-buffer dump immediately before
  the `context_append()` write (in addition to the existing
  post-header-format checkpoint) -- both showed completely clean,
  correct content (matching what was actually typed, including the
  `Subject:`/`To:` fields the old tail-only dump never covered) across
  multiple real Sends after the fix, where the same test pattern
  reliably produced corruption before it.
- **Size delta**: negligible (one field access replacing one `strlen()`
  call at each of two sites).
- **Next step / new issue found**: a real-hardware Send hit a
  previously-unseen `"Bug in Pine detected: \"Out of free storage
  (1001 bytes)\""` crash (`IOT trap`) during Fcc append, in a message
  whose own `STORE_S` buffer was confirmed clean by the same dump --
  a distinct issue, not corruption; see `TODO-pdp11.md`'s open items.
  All temporary diagnostics from this and the overlay-bug
  investigation (canary buffers/checks on every `STORE_S` and on
  `gf_out_buf`, `dump_so_tail()`/`dump_so_full()`, the `CP1`-`CP8`
  syslog checkpoints, `gf_pipe()`'s progress logging, the
  `/tmp/pine-debug-lastsend.txt` capture hook, and the
  `append_message2_watchdog()` `SIGALRM` abort-forcer) have been
  removed now that both bugs are fixed and verified.

### Shrink `strings.c`'s string literal footprint (`pine-ovl` data+bss)

Resuming the file-by-file string-cutting pass (see the `mailcmd.c`/
`mailview.c`/etc. entries above) after the Send corruption/overlay
investigation. `strings.c` had 73 string literals totaling ~569 bytes
on this dev build's measure, the biggest remaining un-cut file (every
bigger `.c` file in the tree was already done).

- **`parse_date()`'s timezone block**: 11 `if`/`else if` branches, each
  matching a lowercase three-letter abbreviation (`struncmp(p, "gmt",
  3)`) then separately `strcpy`-ing the uppercase version (`"GMT"`)
  into `d->zone_name` -- 22 literal occurrences for 11 zones.
  `struncmp()` already case-folds both operands for comparison, so
  `struncmp(p, "GMT", 3)` matches lowercase input identically to
  `struncmp(p, "gmt", 3)` -- the separate lowercase copy was pure
  duplication. Rewrote each branch to assign the single uppercase
  literal into a local `char *tz` inside the comparison
  (`struncmp(p, (tz = "GMT"), 3) == 0`), then `strcpy(d->zone_name,
  tz)` once after the chain -- one literal per zone (11 total) instead
  of 22, plus the repeated `strcpy(...)`/`d->min_off_gmt = 0` in every
  branch collapsed to one copy each. (A table-driven version was tried
  first -- a `static struct {char *name; short hrs;}` array plus a
  lookup loop -- but measured *worse* on real hardware: the table's
  own per-entry pointer+short storage cost more than the string
  dedup saved, a net data-segment loss. Reverted in favor of the
  if/else-chain-with-shared-pointer version above.)
- **`pretty_command()`**: the `c > '~'` and final `else` branches were
  byte-for-byte identical (`sprintf(buf, "%c", c); return(buf);`) --
  dead duplication, merged into one branch.
- **`byte_string()`**: `"0 bytes"` (the `bytes == 0` case) and
  `"bytes"` (the generic case's `*a ? "B" : "bytes"`) were two
  separate literals for overlapping text. Introduced one local
  `bytes_str = "bytes"` and reused it in both
  (`sprintf(string, "0%s", bytes_str)` for the zero case), leaving
  only one copy of `"bytes"` in the object file.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite 31/31
  (no test asserts on any touched string or on timezone-name/offset
  parsing). Real hardware (`pine-build.py`): `data` `29456` -> `29400`
  (-56 bytes) and, more surprisingly, overlay group 5 (`send.o
  strings.o ttyin.o`) `text` `23360` -> `23104` (-256 bytes) --
  collapsing 11 near-identical `if`/`else if` branches (and the
  `pretty_command()` dead-branch merge) shrank *code*, not just
  string data, on this target. Total overlay text `212736` ->
  `212480`.
- **Size delta**: -56 bytes data, -256 bytes overlay text (real
  2.11BSD hardware, see above).

### INBOX wouldn't open on real hardware: unprototyped read()/memcpy() silently corrupted `long`-typed argument widths

Separate, real-mail bug report: opening a genuinely UUCP-delivered
INBOX (as opposed to pine's own Fcc'd copies, all this session's prior
testing) failed immediately with `"Mailbox format invalidated (consult
an expert), aborted"`, even though the mailbox file itself, checked
byte-for-byte via `dz-download.py`, was a perfectly well-formed mbox
file that independently validated fine against `bezerk.h`'s `VALID()`
macro in a standalone test compiled with the same real `cc`.

- **Root cause (bug 1, `bezerk_parse()`'s `read()` call)**: this
  target's `unistd.h` declares `read()`/`write()`/`lseek()` etc. as
  empty-parens K&R-style declarations (e.g. `ssize_t read();`) --
  *zero* argument-type checking or coercion. `bezerk_parse()`'s
  chunked-read loop calls `read(fd,s1,i)` with `i` declared `long`
  (needed elsewhere in the same function for byte offsets that can
  exceed 32K). With no prototype forcing a narrower type, the compiler
  pushes `i` as a full 32-bit `long` (two words on this 16-bit
  target), but the real `read()` syscall stub's count parameter is a
  single-word `unsigned` (confirmed: `size_t` is `typedef u_int
  size_t` on this target's `sys/types.h`) -- the callee only consumes
  ONE of the two pushed words. Added temporary syslog tracing
  (`fd`/`requested`/`got`/`errno`) directly in `bezerk_parse()` and
  confirmed on real hardware: `read(fd=4, buf, 559)` returned `got=0,
  errno=0` (clean EOF, not an error) even though `lseek`/`tell`
  confirmed the file position was genuinely 0 and `fstat` reported the
  real 559-byte size -- i.e. the kernel received an effective count of
  0, not 559. Casting the call to `read(fd,s1,(unsigned)i)` fixed it
  immediately: same test, `got=559`, correct bytes (confirmed
  character-by-character in the trace).
- **Root cause (bug 2, `bezerk_parse()`'s `memcpy()` call)**: fixing
  bug 1 got past mailbox validation but immediately hit a new crash,
  `fatal("Bogus entry in new cache list")`. Same bug class: `string.h`
  on this target declares `memcpy()` (and `memset`/`memmove`/`memcmp`/
  `memccpy`/`memchr`) the same empty-parens way, and the per-message
  cache-slicing loop calls `memcpy(n->internal,s,j)` with `j` (a
  message length) declared `long`. Fixed the same way: `memcpy
  (n->internal,s,(unsigned)j)`. Confirmed on real hardware: INBOX
  opened successfully ("Folder \"INBOX\" opened with 2 messages") and
  the folder index displayed both messages correctly.
- **Broader pattern, partially audited**: this class of bug (a
  `long`-typed size/count argument passed to a `read`/`write`/`mem*`/
  `str*` function this target declares with empty-parens K&R
  prototypes, silently losing/misreading a word of the argument) is
  not limited to `bezerk.c`. Audited and fixed the same pattern
  wherever a `long`/`unsigned long` variable (not an `int`, and not a
  bare `sizeof(...)`, which is already single-word-safe by definition)
  was passed as a size/count argument, in the files touched by this
  investigation:
    - `bezerk.c`: the `memmove(LOCAL->buf,s,i)` unparsed-data-shift
      call, `memset(s,0,f)`/`write(fd,s,f)` in `bezerk_extend()`
      (mailbox-growing, used by Fcc/append -- plausibly the real
      explanation for the still-open "Out of free storage (1001
      bytes)" Fcc-append crash, see `TODO-pdp11.md`), the KOD
      (Kiss-Of-Death lock-busy) `read()`/`write()` calls in
      `bezerk_open()`, and `bezerk_snarf()`'s two `memcpy()` calls
      (Tenex-driver-only, likely unreachable on this fork, fixed
      anyway for correctness).
    - `mail.c`: `mail_fetchfrom()`/`mail_fetchsubject()`'s
      `memset()`/`memcpy()`/`strncpy()` calls (`length` is `long`) --
      these build the From/Subject columns for *every* line of the
      folder index, so this was a near-certain next blocker once the
      index actually started rendering.
    - `rfc822.c`: `rfc822_contents()`'s `memcpy(*dst,src,srcl)`
      (`srcl` is `unsigned long`) -- the BINARY-encoded MIME body
      path (attachments).
  **Not** exhaustively audited across the rest of `pine`/`pico`/
  `c-client` -- this was fixed opportunistically for the call sites
  this session's real-hardware testing actually reached, following
  the same "fix what you hit, verify, move on" discipline as
  everything else in this log. A dedicated tree-wide grep for
  `long`/`unsigned long` arguments to unprototyped `mem*`/`str*`/
  `read`/`write` calls is a good candidate for a future session --
  see `TODO-pdp11.md`.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: INBOX (a genuine, externally-delivered mbox
  file, not one of pine's own Fcc copies) now opens, the folder index
  displays correctly, and messages can be opened and read -- all of
  which failed outright before this fix. Temporary syslog tracing
  removed from `bezerk_parse()` once both bugs were confirmed fixed.
- **Size delta**: real hardware, `data` `29744` -> `29400` (-344
  bytes, from removing the temporary tracing); `text`/overlays
  likewise shrank slightly for the same reason. The `(unsigned)` casts
  themselves are cost-free (a handful of instructions each).

### Index-line cache (`mailindx.c`) unconditionally allocated for 100 messages on the very first index redraw

Immediately after fixing the two bugs above, INBOX opened but drawing
the folder index crashed: `"Bug in Pine detected: \"Out of free
storage (8181 bytes)\""`. `adb`'s stack trace (`$c` on the core file)
pointed at `i_cache_size()` (`mailindx.c`) via `get_index_cache()` <-
`build_header_line()` <- `redraw_index_body()`.

- **Root cause**: `i_cache_size()`'s cache-growth increment,
  `#define IC_SIZE 100`, means the very first call (drawing line 1 of
  a brand new INBOX with as few as 2 messages) immediately allocates
  room for 100 cache lines: `fs_get((icache.num+1) * (screen_cols+1))`
  = `fs_get(101 * 81)` = `fs_get(8181)` at an 80-column terminal --
  regardless of how many messages the folder actually has. On a
  target where `data`+`bss` alone already consume most of a single
  64KB address space (leaving roughly 20KB for heap+stack combined,
  per this file's `size pine` history), an unconditional 8KB-plus
  up-front allocation for index caching is the same class of problem
  as `MSIZE_INIT`/`pico/edef.h`'s `SLEN` scratch buffer -- oversized
  "just in case" headroom this target can't spare. The cache already
  grows on demand via `fs_resize()` in the `else` branch, so a smaller
  increment only changes resize frequency for a large folder, not
  correctness.
- **Fix**: `IC_SIZE` 100 -> 20 (first allocation for an 80-column
  terminal drops from 8181 bytes to 1701). Also fixed two `memset()`
  calls in the same function (`memset(icache.cache,0,(icache.num+1)*
  tmplen)` and the resize-tail one) to cast their `long`-typed size
  expression to `(unsigned)` -- same unprototyped-`memset()` bug class
  as the `bezerk.c`/`mail.c`/`rfc822.c` entry above; these two call
  sites happened to be silently zeroing 0 bytes (a no-op, since the
  high word of a value that fits in 16 bits is 0) rather than crashing,
  so newly-grown cache slots weren't guaranteed zero-filled before
  this fix.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: index now draws successfully for the
  2-message INBOX from the entry above.
- **Size delta**: not separately measured (bundled into the next
  real-hardware `size pine` alongside other changes); the allocation
  itself now starts at 1701 bytes instead of 8181 for a small folder,
  growing by 1620-byte (20-line) increments instead of 8100-byte
  (100-line) ones as a folder grows past that.
- **Next issue found**: viewing a second message after the first
  crashed with `"Out of free storage (1024 bytes)"` from
  `pretty_addr_string()` (`reply.c`, called while formatting the
  message's header for display) -- `adb`'s trace (`main ->
  mail_view_screen -> format_message -> format_envelope ->
  pretty_addr_string -> fs_get`) shows a plain allocation failure, not
  an obviously-corrupted size (1024 is exactly `MAILTMPLEN`, this
  function's minimum buffer size, and `pretty_addr_string()` already
  frees each address's buffer before moving to the next). With
  `data`+`bss` at 43492 bytes and a 64KB D-space ceiling, only ~22KB
  remains for heap and stack combined -- this looks like genuine
  cumulative memory pressure from c-client's normal per-message
  envelope/body caching (kept alive for the stream's lifetime by
  design) rather than a new corruption bug, but not yet confirmed --
  see `TODO-pdp11.md`.

### Shrink `context.c`'s and `ttyout.c`'s string literal footprint (`pine-ovl` data+bss)

Resumed the string-cutting pass with the two next-biggest un-cut files
by real (comment-stripped) literal byte count. Wrote a small
comment-stripping script this time (the earlier naive "grep every
quoted substring" measure, used throughout this pass, counts text
inside `/* ... */` comments as if it were a real literal, which had
been overstating several files' remaining potential -- e.g. `status.c`
and part of `ttyout.c`'s and `context.c`'s totals turned out to be
comment prose, not code).

- **`context.c`**: `context_digest()` had three separate
  `return("Context too long")` sites -- an old single-pass `cc`
  doesn't dedupe identical literals even within one function (same
  finding as the `mailcmd.c`/`strings.c` entries above), so each cost
  the full 17 bytes. Introduced one `static char *too_long = "Context
  too long";` and made all three sites `return(too_long);`.
- **`ttyout.c`**: real content here is almost entirely termcap/
  terminfo capability names (`"smcup"`, `"cuu1"`, `"sgr0"`, ...) --
  mandatory, fixed-by-convention 2-5 byte identifiers passed to
  `tgetstr()`/`tigetstr()`, not shortenable or dedupeable without
  breaking terminal setup. The one real duplicate: the `#else`
  (non-terminfo) branch of the terminal-setup function called
  `getenv("TERM")` twice in three lines (once to check for NULL, again
  to actually use the value) -- cached the first call's result in a
  local `ttnm` instead of a second call, removing the second `"TERM"`
  literal and a redundant environment lookup. (A third `"TERM"` exists
  in the `#ifdef USE_TERMINFO` branch, but that and the `#else`
  branch are mutually exclusive at compile time -- only one is ever
  in the built binary, so it isn't a real duplicate.)
- **Not touched, flagged for a possible future, separate pass**: every
  `.c` file in this tree (not just these two) carries an identical-
  shaped `static char rcsid[] = "$Id: <file>,v ... $";` under `#if
  !defined(lint) && !defined(DOS)`, averaging ~50 bytes -- a real
  string literal, referenced by nothing at runtime except the classic
  RCS `ident`/`what(1)` utilities for identifying a binary's source
  version in the field. Deliberately left alone here (removing it is
  a tree-wide, ~25-file mechanical change, a different shape of task
  than "cut this file's literals," and changes a long-standing
  convention rather than fixing a bug or an oversized buffer) --
  worth a dedicated pass if the ~1-1.5KB of accumulated savings across
  the whole tree is wanted.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: `data` `29400` -> `29364` (-36 bytes, matching
  hand estimates: -32 for the two removed `"Context too long"`
  copies, -4 for the removed `"TERM"` copy); `text`/overlays
  unchanged (string cuts only affect `.data`).
- **Size delta**: -36 bytes data (real 2.11BSD hardware).

### Tree-wide removal of every `.c` file's unused RCS `$Id$` identifier string

Per the previous entry's "flagged for a possible future pass" note --
the user asked for it. Every `.c` file under `pine/`, `pico/`, and
`pine/osdep/` carries an identical-shaped block:
```c
#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: <file>,v <rev> <date> mikes Exp $";
#endif
```
Confirmed via `grep -rn '\brcsid\b'` across every `.c`/`.h` in both
trees that the variable is referenced nowhere else (no about-screen,
no `-version` flag, nothing) -- it exists solely for the classic RCS
`ident`/`what(1)` utilities to extract a source-version string from a
built binary, a nicety with no user on a personal, offline PDP-11 mail
station. `imap/ANSI/c-client/*.c` does *not* use this pattern (a
different upstream convention -- Mark Crispin's c-client), so it was
untouched.

- **Fix**: a small Python script matched the exact 3-line block
  (`#if !defined(lint) && !defined(DOS)` / `static char rcsid[] =
  "$Id: ...$";` / `#endif`, tolerating the `#if`/tab whitespace
  variants actually present) and removed it, once per file, across
  every `pine/*.c`, `pico/*.c`, and `pine/osdep/*.c` file that had one
  -- 41 files total (`pine/pine-use.c` and `pine/osdep/includer.c`
  included for tree-wide consistency, even though neither links into
  the final `pine`/`pico` binaries -- standalone tool and a build-time
  code generator, respectively). `pico/osdep.c` also matched (it's a
  build-time `cp` of `pico/os_unix.c`, gitignored per `.gitignore`) --
  harmless to have edited directly since the next build overwrites it
  from the now-fixed `os_unix.c`.
- **Verified**: `./build clean && ./build lnx`, exit 0, 0 warnings;
  full smoketest suite 31/31. Real hardware: pushed all 20 changed
  `pine/*.c` files and all 18 changed `pico/*.c` files, rebuilt
  `pico-ovl` (first real-hardware pico link this session -- note
  `pico/makefile.bsd`'s plain `pico` target still fails with `ld:
  text overflow` on real 2.11BSD, a pre-existing, already-documented-
  in-makefile issue unrelated to this change; `pico-ovl` is the
  correct target, same as `pine-ovl` for pine) and `pine-ovl` --
  both linked cleanly with unchanged overlay/module structure.
- **Size delta**: this dev (`lnx`) build: `bin/pine` data `10184` ->
  `8968` (-1216 bytes), `bin/pico` data `6356` -> `5240` (-1116
  bytes). Real 2.11BSD hardware: `pine` data `29364` -> `28292`
  (-1072 bytes, `text`/`bss`/overlay structure unchanged, as expected
  for a pure `.data` cut spread across 20 files averaging ~54 bytes
  each). `pico`'s real-hardware delta wasn't separately measured
  (no prior real-hardware `size pico` baseline existed before this
  session touched it) but the dev-build delta (-1116 bytes) confirms
  the same win applies there.

### Cross-file string literal deduplication, found via `nm --size-sort` ground-truth inspection instead of source grep

Every string-cutting entry so far found candidates by grepping one
file's own source for repeated literals. That method structurally
cannot see duplication *across* files -- and this toolchain's linker
never merges identical string constants across separate `.o` files
either (each `.c` file is its own translation unit; there's no
COMDAT-style folding on this old `a.out` toolchain), so a message
string repeated verbatim in three different `.c` files costs three
full copies in the final binary, invisible to any one file's own
literal audit.

Found these instead by inspecting the *linked* binary directly:
`nm --size-sort -S bin/pine` (the ELF `lnx` dev build) ranks every
symbol by size, giving quick ground-truth visibility into the biggest
`.data`/`.bss` consumers (confirmed, as a sanity check, that this
surfaces the already-tracked `KEYMENU_S`/`struct key` duplication
issue -- `index_keys`/`view_keys`/`folder_keys`/`main_keys` and the
help-key tables all show up near the top, exactly matching that
already-deferred TODO item). Cross-file *string* duplication doesn't
show up this way, though (gcc already merges identical `.rodata`
constants within one binary before `nm` ever sees it) -- for that,
wrote a small script extracting every string literal (comment-
stripped, same technique as the `context.c`/`ttyout.c` entry) from
*every* `pine/*.c` file at once and grouping by exact text across
files, rather than per-file.

- **Found and fixed** (same "define once, `extern` elsewhere" pattern
  already established for `already_first_page`/`cancelled_msg`/
  `word_not_found_msg`):
    - `"\007\"%s\" undefined here (%s for help)"` (39 bytes) --
      identical in `mailcmd.c`, `mailview.c`, and `pine.c`. New shared
      `undefined_here_msg`, owned by `mailcmd.c`.
    - `"Word to search for [%s] : "` (27 bytes) -- `mailcmd.c` and
      `mailview.c`. New shared `word_search_prompt`, owned by
      `mailcmd.c`.
    - `"richtext"` (8 bytes), `"external-body"` (13 bytes), `"
      [ERROR fetching text of message]"` (37 bytes), and `"%s%s
      [Formatting error: %s]%s"` (33 bytes) -- all four duplicated
      between `mailview.c` and `reply.c` (these two files' message-
      body-formatting functions are close cousins, which is why so
      much overlapped). `richtext` already existed as a `static`
      variable in `mailview.c`; made it non-`static` and pointed
      `reply.c` at it via `extern`. Added three new shared variables
      (`external_body`, `err_fetching_text_msg`,
      `formatting_error_fmt`) owned by `mailview.c`.
- **Not touched**: `"To: "`/`"Cc: "` (`mailview.c`/`reply.c`, via
  `pretty_addr_string()` calls) -- only 4 bytes each, and `mailview.c`
  has three *other* field-name literals (`"From: "`, `"Reply to:
  "`, `"Bcc: "`) in the same call group that aren't duplicated
  anywhere, so sharing just these two would be an inconsistent,
  low-value special case. `"TERM"` (`pine.c`/`ttyout.c`) and the
  `KEYMENU_S` table duplication: left alone, see `TODO-pdp11.md`.
- **New warning class hit and fixed**: two of the four newly-shared
  variables (`word_search_prompt`, `formatting_error_fmt`) are used as
  the *format* argument to `sprintf()` -- unlike `q_status_message()`/
  `q_status_message2()` (this tree's usual message-string sink, which
  isn't `printf`-attribute-annotated, so passing it a non-literal
  format never triggers anything), `sprintf()` is a real, GCC-
  recognized `printf`-family function, so a variable (non-literal)
  format argument trips `-Wformat=2`'s `-Wformat-nonliteral` -- a
  real, if slightly surprising, new warning class from this exact
  refactor. Fixed with the same `#pragma GCC diagnostic push` /
  `ignored "-Wformat-nonliteral"` / `pop` bracketing already
  established in `ttyout.c`'s `PutLineN()` family for the identical
  reason.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31.
- **Size delta**: counterintuitively, this *dev* build's `data`
  went **up** (`8968` -> `9000`, +32 bytes): every new shared
  variable is a `char *` pointer, 8 bytes on this 64-bit ELF build,
  while the deduplicated string *content* itself lives in `.rodata`
  (reported under `text`, which dropped `272842` -> `272706`, -136
  bytes) -- so the dev build's `data` number is actively misleading
  for this specific kind of change. On real 2.11BSD hardware, where a
  `char *` is 2 bytes, not 8, the arithmetic flips heavily favorable:
  confirmed `data` `28292` -> `28112` (**-180 bytes**), matching a
  hand estimate (five new 2-byte pointers, six removed duplicate
  string copies) to within a few bytes.

### `longjmp botch` sending mail: SIGWINCH handler and its `longjmp()` target weren't guaranteed resident under `pine-ovl`'s auto-overlay text

Real-hardware report: reading mail now worked (see the INBOX-open
entries above), but sending hit `"sent-mail" doesn't exist. Create?`
-> answering `y` immediately produced `longjmp botch` followed by a
hung CPU (the literal, infinite-loop `botch()` routine in old BSD's
`setjmp`/`longjmp` on a stack-frame-relationship sanity-check
failure) -- reachable on the very first real Send attempt, not an
occasional flake.

- **Root cause**: `pine-ovl` links with real 2.11BSD `ld -i -X`
  (auto-overlay text). `mkovmake` partitions object files into a
  resident BASE plus swapped OVERLAY segments purely by *statically
  visible calls* between modules -- it has no way to see a signal
  handler registered with `signal()` (a raw function pointer the
  *kernel*, not any of this program's own code, invokes later,
  asynchronously) or a `longjmp()` target recorded by a `setjmp()` in
  another file. `pine/signals.c`'s `winch_signal()` (SIGWINCH handler)
  called `longjmp(winch_state, 1)` back into a `setjmp(winch_state)`
  in `pine/ttyin.c`'s `read_with_timeout()`, guarded by a
  `ready_for_winch` flag meant to only allow the jump while genuinely
  blocked there. Neither `signals.o` nor `ttyin.o` was pinned to the
  base (`pine/makefile.bsd`'s `pine-ovl` recipe passed `$(obj)` to
  `mkovmake -O2 8k` with no `-Z` at all, leaving *every* module,
  including these two, to the optimizer's free choice). Confirmed via
  the real `mkovmake`/`ld` output that `ttyin.o` landed in a
  `send.o`/`strings.o`/`ttyin.o` overlay, while the actual "create
  sent-mail" code path for a non-ambiguous folder name
  (`mail_create()` -> `bezerk_create()`) lives in `../c-client/
  bezerk.o`'s own, different overlay -- so a `longjmp()` into
  `ttyin.o`'s recorded frame while `bezerk.o`'s overlay is the one
  actually resident jumps into whatever instruction bytes that
  overlay happens to have loaded at that address instead. Same
  underlying hazard as this file's `gf_terminal()`/`gf_out_putc` entry
  above (an explicit function pointer instead of a signal handler);
  the `ready_for_winch` staleness angle was investigated at length
  first (never conclusively ruled in or out as *the* trigger) before
  the overlay angle -- prompted by the user directly asking whether
  signal handlers and `longjmp` targets need to live outside the
  overlays -- turned out to explain it far more simply: it doesn't
  need to be a narrow race at all if the target overlay is routinely
  not the one resident when Fcc creation runs.
- **Fix**, two parts:
    1. Eliminated the `longjmp()`-out-of-signal-handler pattern
       entirely instead of trying to keep both `signals.o` and
       `ttyin.o` simultaneously resident. `winch_signal()` now only
       sets the existing `winch_occured` flag; `read_with_timeout()`
       already had a fast-path check for that flag, and now also
       checks it on the `EINTR` a caught signal already produces from
       its blocking `select()`/`read()`, returning `KEY_RESIZE`
       through the same ordinary return-value path it always used for
       the non-blocked case -- no more raw-PC jump, so no more
       requirement that `ttyin.o` be resident at signal-delivery time.
       Removed `jmp_buf winch_state` and `ready_for_winch` entirely
       (both now dead).
    2. Forced `signals.o` into `mkovmake`'s base
       (`pine/makefile.bsd`'s `pine-ovl`: `signals.o -Z $(ovlobj)
       ...`, new `ovlobj` = `$(obj)` minus `signals.o`) -- this file
       is nothing but signal-handler plumbing (`auger_in_signal`,
       `quit_signal`, `hup_signal`, `term_signal`, `usr2_signal`,
       `winch_signal`), every one of which has the identical
       "kernel can invoke this while any overlay is resident" hazard,
       not just `winch_signal`. This is the same *kind* of fix the
       `gf_terminal()` entry above tried and abandoned for `filter.o`
       (`ld: too big for type 431`) -- but `signals.c` is roughly a
       fifth the size (~380 lines vs. ~1750) and, unlike `filter.o`,
       doesn't need to pull anything else along with it into the
       base: everything it calls afterward (`want_to()`,
       `mail_close()`, etc.) is an ordinary statically-visible call,
       already handled correctly by the normal overlay trampoline
       mechanism regardless of which overlay the callee ends up in.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31 (including `test_signal_handlers` and the composer-resize
  tests, which already exercised `winch_signal()`/`SIGWINCH`
  indirectly); a standalone manual check resizing the terminal while
  genuinely blocked at the main index screen (not the composer),
  confirming the new `EINTR`+flag path still redraws promptly instead
  of merely surviving. On real hardware: `pine-ovl` linked
  successfully with `signals.o` confirmed first in the base module
  list (`/bin/ld -i -X ... /lib/crt0.o signals.o addrutil.o args.o
  help.o log.o mailbox.o ../c-client/dummy.o ../c-client/misc.o -Z
  ...`), no budget-overflow error. Brute-force SIGWINCH flooding
  against the flat `lnx` dev build (which has no overlays at all)
  could *not* reproduce the original crash -- expected in hindsight,
  since the whole bug depends on overlay swapping that dev build
  doesn't have, and a useful confirming data point once the real root
  cause was found. End-to-end confirmation came from the user retrying
  a real Send on hardware: the `longjmp botch` is gone (a message now
  gets far enough to hit a separate, unrelated `data`+`bss` capacity
  issue instead -- see `TODO-pdp11.md`).
- **Size delta**: real hardware `data` `28112` -> `28110` (-2 bytes,
  removing the one initialized `int ready_for_winch = 0;`; `jmp_buf
  winch_state` had no initializer so was already in `bss`, not
  `data`). Not a size-reduction change; noted for completeness only.

### Deduplicated the common navigation-command strings shared across every screen's key-menu table

Real hardware hit `Bug in Pine detected: "Out of free storage"` while
just navigating between messages -- prompted going back to the
data+bss reduction pass. `TODO-pdp11.md`'s `KEYMENU_S`/`struct key`
entry had already identified these eight tables
(`index_keys`/`folder_keys`/`help_keys`/`composer_help_keys`/
`view_keys`/`text_att_view_keys`/`main_keys`/`oe_keys`, across
`mailindx.c`/`folder.c`/`mailview.c`/`pine.c`/`ttyin.c`) as a large
source of duplication, but deferred it as requiring a bigger
restructure (from static compile-time `struct key` array-of-*values*
literals to runtime-populated tables) -- real, and still not
attempted. This entry only dedupes the *string content* inside the
existing array-of-values format, which turned out to be separable
from that bigger refactor and far lower risk.

- **Found**: extracted every string literal from all eight tables at
  once (same cross-file grouping technique as the earlier
  `mailcmd.c`/`mailview.c`/`pine.c`/`reply.c` entry) and counted
  duplicates. The `KEYMENU_S`/`struct key` comment in `pine.h` already
  explains why: `column` is the only genuinely per-instance field, so
  the same navigation commands (`Help`, `OTHER CMDS`, `WhereIs`,
  `PrevPage`/`NextPage`, `Quit`, `GotoFldr`, `Compose`, ...) that
  appear on nearly every screen were each typed out as a fresh literal
  in every table that used them -- up to 12 copies of `"Help"` alone.
  52 distinct strings were duplicated somewhere; the top 24 by
  `(count-1) * (length+1)` accounted for 569 of the 686 total
  duplicated bytes, so only those were worth the churn (the rest are
  1-2 byte single-letter command keys where a shared-variable's
  maintenance cost isn't worth the return).
- **Fix**: same "define once, reference elsewhere" pattern as every
  other dedup this session, with one real wrinkle. The 24 shared
  strings are defined once in `mailindx.c` (chosen as owner since it
  uses all 24) and referenced from the other four files via `extern`.
  But `index_keys[]` etc. are `static` arrays with *static storage
  duration* -- their initializers must be compile-time constant
  expressions, so `char *km_help = "Help";` used directly in
  `{km_help, ...}` doesn't work (`error: initializer element is not
  constant`, since reading a `char *` variable's *value* is a runtime
  operation, unlike a string literal or the address of an array/
  function). Fixed by defining each shared string as `char
  km_help[] = "Help";` (an array, not a pointer) instead --  an array
  name decays to its address, which *is* a valid constant expression,
  same as a string literal itself. `extern char km_help[];` in the
  four consuming files. Mechanical, scripted substitution (Python) was
  used to apply this across all eight tables rather than 200+ manual
  edits; every substitution was scoped to just the `static struct key
  NAME[] = { ... };` block text so no unrelated same-text string
  elsewhere in these files (status messages, etc.) was touched.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31; a manual check of the rendered key menus (main menu and
  folder index) via a scripted pty session confirmed the labels
  (`Help`, `Main Menu`, `PrevMsg`, `PrevPage`, `Delete`, `Reply`,
  `OTHER CMDS`, ...) still render correctly, not just that the binary
  builds. Pushed to real hardware and rebuilt via `pine-ovl` cleanly.
- **Size delta**: real hardware `data` `28110` -> `27550`
  (**-560 bytes**), close to the 569-byte estimate from the
  `(count-1)*(length+1)` accounting (the small gap is alignment/other
  minor effects, not investigated further). Dev `lnx` build: `text`
  `272642` -> `272170` (-472) but `data` `9000` -> `9224` (+224) --
  this build's `char foo[] = "...";` arrays land in writable `.data`
  rather than the `.rodata` a plain string-literal pointer would have
  used (already merged across all uses by gcc within this one
  binary), so the dev build actually gets *worse* by this specific
  metric even though real hardware improves -- the same
  data-segment-numbers-are-misleading-here caveat as the
  `mailcmd.c`/etc. entry above, for a different underlying reason.
  `TODO-pdp11.md`'s `KEYMENU_S` entry updated to note this is now
  partially addressed (content deduped; the array-of-values structural
  duplication itself is not).

### One more cross-file string dedup pass, this time over the whole `pine/*.c` tree at once

Re-ran the cross-file literal-duplication scan (same technique as the
two entries above), but this time over every `pine/*.c` file
simultaneously instead of the handful checked by hand before. Two
false positives worth noting for next time: the naive "extract every
double-quoted string" approach also matches `#pragma ... "-Wformat-
nonliteral"` and `#include "headers.h"`-style preprocessor directives,
which cost *zero* bytes (fully consumed by the preprocessor) -- had to
filter those out before trusting the "savings" numbers.

Of what was left, most candidates were under 10 bytes of real savings
and/or generic single words (`"name"`, `"size"`, `"folder"`) risky to
merge for the return. Did the handful with clear wins and no
ambiguity:
- `"name"` (`mailview.c` x3, `mailcmd.c` x1, all `strucmp(param->
  attribute, ...)` MIME-parameter-name checks) -> new shared
  `attr_name`, owned by `mailview.c`.
- `"inbox"` (`osdep/expnfldr` (assembled into `os.c`) x2, `init.c` x1)
  -> new shared `inbox_name`, owned by the `expnfldr` fragment (had to
  go here and not `pine/os.c` itself -- that file is gitignored/build-
  generated, a symlink to `osdep/os-bsd.c` or `osdep/os-lnx.c`
  depending on target, per this repo's "edit the source, not the
  copy" rule).
- `"TERM"` (`ttyout.c` x2, `pine.c` x2, all `getenv("TERM")`) -> new
  shared `term_envvar`, owned by `ttyout.c`. (`TODO-pdp11-done.md`'s
  earlier cross-file entry had explicitly left this one alone as not
  worth a dedicated pass by itself; picked it up now since the
  broader pass was already touching similar-sized strings anyway.)
- `"Save"` / `"Main Menu"` -- both already had a shared copy from the
  `KEYMENU_S` entry above (`km_save`/`km_mainmenu`, `mailindx.c`), just
  not referenced from every non-table use of the same text yet
  (`mailcmd.c`'s attachment-view prompt, `log.c`'s IMAP-status-message
  rewrite, `folder.c`'s runtime `folder_keys[MAIN_KEY].label`
  assignment). These are ordinary runtime uses (a plain assignment/
  function-argument), not static initializers, so the existing `char
  km_save[]`/`char km_mainmenu[]` arrays could be referenced directly
  with no new variable needed.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. Real hardware: `pine-ovl` rebuilt cleanly.
- **Size delta**: real hardware `data` `27550` -> `27496`
  (**-54 bytes**) -- smaller than the earlier two rounds, as expected
  given how much smaller and more scattered these remaining
  duplicates were. Diminishing returns from this specific technique at
  this point; the `pico/edef.h` `SLEN` audit and the still-open
  structural `KEYMENU_S` duplication are the larger leads left in
  `TODO-pdp11.md`.

### Shrink `pico/edef.h`'s `SLEN` (global scratch buffer `s`) from 8192 to 2048

Prompted by a real-hardware "Out of free storage" crash sending mail
(see `TODO-pdp11.md`) -- re-audited the global scratch buffer flagged
as an unaudited shrink candidate in the "Cross-overlay indirect
function pointer" entry above.

- **Root cause of the original 8192 being wrong**: the existing
  comment's "2x MAXPATHLEN" rationale was computed against *this dev
  build's* `MAXPATHLEN` (4096, from Linux's `<sys/param.h>` --
  `2*4096` is exactly 8192) rather than the real 2.11BSD target's,
  which a one-line probe compiled and run on real hardware confirmed
  is 256 -- a 16x-oversized number on the machine that actually
  matters, same class of dev-build-vs-real-target mismatch as the
  `MAXPATH`-sized `static char tmp[MAXPATH]`-style buffers found (and
  ruled out as false leads) while looking for other targets in this
  same session.
- **Audit**: traced every writer of `s` (`strcpy`/`sprintf`/`strcat`
  into it) across all 20 files that include `edef.h`. All of them are
  in `browse.c` (directory-browser path-joining), `os_unix.c`
  (`compresspath()`), `composer.c` (short field-help prompts), and
  `display.c` (`mlyesno()`/`wkeyhelp()` -- the latter already
  self-bounds against `sizeof(s)`, confirmed not a size driver).
  Every path-shaped input feeding these (`dname`/`child`/`path`/
  `base`, all `char foo[NLINE]`, `pico/estruct.h`'s `NLINE` = 256 --
  unrelated to and much smaller than `MAXPATHLEN`) traces back to a
  256-byte cap, and `pfnexpand()`'s `~user` expansion explicitly
  rejects anything that would overflow its caller-supplied `len`
  rather than truncating silently. Worst traced case: `dname` +
  separator + `current->fname` (a `readdir()` entry, so at most
  whatever the filesystem's own name-length limit is) lands around
  511-533 bytes, not thousands. The Ctrl-T Fcc-folder-picker that used
  to need much more headroom is already gone (see this file's earlier
  entry on it).
- **Fix**: `SLEN` 8192 -> 2048 (roughly 4x the traced worst case).
- **Bugs found and fixed along the way**, surfaced while trying to
  drive the file-browser through the actual worst-case path under
  AddressSanitizer to confirm the new size holds (per this repo's
  verification discipline for buffer-size changes) -- both real,
  both independent of `SLEN`/`s` itself:
    - **`mlreplyd()`'s insert-character loop read one byte before the
      line buffer on literally the first keystroke of any prompt**
      (`pico/display.c`, the `do { b[i+1] = b[i]; } while(i-- >= 0);`
      shift-right-to-insert loop). The `>=` re-tests the
      *pre-decrement* value of `i`, so when `i` reaches 0 (the normal
      case -- shifting the NUL terminator when appending) the loop
      runs one extra iteration reading/writing `b[-1]`. Silently
      overwritten by the real character write immediately after, so
      harmless *in practice* on a normal libc (whatever byte happens
      to sit before the buffer is read then instantly discarded), but
      a genuine out-of-bounds read confirmed via AddressSanitizer
      (`stack-buffer-underflow`, caught because `mlreplyd()`'s `fname`
      buffer in `insfile()` happened to sit at the very start of its
      stack frame in that build). Fixed by changing `>=` to `>`.
    - **`getfnames()` (`pico/os_unix.c`) sized its output buffer from
      the directory's own `stat()` `st_size`**, which is a filesystem
      block-accounting number with no reliable relationship to the
      total bytes of the entry names inside it (this only ever worked
      out on classic V7-style fixed-16-byte directory records, where
      `st_size` really was `num_entries*16` -- wrong on every
      filesystem where it isn't, which is effectively all of them
      today). Confirmed via AddressSanitizer: a single long-named
      entry in an otherwise-small directory (`tmpfs`, so a tiny
      `st_size`) heap-overflowed the undersized `malloc()`. Fixed by
      switching to two passes over the same open directory handle
      (`readdir()` once to sum real name lengths, `rewinddir()`, then
      the original copy pass) instead of trusting `st_size`.
- **Also found while getting `pico-ovl` to relink after all of the
  above**: `pico/makefile.bsd`'s `pico-ovl` target ran plain `mkovmake
  -O2` with no basesize hint, which packed nearly everything into the
  base (real hardware: base 49408 bytes against just 3 small
  overlays) and then failed to link (`ld: too big for type 431` --
  same unexplained internal `ld` limit as the `gf_terminal()`/
  `gf_out_putc` entry above hit for `pine`, not the documented
  `round8k(base)+round8k(overlay)<=64KB` ceiling, which isn't close to
  binding here). Fixed the same way as that entry's `pine/makefile.bsd`
  precedent: gave `mkovmake` an explicit basesize hint
  (`MKOVFLAGS= -O2 16k`), confirmed working (base 36160, overlays
  15872/15936/13696). Also added the `rm -f pico` step `pine-ovl` already
  had before its own `make -f mkov` -- without it, `make -f mkov` reported
  "'pico' is up to date" and silently skipped relinking even when the
  object files backing it had just changed, one full round-trip lost
  to this while verifying the fix above.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31. AddressSanitizer build (`./build lnx ASAN=1`) driving `bin/pico`
  directly (pexpect) through Ctrl-R (insert file) -> Ctrl-T (browse) ->
  entering a real on-disk directory with a ~200-byte name containing
  another long-named entry -- the exact `dname + fname` worst-case
  concatenation path -- with no ASan report, after both bugs above were
  fixed (both reproduced and were confirmed via ASan before their
  fixes). Real hardware: `pico-ovl` rebuilds cleanly end-to-end via the
  plain `make -f makefile.bsd pico-ovl` target.
- **Size delta**: dev `lnx` build: `bin/pico` `bss` `10528` -> `4384`
  (**-6144 bytes**, exactly matching `SLEN`'s 8192->2048 reduction,
  confirming no other side effects). Real hardware: `bss` `3600`
  after all of the above (no isolated pre-change real-hardware `pico`
  baseline was captured before this entry's fixes landed together,
  but the `SLEN` byte count itself is platform-independent -- `s` is a
  plain `char` array, not pointer-sized -- so the same 6144-byte
  reduction applies there too).

### The send/read/compose crash chain: a cross-overlay Send crash, a stack-collision Fcc-append crash, and the real shape of this target's memory ceiling

A single extended real-hardware testing session (send a message, read it
back, compose again, repeat) surfaced a chain of distinct, real bugs --
each one fixed and verified before the next became visible underneath
it -- and ended with a clear, evidence-based picture of why "Out of
free storage" keeps recurring on this target no matter how many
individual buffers get trimmed. Documenting the whole arc together
since the entries build on each other and the final architectural
finding reframes how to read every earlier one.

- **`gf_flush_net()`/`gf_out_putc()`: a second cross-overlay stale
  function pointer, crashing on every real Send.** The
  `gf_terminal()`/`gf_out_putc` fix earlier in this file (see "Cross-
  overlay indirect function pointer" above) turned out to fix only
  half of this hazard. `gf_out_putc()`/`gf_flush_net()` (`filter.c`)
  still reached their actual output sink through a second stored
  pointer, `gf_out_f` (a `soutr_t`, set by `gf_set_output()` from
  `send.c` to `unix_puts`) -- the exact same "raw pointer captured in
  one overlay, invoked from a different one after `gf_pipe()`'s filter
  chain has swapped several other overlays through the same memory"
  hazard, just one indirection further out. Confirmed via a real adb
  stack trace: `SIGSEGV`/`SIGILL` inside `gf_flush_net()`, called from
  `pine_rfc822_output_body()` -> `append_message2()` ->
  `call_sendmail()` -- i.e. on every real Send with a recipient, not
  an edge case. Fixed the same way as the earlier hazard, but without
  needing to relocate any code this time: grep confirmed `unix_puts`
  is the *only* function `pine_rfc822_output_body()`'s `f` parameter
  is ever called with (one non-recursive call site, in
  `append_message2()`), so `gf_out_putc()`/`gf_flush_net()` now call
  it directly by name -- an ordinary statically-visible cross-file
  call, which the auto-overlay linker's trampoline mechanism (unlike a
  raw runtime pointer) already handles correctly regardless of which
  overlay `unix_puts` ends up in. `unix_puts()` (`send.c`) made
  non-`static` for this; `gf_set_output()`'s signature dropped the now-
  unused `soutr_t f` parameter it used to just store and pass along.
  `pine_rfc822_output_body()`'s own internal `(*f)(...)` call sites
  (its multipart-boundary/cookie-line writes) were *not* touched --
  those execute from within `send.c` itself, calling a function also
  defined in `send.c`, so they're safe by construction (the calling
  code executing at all already proves that file's overlay is
  resident) in a way the filter.c-to-send.c crossing never was.
  Verified: `./build lnx` clean, 31/31 smoketests (one regression
  caught and fixed by the suite itself -- see below), real hardware
  Send confirmed working after this fix where it had reliably crashed
  before.
    - **Regression caught by the smoketest suite**: the first version
      of this fix dropped `gf_out_f`'s NULL case entirely, assuming
      it was always `unix_puts`. Wrong -- `call_mailer()`'s Fcc-only/
      no-real-recipient path calls `pine_rfc822_output()` with
      `f=NULL, s=NULL` on purpose (no real destination to write to,
      only the Fcc copy), and `test_send_body_header_overflow`
      immediately caught the resulting NULL-`FILE*` crash. Fixed by
      checking `gf_out_stream` (which is NULL in exactly the same
      case `gf_out_f` used to be) before calling `unix_puts()`,
      instead of trying to preserve the removed pointer's NULL state
      directly.
- **`bezerk_append()`: an 8192-byte stack buffer, causing an
  "Illegal instruction" crash appending the sent copy to `sent-mail`.**
  Once Sends stopped crashing in `gf_flush_net()`, the very next step
  (copying the sent message into `sent-mail` via c-client's own
  `mail_append()` -> `bezerk_append()`) crashed differently: `Illegal
  instruction (core dumped)`, adb pinning it inside
  `bezerk_append_putc()`. `bezerk_append()` (`bezerk.c`) declares
  `char buf[BUFLEN]` where `BUFLEN` was `8*MAILTMPLEN` (8192) --  a
  plain **stack**-allocated write-batching buffer (accumulate bytes,
  flush to `write(2)` when full), not heap. Combined with a second
  `lock[MAILTMPLEN]` (1024) in the same frame, that's over 9KB in one
  stack frame on a target where the whole D-space is 64KB and this
  call happens right after a Send has already pushed `data`+`bss`+heap
  close to their own ceiling -- exactly the kind of stack-vs-heap
  collision this repo's "deep, buffer-heavy call chain" precedent
  (see `pine_send()`'s `bodyfile_path`, made `static` for the same
  reason) already knew to watch for, just not yet audited in
  c-client's own driver code. Shrunk `BUFLEN` to `MAILTMPLEN/2` (512)
  -- it only controls how often `bezerk_append_putc()` flushes to
  `write()`, not correctness. Verified: `./build lnx`, 31/31
  smoketests, real hardware Send-and-append confirmed working.
- **Three more instances of the "oversized MAILTMPLEN floor/initial-
  size" pattern**, found by continuing to retest after each fix
  exposed the next-smallest failing allocation (`"Out of free storage
  (N bytes)"` with `N` shrinking each time: 1024, 1025, 1024, 400,
  256, 257, 65...) -- each one a real, independently-confirmed
  real-hardware crash, each fixed the same low-risk way as this
  session's earlier `MSIZE_INIT`/`RBUFSZ`-style fixes (shrink an
  initial/floor size that an existing grow-on-demand or dynamic-sizing
  path already handles correctly regardless of size):
    - `mail_view_screen()`'s `so_get(CharStar, NULL, EDIT_ACCESS)`
      (`mailview.c`) -- the message-display buffer -- inherited
      `MSIZE_INIT`/`MSIZE_INC` (see the `filter.c` entry earlier in
      this file). Confirmed via adb (`so_get` -> `fs_get`) at exactly
      1024 bytes. Already covered by that entry's `MSIZE_INIT`
      shrink to 256 -- included here because it's the crash that
      first showed this specific call site was live.
    - `format_scroll_text()`'s `text_lines[]`/`line_lengths[]` arrays
      (`mailview.c`) started at `TYPICAL_BIG_MESSAGE_LINES` (200)
      slots and doubled (`fs_resize()`) on demand -- 200 `char*`
      slots cost exactly 400 bytes on this target's 2-byte pointers,
      matching a real `"Out of free storage (400 bytes)"` crash
      (adb: `mail_view_screen` -> `scrolltool` -> `format_scroll_text`
      -> `fs_get`) hit with more messages in the inbox than earlier
      sessions had. Shrunk to 25 -- a short message needs far fewer
      slots, and doubling still gets a long one to the same eventual
      capacity either way, just via a couple more `fs_resize()` calls.
    - `pretty_addr_string()` (`reply.c`) already computed a correctly
      dynamic per-address size (`2 * (field lengths) + 32`, covering
      RFC822 quoting/escaping) but then applied
      `if(need < MAILTMPLEN) need = MAILTMPLEN;` on top of it,
      forcing every address in every viewed message's To/Cc/From up
      to a full 1024 bytes regardless of actual size. Confirmed via
      adb (`mail_view_screen` -> `format_message` -> `format_envelope`
      -> `pretty_addr_string` -> `fs_get`, 1024 bytes). Removed the
      floor entirely -- the dynamic computation was already correct
      and was the whole point of the fix that introduced it (see that
      function's own comment, and the `rfc822.c` sibling below).
    - `rfc822_address_line()` (`imap/ANSI/c-client/rfc822.c`) --
      `pretty_addr_string()`'s header-*writing* counterpart, used
      building outgoing To/Cc/From lines -- had the exact same
      `if (tmpneed < MAILTMPLEN) tmpneed = MAILTMPLEN;` floor on top
      of an equally-correct dynamic computation. Found by grepping for
      the same `< MAILTMPLEN` floor pattern across the whole tree
      after finding `pretty_addr_string()`'s copy, not from a separate
      crash. Removed the same way.
    - `set_mime_types()`'s `RBUFSZ` (`send.c`, already `MAILTMPLEN+1`
      = 1025 per an earlier entry in this file) hit *two more* real
      crashes at that size and its next reduction (`"Out of free
      storage (1025 bytes)"`, then again at `"(257 bytes)"`, both adb-
      confirmed at the same `fs_get()` call site) during ordinary
      compose+Send, not specially large messages. Shrunk twice more,
      257 then 65 -- the refill loop already handles a chunk boundary
      landing anywhere including mid-line (see that function's own
      comment), so this remains the same "changes refill frequency,
      not correctness" tradeoff, just pushed further given how little
      real headroom this target has in practice.
- **The real driver behind the recurring crashes: c-client caches
  every viewed message's `ENVELOPE`/`BODY` for the life of the stream,
  and nothing in this tree ever called `mail_gc()` to free any of it.**
  Once the buffer-floor fixes above stopped being enough on their own
  -- the same session hit a fresh crash ("Out of free storage (256
  bytes)" starting a new compose, right after successfully reading
  several messages) at a call site with no oversized buffer of its
  own -- the pattern across the whole session's crash log (failing
  request sizes shrinking release over release, at *different* call
  sites each time, not a stack loop of the same one) pointed at
  cumulative growth from browsing, not any single buffer. Grepping
  this tree for `mail_gc`/`mail_free_envelope`/`mail_free_body` found
  they're called only for pine's own *outgoing* (composed) envelopes
  (`send.c`, `reply.c`) -- never for anything fetched while reading.
  Every message ever opened or even just summarized in the index
  keeps its full `ENVELOPE` (personal names, mailbox/host/adl strings,
  `ADDRESS` nodes for every recipient) and `BODY` alive in the
  stream's `LONGCACHE` for the rest of the session, unconditionally.
  Two call sites needed the fix, found in order:
    - `mail_view_screen()` (`mailview.c`): added
      `mail_gc(pine_state->mail_stream, GC_ENV)` right after each
      message is displayed (`so_give(&store)`), so at most one
      message's envelope/body is live at a time while reading.
      `mail_gc(..., GC_ENV)` is a whole-stream sweep (loops every
      `nmsgs`), but this call site only runs once per message actually
      opened for reading, not once per index row, so the O(nmsgs) cost
      per call is fine here.
    - `build_header_line()` (`mailindx.c`): the *other* place a full
      `ENVELOPE` gets fetched is populating the folder index itself --
      `mail_fetchstructure()` runs once per row (guarded by
      `get_index_cache()`'s own already-bounded cache, so only on a
      cache miss) purely to build one formatted summary line, and the
      line is all any caller ever needs again. Confirmed the remaining
      accumulation via adb: `"Out of free storage (12 bytes)"` -- a
      bare `ADDRESS` struct, about as small an allocation as this tree
      ever makes -- inside `mail_newaddr()`, deep inside
      `rfc822_parse_msg()`, while just populating the index for a
      bigger inbox than earlier sessions had used, i.e. before any
      message had even been individually opened. Whole-stream
      `mail_gc()` would be O(nmsgs) *per row* here (O(nmsgs^2) total)
      since this runs once per row, not once per session, so freed
      just this one message's entry directly instead, via
      `mail_lelt()` (the same fields `mail_gc()` itself frees:
      `mail_free_envelope(&lelt->env)`, `mail_free_body(&lelt->body)`).
    - Both confirmed not to break the visible index/message-view
      content (a scripted pty check after the `mailindx.c` fix
      confirmed From/Subject/Date still render correctly for every
      row; re-viewing an already-summarized-or-read message now costs
      a re-fetch/re-parse from the mailbox file instead of a cache
      hit, the expected memory-for-speed tradeoff). Verified: `./build
      lnx` clean, 31/31 smoketests both times, real hardware
      confirmed each fix let the *next* crash in the chain surface
      (concrete forward progress each time, not a no-op).
- **`fs_get()`/`fs_resize()`'s failure message now reports `brk`, an
  approximate stack pointer, and the gap between them**
  (`imap/ANSI/c-client/os_bsd.c`) -- a permanent diagnostic
  improvement, not a bug fix, kept because it directly answered the
  question the rest of this entry needed answered. Originally added
  as temporary `syslog()` tracing (a `MEMCK` macro at a handful of
  guessed checkpoints in `pine.c`/`mailcmd.c`/`send.c`, later a
  log-every-call version in `fs_get()`/`fs_resize()` themselves) --
  both approaches worked but cost real bytes on the very target being
  debugged and needed guessing where to look. Landed on the simplest
  version: fold `brk(2)`'s current value, the address of a local
  variable (an approximation of the current stack pointer -- this
  target has no portable way to read the real SP register from C),
  and their difference directly into the existing `fatal()` message
  that already fires on every allocation failure, no separate log
  needed. `msg[]` widened from 64 to 96 bytes to fit the longer
  message (worst case ~88 bytes on this target's 32-bit
  `long`/`size_t`; the wider dev-build types were checked too and
  don't matter since this file only builds for the real target).
- **What the `brk`/`sp`/`gap` numbers actually revealed**: every "Out
  of free storage" crash chased in this entry showed `gap` (the
  measured distance from the current stack pointer to `brk`) in the
  *thousands* of bytes -- nowhere close to the failing request size --
  yet the allocation still failed, and always right around `brk`
  57344. Two independent real-hardware crashes hit that exact number.
  57344 = 7 * 8192, and 65536 - 57344 = 8192 exactly -- one full
  8192-byte page short of the 64KB D-space, matching this target's
  KT11 MMU page size precisely. The likely mechanism (not yet
  confirmed by reading the kernel's own exec/VM setup code, but
  consistent with every measurement so far and with
  `pdp/machparam.h`'s `SSIZE` = 20 * 64 = 1280-byte *initial* stack
  content, comfortably fitting inside one such page): stack growth
  happens in whole-page units via the same APR mechanism as everything
  else, so a full 8KB page gets mapped for stack at exec time
  regardless of how little of it the initial ~1280 bytes actually
  uses, and `brk()` can never claim any part of a page once it's
  mapped for stack -- a *static*, page-granularity reservation decided
  once at process start, not a dynamic boundary that tracks the
  stack's actual current depth. This is why the `sp`/`gap`
  instrumentation above didn't predict any of these failures: the real
  constraint isn't "how deep is the stack right now", it's "how many
  pages were handed to stack at exec time", and that's fixed for the
  process's whole lifetime. Checked for a `MAXDSIZ`/`MAXSSIZ`-style
  tunable constant in the kernel source (`limit`'s `datasize`/
  `stacksize` both show as the shell's unlimited default, not a
  compiled kernel maximum) -- not found by name; if this split is
  hardcoded in the kernel's own exec/VM image setup rather than a
  tunable, moving it would mean real kernel VM code changes, not a
  `#define`, and hasn't been attempted. **Practical upshot for future
  work**: 57344 bytes is very likely a hard ceiling for `data+bss
  +heap` combined on this target, not a soft one that more `sp`-aware
  code can negotiate around -- the only lever available from pine's
  own source is minimizing how much of that fixed 57344-byte budget
  gets used before it's needed, which is exactly what this session's
  `MSIZE_INIT`/`RBUFSZ`/floor-removal/`mail_gc()` fixes (and the
  ongoing data+rodata reduction work elsewhere in this file) already
  do -- there's no fix available that raises the ceiling itself from
  pine's side.
- **Found, not yet fixed**: `folder_exists()` (`pine/folder.c`, used
  by `send.c`'s Fcc-exists check) and `mail_create()`'s internal
  `mail_valid()` check (`imap/ANSI/c-client/mail.c`) disagree about
  whether a folder exists -- hit live when answering "y" to `"sent-
  mail" doesn't exist. Create?` produced `Can't create ... folder
  already exists` immediately after. `folder_exists()` answers via
  `context_find_all()` (a pattern-matching folder *search*, built for
  browsing/completion UI), while `mail_create()` answers via
  `mail_valid()` (a direct existence check) -- plausible that
  `context_find_all()` doesn't correctly match a plain, non-wildcard
  name when called with a NULL context (`send.c`'s exact call shape),
  but not traced further this session. Logged in `TODO-pdp11.md`
  rather than chased immediately -- real and worth fixing, but a
  distinct bug from the memory-pressure chain above, with a trivial
  workaround (don't confirm the create when it's wrong) that didn't
  block continuing to test the crashes that mattered more.

### Five more `context.c` functions still had `MAILTMPLEN` stack locals the earlier resize missed, plus a real aliasing bug found fixing them

Resuming the general data/rodata reduction pass (broadened past
reacting to specific crashes), grepping for `MAILTMPLEN`-sized buffers
tree-wide turned up `context_create()`/`context_open()`/
`context_rename()`/`context_delete()`/`context_append()`
(`pine/context.c`) -- five siblings of `context_fqn_to_an()`/
`context_digest()`/`context_find_all()`/`context_apply()`, already
resized from `MAILTMPLEN` to `MAXPATH` in an earlier entry in this
file, with the *exact* same "build FQN from ambiguous name" comment
and code shape, that the earlier pass simply didn't reach. All five
hold the same folder/context-path content the earlier fix's contract
already established as `MAXPATH`-bounded.

- **Fix**: resized all five to `MAXPATH` (256, from `MAILTMPLEN`
  1024). `context_append()` in particular sits directly in the Fcc-
  append call chain this session's crash-chasing entry already found
  tight on stack (`bezerk_append()`'s 8192-byte buffer, above).
- **Found and fixed while resizing**: the `else if` branch in each of
  these functions did a raw, unbounded `strcpy()` of caller-supplied
  content (`mailbox`/`name`/`old`/`new` -- all "no length cap
  anywhere upstream" per this session's established pattern) into the
  local buffer, with no bounds check at all -- reachable at the old
  1024-byte size too, just needing a longer trigger string, so this
  isn't new exposure from the resize, but shrinking the buffer without
  also bounding the write would have tightened the trigger length.
  Replaced every such `strcpy()` with a `strncpy()` capped to the
  buffer's remaining space plus explicit NUL-termination.
  `context_create()`'s two `sprintf()` calls that copy caller content
  into a same-sized `errbuf`/`tmp` got explicit `%.219s`-style
  precision for the same reason.
- **Found and fixed while resizing, a genuine pre-existing bug**:
  `context_rename()`'s second branch (building the fully-qualified
  *new* name) wrote into `tmp` -- the *first* branch's buffer --
  instead of the `tmp2` it was clearly meant to use (declared for
  exactly this, named to match, just never referenced). If both `old`
  and `new` took that branch, `s` and `t` ended up aliased to the same
  buffer, and computing the new name would silently overwrite the
  already-computed old name before `mail_rename()` ever ran. Fixed by
  writing into `tmp2` as intended.
- **Found and left alone, flagged for later**: `context_apply()`
  itself (called by all six of these functions, both the four fixed
  earlier and the five fixed here) writes into its caller-supplied
  buffer via an unbounded `sprintf()`/`strcpy()` with no size
  parameter at all -- every caller passing a `MAXPATH`-sized local
  (after this and the earlier entry) means a `n` (ambiguous name)
  longer than that overflows it. Not fixed here: closing it properly
  means adding a size parameter to `context_apply()` and updating
  every call site, a bigger, more invasive change than this pass's
  per-caller resizes, and out of scope for what was otherwise a
  data-segment reduction pass. Logged in `TODO-pdp11.md`.
- Also found and removed while re-reading `context_open()`: a
  `char host[MAILTMPLEN]` local that's declared and never referenced
  anywhere in the function -- dead code, not merely oversized.
- **Verified**: `./build lnx`, 0 warnings; full smoketest suite
  31/31, including the Fcc-overflow-specific tests
  (`test_fcc_netmbx_overflow`/`test_fcc_tilde_overflow`) that exercise
  this exact `context_isambig()`/`context_apply()` chain. Pushed and
  rebuilt on real hardware.
- **Size delta**: these five buffers are all plain stack locals (not
  `static`), so their resize doesn't show up in `size`'s `data`/`bss`
  columns on either build -- the real effect is reduced stack depth
  in the Fcc-append call chain (and every other caller of these
  functions), the same class of fix as `bezerk_append()`'s `BUFLEN`
  above, not a `size`-measurable one.

### `ttyout.c`'s already-audited `temp_format` tightened from `MAILTMPLEN` to its real measured worst case

`PutLine1()`..`PutLine5()`'s shared `temp_format` buffer already had a
thorough worst-case audit from an earlier session (every call site
checked, real worst case ~570 bytes from `update_titlebar_message()`'s
format string), but was sized to `MAILTMPLEN` (1024) anyway "matching
this codebase's existing convention" rather than the audited number.
Continuing the general data+rodata reduction pass, tightened to 640 --
still ~70 bytes of margin over the existing audit, which this change
doesn't revisit or invalidate. Unlike the stack-local fixes elsewhere
in this file, `temp_format` is `static` (BSS), so every byte here is a
permanent, unconditional cost for the process's whole lifetime, not a
conditional one.

- **Verified**: `./build lnx`, 0 warnings; full smoketest suite 31/31.
  Pushed and rebuilt on real hardware.
- **Size delta**: real hardware `bss` (or `data`, depending on how
  this target's linker classifies an uninitialized `static` array)
  dropped by exactly 384 bytes (1024-640) alongside this entry's
  `context.c` changes (which are stack-only and don't show up in
  `size` at all) -- confirmed via a real-hardware rebuild showing the
  full 384-byte drop with no other `size`-visible change in the same
  build.
