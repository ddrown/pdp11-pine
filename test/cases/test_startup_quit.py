from smoketest import Session, make_mbox


def run():
    session = Session(mbox=make_mbox([]))
    try:
        session.expect("0 Messages")
        session.quit()
        assert "Pine finished" in session.screen()
    finally:
        session.close()
