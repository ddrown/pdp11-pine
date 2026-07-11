# PDP-11 / 2.11BSD size reduction TODO

Goal: shrink pine to fit a PDP-11 address space. Target environment: no
TCP/IP, purely local mbox mail, outgoing delivery via local sendmail
(sm_unix). Current x86-64 debug build is 1.7MB; stripped/optimized text+data
is still ~970KB, way over budget.

Completed cuts and fixes have their detailed write-ups split out into
`TODO-pdp11-done.md` (same repo directory) to keep this file focused on
what's still open; check there for history/rationale on anything marked
DONE elsewhere or referenced in passing below.

Buffer right-sizing (`tmp_20k_buf`, `s`, `temp_format` -- not feature
cuts, but required regardless) is now fully done; see
`TODO-pdp11-done.md`.

### Follow-up: buffer-overflow security audit (raised alongside the above)
- [ ] Both `tmp_20k_buf`'s old fixed-size behavior and `temp_format`'s
      current one share the same root issue: code that formats
      unbounded/attacker-influenced content (recipient addresses,
      subjects, filenames from a mailbox) into a buffer sized only by
      guess/precedent, with no bounds checking at the point of the write
      itself (`sprintf`/`strcat`/`rfc822_cat` don't know the buffer size).
      `tmp_20k_buf` is fixed for its one truly unbounded use
      (`pine_rfc822_header`) via a matching size estimate, but this
      class of bug should get a dedicated sweep: grep for other
      `sprintf`/`strcat`/`rfc822_cat`/`rfc822_address_line`-style calls
      into fixed local/global buffers (e.g. `char tmp[MAILTMPLEN]` in
      `send.c`'s per-MIME-part header writer, `bezerk.c`'s `LOCAL->buf`)
      and confirm each one's real worst-case input is actually bounded by
      its buffer size, rather than "has been big enough so far".
      Two more instances already found while auditing `tmp_20k_buf`
      itself (both described above, now fixed): `compose_mail()`'s
      postponed-message-resume read loop, and `display_parameters()`'s
      MIME-parameter formatting (remote-content-driven, the more
      concerning of the two). A third, more severe instance found while
      auditing `s` (also described above, now fixed): `pico/attach.c`'s
      `ParseAttach()` overflowing `SyncAttach()`'s fixed *stack* arrays
      via unchecked `strcpy()`s from an unbounded parse buffer — an
      actual stack smash confirmed under `valgrind`, reachable by
      ordinary composer typing/pasting, no crafted email needed.
      A fourth candidate, found while closing out `s`'s sizing (see
      `TODO-pdp11-done.md`) but not yet fixed: `pico/composer.c`'s
      Ctrl-T Fcc-folder-picker passes the shared `s` straight to
      `(*Pmaster->folders)()` (pine's `folders_for_fcc()`,
      `pine/folder.c`), which can in turn call `context_apply()`
      (`pine/context.c`) -- both assume their caller's buffer is at
      least `MAXPATHLEN` (+ a `MAILTMPLEN`-sized context pattern)
      bytes, matching this whole class of bug. Not confirmed
      reachable in practice (needs a locally-configured
      folder-collection context long enough to matter), so `s` was
      just sized with headroom over this combined worst case rather
      than fixing the root call chain -- fold into this sweep instead
      of re-opening the `s` item.
- [ ] `pico/display.c`'s `resize_pico()` (~line 1693) has a similar
      uninitialized-`pscreen`-cell gap to the one just fixed in
      `vtinit()` above, but on the terminal-resize path: new rows added
      via `malloc`, and the grown tail of existing rows' `CELL` arrays
      via `realloc`, are left uninitialized. `vscreen`'s equivalent gap
      is already handled there (an explicit `vteeol()` call fills the
      newly-grown column range, gated on `old_ncol < term.t_ncol`), but
      there's no matching call for `pscreen`. Not fixed alongside
      `vtinit()` because `resize_pico()` sets `v_flag = VFCHG` (not 0) on
      these rows, so a blanket `memset` would wipe that flag too — the
      correct fix needs to clear only the grown tail region (mirroring
      `vscreen`'s `vteeol()` approach) rather than the whole-block
      `memset` used in `vtinit()`, and touches resize/redraw logic that's
      riskier to change without a resize-path regression test.
      Note: `resize_pico()` was actually *dead code* on this Linux
      build until the `s`-sizing pass (see `TODO-pdp11-done.md`) fixed
      a missing `#include <sys/ioctl.h>` that had silently compiled out
      every `#ifdef TIOCGWINSZ` block (including both call sites of
      `resize_pico()` itself) -- so this uninitialized-memory gap is
      now genuinely reachable via a real terminal resize, not just a
      latent, unreachable bug.

### c-client: dead TCP/`server_login` code in `osdep.o`
- [ ] Investigated why `imap/ANSI/c-client/c-client.a` is 500KB+: mostly
      an illusion -- its makefile's `CFLAGS = -g -O` leaves every member
      unstripped, and `strip --strip-debug` alone takes it from 514KB to
      164KB with no code changes (same story for `bin/pine`'s on-disk
      size vs. its real `size`-reported text+data+bss). Worth remembering
      when eyeballing `ls -la` on any of these.
      Separately, a real (if small) dead-code pocket was found in
      `osdep.o` (built from `imap/ANSI/c-client/os_lnx.c`, the Linux
      stand-in for whatever os-dependent file eventually targets
      2.11BSD): checked every symbol `osdep.o` defines against what the
      other seven linked `.o` files (`mail bezerk mbox dummy rfc822 misc
      sm_unix`) reference, then cross-checked the remainder against all
      of `pine/`, `pico/`, and `imap/` for any caller outside c-client.
      Now that `smtp.o`/`phile.o` are gone (see `TODO-pdp11-done.md`),
      the entire TCP client stack in `osdep.o` is unreachable -- its only
      callers were `smtp.c`, which isn't compiled:
      `tcp_open`, `tcp_aopen`, `tcp_getline`, `tcp_getbuffer`,
      `tcp_getdata`, `tcp_sout`, `tcp_soutr`, `tcp_close`, `tcp_abort`,
      `server_login`. Also entirely unreferenced anywhere: `hdname`,
      `uname`, `tcp_host`, `tcp_localhost`. Together ~2.9KB of
      text/data/bss -- confirmed `rfc822_date`, living in the same file,
      is *not* part of this (it's called directly by `pine/send.c:860`).
      Not yet removed. Small in isolation, but cheap/clean to cut now,
      and more importantly this whole TCP/`server_login` block shouldn't
      be carried over at all when a real 2.11BSD os-dependent file gets
      written to replace `os_lnx.c`'s role -- no TCP/IP on the target
      means none of it is relevant there either.

### Drop compose-side attachment support
- [ ] Raised while comparing this fork's remaining c-client/pine size
      against mailx's much smaller footprint: mailx's simplicity mostly
      comes from doing no MIME parsing at all (treats the body as an
      opaque blob) and having no built-in composer (shells to
      `$EDITOR`). This codebase already can't/shouldn't go that far --
      it still needs to receive and render real MIME mail arriving over
      the UUCP feed -- but *composing* attachments is a separable,
      self-contained feature that's a poor fit for a PDP-11 mail
      station regardless of size:
      - `pico/attach.c` (Ctrl-J attach-file picker/UI in the composer)
        is 9.2KB text, 100% attachment-specific, nothing else lives in
        that file -- a clean whole-file removal.
      - `pine/send.o`: of its ~25KB text, roughly 6.5KB is
        outgoing-multipart-building/encoding (`set_mime_types` 1.9KB,
        `pine_rfc822_output_body`, `pine_write_body_header`,
        `pine_encode_body`, `mime_recur`, `file_type`, `mime_stats`).
        The rest (`pine_send`, `call_mailer`, address marshaling, header
        assembly) is needed for any outgoing mail and stays.
      - Receive-side MIME rendering (`pine/mailview.c`'s
        `describe_mime`/`type_desc`/`part_desc`/`display_parameters`/
        `make_file_index`/`mime_show`, c-client's
        `rfc822_parse_content`/BODYSTRUCTURE machinery) is explicitly
        *not* part of this cut -- still needed to view/save attachments
        other people send in.
      Net expected win: roughly 15-16KB across pine+pico from deleting
      `pico/attach.c` and the compose-side multipart-building in
      `send.o`, with no change to c-client and no loss of the ability to
      read MIME mail that arrives already-attached. Not yet scoped in
      detail (needs finding every call site of `pico/attach.c`'s
      entry point from `composer.c`, and every compose-time-only MIME
      building path in `send.c`, to confirm nothing else depends on
      them).

### Consider: run the composer as a separate exec'd process
- [ ] Raised alongside the attachment-cut discussion above: pine does
      *not* fork/exec a subprocess for composing -- `pine/send.c:825`
      calls `pico(&pbuf)` directly in-process, linked in via
      `pine/makefile.lnx`'s `LOCLIBES = ../pico/libpico.a
      ../c-client/c-client.a`. That means the composer's entire code --
      editor core, buffer/line/word handling, termcap/display, the
      attach-file picker, search, etc. -- sits resident in the same
      address space as pine's mail-index/folder/MIME-display code at
      all times, not just while actually composing.
      Measured: `pico/libpico.a`'s member `.o` files total
      text=113427+data=4983+bss=10560 (~129KB), essentially matching
      the already-existing standalone `bin/pico` binary (138KB) --
      confirming nearly all of libpico.a gets pulled into `bin/pine`
      (currently 532818 bytes text+data+bss total), i.e. roughly a
      quarter of pine's current footprint is composer code sitting
      resident whether or not the user is composing anything.
      Since `bin/pico` already exists as a complete, separately
      invocable standalone binary (used today for plain-file editing
      outside pine), the pieces for a fork/exec split already exist;
      what's missing is the glue. Known integration seams that make
      this a real architecture change, not a mechanical trim:
      - Headers/body/attachment-list/disposition (send, cancel,
        postpone) currently pass as live C structs (`pbuf`) into
        `pico()` and back -- would need to become a serialized
        protocol (temp file for the body is cheap; the rest needs a
        small format).
      - The Ctrl-T Fcc-folder-picker calls back into pine's own
        `folders_for_fcc()`/`context_apply()` (`pine/folder.c`,
        `pine/context.c`) through a `Pmaster->folders` function
        pointer (see the `s`-buffer writeup in `TODO-pdp11-done.md`) --
        a fork/exec'd composer can't call back into the parent's
        memory directly; would need either the child re-reading the
        folder-collection config itself, or a request/response channel
        back to the blocked parent.
      - Terminal/curses hand-off itself is cheap and already a solved
        pattern in Unix curses programs (suspend, exec, re-init on
        return) -- not a real risk, just needs doing.
      Worth pursuing given the size (~129KB, proportionally the
      biggest single win identified so far) but bigger in kind than
      any cut done to date -- closer to a driver-interface redesign
      than a feature removal or buffer fix.

      `Pmaster->` callback audit (done): the full `PICO` struct
      (`pico/pico.h:150-197`) has 8 function-pointer callbacks, all
      wired up in one place (`pine/send.c:780-787`). Traced each to its
      pine-side implementation:
      - `clearcur`/`keybinit`/`raw_io`/`helper` (-> `clear_cursor_pos`,
        `init_keyboard`, `Raw`, `helper`) are all trivial/self-contained
        (reset two cursor globals; print one termcap escape; a termios
        raw-mode toggle; render a static help-text array) -- no pine
        mail state involved. Confirmed these don't even need bridging:
        pico already has an extensive existing `if(Pmaster)`/
        `if(Pmaster == NULL)` fork throughout nearly every one of its
        own files (`basic.c buffer.c browse.c os_unix.c random.c pico.c
        window.c file.c line.c word.c display.c tinfo.c tcap.c search.c
        bind.c attach.c`) precisely because it's already dual-purpose
        (standalone `bin/pico` vs. pine's composer) -- a forked
        composer could just always take the existing `Pmaster == NULL`
        path for these four, the same one `bin/pico` already uses
        today. Nothing to design here.
      - `buildaddr` (-> `build_address`, `pine/addrutil.c`) calls
        c-client's `expand_address()`/`rfc822_write_address()` using a
        handful of config values (`maildomain`, `localdomain`,
        `hostname`, `use_full_hostname`, `do_local_lookup`) -- no live
        mail state, just config strings/flags easy to hand a forked
        child via argv/env/a small file.
      - `showmsg` (-> `display_message`, `pine/status.c`) reads pine's
        status-line message-queue globals (notices queued before
        entering the composer, e.g. "message saved"). A forked composer
        could just print directly instead of replicating that queue.
      - `folders` (-> `folders_for_fcc`, `pine/folder.c`) is the one
        already known from the `s`-buffer work: genuinely needs pine's
        in-memory parsed folder-collection/context list
        (`ps_global->context_list`), built from the pinerc at startup.
        Real design needed: either the child re-parses that config
        itself, or a request/response channel back to the (blocked)
        parent.
      - `newmail` (-> `new_mail`/`new_mail_mess`, `pine/newmail.c`)
        needs the live, open `MAILSTREAM*` to poll for new arrivals
        while composing -- not reasonably serializable across a process
        boundary. Simplest path is probably dropping the feature
        outright: surface "you have new mail" on return to the
        mail-reading process rather than live mid-compose.
      - `attachments` (a `PATMT*` list, not a callback) -- this
        coupling disappears entirely once the attachment-composition
        cut above lands, which is a nice synergy: do that one first.
      Net: the real remaining design surface is much narrower than "the
      whole PICO struct" -- just `folders_for_fcc` and a decision on
      `new_mail`'s behavior; `buildaddr` is easy config-passing; the
      other four and the attachments field need no bridging at all
      (four already have a standalone code path, the fifth goes away
      with the attachment cut). Not yet started -- this was scoping
      only.

      Dead end considered and ruled out: reviving the `^_` alternate-
      editor hook removed in commit `21fd318a` (`pico/os_unix.c`'s
      `alt_editor()`) does *not* get any part of the way here. Checked
      what it actually did before removal: it only forks `$EDITOR` for
      the *body* text from within an already-fully-resident, already-
      running pico session -- the headers screen, attach picker, Fcc
      picker, and send/cancel/postpone state machine all stayed exactly
      as resident as before, since `alt_editor` never touched any of
      that code. Worse, even the body-editing code it substitutes for
      (`buffer.o line.o word.o search.o basic.o random.o`, ~18.4KB) isn't
      actually removable, because header-field editing (To:/Cc:/Subject:/
      Fcc: lines) reuses the same line-editing primitives applied to
      one-line buffers -- there's no separate simpler input path for
      headers. So even 100% `alt_editor` usage leaves realistically zero
      deletable code, versus the ~129KB `libpico.a` costs pine today. The
      mailx comparison that motivated this doesn't hold up either: mailx
      has no full-screen header-editing UI at all (flat sequential
      `To:`/`Subject:` prompts), which is a materially different, larger
      rewrite than reviving one keybinding -- and still doesn't address
      the actual finding above (the whole composer, however implemented,
      lives in pine's address space at all times).

### Drop `init.c`'s convenience/self-documenting features
- [ ] Found while auditing `init.c`/`args.c` for one-shot startup code
      that could move into a separate bootstrap process (see the
      composer-split item above). Three pieces aren't part of that
      load-bearing config-parsing core -- they're conveniences that
      trade code for either less user-friendliness or less flexibility,
      which is the right trade for this target: every byte that can be
      practically cut should be, even at the cost of a rougher edge for
      the user. None removed yet.
      - `write_pinerc()` (`pine/init.c:1540`, ~2.6KB, the second-biggest
        function in the file) exists only to auto-rewrite the user's
        `.pinerc` with newly-introduced default entries and a
        trademark/copyright banner on first run or version upgrade --
        `pine.c`'s own call site calls it "Temporary, to get new entries
        in .pinerc". Unset variables already work fine from in-memory
        defaults without this; cutting it just means pine never rewrites
        the user's config file for them. Clean removal, no functional
        loss beyond that one first-run convenience.
      - The sent-mail pruning/expiration subsystem
        (`expire_sent_mail`+`prune_folders`+`delete_old_mail`+
        `get_mail_list`+`compare_sm_files`+`init_save_defaults`,
        ~3.8KB) interactively asks the user at startup whether to
        delete old sent-mail copies past a configured age. Automatic
        disk housekeeping is arguably more relevant on a
        storage-constrained target, not less -- but per the
        less-flexible-is-fine principle, cutting it is in bounds: the
        user can prune the sent-mail folder by hand instead of pine
        doing it for them.
      - `dump_global_conf()` + the `-conf` command-line flag
        (`pine/args.c`, `pine/init.c:1807`, ~1KB) prints a blank
        template listing every valid pinerc directive. This is currently
        the *only* way to discover valid variable names, since the
        interactive "Options" screen (`pine/pine.c`'s Setup mini-menu)
        is an unimplemented upstream stub that just prints "No options
        screen yet" -- so cutting this means pinerc directives become
        documentation-only (`README.md`/comments), not
        runtime-discoverable. Accepted trade under the same principle.
      Combined estimate ~7.4KB. Not yet scoped in detail (need each
      function's exact call graph confirmed dead-after-removal the same
      way the `VAR_IMAGE_VIEWER` cut was, plus the `-conf`/`args.c`
      wiring and the `variables[]`/`pine.c` Setup-menu references that
      point at whichever of these get removed).
