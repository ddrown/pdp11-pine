# Regression test for TODO-pdp11.md's "pico/display.c's resize_pico()
# uninitialized pscreen-cell gap" item. resize_pico() (~line 1694)
# mirrors vtinit()'s per-row malloc/realloc for both vscreen and
# pscreen, but only vscreen got a "don't let any garbage in" fill
# (vteeol()) for newly-added rows/columns -- pscreen's equivalent rows
# were left with whatever malloc()/realloc() happened to return.
#
# Reachable via an ordinary terminal resize while composing: shrink the
# terminal first (so a later grow has to both malloc brand new rows and
# realloc existing ones wider), then grow past the *original* terminal
# size so resize_pico() actually has new/widened space to fill.
#
# This doesn't reliably surface as a crash in a plain pty session --
# it's stale/uninitialized data feeding update()'s vscreen-vs-pscreen
# diff, not a bounds violation -- so it was actually confirmed and
# fixed via a standalone AddressSanitizer-adjacent check: running
# `bin/pico` directly under `valgrind --track-origins=yes` through the
# same shrink-then-grow sequence reproduced two "Conditional jump or
# move depends on uninitialised value(s)" errors in updateline(),
# originating from resize_pico()'s malloc() call, before the fix, and
# confirmed clean (those two errors gone) after. Driving it through
# `bin/pine` instead of standalone `bin/pico` doesn't work for that
# valgrind check without `--trace-children=yes`, since the composer
# runs as a separate exec'd `pico` subprocess (see TODO-pdp11-done.md's
# "Run the composer as a separate exec'd process" entry) -- valgrind
# doesn't follow it by default.
#
# This smoketest doesn't have valgrind wired in (see
# run_smoketests.py), so it can only check for the coarser regression
# (no crash, still responsive) -- the uninitialized-value fix itself
# was verified manually as described above, not by this test.
import time

from smoketest import Session


def run():
    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")

        # Shrink first so the later grow has to both malloc() brand new
        # rows and realloc() existing ones wider -- exercises both
        # branches of resize_pico()'s loop.
        session.resize(15, 40)
        time.sleep(1)

        # Grow past the *original* 24x80 default, not just back to it.
        session.resize(45, 160)
        time.sleep(1)

        assert session.child.isalive(), "pine crashed after grow-resize"

        session.send("more text after resizing")
        time.sleep(1)
        assert session.child.isalive(), "pine crashed after typing post-resize"

        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
