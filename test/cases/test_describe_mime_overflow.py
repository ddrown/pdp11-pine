# Regression test for a real stack-buffer overflow in pine/mailview.c's
# describe_mime() (TODO-pdp11.md "Follow-up: buffer-overflow security
# audit"). Its non-multipart branch formatted an attachment's
# description line with:
#
#   sprintf(string, "%s%s%.*s%s", type_desc(...), ...,
#           sizeof(string)-20, body->description, ...);
#
# into a fixed `char string[200]`. The "-20" budget only left room for
# type_desc()'s output on the (incorrect) assumption it's always short,
# but type_desc()'s own TYPEAPPLICATION branches can return up to ~112
# bytes (e.g. "Application/" + a 100-byte attacker-controlled subtype).
# A message with both a long subtype *and* a long Content-Description
# (both taken verbatim from the received message, uncapped anywhere
# upstream) overflows string[] by roughly 100 bytes.
#
# Confirmed via this test: a plain (non-ASan, non-valgrind) build
# reliably aborts ("Bug in Pine detected: Received abort signal") when
# describe_mime() formats the crafted part's description -- no
# attachment save/forward needed, just viewing the message. Also
# confirmed under AddressSanitizer as a genuine stack-buffer-overflow at
# the sprintf() call site.
#
# The same describe_mime() also had two sibling bugs fixed alongside
# this one (not separately regression-tested here, since a plain build
# doesn't reliably crash on them -- confirmed only under
# AddressSanitizer, matching CLAUDE.md's note that this bug class can
# silently corrupt an adjacent same-frame stack variable rather than
# crash outright):
#   - numx[50]: built via sprintf(numx, "%s%d.", prefix, n) once per
#     MIME nesting level with no depth limit, so a deeply-nested
#     multipart structure eventually overflows it.
#   - alts[50] (MimeShow alts[50]): indexed by a multipart/alternative's
#     part count with no bounds check at all, so a part with more than
#     50 alternative sub-parts overflows it.
#
# Fixed by: computing the %.*s precision from type_desc()'s actual
# length instead of a flat "-20", and bounding both the numx/prefix
# growth and the alts[] index against their buffers' real sizes.
from smoketest import Session

LONG_SUBTYPE = "A" * 100
LONG_DESC = "B" * 200

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: smoketest@example.com\n"
    "To: user@example.com\n"
    "Subject: describe_mime overflow test\n"
    "MIME-Version: 1.0\n"
    "Content-Type: multipart/mixed; boundary=\"BOUND\"\n"
    "\n"
    "--BOUND\n"
    "Content-Type: text/plain; charset=us-ascii\n"
    "\n"
    "Hello.\n"
    "--BOUND\n"
    "Content-Type: application/" + LONG_SUBTYPE + "\n"
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
        session.expect("describe_mime overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        # Real assertion is implicit: reaching MESSAGE TEXT without a
        # crash means describe_mime() survived formatting the oversized
        # subtype + description into string[200].

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
