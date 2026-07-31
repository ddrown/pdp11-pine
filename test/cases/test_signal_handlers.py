# Coverage test for pine/signals.c, not a bug regression.
# TODO-pdp11.md's "Investigation targets: low-smoketest-coverage
# files" flagged signals.c at 29.8% (34/114 lines) -- every handler
# past init_signals()/winch_signal() (already exercised by other tests
# via SIGWINCH-on-resize) was 0%, since nothing in the suite ever sent
# the process a real signal. signals.c already went through its own
# DOS-macro dead-code sweep (TODO-pdp11-done.md's "bucket 2 of 3"), so
# this isn't a dead-code hunt -- just driving real signals at a running
# `pine` and checking it reacts the way the source says it should.
#
# Two handlers are deliberately NOT exercised here: auger_in_signal()
# (SIGSEGV/SIGILL/etc.) calls panic() -> exit(-1), and end_signals()
# is *only* reachable from panic() -- both would mean deliberately
# crashing the test binary, not worth automating. quit_signal()'s
# abort branch (answering "y" to the emergency-exit prompt) calls
# coredump() for the same reason; only its decline branch is driven
# here.
import os
import signal

import pexpect

from smoketest import Session, make_mbox

MBOX = make_mbox([("first message", "body one")])


def run():
    _check_hangup_signal(signal.SIGHUP, "hang up")
    _check_hangup_signal(signal.SIGTERM, "terminate")
    _check_usr2_signal()
    _check_quit_signal_decline()
    _check_suspend_resume()


def _check_hangup_signal(sig, word):
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        os.kill(session.child.pid, sig)
        session.expect("Pine finished. Received %s signal" % word)
        session.expect(pexpect.EOF)
        status = session.child.wait()
        assert status == 0, "expected clean exit, got %r" % status
    finally:
        session.close()


def _check_usr2_signal():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        os.kill(session.child.pid, signal.SIGUSR2)
        session.expect("Session now Read-Only")

        # confirm the process is still alive and usable afterward
        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()


def _check_quit_signal_decline():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        os.kill(session.child.pid, signal.SIGQUIT)
        session.expect("REALLY ABORT PINE RIGHT NOW")
        session.send("n")
        session.expect("repaint the screen")

        session.send("i")
        session.expect("FOLDER INDEX")
        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()


def _check_suspend_resume():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        pgid = os.getpgid(session.child.pid)

        session.sendcontrol("z")
        # do_suspend() calls stop_process() (kill(0, SIGSTOP)) on
        # pine's own process group, which pexpect gives its own pgid
        # separate from ours -- safe to stop/continue without touching
        # this test process.
        session.expect("Pine suspended")
        os.killpg(pgid, signal.SIGCONT)

        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
