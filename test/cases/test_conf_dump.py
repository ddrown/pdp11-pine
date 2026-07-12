# Regression check for the init.c cut (TODO-pdp11.md): `-conf` (and the
# dump_global_conf() it drove) was removed as a first-run/discoverability
# convenience this build doesn't need. `pine -conf` should no longer dump
# a blank global config to stdout -- since the multi-char "-conf" check is
# gone, args.c now parses it char-by-char and rejects the unrecognized
# "c" flag, exiting nonzero without ever printing variable names.
import subprocess

from smoketest import PINE_BIN


def run():
    result = subprocess.run([PINE_BIN, "-conf"], capture_output=True, text=True, timeout=10)
    assert result.returncode != 0, "pine -conf unexpectedly succeeded; -conf should be gone"

    out = result.stdout
    assert "user-domain=" not in out, "removed -conf dump reappeared in output"
    assert "inbox-path=" not in out, "removed -conf dump reappeared in output"
    assert "unknown flag" in result.stderr, (
        "expected -conf to be rejected as unknown flags, got: %r" % result.stderr
    )
