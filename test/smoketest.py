"""Pty-driven smoke-test harness for the built `pine` binary.

A Session spawns bin/pine under pexpect with an isolated scratch HOME
(so it never touches a real ~/.pinerc or ~/mail) and gives test cases
a small API for driving the curses-ish UI and asserting on the text it
renders. Terminal output is padded with NUL bytes (old-terminal delay
padding); expect()/screen() strip those before matching/returning text.

Run all cases in test/cases/ via run_smoketests.py.
"""

import os
import re
import shutil
import tempfile

import pexpect

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PINE_BIN = os.path.join(REPO_ROOT, "bin", "pine")

# Generous default: pine paces its output with delay-padding bytes, so a
# response to a keystroke can take a couple of seconds to fully arrive.
DEFAULT_TIMEOUT = 10


class SmoketestFailure(AssertionError):
    pass


def _clean(text):
    return re.sub(r"\x00+", "", text or "")


def make_mbox(messages):
    """Build mbox-file text from a list of (subject, body) pairs."""
    parts = []
    for i, (subject, body) in enumerate(messages):
        parts.append(
            "From smoketest@example.com Mon Jan  1 00:00:%02d 2024\n"
            "Date: Mon, 1 Jan 2024 00:00:%02d -0000\n"
            "From: smoketest@example.com\n"
            "To: user@example.com\n"
            "Subject: %s\n\n%s\n" % (i, i, subject, body)
        )
    return "".join(parts)


class Session:
    """A running `pine` process in its own scratch HOME."""

    def __init__(self, mbox=None, args=None, timeout=DEFAULT_TIMEOUT,
                 dismiss_splash=True, postponed_mail=None):
        if not os.path.exists(PINE_BIN):
            raise SmoketestFailure(
                "%s not found -- build it first (./build lnx)" % PINE_BIN)

        self.home = tempfile.mkdtemp(prefix="pine-smoketest-")
        self.timeout = timeout
        self.mbox_path = os.path.join(self.home, "testmbox")

        env = dict(os.environ)
        env["HOME"] = self.home
        env["TERM"] = "vt100"

        argv = []
        if mbox is not None:
            with open(self.mbox_path, "w") as f:
                f.write(mbox)
            argv += ["-f", self.mbox_path]
        argv += ["-p", os.path.join(self.home, ".pinerc")]
        if args:
            argv += list(args)

        if postponed_mail is not None:
            # Seed ~/mail/postponed-mail *before* pine starts, so a test
            # can drive "Continue postponed composition" against a fresh
            # process -- unlike composing-then-postponing in one running
            # process, this doesn't benefit from any buffer growth an
            # earlier action in the same process already triggered.
            mail_dir = os.path.join(self.home, "mail")
            os.makedirs(mail_dir, exist_ok=True)
            with open(os.path.join(mail_dir, "postponed-mail"), "w") as f:
                f.write(postponed_mail)

        self.child = pexpect.spawn(
            PINE_BIN, argv, cwd=REPO_ROOT, env=env,
            dimensions=(24, 80), encoding="utf-8", codec_errors="replace",
            timeout=timeout,
        )

        if dismiss_splash:
            # First launch shows a "Welcome to Pine" splash on top of the
            # Main Menu chrome; any keystroke dismisses it.
            self.expect("MAIN MENU")
            self.send("\r")
            self.expect("MAIN MENU")

    def expect(self, pattern, timeout=None):
        try:
            self.child.expect(pattern, timeout=timeout or self.timeout)
        except (pexpect.TIMEOUT, pexpect.EOF) as exc:
            raise SmoketestFailure(
                "expected %r but got:\n%s" % (pattern, _clean(self.child.before))
            ) from exc
        after = self.child.after
        return after if pattern is pexpect.EOF else _clean(after)

    def send(self, s):
        self.child.send(s)

    def sendcontrol(self, c):
        self.child.sendcontrol(c)

    def resize(self, rows, cols):
        """Change the pty's window size and let SIGWINCH be delivered."""
        self.child.setwinsize(rows, cols)

    def screen(self):
        """NUL-stripped snapshot of output consumed by the last expect()."""
        return _clean(self.child.before)

    def quit(self):
        self.send("q")
        self.expect("Really quit")
        self.send("y")
        self.expect(pexpect.EOF)
        status = self.child.wait()
        if status != 0:
            raise SmoketestFailure("pine exited with status %r" % status)

    def close(self):
        try:
            if self.child.isalive():
                self.child.terminate(force=True)
        finally:
            shutil.rmtree(self.home, ignore_errors=True)
