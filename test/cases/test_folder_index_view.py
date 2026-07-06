from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([
        ("first message", "body of the first message"),
        ("second message", "body of the second message"),
    ]))
    try:
        session.expect("2 Messages")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("first message")
        session.expect("second message")

        session.send("P")  # PrevMsg, so we land on message 1 regardless of
                            # where the index cursor started
        session.send("\r")
        session.expect("MESSAGE TEXT")
        session.expect("body of the first message")

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
