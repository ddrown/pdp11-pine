# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### Remove dead `OP_READONLY` bit-check in `mail_open()`

- [ ] `imap/ANSI/c-client/mail.c`'s `mail_open()` still has
      `stream->readonly = (options & OP_READONLY) ? T : NIL;` --
      spotted while removing its `OP_DEBUG`/`OP_ANONYMOUS`/`OP_HALFOPEN`
      neighbors (see `TODO-pdp11-done.md`). Confirmed via grep that no
      caller anywhere in the built tree ever passes `OP_READONLY` in a
      `mail_open()`/`bezerk_open()` `options` argument (its only setter
      was the `-o` flag's block in `mailcmd.c`, already removed per the
      "Remove read-only mode" writeup), so this line always evaluates
      to `stream->readonly = NIL` -- a dead initializer, not a dead
      field: `stream->readonly` itself stays genuinely live via its
      other independent setters (`bezerk_open()`'s own lock-failure
      detection, `signals.c`'s crash cleanup), so don't touch the field
      or `READONLY_FOLDER` (`pine/pine.h`) -- only the `OP_READONLY`
      `#define` (`mail.h`) and this one dead-initializer line are the
      cut. Double check the line still reads exactly this way (options
      parameter name may differ) before touching it.

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

