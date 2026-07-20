# Regression test for the read_pinerc() cut (TODO-pdp11.md's
# gcov-flagged-low-coverage item, moved to TODO-pdp11-done.md as
# "Simplify read_pinerc() to a bootstrap-only stub"): a deliberate,
# accepted functionality loss, not a dead-code removal like the rest
# of this project's cuts.
#
# pine/init.c's read_pinerc() used to parse a real .pinerc file line
# by line, storing recognized variables' values into
# v->user_val/v->global_val. It's now collapsed down to just the
# original bootstrap branch: open the file, and if it exists, note
# that (so first_time_user doesn't get set) but never look at its
# contents -- every variable stays at its compiled-in default on every
# run, whether or not a real .pinerc is present.
#
# This test seeds a real .pinerc setting "personal-name" (which
# controls the From: header's display name on outgoing mail) to a
# distinctive value, sends a message via the Fcc-only path (blank
# To:/Cc:, matching test_send_body_header_overflow.py's pattern -- no
# real sendmail invocation), and confirms the custom personal-name
# never appears in the resulting From: header. This is the inverse of
# what pine 3.89 upstream would do: a working .pinerc parser would
# make this assertion fail.
import os

from smoketest import Session

CUSTOM_NAME = "Custom Pinerc Test Name"
PINERC = "personal-name=%s\n" % CUSTOM_NAME


def run():
    session = Session(pinerc=PINERC)
    try:
        session.expect("MAIN MENU")
        session.send("c")  # Compose
        session.expect("To")
        session.sendcontrol("x")  # Send with blank To (Fcc-only path)
        session.expect("Send message", timeout=15)
        session.send("y")
        session.expect("MAIN MENU", timeout=15)

        with open(os.path.join(session.home, "mail", "sent-mail")) as f:
            sent = f.read()
        assert CUSTOM_NAME not in sent, (
            "personal-name from a hand-written .pinerc took effect -- "
            "read_pinerc() is parsing again, not just bootstrap-checking:\n%s"
            % sent
        )

        session.quit()
    finally:
        session.close()
