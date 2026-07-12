# Regression test for a real bug in rfc822_date() (imap/ANSI/c-client/
# os_lnx.c): the Date header stamped on every sent/postponed message
# used the wrong UTC offset whenever DST was in effect, regardless of
# the system's actual configured timezone.
#
# Root cause: rfc822_date() computed the zone from the `struct
# timezone` that gettimeofday() optionally fills in:
#   zone = (t->tm_isdst ? 60 : 0) - tz.tz_minuteswest;
# but glibc's gettimeofday() no longer populates that struct from
# /etc/localtime (or the TZ environment) at all -- on modern Linux it
# always reports tz_minuteswest=0, tz_dsttime=0. So whenever
# t->tm_isdst is true, "zone" always came out to exactly 60 minutes,
# producing a Date header of "+0100" no matter what timezone the
# machine is actually in (confirmed on a box configured for
# America/Chicago/CDT, i.e. the real offset should be "-0500").
#
# Sibling os-dependent files in the same c-client tree (os_bsi.c,
# os_osf.c) already used the correct, simpler approach: struct tm's
# tm_gmtoff field (populated correctly by localtime() itself) directly
# gives the real UTC offset in seconds, no gettimeofday()/struct
# timezone needed. Fixed os_lnx.c's rfc822_date() to match.
#
# This test postpones a message (which stamps outgoing->date via
# rfc822_date() exactly like Send does -- see pine/send.c) and checks
# the resulting Date header's UTC offset against Python's own idea of
# the local UTC offset, rather than hardcoding an expected value, so
# the test remains correct regardless of which timezone/DST state the
# machine running it happens to be in.
import os
import time

from smoketest import Session


def run():
    session = Session(mbox=None)
    try:
        session.send("c")
        session.expect("To")
        session.send("u@example.com")
        for _ in range(2):  # To -> Cc -> Subject
            session.sendcontrol("n")
        session.expect("Subject")

        session.send("tz test")
        session.sendcontrol("n")  # Subject -> message body
        session.expect("Message Text")

        session.send("body\n")
        session.sendcontrol("o")  # Postpone
        session.expect("postponed", timeout=15)

        session.quit()

        path = os.path.join(session.home, "mail", "postponed-mail")
        with open(path) as f:
            content = f.read()

        date_line = None
        for line in content.splitlines():
            if line.startswith("Date:"):
                date_line = line
                break
        assert date_line is not None, "no Date header in postponed message"

        offset_str = date_line.rsplit(" ", 1)[-1]
        assert len(offset_str) == 5 and offset_str[0] in "+-", (
            "Date header missing a well-formed numeric UTC offset: %r" % date_line
        )
        got_offset_minutes = int(offset_str[0] + offset_str[1:3]) * 60 + int(
            offset_str[0] + offset_str[3:5]
        )

        expected_offset_seconds = -time.timezone
        if time.localtime().tm_isdst > 0 and time.daylight:
            expected_offset_seconds = -time.altzone
        expected_offset_minutes = expected_offset_seconds // 60

        assert got_offset_minutes == expected_offset_minutes, (
            "Date header UTC offset %s (%d min) doesn't match system "
            "timezone (%d min)" % (offset_str, got_offset_minutes, expected_offset_minutes)
        )
    finally:
        session.close()
