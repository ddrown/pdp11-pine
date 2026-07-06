import base64

from smoketest import Session


def run():
    plaintext = "Hello from a base64-encoded message body."
    encoded = base64.b64encode((plaintext + "\n").encode("ascii")).decode("ascii")

    mbox = (
        "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
        "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
        "From: smoketest@example.com\n"
        "To: user@example.com\n"
        "Subject: base64 body test\n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=us-ascii\n"
        "Content-Transfer-Encoding: base64\n"
        "\n" + encoded + "\n"
    )

    session = Session(mbox=mbox)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("base64 body test")

        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect(plaintext)  # decoded, not the raw base64 text

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
