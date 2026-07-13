# Regression test for a real stack-buffer overflow in
# pine/reply.c's pretty_addr_string(), found while chasing down the
# "export_message TODO" (TODO-pdp11.md's buffer-overflow audit) -- a
# 2000-byte From: address crashed *before* Export was ever invoked,
# just from opening the message in MESSAGE TEXT view.
#
# pretty_addr_string() formats each envelope address line (From/To/Cc/
# Bcc, also used when quoting To/Cc in a reply) shown when viewing a
# message:
#
#   char buf[MAILTMPLEN];              /* 1024 bytes */
#   rfc822_write_address(buf, addr);   /* unbounded strcat()s */
#
# rfc822_write_address()/rfc822_cat() do unbounded strcat()s trusting
# the caller's buffer is "big enough" -- addr->mailbox/host/personal/
# adl are cpystr()'d from the message with no length cap anywhere, so
# one long address overflows this fixed 1024-byte stack buffer.
# Confirmed under valgrind: "Invalid read of size 8 ... Address
# 0x4141...69 is not stack'd, malloc'd or (recently) free'd" -- the
# stack smash corrupted the return address, reachable simply by
# opening any message with a long From/To/Cc/Bcc address (format_envelope()
# calls pretty_addr_string() for every message viewed).
#
# Fixed by sizing the address buffer per-address with fs_get()/
# fs_give() (same technique as addrutil.c's addr_string(), which had
# an identical bug fixed earlier in this audit) instead of assuming
# MAILTMPLEN is always enough for one address.
from smoketest import Session

LONG_LOCAL = "A" * 2000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: " + LONG_LOCAL + "@example.com\n"
    "To: user@example.com\n"
    "Subject: pretty_addr_string overflow test\n"
    "\n"
    "Hello.\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("pretty_addr_string overflow test")

        session.send("\r")
        # Real assertion is implicit: reaching MESSAGE TEXT without a
        # crash means format_envelope()/pretty_addr_string() survived
        # formatting the oversized From address.
        session.expect("MESSAGE TEXT", timeout=15)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
