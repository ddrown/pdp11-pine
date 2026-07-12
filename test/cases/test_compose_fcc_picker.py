# Regression test for the composer-as-a-separate-process cut (see
# TODO-pdp11-done.md). The Ctrl-T Fcc-folder-picker used to call back
# into pine's own folders_for_fcc()/context_apply() (live
# ps_global->context_list) through a Pmaster->folders function
# pointer -- not possible once the composer is a separate exec'd
# process with no access to pine's memory. Replaced with: pine
# precomputes a flat list of configured folder-collection nicknames
# (pine/folder.c's fcc_candidate_list()) and passes it in the handoff
# file; the composer subprocess's Ctrl-T handler (pico/compose_stub.c's
# compose_folders()) does a simple in-process numbered-menu prompt
# over that list instead of browsing live folders.
#
# This exercises the whole round trip: the candidate list crossing the
# handoff file, the numbered prompt appearing, a selection crossing
# back via the result file into the Fcc header field.
import re
import time

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

        session.sendcontrol("t")  # Ctrl-T: Fcc folder picker
        session.expect("number or name")

        session.send("1\r")
        time.sleep(1)

        # expect()/screen() only look at output *before* the next match,
        # which isn't enough here (the redrawn Fcc value comes right
        # after the "Fcc" label matches) -- read the raw redraw stream
        # instead and confirm the Fcc field line actually gained
        # content, the real assertion that the picked candidate
        # round-tripped through the composer subprocess's handoff/
        # result files rather than the field silently staying empty
        # (the shape a parse error, crash, or missing result field
        # would all take).
        raw = session.child.read_nonblocking(size=20000, timeout=3)
        text = raw if isinstance(raw, str) else raw.decode("utf-8", "replace")
        text = re.sub(r"\x00+", "", text)
        text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", " ", text)

        m = re.search(r"Fcc\s*:\s*(\S.*)", text)
        assert m, (
            "Fcc field still empty after picking folder 1 -- "
            "composer subprocess's folder picker didn't round-trip:\n%s"
            % text
        )

        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
