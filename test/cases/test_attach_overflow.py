# Regression test for a real stack-buffer overflow found while auditing
# pico/edef.h's `s` global scratch buffer (TODO-pdp11.md "Buffer
# right-sizing"). pico/attach.c's ParseAttach() parses the Attchmnt
# header field's "N. filename (size) "comment"" syntax by walking a
# pointer through `s` one character at a time with no bound of its own
# (a fixed 16KB buffer, but freely typed/pasted attachment text has no
# length limit at all), then `strcpy()`s the parsed filename/size/comment
# straight from `s` into the caller's (SyncAttach's) fixed *stack* arrays
# -- `char file[NLINE], size[32], comment[1024]` -- with no length check.
# Typing an attachment filename longer than NLINE (256) bytes overflowed
# `file[NLINE]` on the stack; a "(size)" field longer than 32 bytes
# overflowed `size[32]`. Both are reachable just by typing/pasting into
# the Attchmnt field -- no remote/crafted message needed.
#
# Fixed by adding an fnlen/szlen/cmntlen capacity to ParseAttach() and
# using a bounded, truncating copy (attach_bcpy) instead of strcpy() for
# all three fields, plus bounding the char-by-char accumulation into `s`
# itself against `sizeof(s)` as defense in depth.
#
# This test types a ~4000-byte bogus "filename" into the Attchmnt field
# and Postpones (Ctrl-O runs SyncAttach() same as Send would) -- before
# the fix this overflowed the stack past `file[NLINE]` in SyncAttach;
# confirmed under valgrind as an "Invalid write" past the stack redzone.
# After the fix, ParseAttach reports a parse problem (the bogus filename
# doesn't exist on disk) and pine just asks for confirmation to postpone
# anyway, which this test answers.
import os

from smoketest import Session

LONG_NAME = "A" * 4000


def run():
    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")

        for _ in range(2):  # To -> Cc -> Attchmnt
            session.sendcontrol("n")
        session.expect("Attchmnt")

        session.send("1. " + LONG_NAME + ",")
        session.sendcontrol("o")  # Postpone -> runs SyncAttach/ParseAttach

        # Two separate "...anyway?" confirmations can fire here: pico's
        # own SyncAttach()-detected parse problem, and pine's later
        # "attachments won't be included" check when it actually tries
        # to build the postponed message. Answer y to as many as show up.
        for _ in range(2):
            session.expect("[Aa]nyway", timeout=15)
            session.send("y")
        session.expect("postponed", timeout=15)

        session.quit()

        # The real assertion is implicit: session.quit() above only
        # succeeds if pine exited cleanly (status 0) rather than
        # crashing on the overflow. Also sanity-check the postponed
        # file was actually written.
        path = os.path.join(session.home, "mail", "postponed-mail")
        assert os.path.getsize(path) > 0
    finally:
        session.close()
