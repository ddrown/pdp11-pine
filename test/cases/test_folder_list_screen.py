# Coverage test for the Folder List screen (pine/folder.c), not a bug
# regression. TODO-pdp11.md's "Investigation targets: low-smoketest-
# coverage files" flagged folder.c at 16.1% coverage as the lowest of
# any non-trivial source file -- an audit of folder_lister() and its
# helpers (see TODO-pdp11-done.md's folder.c write-ups) found no dead
# code left to cut; the whole 0%-covered chunk is simply the Folder
# List screen ('l'/OPF5 from the main menu and message index), which
# no existing test ever opens. This drives it end to end instead:
# opening the list, both navigation-boundary messages (folder_lister's
# ctrl('B')/'p' and ctrl('F')/'n' cases), add/rename/delete a folder
# (add_new_folder()/rename_folder()/delete_folder()), and selecting a
# folder to open it (the do_broach_folder() branch).
from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([("first message", "body one")]))
    try:
        session.expect("1 Message")
        session.send("l")
        session.expect("FOLDER LIST")
        session.expect("INBOX")
        session.expect("saved-messages")
        session.expect("sent-mail")

        # Cursor starts on INBOX (first entry) -- walking left immediately
        # hits the "already on first folder" boundary case.
        session.send("p")
        session.expect("Already on first folder")

        # Three folders total (INBOX, saved-messages, sent-mail): two
        # NextFldr presses land on sent-mail, a third hits the opposite
        # boundary case.
        session.send("n")
        session.send("n")
        session.send("n")
        session.expect("Already on last folder")

        # Add, rename, then delete a folder.
        session.send("a")
        session.expect("Name of folder to add")
        session.send("testfolder\r")
        session.expect('Folder "testfolder" created')

        session.send("r")
        session.expect("Change name of folder")
        session.sendcontrol("a")  # start of line
        session.sendcontrol("k")  # kill to end -- clear the prefilled name
        session.send("renamedfolder\r")
        session.expect('renamed to "renamedfolder"')

        session.send("d")
        session.expect("Really delete")
        session.send("y")
        session.expect('"renamedfolder" deleted')

        # Cursor now sits back on sent-mail (the last remaining entry);
        # select it to exercise the do_broach_folder() open path.
        session.send("v")
        session.expect("FOLDER INDEX")
        session.expect("Folder: sent-mail")
        session.expect("Message 0 of 0")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
