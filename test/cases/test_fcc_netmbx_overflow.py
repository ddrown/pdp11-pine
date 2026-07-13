# Regression test for TODO-pdp11.md's "imap/ANSI/c-client/mail.c's
# mail_valid_net_parse()" finding: strcpy(mb->mailbox,t+1) into
# NETMBX.mailbox (char[MAILTMPLEN]) with no length check on the parsed
# "{host}mailbox" components. The TODO note only named the mailbox
# copy, but auditing the whole function found two more unbounded
# copies in the same struct: mb->host (via strncpy with an unbounded
# length) and mb->service (char[MAXSRV+1], only 21 bytes -- the
# smallest, easiest-to-hit buffer of the three, via a "/service=..."
# switch value).
#
# Reachable the same way as test_fcc_tilde_overflow.py: type a
# "{host/service=...}mailbox"-shaped string into the composer's Fcc
# field and hit Send. context_isambig() treats a name starting with
# '{' as fully-qualified, so it goes straight to call_mailer()'s
# folder_exists(NULL, fcc) check; bezerk_valid() explicitly rejects
# names starting with '{', so the fallback dummy driver accepts it,
# and folder_exists() calls mail_valid_net() -> mail_valid_net_parse()
# on it before ever getting to a "folder doesn't exist" prompt.
#
# Confirmed each of the three overflows with a standalone ASan repro
# before fixing (this class of same-frame overflow doesn't reliably
# show up under valgrind alone), and confirmed the real crash and its
# fix via a real pty session under valgrind (0 errors after the fix).
from smoketest import Session


def run():
    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")

        session.sendcontrol("r")  # reveal Bcc/Fcc (hidden until populated)
        session.expect("Fcc")

        for _ in range(3):  # To -> Cc -> Bcc -> Fcc
            session.sendcontrol("n")

        # mb->service is only char[MAXSRV+1] (21 bytes) -- 300 'A's is
        # comfortably enough to smash it and beyond.
        session.send("{h/service=" + "A" * 300 + "}mbox")
        session.sendcontrol("x")  # Send
        session.expect("Send message", timeout=15)
        session.send("y")

        # Real assertion is implicit: reaching this prompt (rather than
        # pine crashing first) means mail_valid_net_parse() survived
        # parsing the oversized host/service/mailbox components.
        session.expect("doesn't exist", timeout=15)
        session.send("n")  # don't create the bogus folder
        session.expect("MAIN MENU|COMPOSE|rejected", timeout=15)

        session.sendcontrol("c")  # Cancel back out of the composer
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
