# Regression test for pico_child_path()'s $PATH-lookup fallback
# (pine/send.c) -- reported live: installing pine to a directory on
# $PATH and running it by bare name ("pine", not "./pine" or a full
# path) made every Compose fail with "[Composer subprocess produced no
# result]".
#
# Root cause: pico_child_path() derives the composer's path from
# ps_global->pine_exe, a raw copy of argv[0] (pine/args.c). When a
# shell finds a program via a $PATH search and runs it by bare name,
# it passes argv[0] as exactly the bare name typed -- no '/' -- not
# the resolved path (verified directly against a real compiled binary,
# both bash -c and dash -c; an earlier attempt to verify this checked
# a Python script instead and got the wrong answer, because a shebang
# script's argv[0] is rewritten by the kernel's interpreter-dispatch
# mechanism on the way to the interpreter -- a real ELF binary like
# pine itself doesn't go through that). So pico_child_path() falls
# back to a bare "pico" string, intending (per its own comment) to
# rely on $PATH. But that string was handed to execl(), which never
# searches $PATH (only execlp()/execvp() do) -- a pathless "pico" was
# instead looked up relative to the current working directory, so the
# exec silently failed (child _exit(-1), no result file ever written)
# unless the caller happened to be cd'd into pico's own directory.
# Fixed by switching to execlp(), which searches $PATH when its
# argument has no '/' and behaves exactly like execl() when it does.
#
# (Separately, but easily confused with this: if pico isn't installed
# in the same directory as pine at all -- e.g. only the pine binary
# was copied somewhere -- pico_child_path()'s normal (has-a-'/')
# branch constructs a path to a pico that doesn't exist, reproducing
# the identical "[Composer subprocess produced no result]" message for
# an unrelated reason that this test does not exercise.)
#
# This scenario is specifically about *how the pine binary itself is
# invoked* (bare name via $PATH vs. a path containing '/'), which
# smoketest.Session can't reproduce -- it always spawns bin/pine via a
# fixed absolute path, so argv[0] would never hit the buggy fallback
# branch. This test instead copies bin/pine and bin/pico into their
# own directory, puts that directory on $PATH, and spawns a real shell
# that itself performs the $PATH lookup and exec ("pine", not a path)
# -- matching the reported bug exactly. plain pexpect.spawn() can't be
# used to invoke pine directly for this even via a shell binary path:
# ptyprocess.PtyProcess.spawn() always resolves its command through
# its own which() and substitutes the *resolved full path* into
# argv[0] no matter what string is passed in -- irrelevant for bash
# itself here (it's spawned by full path), but this is *why* the
# lookup has to happen inside the spawned shell, not via pexpect.spawn.
import os
import shutil
import tempfile

import pexpect

from smoketest import PINE_BIN, SmoketestFailure


def run():
    pine_src = PINE_BIN
    pico_src = os.path.join(os.path.dirname(PINE_BIN), "pico")
    if not (os.path.exists(pine_src) and os.path.exists(pico_src)):
        raise SmoketestFailure("bin/pine and bin/pico must both be built first")

    bindir = tempfile.mkdtemp(prefix="pine-argv0-bindir-")
    home = tempfile.mkdtemp(prefix="pine-argv0-home-")
    # Deliberately separate from bindir: if this test ran with
    # cwd=bindir, execl()'s (buggy) cwd-relative lookup for a bare
    # "pico" would find it there by coincidence and the test would
    # pass either way -- an empty cwd means only a real $PATH search
    # (execlp()) can find it.
    cwd = tempfile.mkdtemp(prefix="pine-argv0-cwd-")
    try:
        shutil.copy(pine_src, os.path.join(bindir, "pine"))
        shutil.copy(pico_src, os.path.join(bindir, "pico"))
        os.chmod(os.path.join(bindir, "pine"), 0o755)
        os.chmod(os.path.join(bindir, "pico"), 0o755)

        mail_dir = os.path.join(home, "mail")
        os.makedirs(mail_dir, exist_ok=True)
        for default_folder in ("sent-mail", "saved-messages"):
            open(os.path.join(mail_dir, default_folder), "a").close()

        env = dict(os.environ)
        env["HOME"] = home
        env["TERM"] = "vt100"
        env["PATH"] = bindir + os.pathsep + env.get("PATH", "")

        # Bare "pine", found via $PATH -- bash itself passes argv[0]
        # as exactly "pine", no '/', to the real pine binary.
        child = pexpect.spawn(
            "/bin/bash", ["-c", "pine"], cwd=cwd, env=env,
            dimensions=(24, 80), encoding="utf-8", codec_errors="replace",
            timeout=10,
        )
        try:
            child.expect("MAIN MENU")
            child.send("c")
            child.expect("To")

            child.sendcontrol("c")
            child.expect("Cancel")
            child.send("y")

            # The bug's failure mode is a status-line message, not a
            # crash -- explicitly assert its absence rather than just
            # confirming MAIN MENU reappears (which happens either way).
            index = child.expect(["produced no result", "MAIN MENU"])
            if index == 0:
                raise SmoketestFailure(
                    "composer subprocess failed to start when pine was "
                    "invoked by bare name via $PATH (argv[0] had no '/')")

            child.send("q")
            try:
                child.expect("[Rr]eally", timeout=3)
                child.send("y")
            except Exception:
                pass
        finally:
            child.close(force=True)
    finally:
        shutil.rmtree(bindir, ignore_errors=True)
        shutil.rmtree(home, ignore_errors=True)
        shutil.rmtree(cwd, ignore_errors=True)
