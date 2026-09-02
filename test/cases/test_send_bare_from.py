# Regression test for pine_rfc822_header()'s From: line (pine/send.c).
#
# Real-hardware report: on the actual 2.11BSD/UUCP station, an outgoing
# message's Return-Path/X-Envelope-From came out correctly qualified
# (sendmail derives those from its own envelope/rewriting rules), but
# the message's own From: header showed "mrbawb <abob@pdp11.ARPA>" --
# a domain the outside world doesn't know this station by. That
# "@pdp11.ARPA" came from pine itself: compose_mail() (send.c) sets
# outgoing->from->host to ps_global->maildomain, which init_hostname()
# (init.c) derives from a bare gethostname() call (this target has no
# resolver -- see getdomainnames()'s comment in os.c) with no way to
# override it any more, since pinerc/pine.conf config-file parsing was
# removed entirely in an earlier session (see TODO-pdp11-done.md's
# read_pinerc() removal entries) -- there is no config knob left to
# just set the right domain.
#
# Fixed by not appending "@host" to the From: line at all: sendmail's
# own ruleset 3 canonicalization already fully-qualifies any header
# address with no "@" using its own notion of this host's mail domain
# -- the same mechanism that was already getting Return-Path right --
# so pine just needs to emit the bare local-part and let sendmail do
# the qualification, instead of guessing at a domain it has no way to
# know correctly. See pine_rfc822_header()'s From: special case in
# send.c for why this couldn't just be "set outgoing->from->host to
# NULL": rfc822_address_line()/rfc822_address() (c-client's rfc822.c)
# already overload a NULL host to mean "this ADDRESS starts an RFC822
# *group*", not "bare address" -- so a NULL host there would produce a
# malformed "From: abob: " group opener with no terminator, not a
# clean bare address.
#
# This test composes and sends an ordinary message through the real
# call_sendmail() shell-out (via smoketest.Session's sendmail_stub,
# which bind-mounts a harmless stub over the real sendmail binary --
# see test_send_body_header_overflow_via_sendmail.py for why this is
# needed rather than a plain Session) and asserts the captured From:
# line has no "@" in it at all.
from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([]), sendmail_stub=True)
    try:
        session.expect("0 Messages")

        session.send("c")
        session.expect("To")
        session.send("outbound@example.com")
        session.expect("Cc")
        session.send("\r")
        session.expect("Subject")
        session.send("bare from test\r")
        session.expect("Message Text")
        session.sendcontrol("x")
        session.expect("Send message", timeout=15)
        session.send("y")
        session.expect("MAIN MENU", timeout=15)
        session.quit()

        with open(session.sendmail_log) as f:
            log = f.read()
        from_lines = [line for line in log.splitlines()
                      if line.startswith("From:")]
        assert len(from_lines) == 1, (
            "expected exactly one From: line in the outgoing message, "
            "got: %r" % from_lines)
        assert "@" not in from_lines[0], (
            "From: line should be a bare local-part with no host -- "
            "sendmail is responsible for qualifying it, not pine -- "
            "got: %r" % from_lines[0])
    finally:
        session.close()
