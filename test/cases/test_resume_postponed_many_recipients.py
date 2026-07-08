# Regression test for a real heap overflow found while reviewing the
# tmp_20k_buf dynamic-allocation conversion (TODO-pdp11.md "Buffer
# right-sizing"): compose_mail()'s "Continue postponed composition"
# path re-reads a saved postponed-mail file by walking a pointer
# through tmp_20k_buf via repeated fgets(), with no capacity check of
# its own. With tmp_20k_buf shrunk from a fixed 20K to a 2K baseline,
# resuming a postponed message whose header exceeds 2K overflowed the
# buffer -- confirmed under valgrind (an "Invalid write" past the
# malloc'd block, severe enough to corrupt valgrind's own heap
# metadata and abort it). Fixed by growing tmp_20k_buf as the slurp
# loop advances, and again before rfc822_parse_msg() reuses it as
# scratch space for the longest unfolded logical header line.
#
# This seeds ~/mail/postponed-mail directly (via Session's
# postponed_mail= param) rather than composing-then-postponing in one
# running process, because tmp_20k_buf never shrinks back down once
# grown -- postponing in the same process would leave the buffer
# already big enough from that write, masking the bug in the
# separate, independent read path this test targets.
import os

from smoketest import Session

N = 450


def _make_postponed_mail():
    to_line = "To: " + ", \n    ".join(
        "u%d@example.com" % i for i in range(N)
    ) + "\n"
    return (
        "Fcc: sent-mail\n"
        "Date: Tue, 7 Jul 2026 00:00:00 +0100\n"
        "From: someone <someone@example.net>\n"
        "Subject: many recipients test\n"
        + to_line +
        "Message-ID: <fake.message.id@example.net>\n"
        "MIME-Version: 1.0\n"
        "Content-Type: TEXT/PLAIN; charset=US-ASCII\n"
        "\n"
        "body text marker XYZZY\n"
    )


def run():
    session = Session(mbox=None, postponed_mail=_make_postponed_mail())
    try:
        session.send("c")
        session.expect("Continue postponed composition")
        session.send("y")
        session.expect("Message Text")

        # The composer only shows a window of a long, multi-line To:
        # field (scrolled to the last address by default), so re-Postpone
        # unmodified and check the file on disk instead of the screen --
        # a reliable way to confirm the whole header/body round-tripped
        # through resume intact rather than being truncated/corrupted.
        session.sendcontrol("o")
        session.expect("postponed")

        path = os.path.join(session.home, "mail", "postponed-mail")
        with open(path) as f:
            content = f.read()

        for i in (0, N // 2, N - 1):
            addr = "u%d@example.com" % i
            assert addr in content, "%s missing after resuming postponed mail" % addr
        assert "XYZZY" in content

        session.quit()
    finally:
        session.close()
