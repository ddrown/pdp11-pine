# `pine -conf` needs no pty -- it just dumps a blank global config to
# stdout and exits. Also checks that variables marked "disabled" during
# the PDP-11 size-reduction cuts (TODO-pdp11.md) stay out of the dump:
# smtp-server (SMTP client removed) and address-book (address book removed).
import subprocess

from smoketest import PINE_BIN, SmoketestFailure


def run():
    result = subprocess.run([PINE_BIN, "-conf"], capture_output=True, text=True, timeout=10)
    if result.returncode != 0:
        raise SmoketestFailure("pine -conf exited with status %r:\n%s"
                                % (result.returncode, result.stderr))

    out = result.stdout
    assert "user-domain=" in out, "expected user-domain var in -conf output"
    assert "inbox-path=" in out, "expected inbox-path var in -conf output"
    assert "smtp-server=" not in out, "removed smtp-server var reappeared in -conf output"
    assert "address-book=" not in out, "removed address-book var reappeared in -conf output"
