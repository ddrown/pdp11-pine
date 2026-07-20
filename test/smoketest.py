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

# Must match pine/os.h's SENDMAIL. call_sendmail() (pine/send.c) shells
# out to this absolute path directly -- it isn't looked up via $PATH,
# so a test can't override it just by adjusting PATH/env.
REAL_SENDMAIL_PATH = "/usr/lib/sendmail"

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
                 dismiss_splash=True, postponed_mail=None,
                 sendmail_stub=False):
        if not os.path.exists(PINE_BIN):
            raise SmoketestFailure(
                "%s not found -- build it first (./build lnx)" % PINE_BIN)

        self.home = tempfile.mkdtemp(prefix="pine-smoketest-")
        self.timeout = timeout
        self.mbox_path = os.path.join(self.home, "testmbox")
        self.sendmail_log = None

        # Pre-create ~/mail so pine's init_mail_dir() (pine/init.c) takes
        # its "already exists" path instead of the "must create it" one --
        # the latter does an unconditional `sleep(4)` after printing a
        # one-line notice, on the theory that a real interactive user
        # should have a few seconds to read it. Every fresh scratch HOME
        # here would otherwise eat that 4s on every single test.
        mail_dir = os.path.join(self.home, "mail")
        os.makedirs(mail_dir, exist_ok=True)

        # Pre-create empty default-fcc/default-save folders (DF_DEFAULT_FCC
        # "sent-mail" / DEFAULT_SAVE "saved-messages", pine/os.h) so a Send
        # or Save doesn't hit the "Folder ... doesn't exist. Create?"
        # prompt this harness doesn't expect. Before c-client/os_lnx.c's
        # myhomedir() was fixed to honor $HOME, every write to these
        # default folders silently landed in the real developer's actual
        # ~/mail (which already has them) instead of this scratch one --
        # masking the fact that tests never handled this prompt.
        for default_folder in ("sent-mail", "saved-messages"):
            open(os.path.join(mail_dir, default_folder), "a").close()

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
            with open(os.path.join(self.home, "mail", "postponed-mail"), "w") as f:
                f.write(postponed_mail)

        spawn_cmd, spawn_argv = PINE_BIN, argv
        if sendmail_stub:
            spawn_cmd, spawn_argv = self._wrap_with_sendmail_stub(argv)

        self.child = pexpect.spawn(
            spawn_cmd, spawn_argv, cwd=REPO_ROOT, env=env,
            dimensions=(24, 80), encoding="utf-8", codec_errors="replace",
            timeout=timeout,
        )

        if dismiss_splash:
            # First launch shows a "Welcome to Pine" splash on top of the
            # Main Menu chrome; any keystroke dismisses it.
            self.expect("MAIN MENU")
            self.send("\r")
            self.expect("MAIN MENU")

    def _wrap_with_sendmail_stub(self, argv):
        """Run pine in a private mount namespace (bwrap) with a stub
        script bind-mounted over the real sendmail binary, so a test can
        drive an actual Send (through call_sendmail()'s real shell-out,
        not just the no-recipient Fcc-only path) without risking real
        outbound mail -- this dev box has a live Postfix wired up to
        REAL_SENDMAIL_PATH, so a real Send here would otherwise hand it
        an actual outbound message.

        REAL_SENDMAIL_PATH is usually a symlink chain (e.g.
        /usr/lib/sendmail -> .../mta-sendmail -> .../sendmail.postfix ->
        ../bin/sendmail.postfix); bwrap's --bind needs the fully
        resolved regular-file target, binding onto the top-level symlink
        path itself fails ("Can't create file").
        """
        if not shutil.which("bwrap"):
            raise SmoketestFailure(
                "bwrap (bubblewrap) not found -- required for "
                "sendmail_stub=True so a test can drive a real Send "
                "without risking real outbound mail")

        real_target = os.path.realpath(REAL_SENDMAIL_PATH)

        self.sendmail_log = os.path.join(self.home, "fake-sendmail.log")
        stub_path = os.path.join(self.home, "fake-sendmail")
        with open(stub_path, "w") as f:
            f.write(
                "#!/bin/sh\n"
                "echo \"ARGS: $*\" >> %s\n"
                "cat >> %s\n"
                "exit 0\n" % (self.sendmail_log, self.sendmail_log)
            )
        os.chmod(stub_path, 0o755)

        bwrap_argv = [
            "--dev-bind", "/", "/",
            "--bind", stub_path, real_target,
            "--", PINE_BIN,
        ] + argv
        return "bwrap", bwrap_argv

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
