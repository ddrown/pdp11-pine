# Regression test for four real, distinct bugs found together while
# investigating pico/edef.h's `s` global scratch buffer (TODO-pdp11.md
# "Buffer right-sizing") -- specifically pico/composer.c's
# resolve_niks(), which resolves nicknames/expands addresses in the
# To:/Cc:/Bcc: field whenever the user leaves that field (Tab, Postpone,
# Send). All four are reachable just by resuming a postponed message
# with one very long (or, for the third, merely >1024-byte) address and
# tabbing away from or postponing/sending the field again -- no remote
# message content needed, and no interactive typing of a huge string
# (which is impractically slow through a pty; seeding a postponed-mail
# file lets the composer load the huge field in one fast pass).
#
# 1. pico/composer.c resolve_niks(): after rebuilding an entry's line
#    list via InitEntryText()+zotentry(), ods.top_l (and, in callers
#    other than CTRL|'X', ods.cur_l) could be left as a dangling
#    pointer into the just-freed old list -- the function's own
#    "BEWARE" comment already flagged this as a known risk. Confirmed
#    under AddressSanitizer as a heap-use-after-free in next_line(),
#    reachable by typing ordinary text into To: and tabbing to the next
#    field (no huge address needed for this one specifically). Fixed by
#    repointing ods.top_l/ods.cur_l at the freshly built list when they
#    referenced the entry being resolved, before freeing the old one.
#
# 2. pine/addrutil.c addr_string(): formatted an ADDRESS into a fixed
#    static char string[MAX_ADDR_EXPN+1] (1001 bytes) via
#    rfc822_write_address(), which does unbounded strcat()s
#    (rfc822_cat()) on the assumption the buffer is "big enough".
#    mailbox/host/personal/adl come from cpystr()'d fields with no
#    length cap anywhere upstream, so an address longer than ~1000
#    bytes overflowed this global. Reachable simply by resuming a
#    postponed message with a long To: address -- fires in
#    outgoing2strings() before the composer even redraws. Fixed by
#    sizing the buffer dynamically (same technique as tmp_20k_buf).
#
# 3. pico/composer.c resolve_niks(): passed the shared 16KB global `s`
#    directly to (*Pmaster->buildaddr)() (pine's build_address()) as
#    the output buffer, which -- like addr_string() above -- writes
#    with unbounded strcat()s. A single address field longer than 16KB
#    overflowed `s` itself, corrupting the globals declared right after
#    it in edef.h (confirmed under AddressSanitizer). Fixed by giving
#    build_address() its own buffer sized off the same upper bound used
#    to size the input buffer, instead of the shared fixed-size `s`.
#
# 4. imap/ANSI/c-client/rfc822.c rfc822_address_line(): formatted each
#    address in a list into a fixed local char tmp[MAILTMPLEN] (1024
#    bytes) stack buffer, again via unbounded strcat()s. A single
#    address longer than MAILTMPLEN overflowed this stack buffer.
#    Reachable via an ordinary Postpone or Send once pine_rfc822_header()
#    writes the header (e.g. into the postponed-mail file). Fixed by
#    sizing `tmp` per-address instead of assuming MAILTMPLEN is enough.
#
# This test seeds a postponed-mail file with a 20000-byte To: address
# (well past every fixed buffer above), resumes it, moves the cursor
# back up onto the To: field, and Postpones again (CTRL|'O') -- which
# calls resolve_niks() -> build_address() -> rfc822_write_address() and
# then, to actually write the new postponed-mail file, pine_rfc822_header()
# -> rfc822_address_line() -> rfc822_address(). Before all four fixes
# this reliably crashed (confirmed individually under AddressSanitizer
# while diagnosing each one); after the fixes it postpones cleanly.
import time

from smoketest import Session

LONG_ADDR = "A" * 20000


def _make_postponed_mail():
    return (
        "Fcc: sent-mail\n"
        "Date: Tue, 7 Jul 2026 00:00:00 +0100\n"
        "From: someone <someone@example.net>\n"
        "Subject: resolve_niks overflow test\n"
        "To: " + LONG_ADDR + "\n"
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

        # Cursor lands in the body; CTRL|'P' walks back up through
        # Subject/Attchmnt/Cc onto the last physical (wrapped) line of
        # the huge To: field -- entry boundaries are adjacent in the
        # line list regardless of how many lines To: wraps to, so this
        # is a small, fixed number of keystrokes regardless of length.
        for _ in range(5):
            session.sendcontrol("p")
            time.sleep(0.3)

        # CTRL|'O' (Postpone) resolves niks for cur_e (To:) and then
        # rewrites the postponed-mail file via pine_rfc822_header().
        session.sendcontrol("o")
        session.expect("postponed", timeout=20)

        session.quit()
    finally:
        session.close()
