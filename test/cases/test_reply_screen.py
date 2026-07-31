# Coverage test for the Reply command (pine/reply.c's reply()), not a
# bug regression. TODO-pdp11.md's "Investigation targets: low-
# smoketest-coverage files" flagged reply.c at 36.8% -- gcov -f showed
# reply() itself (137 lines, the biggest function in the file) at 0%,
# along with everything it calls exclusively (reply_delimiter(),
# generate_reply_to(), first_addr()): no test ever pressed 'r'. While
# investigating, also found copy_envelope() had zero callers anywhere
# in the built tree (only the unbuilt pine.def DOS/OS2 module-
# definition file still mentioned it) and deleted it.
#
# This drives Reply end to end: the "Include original message" and
# "Reply to all recipients" prompts, landing in the composer with the
# quoted original text and delimiter line, then Cancels out (same
# pattern as test_compose_cancel.py) rather than actually sending.
from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([("first message", "body one")]),
                       sendmail_stub=True)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("\r")
        session.expect("MESSAGE TEXT")

        session.send("r")
        session.expect("Include original message in Reply")
        session.send("y")
        session.expect("Reply to all recipients")
        session.send("n")

        session.expect("COMPOSE MESSAGE REPLY")
        session.expect("Re: first message")
        session.expect("On Mon, 1 Jan 2024 smoketest@example.com wrote")
        session.expect("body one")

        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MESSAGE TEXT")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
