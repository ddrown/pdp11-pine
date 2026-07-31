# Coverage test for pine/ttyin.c's read_char() escape-sequence
# decoder, not a bug regression. TODO-pdp11.md's "Investigation
# targets: low-smoketest-coverage files" flagged ttyin.c at 37.8% --
# read_char() itself (153 lines) was only 8.5% covered, since every
# other smoketest drives pine with plain ASCII keys ('p'/'n'/etc.)
# rather than the multi-byte escape sequences a real terminal sends
# for arrow keys. Sends the two escape sequences a vt100-class
# terminal (like the project's real WY-60, see blog/) actually uses
# for cursor keys: CSI (`ESC [ A`/`ESC [ B`, vt200-style) and SS3
# (`ESC O A`/`ESC O B`, vt100 application-mode), and confirms they
# move the FOLDER INDEX cursor the same as 'p'/'n' (pine/mailcmd.c's
# process_cmd() binds KEY_UP/KEY_DOWN identically to those keys).
#
# While investigating, also found read_char()'s `#else` half (gated
# on `OLD_READ_CHAR`, never defined anywhere in the tree) was dead
# code -- same never-defined-macro pattern as the DOS/LWP/PCTCP sweeps
# elsewhere in this project -- and deleted it (158 lines, source-only:
# the preprocessor had already discarded it in every build, so this
# doesn't move the coverage denominator or bin/pine's size).
from smoketest import Session, make_mbox

MBOX = make_mbox([
    ("first message", "body one"),
    ("second message", "body two"),
])


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("2 Messages")
        session.send("i")
        session.expect("FOLDER INDEX")

        session.send("\x1b[B")  # CSI Down (vt200-style)
        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect("second message")

        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("\x1b[A")  # CSI Up
        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect("first message")

        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("\x1bOB")  # SS3 Down (vt100 application mode)
        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect("second message")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
