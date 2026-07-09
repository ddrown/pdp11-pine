# Regression test for a real heap overflow found while auditing
# pico/edef.h's `s` global scratch buffer (TODO-pdp11.md "Buffer
# right-sizing"). pico/composer.c's InitMailHeader() builds the
# Attchmnt field's display text by walking mp->attachments and, for
# each entry, sprintf()'ing "N. filename (size) "description"," into
# the shared global `s` scratch buffer and then strcat()'ing that into
# addrbuf -- a fixed 1024-byte malloc(), regardless of how many
# attachments there are or how long their fields are.
#
# pine/send.c's parse_postponed_header() -- which rebuilds the
# attachment list when resuming a postponed message -- reads the
# "Attachment-file:"/"Attachment-description:" pseudo-header lines with
# cpystr(), with no length cap of its own. So a postponed-mail file
# with a single long Attachment-description: line overflows both the
# shared 16KB `s` buffer (silently, since it's a static array rather
# than a heap block) and, confirmed under valgrind, the 1024-byte
# addrbuf heap block via strcat() -- severely enough to corrupt
# valgrind's own heap metadata and abort it. Reachable just by
# resuming a postponed message with one oversized attachment
# description, no attachment file or user typing involved.
#
# Fixed by sizing addrbuf exactly (summing the real field lengths up
# front) and formatting each entry directly into it instead of through
# the shared `s` buffer.
#
# Fixing this also surfaced a second instance of an already-fixed bug
# class: pico.c's main pico() loop declared `int i;` (~line 142)
# uninitialized and passed &i to (*Pmaster->newmail)(&i, ...), which
# only sets *cursor_moved on paths returning >= 0 -- but
# `if(i || mpresf)` (~line 234) is unconditional, identical to the
# LineEdit() `j` bug fixed earlier this session. Fixed the same way:
# initializing `i = 0` at declaration.
import os

from smoketest import Session

LONG_DESCRIPTION = "A" * 20000


def _make_postponed_mail():
    return (
        "Fcc: sent-mail\n"
        "Date: Tue, 7 Jul 2026 00:00:00 +0100\n"
        "From: someone <someone@example.net>\n"
        "Subject: attach resume overflow test\n"
        "To: user@example.com\n"
        "Attachment-file: /tmp/foo.txt\n"
        "Attachment-description: " + LONG_DESCRIPTION + "\n"
        "Message-ID: <fake.message.id@example.net>\n"
        "MIME-Version: 1.0\n"
        "Content-Type: TEXT/PLAIN; charset=US-ASCII\n"
        "\n"
        "body text marker XYZZY\n"
    )


def run():
    session = Session(mbox=None, postponed_mail=_make_postponed_mail())
    try:
        session.send("c")
        session.expect("Continue postponed composition")
        session.send("y")
        session.expect("Message Text", timeout=15)

        # Real assertion is implicit: reaching Message Text without a
        # crash means InitMailHeader() survived formatting the
        # oversized attachment description.
        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
