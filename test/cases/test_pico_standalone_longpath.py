# Regression test for TODO-pdp11.md's "pico/main.c strcpy(bp->b_fname,
# argv[carg])" finding, surfaced incidentally while verifying the
# resize_pico() fix.
#
# pico/main.c's standalone "open a file named on argv" startup path did
# `strcpy(bp->b_fname, argv[carg])` with no length check -- b_fname is
# char[NFILEN=80], and BUFFER structs come from a plain malloc(sizeof
# (BUFFER)) (pico/buffer.c's bfind()) with b_fname *not* the last field,
# so a long enough argv overflows straight past the end of that heap
# block. readin() (pico/file.c) had the identical unbounded
# strcpy(bp->b_fname, fname) right after, called with the same string.
#
# This is NOT reachable from pine's mail-composing flow -- the composer
# subprocess is invoked via a `-P <handoff> <result>` dispatch mode
# (pico/main.c, see compose_stub.c) that never touches argv[carg] --
# only direct standalone `bin/pico <longpath>` use is affected. That's
# why this test spawns bin/pico directly with pexpect instead of using
# smoketest.Session (which always drives bin/pine).
#
# Confirmed via valgrind before the fix: two "Invalid write of size 1"
# heap-buffer-overflow errors (0 bytes after a 152-byte malloc'd block),
# one from main.c's strcpy and one from readin()'s. Also reproduces as
# a bare SIGSEGV with no instrumentation at all (glibc malloc chunk
# corruption), which is what this test checks for directly. Both gone
# after bounding both strcpy calls to NFILEN-1 with explicit
# NUL-termination.
import os
import time

import pexpect

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PICO_BIN = os.path.join(REPO_ROOT, "bin", "pico")


def run():
    longpath = "A" * 500

    child = pexpect.spawn(PICO_BIN, ["-z", longpath], timeout=8, cwd=REPO_ROOT)
    try:
        time.sleep(1)
        assert child.isalive(), "pico crashed on startup with a long argv path"

        child.sendcontrol("x")
        time.sleep(1)
        child.send("n")
        child.expect(pexpect.EOF)

        assert child.signalstatus is None, (
            "pico died from signal %r (expected clean exit)" % (child.signalstatus,)
        )
        assert child.exitstatus == 0, (
            "pico exited %r (expected 0)" % (child.exitstatus,)
        )
    finally:
        if child.isalive():
            child.terminate(force=True)
