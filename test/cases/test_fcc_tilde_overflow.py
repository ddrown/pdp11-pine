# Regression test for a chain of stack-buffer overflows found while
# reviewing TODO-pdp11.md's lower-priority "bezerk/dummy_file tmp"
# finding (imap/ANSI/c-client/bezerk.c's bezerk_file()/dummy.c's
# dummy_file(), both `case '~': strcpy(tmp,name+1)` with no length
# check). That review turned up a much more directly reachable variant
# than the TODO note assumed ("needs a contrived local config/folder
# name"): typing a long "~something" string into the composer's Fcc
# field and hitting Send is enough, no .pinerc editing required.
#
# Fixing just the named bezerk_file()/dummy_file() overflow wasn't
# enough to stop the crash -- chasing an ASan repro through the real
# call chain (folder_exists() -> mail_open() -> bezerk_valid() ->
# bezerk_isvalid()) turned up a second, more directly hit overflow in
# bezerk_isvalid() itself: its very first line unconditionally
# strcpy()s the full candidate name into a MAILTMPLEN buffer before
# ever looking at what the name starts with. And even after fixing
# that, the same crafted Fcc value still smashed the stack one level
# further out, in pine/context.c's context_find_all() (and its twin
# context_find()): both build a "Find of mailbox outside context: %s"
# message and then strcpy the raw (unbounded) name into another
# MAILTMPLEN tmp buffer whenever the name doesn't look "ambiguous"
# (i.e. doesn't start with an alnum or '.' -- true for a "~"-led name).
#
# All of these got hit as one crafted string travels: Send's Fcc check
# (call_mailer() in send.c) -> folder_exists() -> mail_open() ->
# bezerk_valid()/bezerk_isvalid() -> (rejected) -> dummy_valid()
# (accepts anything) -> back in folder_exists(), find_stream is NULL
# (OP_PROTOTYPE + dummy always returns NULL for a NULL stream) ->
# context_find_all(NULL, NULL, fn) -> the crash above.
#
# Confirmed with a standalone ASan repro before fixing (this overflow
# doesn't reliably show up under valgrind alone -- see CLAUDE.md's
# verification-discipline note about same-frame local overflows), and
# confirmed the real crash and its fix via a real pty session under
# valgrind (0 errors after the fix, "Bug in Pine detected: Received
# abort signal" / corrupted-return-address backtrace before it).
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

        # No real user has a 1200-character username; this is just
        # long enough to overflow every MAILTMPLEN (1024) buffer in the
        # call chain described above.
        session.send("~" + "A" * 1200)
        session.sendcontrol("x")  # Send
        session.expect("Send message", timeout=15)
        session.send("y")

        # Real assertion is implicit: reaching this prompt (rather than
        # pine crashing first) means the whole folder_exists()/
        # bezerk_isvalid()/context_find_all() chain survived formatting
        # the oversized Fcc name.
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
