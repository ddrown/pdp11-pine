# Regression test for the tmp_20k_buf -> dynamic buffer conversion
# (TODO-pdp11.md "Buffer right-sizing"). pine_rfc822_header() formats
# the whole To/Cc/Bcc address list into tmp_20k_buf with no bounds
# checking of its own; the buffer is grown to fit beforehand via
# grow_tmp_20k_buf()+pine_rfc822_header_size(). A recipient list this
# long produces a header well past the 2048-byte starting allocation,
# forcing at least two reallocs, so this exercises that growth path
# without needing a real MTA (Postpone runs the same header-building
# code as Send, but just writes to ~/mail/postponed-mail instead of
# invoking sendmail).
import os

from smoketest import Session

N = 450


def run():
    addrs = ",".join("u%d@example.com" % i for i in range(N))
    subject = "many recipients test"
    body_marker = "body text marker XYZZY"

    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")

        session.send(addrs)
        for _ in range(3):  # To -> Cc -> Attchmnt -> Subject
            session.sendcontrol("n")
        session.expect("Subject")

        session.send(subject)
        session.sendcontrol("n")  # Subject -> message body
        session.expect("Message Text")

        session.send(body_marker + "\n")
        session.sendcontrol("o")  # Postpone
        session.expect("postponed", timeout=15)

        session.quit()

        path = os.path.join(session.home, "mail", "postponed-mail")
        with open(path) as f:
            content = f.read()

        assert len(content) > 8192, (
            "postponed message unexpectedly small (%d bytes) -- "
            "did tmp_20k_buf fail to grow?" % len(content)
        )
        for i in (0, N // 2, N - 1):
            addr = "u%d@example.com" % i
            assert addr in content, "%s missing from postponed header" % addr
        assert subject in content
        assert body_marker in content
    finally:
        session.close()
