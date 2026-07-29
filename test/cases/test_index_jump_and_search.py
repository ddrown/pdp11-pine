# Coverage/regression test for FOLDER INDEX's Jump ('j') and Search
# ('w', "where is") commands (pine/mailcmd.c's jump_to()/search_headers(),
# dispatched from process_cmd()'s shared do_jump: label).
#
# Written after removing dead fallout left behind by an earlier cut
# (TODO-pdp11-done.md's feature-list F_* collapse deleted the digit
# '0'-'9' case labels that used to jump straight to a message number).
# process_cmd() still carried three checks for that: two
# `command >= '0' && command <= '9'` conditions and a `command == PF10`
# one (PF10/'u' is Undelete, a different case block entirely -- neither
# could ever be true once the digit cases were gone) -- and jump_to()
# itself still took a now-always-'\0' `first_num` parameter. Collapsed
# all of that to the reachable 'j'/OOPF7 vs. everything-else split; this
# test exercises the surviving path live to make sure the collapse
# didn't change real behavior.
from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([
        ("first message", "body one"),
        ("second message", "body two"),
        ("third message", "body three"),
    ]))
    try:
        session.expect("3 Messages")
        session.send("i")
        session.expect("FOLDER INDEX")

        session.send("j")
        session.expect("Message number to jump to")
        session.send("3\r")
        session.send("\r")  # jump_to() only repositions; Enter opens it
        session.expect("MESSAGE TEXT")
        session.expect("third message")
        session.expect("body three")

        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("w")
        session.expect("Word to search for")
        session.send("second\r")
        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect("second message")
        session.expect("body two")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
