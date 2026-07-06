# Compose + cancel is a light touch on send.c/call_mailer, which was
# reworked when the SMTP client was dropped (TODO-pdp11.md item 1).
# This never actually calls the mailer -- it only exercises getting into
# and cleanly out of the composer.
from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([]))
    try:
        session.expect("0 Messages")

        session.send("c")
        session.expect("To")

        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
