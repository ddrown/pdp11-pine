# Regression test for real stack/heap overflows in
# pine/mailcmd.c's save_attachment() (TODO-pdp11.md's buffer-overflow
# audit -- the item literally named "save_attachment() filename[MAXPATH+1]
# overflow").
#
# save_attachment() pre-filled the Save-as filename prompt with the
# attachment's suggested name straight from the MIME Content-Type
# "name" parameter:
#
#   char filename[MAXPATH+1];        /* 4097 bytes */
#   strcpy(filename, param->value);  /* unbounded, no length cap */
#
# param->value is cpystr()'d from the message with no length cap
# anywhere upstream, so a long enough "name" parameter overflows this
# fixed stack buffer -- before the length-capped optionally_enter()
# prompt/edit step ever runs. Confirmed under valgrind: with a
# 6000-byte name, the corrupted stack later manifests as "Jump to the
# invalid address 0x4141414141414141" (a smashed return address), and
# reliably aborts even on a plain (non-ASan, non-valgrind) build.
#
# Fixing that turned up two more related overflows reachable from the
# very same "View/Save attachment" flow, once full_filename (built from
# the now-still-attacker-influenced filename) is echoed back to the
# user:
#   - `sprintf(prompt_buf, "File \"%s\" already exists...", full_filename)`
#     into a fixed `char prompt_buf[200]` when the destination file
#     already exists.
#   - Every q_status_message2()/q_status_message7() call reporting
#     success/failure embeds full_filename, formatted into the shared
#     tmp_20k_buf -- which starts at only a 2048-byte baseline and was
#     never grown at this call site.
#
# Fixed by: truncating the initial strcpy to MAXPATH (matching what
# optionally_enter() would enforce on user edits anyway), capping
# prompt_buf's %s precision at 150, and calling grow_tmp_20k_buf()
# before each status-message call that embeds full_filename.
from smoketest import Session

LONG_NAME = "A" * 6000

MBOX = (
    "From smoketest@example.com Mon Jan  1 00:00:00 2024\n"
    "Date: Mon, 1 Jan 2024 00:00:00 -0000\n"
    "From: smoketest@example.com\n"
    "To: user@example.com\n"
    "Subject: save_attachment overflow test\n"
    "MIME-Version: 1.0\n"
    "Content-Type: multipart/mixed; boundary=\"BOUND\"\n"
    "\n"
    "--BOUND\n"
    "Content-Type: text/plain; charset=us-ascii\n"
    "\n"
    "Hello.\n"
    "--BOUND\n"
    "Content-Type: application/octet-stream; name=\"" + LONG_NAME + "\"\n"
    "\n"
    "somedata\n"
    "--BOUND--\n"
)


def run():
    session = Session(mbox=MBOX)
    try:
        session.expect("1 Message")
        session.send("i")
        session.expect("FOLDER INDEX")
        session.expect("save_attachment overflow test")

        session.send("\r")
        session.expect("MESSAGE TEXT", timeout=15)
        session.send("v")
        session.expect("Enter attachment number")
        session.send("2\r")
        session.expect("Save or View attachment")
        session.send("s")
        session.expect("save attachment in", timeout=15)
        session.send("saved_attach\r")
        # Real assertion is implicit: reaching either outcome without a
        # crash means save_attachment() survived formatting the
        # oversized name into filename[]/full_filename and into the
        # status-message buffers that echo it back. The local
        # filesystem's own path-component length limit means this
        # absurdly long name will usually fail to actually write
        # (ENAMETOOLONG), which is fine -- that's not what's under test.
        session.expect("written to|Error opening destination|MESSAGE TEXT",
                        timeout=15)

        session.send("m")
        session.expect("MAIN MENU")
        session.quit()
    finally:
        session.close()
