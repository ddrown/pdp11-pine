# Regression test for a real stack-buffer overflow in
# pine/mailcmd.c's export_message() -- the item TODO-pdp11.md's
# buffer-overflow audit originally named ("send.c's per-MIME-part
# header writer, bezerk.c's LOCAL->buf" sweep; this is a third
# instance found during the same sweep, distinct from
# test_export_message.py's unrelated ctime()/time.h crash).
#
# export_message() built the mbox "From " separator line with:
#
#   char from[MAX_ADDRESS+1];      /* 101 bytes */
#   sprintf(from, "%s%s%s", env->from->mailbox, ..., env->from->host);
#
# env->from->mailbox/host are cpystr()'d from the message's own From:
# header with no length cap anywhere upstream, so a long enough
# address overflows this fixed 101-byte stack buffer. A short address
# (a few hundred bytes) only silently corrupts an adjacent same-frame
# local (only observable under AddressSanitizer, confirmed separately);
# a long enough one (some thousands of bytes) reliably aborts even on
# a plain build -- confirmed under valgrind too ("Invalid write of
# size ..." at export_message()'s sprintf).
#
# Fixed by sizing "from" with fs_get()/fs_give() to fit the actual
# address, and growing the shared tmp_20k_buf (via grow_tmp_20k_buf())
# before formatting it into the "From <address> <date>" separator
# line, since that's also sized only for the old fixed-size "from".
from smoketest import Session

LONG_LOCAL = "A" * 5000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: " + LONG_LOCAL + "@example.com\n"
    "To: user@example.com\n"
    "Subject: export_message from[] overflow test\n"
    "\n"
    "Hello.\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("export_message from\\[\\] overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        session.send("e")
        session.expect("save message text in", timeout=15)
        session.send("exported_msg\r")
        # Real assertion is implicit: reaching this message without a
        # crash means export_message() survived formatting the
        # oversized From address into the mbox separator line.
        session.expect("exported to file", timeout=15)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
