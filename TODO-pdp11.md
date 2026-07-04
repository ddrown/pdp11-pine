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

### 2. Strip vestigial NNTP/news UI (drivers already unlinked, so this is dead code)
- [ ] Remove `CNTXT_NEWS` context type and news-collection branches in
      `pine/folder.c`.
- [ ] Remove `h_news` and other news-only help text from `pine/helptext.c`.
- [ ] Grep pine/*.c for other `news`/`nntp` conditionals tied to the dead driver.

### 3. Cut down inline help system
- [ ] `pine/helptext.c` (3833 lines / ~204KB source) — replace full-screen
      context help with short one-line prompts, or drop context help
      entirely in favor of doc/pine.1.
- [ ] Remove/trim `pine/helpindx.c`, `pine/help.c` accordingly.

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
