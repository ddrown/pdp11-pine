# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Remove dead c-client stream options (mail_debug/OP_HALFOPEN/OP_ANONYMOUS)

- [ ] `imap/ANSI/c-client/mail.c`: `mail_debug()`/`mail_nodebug()`
      (~lines 1110-1130) have zero callers anywhere in the built tree.
      `OP_DEBUG`/`stream->debug` (set ~line 390) is never read by
      bezerk.c/dummy.c/misc.c/osdep.o -- its only would-be reader is
      the never-built smtp.c. `OP_HALFOPEN`/`stream->halfopen` (set
      ~line 395) is never read anywhere in the built tree. `OP_ANONYMOUS`/
      `stream->anonymous` (set ~line 392) is only ever read in
      `pine/imap.c` -- which isn't compiled at all, see the next item.
      Confirm via grep across just the linked object set (mail/bezerk/
      dummy/rfc822/misc/osdep + pine + pico, not the whole c-client
      tree which still has unbuilt smtp.c/imap4.c etc.), then delete the
      dead fields/functions and their setters.

### Delete `pine/imap.c` -- stale unbuilt duplicate source

- [ ] `pine/imap.c` (~544 lines) is not in `pine/makefile.lnx`'s `obj=`
      list (that builds `mailbox.o`, not `imap.o`) and nothing
      `#include`s it -- confirmed zero effect on `bin/pine`. It's a
      leftover pre-split duplicate; `pine/mailbox.c`'s own header
      comment says "extracted from imap.c". Still defines duplicate
      copies of `context_mailbox`/`context_bboard`/`mm_fatal`/etc. that
      could mislead a future grep into thinking they're live or that
      edits there matter. Zero binary-size win (it's not linked), but
      worth deleting as pure repo cruft -- confirm nothing references
      it (build included) before removing.

### Tiny: dead `#ifdef LATER` block in `mailbox.c`'s `context_bboard()`

- [ ] `pine/mailbox.c`'s `context_bboard()` (~lines 90-120) has an
      `#ifdef LATER ... #endif` block (~lines 97-114) rebuilding a
      bulletin-board's unread-count prefix via `context_open()`.
      `LATER` is never `#define`d anywhere in the tree, so this block
      is unreachable dead code. The function itself stays live (it's
      called via `mm_bboard()` from `imap/ANSI/c-client/dummy.c`'s
      local-folder-name pattern matching, per the "mm_bboard pulled
      back out as a false positive" writeup in `TODO-pdp11-done.md`) --
      only the `#ifdef LATER` block itself is the cut.

