# Companion to test_send_body_header_overflow.py, exercising the same
# pine_rfc822_output_body()/pine_write_body_header() overflow
# (TODO-pdp11.md's buffer-overflow audit) via a *real* recipient and
# call_sendmail()'s actual shell-out, instead of the blank-recipient
# Fcc-only path.
#
# The two tests found different bugs during development, which is why
# both are kept rather than picking one:
#   - The Fcc-only path (f=NULL, no real recipient) exposed a NULL
#     function-pointer crash in pine_rfc822_output_body()'s multipart
#     banner write that a real Send never reaches (f is always
#     non-NULL when there's a real recipient).
#   - This real-Send path exercises call_sendmail()'s actual
#     append_message2(..., AppendSendmail, ...) -> pine_rfc822_output_body()
#     call with a real soutr_t callback, plus the mail_cmd shell string
#     call_sendmail() builds -- a different code path than the Fcc-only
#     one, even though both ultimately reach the same
#     pine_write_body_header() bug.
#
# Driving a real Send safely (this dev box has a live Postfix wired up
# to /usr/lib/sendmail) needs smoketest.Session's sendmail_stub=True:
# it runs pine inside a private bwrap mount namespace with a stub
# script bind-mounted over the resolved real sendmail binary, so
# call_sendmail()'s shell-out invokes the harmless stub instead of
# actually queuing mail. See smoketest.py's _wrap_with_sendmail_stub()
# for how the namespace/bind-mount is set up, and why it's scoped to
# just this one test rather than the default for every Session.
from smoketest import Session

LONG_DESC = "B" * 3000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: smoketest@example.com\n"
    "To: user@example.com\n"
    "Subject: body_header overflow test (real sendmail)\n"
    "MIME-Version: 1.0\n"
    "Content-Type: multipart/mixed; boundary=\"BOUND\"\n"
    "\n"
    "--BOUND\n"
    "Content-Type: text/plain; charset=us-ascii\n"
    "\n"
    "Hello.\n"
    "--BOUND\n"
    "Content-Type: application/octet-stream; name=\"data.bin\"\n"
    "Content-Description: " + LONG_DESC + "\n"
    "\n"
    "somedata\n"
    "--BOUND--\n"
)


def run():
    session = Session(mbox=MBOX, sendmail_stub=True)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("body_header overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        session.send("f")               # Forward
        session.expect("To", timeout=15)
        session.send("outbound@example.com")
        session.sendcontrol("x")        # Send
        session.expect("Send message", timeout=15)
        session.send("y")
        # Real assertion is implicit: reaching this point without a
        # crash means pine_rfc822_output_body()/pine_write_body_header()
        # survived formatting the oversized part mini-header, this time
        # via call_sendmail()'s real shell-out (to the stub, not real
        # sendmail).
        session.expect("Sending mail|MAIN MENU|MESSAGE TEXT", timeout=15)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()

        with open(session.sendmail_log) as f:
            log = f.read()
        assert "outbound@example.com" in log, (
            "fake sendmail never saw the outgoing message -- "
            "call_sendmail() may not have run at all:\n%r" % log)
    finally:
        session.close()
