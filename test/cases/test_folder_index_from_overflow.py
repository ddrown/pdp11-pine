# Regression test for a real stack-buffer overflow in
# imap/ANSI/c-client/mail.c's mail_fetchfrom(), found while chasing down
# the "export_message TODO" (TODO-pdp11.md's buffer-overflow audit).
#
# mail_fetchfrom() builds the "From" column shown for each message in
# FOLDER INDEX. When the sender has no personal name, it did:
#
#   char tmp[MAILTMPLEN];         /* 1024 bytes */
#   sprintf(t = tmp, "%s@%s", adr->mailbox, adr->host);
#
# adr->mailbox/host are cpystr()'d from the message's own From: header
# with no length cap anywhere upstream, so a long enough local-part
# overflows this fixed 1024-byte stack buffer. Confirmed under
# valgrind: "Jump to the invalid address" (0x4141...41), i.e. the
# overflow smashed the return address -- reachable simply by opening
# the FOLDER INDEX (build_header_line() calls mail_fetchfrom() for
# every displayed message), no Export/Forward/Save needed.
#
# Fixed by sizing the "mailbox@host" buffer with fs_get()/fs_give()
# to fit the actual address instead of assuming MAILTMPLEN is enough.
from smoketest import Session

LONG_LOCAL = "A" * 2000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: " + LONG_LOCAL + "@example.com\n"
    "To: user@example.com\n"
    "Subject: mail_fetchfrom overflow test\n"
    "\n"
    "Hello.\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        # Real assertion is implicit: reaching FOLDER INDEX without a
        # crash means mail_fetchfrom() survived formatting the
        # oversized From address for the index line.
        session.expect("FOLDER INDEX")
        session.expect("mail_fetchfrom overflow test")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
