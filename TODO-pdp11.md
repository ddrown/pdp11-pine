# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

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
      call site is now fixed (cast to `(unsigned)`), but this Fcc-path
      crash itself hasn't been re-tested since -- worth retrying a
      Send with an Fcc now to see if it's actually gone.

### "Out of free storage (1024 bytes)" viewing a second message, real hardware

- [ ] Found immediately after fixing the INBOX-open crashes (see
      `TODO-pdp11-done.md`): with a freshly-opened, real 2-message
      INBOX, reading the first message works, but going back to the
      index and opening the second hits `Bug in Pine detected: "Out of
      free storage (1024 bytes)"` (`IOT trap`). `adb`'s trace: `main ->
      mail_view_screen -> format_message -> format_envelope ->
      pretty_addr_string -> fs_get`. 1024 is exactly `MAILTMPLEN`,
      `pretty_addr_string()`'s (`reply.c`) minimum per-address buffer
      size, and that function already frees each buffer
      (`fs_give()`) before formatting the next address -- doesn't look
      like an obviously-corrupted/garbage size the way the `bezerk.c`
      bugs were. With real hardware `data`+`bss` at 43492 bytes inside
      a 64KB D-space, only ~22KB remains for heap+stack combined,
      which is tight enough that ordinary c-client per-message
      envelope/body caching (kept alive for the stream's lifetime by
      design, not a leak) could plausibly exhaust it after only two
      small test messages. Not yet confirmed either way. Next steps:
      reproduce again to see if it's message-count-dependent (does a
      3rd/4th message make it worse, or does it reproduce on message 2
      every time regardless of order); check whether anything from
      viewing message 1 is *not* being freed when the user navigates
      back to the index (a real per-view leak, vs. inherent
      structure-cache growth); and consider whether this is better
      framed as an instance of the already-tracked "Close the
      remaining pine-ovl text/overlay and data+bss overage" item below
      rather than a standalone bug.

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

### Track down a small memory leak found while re-verifying the third fuzzing lead's fix

- [ ] The third fuzzing lead (heap corruption in `rfc822_parse_adrlist()`
      -- see `TODO-pdp11-done.md`) is fixed. Re-running the fuzzers to
      confirm (22M+ execs on `adrlist_fuzz`, 14M+ on the full
      `rfc822_fuzz`, zero further crashes) turned up a separate, minor
      LeakSanitizer finding instead: a 2-byte `cpystr()` allocation not
      reachable from any of `env->from`/`sender`/`reply_to`/etc. by the
      time `mail_free_envelope()` runs, on input `From:<a;I<EYE(N)`.
      Confirmed via `gdb`-assisted tracing that the three "a" mailbox
      copies this input's `From:` header produces (the real one plus
      the auto-derived `sender`/`reply_to` copies from
      `rfc822_cpy_adr()`) are *not* the leaking allocation -- all three
      were confirmed freed by cross-referencing their exact addresses
      against every `free()` call in the same process run. The actual
      leaking 2-byte string's identity wasn't pinned down in the time
      available (multiple `malloc(2)`-sized strings exist for this
      input -- three "a"s plus a personal-name "I" -- and a
      `gdb`-Python attempt to auto-diff every `malloc(2)`/`free()`
      pair hit tooling friction rather than a real answer).
    - Not a security bug (no crash, no corruption, bounded to 2 bytes
      per malformed message, and pine is an interactive reader, not a
      long-running server accumulating leaks) -- much lower priority
      than the crash-class bugs found so far, left for a future
      session. `From:<a;I<EYE(N)` reproduces it deterministically and
      standalone (unlike the crash-class bugs), so it should be a much
      easier trace than those were -- likely just needs `gdb`'s
      `commands`/`finish` combination used carefully within a *single*
      process run rather than across separate invocations (addresses
      aren't stable between runs).

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

