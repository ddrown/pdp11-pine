# Regression test for a real buffer overflow in pine/mailview.c's
# type_desc() (TODO-pdp11.md "Follow-up: buffer-overflow security
# audit"). Its TYPEAPPLICATION/octet-stream branch sprintf'd a MIME
# part's "name" parameter -- taken directly from a *received* message,
# with no length limit of its own -- into type_desc's static
# `char type_d[200]` using a bare "%s", unlike every other branch in
# this function, which caps with "%.100s". A long enough attachment
# name parameter overflowed type_d.
#
# Confirmed under valgrind: opening a message with an
# application/octet-stream part whose Content-Type "name" parameter is
# 500 bytes crashed pine (SIGSEGV, stack corrupted with the injected
# 'A' bytes) when the message view's part_desc()/describe_mime()
# formatted the "not shown" attachment description line -- no compose
# or user typing involved, just viewing a crafted/oversized message.
#
# Fixed by adding the same "%.100s" precision cap already used by
# type_desc()'s other branches.
from smoketest import Session

LONG_NAME = "A" * 500

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: smoketest@example.com\n"
    "To: user@example.com\n"
    "Subject: type_desc overflow test\n"
    "MIME-Version: 1.0\n"
    "Content-Type: multipart/mixed; boundary=\"BOUND\"\n"
    "\n"
    "--BOUND\n"
    "Content-Type: text/plain; charset=us-ascii\n"
    "\n"
    "Hello.\n"
    "--BOUND\n"
    "Content-Type: application/octet-stream; name=\"" + LONG_NAME + "\"\n"
    "\n"
    "somedata\n"
    "--BOUND--\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("type_desc overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        # Real assertion is implicit: reaching MESSAGE TEXT without a
        # crash means describe_mime()/part_desc()/type_desc() survived
        # formatting the oversized attachment name.
        session.expect("Attached file")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
