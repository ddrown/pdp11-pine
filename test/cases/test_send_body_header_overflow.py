# Regression test for a real stack overflow in
# pine/send.c's pine_rfc822_output_body()/pine_write_body_header()
# (TODO-pdp11.md's buffer-overflow audit -- the item literally named
# "send.c's per-MIME-part header writer").
#
# When writing a multipart outgoing message, pine_rfc822_output_body()
# builds each part's "--boundary" line plus a mini-header (Content-Type/
# -Transfer-Encoding/-ID/-Description) into:
#
#   char tmp[MAILTMPLEN];                    /* 1024 bytes */
#   pine_write_body_header (&t, &part->body);
#
# Unlike the top-level header (pine_rfc822_header(), which writes into
# the dynamically-grown tmp_20k_buf, sized beforehand via
# pine_rfc822_header_size()), this per-part mini-header had no size
# accounting at all. subtype/parameter values/id/description are all
# cpystr()'d from a message with no length cap anywhere -- reachable by
# receiving a multipart message with a long Content-Description (or
# long parameter/subtype), Forwarding it (copy_body() recurses verbatim
# cpystr()s of the original part's fields when "Forward as MIME
# attachment" isn't used -- off by default), and Sending. Confirmed
# under valgrind: an "Invalid read of size 1" in pine_write_body_header
# at the point it re-measures strlen() of the already-corrupted buffer,
# landing on the injected filler bytes; also reliably aborts on a plain
# (non-ASan, non-valgrind) build at this size.
#
# Fixed by adding pine_body_header_size() (mirroring
# pine_rfc822_header_size()'s style) and fs_get()/fs_give()-ing tmp per
# part instead of assuming MAILTMPLEN always fits one part's header.
#
# This test drives an actual Send (not Postpone -- Postpone strips all
# but the first text part before writing, per send.c's "Attachments
# won't be included in message" handling, so it never reaches this
# code path with the crafted part intact). It deliberately leaves
# To:/Cc: blank instead of naming a real recipient: pine's default
# default-fcc ("sent-mail") means Send still proceeds, but entirely via
# the Fcc-only path (pine_rfc822_output(), called with f=NULL,s=NULL)
# -- call_mailer() never calls call_sendmail(), so this never shells
# out to a real MTA. Machines with a live sendmail/postfix (as this dev
# box has) would otherwise have this test hand it a real outbound
# message addressed to a real domain on every run.
#
# Finding a safe way to drive this exposed one more real, independent
# bug: pine_rfc822_output_body()'s multipart branch unconditionally
# called (*f)(s, "...MIME format..."), with no NULL check, unlike every
# other callback invocation in that function -- f is NULL precisely on
# this Fcc-only/no-recipient path, so it crashed with "Jump to the
# invalid address 0x0" until guarded the same way as its siblings.
from smoketest import Session

LONG_DESC = "B" * 3000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: smoketest@example.com\n"
    "To: user@example.com\n"
    "Subject: body_header overflow test\n"
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
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("body_header overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        session.send("f")               # Forward
        session.expect("To", timeout=15)
        # Leave To:/Cc: blank on purpose -- see module docstring. This
        # relies on pine's default default-fcc ("sent-mail") to let
        # Send proceed via the local-only Fcc path instead of a real
        # sendmail invocation.
        session.sendcontrol("x")        # Send
        session.expect("Send message", timeout=15)
        session.send("y")
        # Real assertion is implicit: reaching this point without a
        # crash means pine_rfc822_output_body()/pine_write_body_header()
        # survived formatting the oversized part mini-header.
        session.expect("copied to|MAIN MENU|MESSAGE TEXT", timeout=15)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
