# Regression test for the "ISO-2022 escape-sequence handling" cut
# (TODO-pdp11.md's "Deliberate-simplification candidates" list,
# TODO-pdp11-done.md for the write-up).
#
# pine/mailview.c used to carry a table of ISO-2022 charset-shift
# escape sequences (iso2022_escapes[]/match_iso2022()) used only to
# stop format_scroll_text()'s line-width accounting from counting an
# escape sequence's bytes toward the screen column -- an i18n
# accommodation with no purpose under this build's US-ASCII-only
# mandate. It was removed project-wide, including in ttyout.c's
# Writechar()/PutLine0n8b() (which used a parallel esc_len mechanism
# to make Writechar's caller-supplied escape length also a no-op for
# cursor-column tracking).
#
# Observable behavior change: a raw ESC (0x1B) byte in a displayed
# message body used to be passed through to the terminal untouched
# (relying on match_iso2022() to decide how much of it was "invisible");
# now it's treated like any other control character -- rendered as a
# literal '?' and counted as one column, same as e.g. a stray '\x05'
# would be. This test locks in that new behavior and confirms viewing
# a message with an embedded escape doesn't crash or corrupt the
# display of the text around it.
from smoketest import Session

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: user@example.com\n"
    "To: user@example.com\n"
    "Subject: iso2022 escape test\n"
    "\n"
    "before\033(Bafter\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("iso2022 escape test")
        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)

        session.expect("after", timeout=5)
        screen = session.screen()
        if "before?" not in screen:
            raise AssertionError(
                "expected escape byte rendered as '?' right after "
                "'before', got:\n%s" % screen
            )

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
