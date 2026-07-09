# Pine for PDP-11

This is a fork of University of Washington's Pine 3.89 (the original
`README` in this directory is the unmodified upstream file), being cut
down to fit the address space of a PDP-11 running 2.11BSD.

The goal isn't a modern mail client — it's the opposite. It's a mail
UI that could plausibly have existed in the early-to-mid 1990s on a
PDP-11 that talks to the outside world over UUCP rather than TCP/IP: a
dial-up store-and-forward machine reading and writing local mbox
files, handing outgoing mail to a local `sendmail` for `uux`/`uucico`
to carry away, with no notion of sockets, IMAP, POP, NNTP, or any other
network protocol. Everything network-shaped that Pine 3.89 shipped
with has been or is being removed; everything that's just "read my
mail, write a reply, file it in a folder" is being kept.

## What's been cut so far

- **All remote mail protocols.** c-client is built with only the
  `bezerk mbox phile dummy rfc822 misc osdep sm_unix` drivers; remote
  IMAP, POP, NNTP, MH, MMDF, and Tenex support are unlinked.
- **SMTP.** There's no TCP/IP on the target, so outgoing mail always
  goes through `execl("/usr/lib/sendmail", ...)` — the SMTP client and
  its `smtp-server` configuration variable are gone.
- **NNTP/news reader UI**, the address book, the printer support, the
  DOS/MS-DOS and 3b1/UWIN ports, ispell/alternate-editor hooks, and
  other dead weight not relevant to a local-mbox-only, US-ASCII-only,
  single-user machine.
- A running audit of fixed-size buffers and `sprintf`/`strcat`-style
  calls (several real stack/heap overflows found and fixed along the
  way) — a PDP-11 target has none of the incidental stack-protector/
  allocator hardening a modern glibc build has, so bugs that are
  invisible here can be fatal there.

See [`TODO-pdp11.md`](TODO-pdp11.md) for the detailed, in-progress
cut list, size measurements, and write-ups of every bug found and
fixed along the way.

## Building

There's no PDP-11 cross-compiler in this environment, so day-to-day
development happens on a native Linux ("lnx") build, used as a stand-in
for iterating on the source before it's eventually cross-built for
2.11BSD:

```
./build lnx
```

This builds `bin/pine` and `bin/pico`. `./build clean` followed by
`./build lnx` is a good way to force a full rebuild from scratch.

## Testing

`test/run_smoketests.py` drives the built `bin/pine` through a real
pseudo-tty (via `pexpect`), exercising composing, sending, reading,
and folder operations end to end:

```
python3 test/run_smoketests.py
```

Add `--coverage` to instrument `pine/*.c` with gcov and get a
per-file coverage summary after the run.

## Status

This is a work in progress. Pine and Pico both build and run on Linux
today; the binary is shrinking but hasn't yet been cross-compiled for
or run on an actual PDP-11. Follow along in
[`TODO-pdp11.md`](TODO-pdp11.md).
