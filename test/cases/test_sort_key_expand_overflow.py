# Regression test for two stack-buffer overflows found while sweeping
# the codebase for more instances of the "walk a string pointer with
# an unbounded s++ copy into a fixed-size local buffer" bug class (the
# same shape as the fuzzer-found rfc822.c overflows -- see
# TODO-pdp11-done.md's harness-1 backslash-audit entries) outside
# fuzzing-reachable email-parsing code. Both of these are reachable
# purely via a command-line argument, no incoming mail involved.
#
# Both live in pine/init.c's expand_variables(), which expands
# "$VAR"/"${VAR}" and a leading "~" in any pine variable's value, and
# is reached at startup for -sort's argument via init_vars()'s
# unconditional set_current_val(vars, V_SORT_KEY, TRUE).
#
# Bug 1: the "$VAR"/"${VAR}" case did
#   while (isalnum(*src)) *p++ = *src++;
# copying into `char word[128]` with no bound -- a "-sort" argument
# containing "$" followed by 128+ alnum characters overflows word[].
# Fixed by capping the copy to sizeof(word)-1 and returning NULL (an
# already-well-exercised "expansion failed" path -- every caller
# already falls back cleanly, see set_current_val()) if it doesn't fit.
#
# Bug 2: the "~" case's own local `char buf[MAXPATH]` copy had the
# same missing bound, and a second, much more easily hit overflow one
# level further in: pine/osdep/fnexpand's `char name[20]` (the
# ~username scratch buffer) had an identical unbounded `*y++ = *x++;`
# copy, so a "-sort" argument like "~" followed by 20+ non-slash
# characters smashes name[20] long before buf[MAXPATH]'s own much
# larger limit could matter. Fixed both: buf's copy now returns NULL
# if the ~-prefixed component doesn't fit, and fnexpand's copy now
# stops writing into name[] once full while still advancing its scan
# pointer to the true end of the username (so the existing
# getpwnam()-failure path, and the x - buf length used afterward, both
# stay correct) -- a too-long username now just fails the pw lookup
# like any other unknown username, instead of overflowing.
#
# Confirmed both with real ASan stack-buffer-overflow reports before
# fixing, and confirmed clean after. Note: this codebase's ASAN=1
# build target must be passed as a make argument (`./build lnx
# ASAN=1`) -- an `ASAN=1` *environment* variable is silently
# overridden by the makefile's own `ASAN=` assignment and produces a
# binary with no ASan instrumentation at all.
from smoketest import Session, make_mbox
import pexpect


def run():
    # Bug 1: "$" + 300 alnum chars. getenv() legitimately fails to find
    # such a long-named variable, so this degrades to the default sort
    # key and pine reaches MAIN MENU normally -- the assertion is
    # implicit in getting there instead of an ASan abort/crash.
    session = Session(mbox=make_mbox([]), args=["-sort", "$" + "A" * 300])
    try:
        session.expect("0 Messages")
        session.quit()
    finally:
        session.close()

    # Bug 2: "~" + 2000 non-slash chars. Once expanded this is a
    # legitimately invalid sort key (no such user), so pine's own
    # validation correctly rejects it and exits early, *before* ever
    # reaching MAIN MENU -- that's the correct, pre-existing behavior
    # for a bad -sort value. The only thing under test is that it
    # exits cleanly rather than via ASan's abort() or a real segfault.
    session = Session(mbox=make_mbox([]),
                       args=["-sort", "~" + "B" * 2000],
                       dismiss_splash=False)
    try:
        session.expect(pexpect.EOF, timeout=15)
        session.child.wait()
        assert session.child.signalstatus is None, (
            "pine was killed by signal %r instead of exiting cleanly"
            % session.child.signalstatus)
    finally:
        session.close()
