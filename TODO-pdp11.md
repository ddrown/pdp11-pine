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

- [ ] Found while string-cutting `mailindx.c` (see `TODO-pdp11-done.md`).
      Every screen (`mailindx.c`'s index, `folder.c`'s folder list,
      `mailview.c`'s message view, `pine.c`'s main menu, etc.) declares
      its own `static struct key foo_keys[] = {{"?","Help",0}, ...}`
      table, and the same single-letter keys and command labels
      (`"C"`/`"Compose"`, `"D"`/`"Delete"`, `"GotoFldr"`, `"OTHER
      CMDS"`, `"Quit"`, ...) are repeated verbatim across most of
      them, since the same command is reachable from multiple screens.
      This is a real, fairly large chunk of duplicated `.data` (dozens
      of short strings across half a dozen files) -- but every one of
      these tables is a compile-time static aggregate initializer, and
      C requires constant expressions there, so none of it can be
      pulled into a shared `char *` variable the way this session's
      other string-sharing wins were. Doing anything about it would
      mean restructuring these tables to populate at runtime (e.g. an
      init function assigning each `.label` field from a shared
      constant, the same trick already used for `folder.c`'s
      `select_label`/`expand_label`, but applied wholesale across
      every key-menu table in the tree) -- a much bigger, more
      invasive change than this pass's one-string-at-a-time edits, and
      worth scoping as its own deliberate task rather than folding
      into the opportunistic string-cutting pass.

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

- [ ] `pine-ovl` (`pine/makefile.bsd`, see `TODO-pdp11-done.md`'s
      several entries on it) links against real 2.11BSD `ld`, which
      rejects the result if `round8k(base_text) + round8k(largest
      overlay) > 64KB` *or* `data + bss > 56KB` (see `ld.c`'s size
      check, reached via `-i`). Last real-hardware numbers (after the
      `gethostbyname()`/resolver removal and the `prettysz()`
      float-removal, both in `TODO-pdp11-done.md`): `text 41984`
      (base) against a largest overlay of `24576` -- `round8k(41984)`
      is `49152` (needs to drop under `40960`, i.e. base needs ~1024
      more bytes off, to reclaim a segment and clear this check);
      `data 46746 + bss 20542 = 67288`, over the 57344-byte ceiling by
      `9944` bytes. Leads found so far, not yet acted on:
    - `pico/edef.h:85`'s `char s[SLEN];` (`SLEN` = 8192, `pico/edef.h`)
      is a non-`static` global scratch buffer shared across many pico
      `.c` files -- confirmed (via `ld.c:1153`'s `bsize = add(bsize,
      csize, "bss")`) that its 8192 bytes only show up in the *final
      linked* binary's bss, invisible when `size`-ing any individual
      `.o`, which is why it took real-hardware `nm -n pine | grep '
      [bBcC] '`-style digging (worked around `nm`'s own out-of-memory
      failure on the full symbol table by running `nm *.o | grep ' C
      '` per object file instead) to find at all. Accounts for 8192 of
      the 9944-byte data+bss shortfall by itself. **Not a bug** --
      already went through a dedicated overflow audit in an earlier
      session (see `TODO-pdp11-done.md`'s "pico/edef.h's 16KB global
      scratch buffer" references) and `static` would be actively
      wrong here (would give every including file its own separate
      8KB copy instead of one shared one). Worth auditing whether
      `SLEN` is generous headroom vs. a tight fit -- would need finding
      the actual largest thing ever copied into `s` across every
      caller before touching the constant, same rigor as any other
      buffer-size change per this repo's verification discipline.
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

