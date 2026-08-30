# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

### `context_apply()` writes into its caller's buffer with no bounds check at all

- [ ] Found while resizing `context.c`'s sibling functions' stack
      buffers to `MAXPATH` (see `TODO-pdp11-done.md`). `context_apply
      (char *b, char *c, char *n)` writes an ambiguous name `n`
      (caller-supplied, no length cap anywhere upstream, same as every
      other address/name field this session found unbounded) into `b`
      via a raw `sprintf()`/`strcpy()`, with no size parameter and no
      check against `b`'s actual capacity. Every call site in the tree
      now passes a `MAXPATH` (256)-sized local (after this session's
      resizes) -- an `n` longer than that overflows it. Not fixed:
      closing this properly means adding a size parameter to
      `context_apply()` and updating every call site (six, all in
      `context.c`), more invasive than this session's per-caller
      resizes. Worth a dedicated pass.

### The real memory budget: `data+bss+heap` appears hard-capped at 57344 bytes, not the full 64KB

- [ ] See `TODO-pdp11-done.md`'s "The send/read/compose crash chain"
      entry for the full derivation. Two independent real-hardware
      "Out of free storage" crashes both failed at exactly `brk=57344`
      despite thousands of bytes of measured gap to the current stack
      pointer -- 57344 = 7*8192, and 65536-57344 is exactly one
      8192-byte page (this target's MMU page size), consistent with a
      *static*, page-granularity stack reservation decided once at
      exec time (not a dynamic boundary that tracks actual stack
      depth). Checked for a `MAXDSIZ`/`MAXSSIZ`-style tunable in the
      kernel source and didn't find one by name (`limit`'s
      `datasize`/`stacksize` both just show the shell's unlimited
      default); `pdp/machparam.h`'s `SSIZE` (20*64=1280 bytes, the
      *initial* stack content at exec) is consistent with this but
      doesn't by itself explain why a whole 8KB page gets reserved for
      it.
    - **Next steps if revisited**: read the kernel's own exec/VM image
      setup code (wherever it decides how many pages/APRs go to stack
      vs. data at process start) to confirm the mechanism and check
      whether the split is actually hardcoded or has an adjustable
      parameter. If it's genuinely fixed, 57344 bytes is the real
      ceiling to design `data+bss` and peak heap usage against -- not
      64KB minus whatever the stack happens to be using at the moment,
      which is what earlier size accounting in this file assumed.
    - **Update**: a still later session tried to directly confirm the
      exact page boundary by painting a known byte pattern down to
      `0xE010` (this ceiling's implied floor) and scanning for
      disturbance -- it crashed the instrumentation itself (`SIGSEGV`
      inside `stack_paint_init()`, confirmed via `adb`), read as
      "reachable only via genuine SP movement, not an arbitrary
      pointer" rather than "the boundary is wrong", but that reading
      was never independently re-verified. See
      `TODO-pdp11-done.md`'s "Measuring real stack depth directly"
      entry -- real stack usage during an ordinary Send measured
      several KB (not the ~1.2k this entry and others had assumed),
      which motivated finding and fixing two permanent bss buffers
      that only needed transient lifetime, freeing 1664 bytes.
    - Until/unless the ceiling itself moves, the only lever available
      from pine's own source is minimizing how much of that fixed
      57344-byte budget gets used -- i.e. exactly the ongoing data+
      rodata reduction work in this file and `TODO-pdp11-done.md`.
    - **Update**: a still later session found the single biggest
      consumer of this budget in real use -- `bezerk_open()` used to
      cache every message's full raw text permanently, bounding usable
      mailbox size against this ceiling far more tightly than any
      amount of buffer-shrinking elsewhere in this file could fix on
      its own. Fixed -- see `TODO-pdp11-done.md`'s "Rewrote bezerk.c's
      message cache to stop permanently storing raw message text"
      entry.
    - **Update**: real-hardware retest after that fix, and after
      splitting `bezerk.c` in two to get the build linking again (see
      `TODO-pdp11-done.md`'s corresponding entries) -- reading and
      sending mail both worked end to end. But the `brk()` checkpoint
      log shows the whole session sitting at exactly `brk=57344` (this
      ceiling, precisely) from `compose_mail() entry` all the way
      through `pine_send()`/`call_mailer()`/`compose_mail() exit` and
      into the next `build_header_line()` calls -- i.e. folder-open
      alone, for whatever mailbox size this test used, already
      consumed 100% of the budget above `data+bss`, with zero bytes of
      headroom left for anything compose/send did afterward. The
      per-mailbox-*byte* scaling bug is gone, but the *fixed*
      per-message floor (one `FILECACHE` record, one `MESSAGECACHE`
      elt, plus whatever `LONGCACHE` envelope/body gets fetched per
      viewed message) still fully saturates the roughly 17KB of heap
      headroom available above real hardware's measured `data+bss`
      (28482+11682=40164; 57344-40164=17180) for a perfectly ordinary
      mailbox. This didn't crash this time, but there is currently
      *no* margin at all -- next candidate investigation: work out
      exactly how many messages were open and what the actual
      per-message fixed cost is now, to see whether the `~17KB` floor
      itself has further headroom to find (e.g. in `MESSAGECACHE`'s
      own per-message fields, or in when/whether `LONGCACHE` entries
      get freed) rather than only in `bezerk.c`.
    - **Update**: exactly this margin ran out for real at 10 messages
      -- `addr_string()`'s unconditional `MAX_ADDR_EXPN` (1000-byte)
      allocation floor turned out to be a real, fixed contributor
      (every From address in the index forced to at least 1001 bytes
      regardless of its actual length) and is fixed -- see
      `TODO-pdp11-done.md`'s "`addr_string()`'s `MAX_ADDR_EXPN` floor"
      entry. Not a full fix for this budget item, just one confirmed
      piece of it -- the folder-open path was already shown pinned at
      the hard ceiling *before* this specific allocation even ran, so
      there's very likely more of the same shape (unconditional floors,
      permanent per-message allocations) still to find along
      `do_broach_folder()`/`build_header_line()`'s path.
    - **Update**: a follow-on session found and fixed three more
      contributors along exactly that path, each caught live on real
      hardware in turn as the previous fix let the same test sequence
      get one step further: `folder_exists()` reporting a real
      `sent-mail` folder as nonexistent (unrelated to memory, but
      blocked testing further), `sort_current_folder()` leaking every
      message's `LONGCACHE` envelope/body via unfreed
      `mail_fetchstructure()` calls in its own comparators, and
      `i_cache_size()` unconditionally jumping to a full 20-entry
      chunk on the index cache's first allocation. See
      `TODO-pdp11-done.md`'s "`folder_exists()` resolves a bare folder
      name against the wrong directory", "`sort_current_folder()`'s
      comparators leak every message's `LONGCACHE` envelope/body", and
      "`i_cache_size()`'s index-cache growth" entries.
    - **Update**: the `i_cache_size()` fix above (size the first
      allocation exactly instead of jumping to a 20-entry chunk) still
      crashed one row later, growing from 1 to 2 entries -- the
      *resize* path had the identical bug the first-allocation fix
      only addressed for `icache.cache == NULL`. Fixed the same way;
      see the same entry above.
    - **Update**: with 11 messages readable end to end, the next
      real-hardware crash moved to opening the *sent-mail* folder
      after composing and sending: `bezerk_parse()`'s original (pre-
      rewrite) two-loop structure still `memcpy()`'d each message's
      raw bytes out to a second, transient scratch copy before
      compacting it -- i.e. every message's bytes were resident
      *twice* at once during parse, on top of the earlier fix's
      "only cache the small permanent record" win. Fixed by merging
      the two loops so each message is compacted and flushed to the
      on-disk cache directly against its span in the read buffer, with
      no second copy ever made. A related zero-body-message crash in
      `bezerk_readcache()` (unrelated `NULL` dereference, left over
      from the original cache rewrite) surfaced during this fix's own
      verification and was fixed alongside it. See
      `TODO-pdp11-done.md`'s "`bezerk_parse()`'s two-loop message
      parse duplicated every message's bytes" entry.
    - **Update**: eliminating that duplicate copy still wasn't enough
      by itself -- the *read-ahead* size (`CHUNK`, 8192 bytes) let
      `bezerk_parse()` read an entire mailbox in one gulp whenever it
      was smaller than that, holding everything simultaneously before
      a single message could be flushed and freed. Shrunk to 1024
      bytes (matching `MAILTMPLEN`) so a message gets flushed as soon
      as its boundary is found instead of after the whole file is
      read. See the same entry above.
    - **Update**: with sent-mail opening cleanly too, the next crash
      moved to the index cache again -- but this time at *exactly* the
      size `i_cache_size()`'s own fix says it should ask for (1863
      bytes for row 22), with a nominally-larger "gap" reported than
      what was requested. This was the first clear real-hardware
      evidence that, once `brk` reaches this ceiling (57344, the same
      value every recent crash has hit), it does not grow further --
      *every* allocation from that point on must come from memory
      already freed earlier in the session, so a request's chances
      depend on finding an existing free chunk of that exact size, not
      on how much free memory exists in aggregate. `i_cache_size()`'s
      single ever-growing array needed an ever-larger *contiguous*
      block on every resize, which is exactly what a fragmented, maxed-
      out heap struggles to produce. Fixed by replacing it with a
      fixed-size ring buffer living in `.bss` (never allocated from
      the heap at all) -- see `TODO-pdp11-done.md`'s "`i_cache_size()`
      replaced with a fixed-size `.bss` ring buffer" entry.
    - **Update**: with all four fixes above in place, a real-hardware
      session read an 11-message INBOX, composed and sent mail, then
      opened and read all 27 accumulated sent-mail messages end to
      end with no crash (`stack high-water mark: depth=4984 bytes
      below main()'s start`, clean exit). This is the first fully
      clean run of this entire test sequence since the investigation
      began. Not closing this entry yet -- a larger mailbox or a
      different usage pattern may still find the next fixed-size
      contributor, following the same shape every fix above has had
      so far -- but this is a real, substantial milestone.

### `do_suspend()`'s IMAP-connection-timeout warning may be dead code on this fork

- [ ] Found while string-cutting `signals.c` (see `TODO-pdp11-done.md`).
      `do_suspend()` (`signals.c`) guards two blocks -- a "your IMAP
      connection will be closed" warning printed before suspending,
      and a "connection broken" status message checked after resuming
      -- behind `if(curf[0] == '{')`, i.e. only when the current
      folder's name looks like a remote IMAP mailbox spec
      (`{host}INBOX`-style). This target's `c-client` only links the
      local `bezerk`/`mbox`/`phile`/`dummy`/`sm_unix` drivers (no
      `imap2`, see `TODO-pdp11-done.md`'s driver-exclusion note), which
      suggests `cur_folder` can likely never actually start with `{`
      on this fork -- making both blocks dead code. Not confirmed:
      would need to trace whether the folder-opening path (`mailcmd.c`
      around `strcpy(ps_global->cur_folder, new_folder)`) rejects a
      `{`-prefixed name before ever reaching this assignment, or lets
      it through and only fails later at `mail_open()`. Left alone for
      now (just shortened the two message strings in place) -- a real
      "confirm unreachable, then delete" investigation, not a string
      edit, and out of scope for the string-cutting pass that found
      it.

### `Ctrl-\` (SIGQUIT) triggers a broken space-printing redraw loop

- [ ] Found while diagnosing the `read_with_timeout()` `select()`
      aliasing bug (`ttyin.c`, see `TODO-pdp11-done.md`) on real
      hardware: `Raw()` (`ttyin.c`) disables `Ctrl-C` at the tty-driver
      level by setting `_raw_tchars.t_intrc = -1` (so pine can handle
      it itself as a plain character), but never does the equivalent
      `_raw_tchars.t_quitc = -1` for `Ctrl-\` -- so `Ctrl-\` still
      raises a real `SIGQUIT`, which lands in whatever `signals.c`
      does for it and produces a loop that repeatedly prints spaces
      over the screen instead of a sane default (pine exiting
      cleanly, or ignoring it like `Ctrl-C`). Not yet investigated --
      lower priority than the keyboard-input blocker it was found
      alongside, since `Ctrl-\` isn't a key pine's own UI ever asks
      the user to press.

### KEYMENU_S/`struct key` tables duplicate command-key strings across every screen

- [x] **Partially addressed** -- see `TODO-pdp11-done.md`'s "Deduplicated
      the common navigation-command strings..." entry (real hardware
      `data` -560 bytes). The premise below (that a compile-time
      static aggregate initializer can't reference a shared string
      without restructuring to runtime population) turned out to be
      wrong for the *content*: `char km_help[] = "Help";` (an array,
      not a `char *`) decays to a valid constant-address expression
      just like a string literal does, so `{km_help, ...}` works fine
      as a static initializer. Used that to share the 24
      highest-value duplicated strings (of 52 found) across all eight
      tables without touching their structure at all.
- [ ] What's *not* addressed, and still open: the `struct key`
      array-of-*values* format itself. Each table is still its own
      full `{char *, char *, short}` triple repeated per entry per
      screen -- deduping the string content doesn't shrink the
      per-entry `column` field or the fact that e.g. `{"D", 0}`-style
      identical (key, label) *pairs* still occupy separate struct
      slots in each table that uses them, just with shared string
      pointers now. Actually eliminating that would mean restructuring
      to runtime-populated tables (e.g. an init function assigning
      each `.label`/`.name` from a shared constant into a table built
      at startup, the same trick already used for `folder.c`'s
      `select_label`/`expand_label`) -- a much bigger, more invasive
      change than the content-level dedup above, and still worth
      scoping as its own deliberate task rather than folding into the
      opportunistic string-cutting pass. The remaining 28
      lower-value duplicated strings (mostly 1-2 byte single-letter
      command keys) were also left alone -- not worth a shared
      variable's maintenance cost for the return.
    - **Investigated further, recommend NOT pursuing**: a later
      session scoped the "array of pointers to shared `struct key`
      instances" approach (predefine each common `{key,label,column}`
      triple once, e.g. `{km_qmark,km_help,0}`/`{km_o,km_othercmds,0}`/
      `{NULL,NULL,0}`, and have each table's array hold pointers to
      the shared instances instead of separate copies) as a lighter-
      weight alternative to full runtime population. Real duplication
      is substantial (`{km_qmark,km_help,0}` alone appears ~11 times
      across the 8 tables, `{NULL,NULL,0}` dozens of times), but
      turned out unsafe: runtime mutation of individual table slots is
      widespread, not an isolated case -- `pine.c`'s
      `show_main_screen()` does `km->keys[DEFAULT_KEY].label = label;`
      (and `DEFAULT_KEY` in `main_keys` is currently one of the
      `{NULL,NULL,0}` slots, exactly the most-shared candidate),
      `folder.c` rewrites its `MAIN_KEY`/`SELECT_KEY` slots, `ttyin.c`
      overwrites a run of slots in `oe_keymenu`, and `screen.c:236`
      does `km->keys[off].column = km->keys[off+6].column = 0;` on a
      *generic* `key_menu *km` with a caller-supplied `off` -- meaning
      it can mutate an arbitrary slot of *any* table depending on call
      site, making "is this slot ever mutated" essentially unprovable
      without tracing every caller. Sharing an instance across tables
      is only safe for slots no mutator can ever reach, and the
      likely-safe set (mostly `{NULL,NULL,0}` filler) heavily overlaps
      with exactly the slots these mutators use as scratch space --
      the real payoff shrinks a lot once those are excluded. Not
      pursued further; if revisited, would need a full trace of every
      `->keys[...]` write site (the four found above, and any others)
      before touching structure, not just a content-level count of
      duplicate triples.

### "Out of free storage (1001 bytes)" crash during Fcc append, on real hardware

- [ ] Found while retesting the `strlen()`-vs-`so->used` Fcc
      corruption fix (see `TODO-pdp11-done.md`) on real hardware: a
      Send of a short, ordinary message hit `Bug in Pine detected:
      "Out of free storage (1001 bytes)"` (c-client's `fs_get()`,
      already hardened in an earlier session to report the real
      requested size on failure -- see `TODO-pdp11-done.md`, so this
      is very likely a genuine allocation failure, not a
      garbage/corrupted size) followed by an `IOT trap (core dumped)`,
      right around `context_append()`'s Fcc write ("Message sent and
      NOT copied to \"\"" was reported first). The message's own
      `STORE_S` buffer was confirmed completely clean via a full dump
      right before the crash -- this looks like a distinct issue from
      the corruption bug, most likely a real, tight memory budget
      during the Fcc/`context_append()` path on this constrained
      target (`size pine` currently shows `data` alone at ~29-31KB
      before any dynamic heap use) rather than a corrupted-size bug.
      Not yet reproduced a second time or investigated further --
      `ps`/`limit` on the real hardware showed no other process or
      leftover state holding memory at the time. Next steps: retry the
      same Send a few times to establish whether it's reliably
      reproducible or transient/load-dependent; if reproducible, trace
      what `context_append()`/`mail_append()`/the `bezerk` driver
      allocates around a 1001-byte request during a folder append.
    - **Update**: a later session found and fixed a real bug in
      exactly this vicinity -- `bezerk_extend()` (`bezerk.c`, the
      mailbox-growing routine used by Fcc/append) called
      `memset()`/`write()` with a `long`-typed size argument against
      this target's unprototyped (empty-parens K&R) `memset()`/
      `write()` declarations, the same bug class documented in
      `TODO-pdp11-done.md`'s "INBOX wouldn't open" entry. That specific
      call site is now fixed (cast to `(unsigned)`).
    - **Update 2**: a still later session (see `TODO-pdp11-done.md`'s
      "send/read/compose crash chain" entry) fixed several more real
      bugs in this same neighborhood -- `bezerk_append()`'s own 8192-
      byte *stack* buffer (a different crash, "Illegal instruction",
      not this entry's "Out of free storage"), plus the unbounded
      c-client envelope/body cache growth that turned out to be the
      actual driver behind most of this session's "Out of free
      storage" crashes generally. Never specifically re-confirmed at
      this exact 1001-byte figure/call site, but everything found
      since points at the same underlying "very little real headroom,
      and the deep Fcc-append call chain is a real stack cost too"
      story -- worth a fresh retest before assuming this one specific
      report is still open on its own.
    - **Update 3**: a still later session directly measured real stack
      depth through this exact call chain for the first time (see
      `TODO-pdp11-done.md`'s "Measuring real stack depth directly"
      entry -- real peak usage turned out to be several KB, ~5x the
      informal "~1.2k" estimate this and other entries had been
      working from) and found/fixed two more permanent bss buffers
      (`ttyout.c`'s `temp_format`, `filter.c`'s `gf_out_buf`) that only
      ever needed transient, single-call lifetime, freeing 1664 bytes
      from the `data+bss+heap` budget. Real hardware went from
      repeatedly hitting "Out of free storage"/"Can't resize free
      storage" during Fcc/`sent-mail` creation to completing a full
      send cleanly -- but only tested with one message so far, so
      still not confirmed fully closed rather than just improved.
    - **Update 4**: stress-tested with a fuller inbox (8 messages, all
      viewed, then a Send) on real hardware -- completed cleanly, no
      crash, stack high-water mark unchanged from the single-message
      test (4984 bytes). Meaningfully raises confidence this is a real
      fix rather than a narrowly-avoided one, though still not
      exhaustive -- a much longer individual message, or many more
      than 8, could still find the edge of the freed headroom.
    - **Update 5**: the crash recurred with a slightly bigger inbox
      (12 messages) -- and this time got fully root-caused rather than
      just improved. See `TODO-pdp11-done.md`'s "Root-caused the 'Out
      of free storage' crash chain" entry: the dominant driver is
      `bezerk_open()` permanently caching every message's full raw
      text at open time, scaling with total mailbox byte size. The
      1664-byte bss fix above genuinely helped (more headroom before
      hitting the wall) but was never going to fully close this -- the
      real ceiling is how big a mailbox this driver's caching model
      can hold at all.
    - **Update 6**: fixed -- see `TODO-pdp11-done.md`'s "Rewrote
      bezerk.c's message cache to stop permanently storing raw message
      text" entry. `bezerk_open()`/`bezerk_parse()` no longer keep any
      message's raw bytes in RAM after parsing; permanent per-message
      cost is now a small fixed-size record, independent of message or
      mailbox size, directly confirmed via a real-vs-old-code
      `mallinfo2()` comparison on the dev build (253056 bytes of
      growth for a 12-message/241910-byte mailbox before the fix, 15168
      bytes -- flat, unrelated to message size -- after).

### "Out of free storage (1024 bytes)" viewing a second message, real hardware

- [x] **Fixed** -- this was `pretty_addr_string()`'s
      `if(need < MAILTMPLEN) need = MAILTMPLEN;` floor forcing every
      address's buffer up to 1024 bytes regardless of the (already
      correctly computed) actual size needed. See `TODO-pdp11-done.md`'s
      "send/read/compose crash chain" entry for the fix and the
      broader investigation it was found alongside (the *real* driver
      of most crashes like this turned out to be c-client's unbounded
      per-message envelope/body caching, not this floor alone --
      fixed separately in the same entry).

### Audit the rest of the tree for unprototyped read/write/mem*/str* calls fed a `long`-typed size

- [ ] `TODO-pdp11-done.md`'s "INBOX wouldn't open" entry found a real,
      confirmed bug class on this target: `unistd.h`/`string.h`
      declare `read`/`write`/`lseek`/`memcpy`/`memmove`/`memset`/
      `memcmp`/`memccpy`/`memchr`/`strcpy`/`strncpy`/`strcat`/
      `strncat` etc. with empty-parens K&R-style prototypes (e.g.
      `ssize_t read();`), which do *no* argument-type checking --
      passing a `long`/`unsigned long`-typed size/count argument
      (rather than `int` or a bare `sizeof(...)`, both of which are
      already single-word-safe) silently pushes an extra word the
      real single-word-`unsigned` implementation never consumes,
      corrupting what the callee actually sees. Confirmed on real
      hardware to cause a `read()` requesting 559 bytes to return 0,
      and (separately) a `memcpy()` to copy the wrong number of bytes
      entirely. Fixed every occurrence *this session's real-hardware
      testing actually reached* (`bezerk.c`, `mail.c`, `rfc822.c`,
      `mailindx.c` -- see `TODO-pdp11-done.md` for the exact list) by
      casting the argument to `(unsigned)` at the call site, but this
      was reactive, not a tree-wide audit. A dedicated pass -- grep
      every `.c` file in `pine`/`pico`/`c-client` for calls to these
      functions, check whether the size/count argument's *declared
      type* is `long`/`unsigned long` (not the runtime value, which is
      usually small enough to look harmless), and cast/fix each one --
      would very likely turn up more of these, since this bug is
      silent (no compiler warning, since there's no prototype to
      conflict with) and was only found this time because it happened
      to reach three different, very visible crashes in a row.

### Close the remaining `pine-ovl` text/overlay and data+bss overage on real 2.11BSD hardware

- [x] **Resolved for now** -- both `pine-ovl` and `pico-ovl` link
      cleanly on real hardware as of this session (`pine`: base text
      `37888`, largest overlay `24320`, `data 27496`, `bss 13728`;
      `pico`: base text `36160`, largest overlay `15936`, `data 14806`,
      `bss 3600` -- see `TODO-pdp11-done.md`'s many entries this
      session, especially the cross-file string dedup passes and the
      `SLEN` shrink). The `round8k(base)+round8k(overlay)<=64KB`
      formula this entry used to cite doesn't actually explain what's
      observed (e.g. pine's current `round8k(37888)+round8k(24320) =
      40960+32768 = 73728`, over 64KB by the old formula, yet the link
      succeeds repeatedly) -- either the formula was always an
      approximation or this target's split I/D space allows more than
      a flat 64KB; not re-derived this session, flagging so a future
      session doesn't trust the old numbers at face value. Re-open
      this item if a future change makes either `-ovl` target fail to
      link again; the leads below are still real, general future
      targets, just no longer tied to an active build failure.
    - ~~`pico/edef.h:85`'s `char s[SLEN];`~~ -- shrunk from 8192 to
      2048 this session after a full audit of every writer, see
      `TODO-pdp11-done.md`'s "Shrink pico/edef.h's SLEN" entry (which
      also found and fixed two real bugs surfaced while verifying it:
      an `mlreplyd()` off-by-one out-of-bounds read, and a
      `getfnames()` heap-buffer-overflow from sizing its allocation off
      a directory's `st_size` instead of its actual entry-name bytes).
    - String literal volume in `pine`'s bigger `.c` files is real,
      live, user-facing text (error messages/prompts/status lines),
      not dead code like the `struct variable` `description` field
      was -- e.g. `mailcmd.c` alone has ~178 string literals totaling
      ~4254 bytes (measured by extracting every double-quoted literal
      from the source), which is why 2.11BSD's `data`-segment numbers
      for these files look so much bigger than the same files' `.data`
      on this repo's ELF/gcc `lnx` dev build (a.out has no separate
      read-only/`.rodata` section -- every string literal lives in
      writable `.data`). Cutting this further means real tradeoffs:
      shortening/consolidating message text (UX cost) or hunting for
      literally-duplicated strings across files to dedupe -- not yet
      attempted.
    - A leftover ~942 bytes of the data+bss shortfall (`9944` total
      minus `s`'s `8192`) is still unaccounted for -- `nm *.o | grep '
      C '` was run across the pulled `libc.a`/`libtermlib.a` members
      (`lib-nm.txt`) and found only tiny (2-4 byte) commons (`_errno`,
      `_ospeed`/`_BC`/`_UP`/`_PC`, `_freemem`, `__sigintr`); the same
      check hasn't yet been run across `pine`'s/`pico`'s/`c-client`'s
      *own* object files, which is the next place to look for it.
    - Replacing `doprnt`/`*printf` with a custom minimal formatter
      (no float support, only whatever specifiers this codebase
      actually uses) was scoped and set aside for now (see
      `TODO-pdp11-done.md`'s `prettysz()` entry) -- real measured
      numbers show the float-specific helpers alone
      (`atof`/`ecvt`/`fltpr`/`gcvt`/`ldexp`/`modf`, excluding `doprnt`
      itself) total 1534 bytes text, which would be enough on its own
      to clear the text/overlay budget (currently short by ~1024), but
      doing it means replacing the shared formatting engine behind
      every `sprintf`/`printf`/`fprintf` call in the whole tree --
      real, invasive, high-effort work, deliberately not started.

