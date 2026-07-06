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

### 4. Drop filter/scoring rule engine
- [ ] Remove `pine/filter.c` (~43KB) pattern-matching filter/score language
      and its config vars/menu entries.

### 5. Restrict to US-ASCII (drop multi-charset MIME conversion)
- [ ] Find and remove non-ASCII charset conversion tables (ISO-8859-*, KOI8,
      etc.) — likely in strings.c / mailview.c MIME text handling.
- [ ] Remove `V_CHAR_SET` config option, hardcode US-ASCII.

### 6. Drop color + mouse support
- [ ] Remove ANSI color config screens and color escape-sequence logic in
      `pine/screen.c` / `pine/ttyout.c`.
- [ ] Remove xterm mouse tracking support.

### 7. Drop external editor + spell-check hooks
- [ ] Remove `F_ENABLE_ALT_ED` / alternate editor exec logic; force built-in
      pico as the only composer.
- [ ] Remove `spell()` ispell integration.

### 8. Drop printer support
- [ ] Remove `pine/print.c` (~11KB): `open_printer`, `select_printer`,
      personal print command config.

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
