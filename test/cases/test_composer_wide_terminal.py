# Regression test for a real global-buffer overflow in pico/display.c's
# wkeyhelp() (paints the composer's key-help footer), found while closing
# out TODO-pdp11.md's `s` (pico/edef.h's global scratch buffer) sizing
# item. wkeyhelp() scales its output with term.t_ncol (spaces =
# t_ncol/6) with no bound against `s`'s actual size -- a wide enough
# terminal overflows `s`.
#
# This was masked on this Linux build by a separate, also-fixed bug:
# pico/os_unix.c never included <sys/ioctl.h> on POSIX builds, so
# TIOCGWINSZ was never defined and every "#ifdef TIOCGWINSZ" block
# (window-size reading, SIGWINCH handling) silently compiled out --
# pico and pine's composer were permanently stuck at the 80x24
# compile-time default (NROW/NCOL, pico/os_bsd.h) and never actually
# saw a real terminal resize, no matter the pty's real size. Fixing
# that include (plus a winch_handler()/signal() prototype mismatch --
# an empty-parens K&R declaration vs signal()'s expected void(*)(int)
# -- that the include fix exposed as a hard compile error) restored
# real resize support. With resize actually working, resizing to a
# very wide terminal while composing was confirmed under
# AddressSanitizer to reliably overflow the global `s` via
# wkeyhelp() -- both in the standalone `pico` binary and in pine's
# composer. Fixed by bounding every write in wkeyhelp() against
# sizeof(s) (same idiom as pico/attach.c's ParseAttach fix from the
# same buffer audit), truncating the rendered key-help line instead of
# overflowing.
#
# This test resizes the pty to 20000 columns while the composer is
# open (well past the ~16374-column threshold that overflowed the old
# 16KB `s`, and past the current 8KB size too) and confirms pine is
# still alive and responsive afterward.
import time

from smoketest import Session


def run():
    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")

        session.resize(50, 20000)
        time.sleep(2)

        assert session.child.isalive(), "pine crashed after wide-terminal resize"

        # Confirm the process is still actually responsive to input, not
        # just not-yet-reaped.
        session.sendcontrol("c")
        session.expect("Cancel")
        session.send("y")
        session.expect("MAIN MENU")

        session.quit()
    finally:
        session.close()
