# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

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

### 9. Drop `phile` driver
- [ ] Remove `phile.o` from c-client build once address book/news are gone
      (lets arbitrary Unix files be opened as folders — narrow use case).

## Buffer right-sizing (not feature cuts, but required regardless)
These fixed global buffers alone can blow a PDP-11 data segment:
- [ ] `tmp_20k_buf` (20KB) — `pine/folder.c`
- [ ] `s` (16KB)
- [ ] `temp_format` (2.75KB)
Shrink these to sizes appropriate for realistic PDP-11 line/terminal widths
(e.g. 80 columns) instead of modern-terminal assumptions.

## After each cut
- [ ] Rebuild on Linux, confirm `size pine/pine` text+data+bss shrinks.
- [ ] Re-run `nm -S --size-sort` to check for newly-dead large symbols.

## Build process issues
- [ ] `pine/osdep/os-lnx.c` is a generated file (assembled by
      `pine/osdep/includer` from `os-lnx.ic` + fragment files, per the
      rule at `pine/makefile.lnx:140-149`), but the fixes needed to
      make it build with a modern glibc were committed directly into
      the generated `os-lnx.c` instead of into the fragments/template
      it's generated from. A `./build clean && ./build lnx` regenerates
      `os-lnx.c` from source and silently reintroduces the bugs,
      clobbering the checked-in fixes (this happened this session;
      reverted `os-lnx.c` by hand rather than fix it properly).
      The generation inputs that actually need the fixes (confirmed by
      diffing a from-scratch regeneration against the committed,
      working `os-lnx.c`):
        - `pine/osdep/os-lnx.ic:117` includes `fgetpos.non` (a
          non-ANSI ftell/fseek-based emulation of fgetpos/fsetpos,
          whose fseek/ftell-as-long assumption doesn't hold for
          glibc's opaque `fpos_t`) — should include the plain
          `fgetpos` fragment instead, which just calls the real ANSI
          `fgetpos()`/`fsetpos()` that glibc actually provides.
        - `pine/osdep/err_desc` references the removed-from-glibc
          `sys_errlist[]` array directly — should call `strerror()`
          instead (this fragment is `include()`'d by most/all `os-*.ic`
          platform manifests, not just Linux's, so check it doesn't
          regress a real non-glibc target).
        - `pine/osdep/tempnam` and `pine/osdep/writ_dir` test
          `st_mode & S_IFDIR`/`(st_mode & S_IFMT) == S_IFDIR` directly
          instead of using the portable `S_ISDIR()` macro, and
          `tempnam` declares a `u_int` (BSD-ism, not always defined)
          instead of `unsigned int`.
      Fix these four generation inputs, then regenerate `os-lnx.c` from
      scratch (`cd pine/osdep && rm -f os-lnx.c && cd .. && make -f
      makefile.lnx osdep/os-lnx.c`) and confirm the regenerated file
      now matches (or improves on) the currently-committed one, so a
      clean rebuild is no longer destructive.
- [x] Hit the same class of bug in `pico/osdep.h`/`osdep.c`, which
      `pico/makefile.lnx:41-47` generates as a plain `cp` of
      `pico/os_unix.h`/`os_unix.c` (no fragment assembly, unlike
      `os-lnx.c`) — but those two files are gitignored, so the
      previously-working copies (evidently hand-patched at some point,
      same as `os-lnx.c`) had no git history and were unrecoverable
      once `./build clean` deleted them this session. Unlike the
      `os-lnx.c` case, this one *is* fixed at the true source now:
      `pico/os_unix.h` had an unguarded `extern int kbseq();` that
      conflicted with `os_unix.c`'s `#ifdef ANSI` prototype
      `int kbseq(int *);` (now wrapped in the same `#ifdef ANSI` as the
      rest of the file), and an `extern char *getcwd(char *, int);`
      that conflicts with glibc's own `getcwd(char *, size_t)` on Linux
      (now excluded via `&& !defined(LINUX)`, matching the `-Dbsd
      -DLINUX` flags `makefile.lnx` builds pico with). A clean
      `./build clean && ./build lnx` now regenerates both `os-lnx.c`
      (still needs the fixes above, since that one's fixes live in the
      untracked generated file) and `pico/osdep.[ch]` (now genuinely
      fixed at the source) and produces the same binary sizes as
      before.
