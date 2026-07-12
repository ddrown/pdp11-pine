# Regression test for a real crash in export_message() (pine/mailcmd.c):
# viewing a message and using (e)xport to save it to a file reliably
# crashed with "Bug in Pine detected: Received abort signal" (a SIGSEGV
# caught by signals.c's auger_in_signal()).
#
# Root cause: pine/osdep/os-lnx.h explicitly commented out
# "#include <time.h>" ("plain time.h isn't enough on some systems"),
# relying on <sys/time.h> alone. On this build that leaves ctime()
# (and localtime()) with no prototype in scope anywhere in pine's *.c
# files, so the compiler implicitly declares them as returning int --
# confirmed via the resulting "makes pointer from integer without a
# cast" warning at export_message()'s
# `strcpy(tmp_20k_buf, ctime(&now))` call. That silently truncates
# ctime()'s real 64-bit char* return value to 32 bits; strcpy() then
# dereferences the truncated/garbage "pointer" and crashes immediately,
# every time, since ctime() is called on every export (used to build
# the mbox "From " separator line's date).
#
# Fixed by restoring a real "#include <time.h>" in os-lnx.h alongside
# the existing <sys/time.h> (kept for struct timeval) -- ctime()
# appears throughout pine/*.c (signals.c, newmail.c, init.c, log.c,
# imap.c, reply.c, status.c), so this was a systemic problem, not
# specific to export_message().
from smoketest import Session, make_mbox


def run():
    mbox = make_mbox([("test export", "hello export body")])
    session = Session(mbox=mbox)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)

        session.send("e")
        session.expect("File")

        session.send("testexport.txt\r")
        session.expect("exported to file", timeout=10)

        # The real assertion is implicit: reaching back to the message
        # view without a crash means export_message() survived calling
        # ctime(). Also confirm the file actually got written with
        # believable content, not just that pine didn't crash.
        assert session.child.isalive(), "pine crashed exporting a message"

        import os
        path = os.path.join(session.home, "testexport.txt")
        with open(path) as f:
            content = f.read()

        assert content.startswith("From "), (
            "exported file missing mbox From-separator line: %r" % content[:80]
        )
        assert "hello export body" in content

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
