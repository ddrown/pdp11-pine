# Regression check for TODO-pdp11.md item 3: full-screen help was ripped
# out to save space, and '?' should now just queue a one-line status
# message instead of paging real help text or crashing.
from smoketest import Session, make_mbox

NO_HELP_MSG = "No help screens in this build"


def run():
    session = Session(mbox=make_mbox([("subject", "body")]))
    try:
        session.expect("1 Message")

        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("?")
        session.expect(NO_HELP_MSG)

        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.send("?")
        session.expect(NO_HELP_MSG)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
