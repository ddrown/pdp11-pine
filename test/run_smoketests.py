#!/usr/bin/env python3
"""Discover and run every test/cases/test_*.py smoketest.

Usage: test/run_smoketests.py [--coverage] [name-substring ...]

Each cases/test_*.py module must define run(), which drives bin/pine
via test/smoketest.Session and raises AssertionError on failure.
Requires the pine binary to already be built (./build lnx) and the
pexpect package (pip install pexpect).

--coverage rebuilds pine/*.o with gcc's --coverage instrumentation
(via the PROFILE= build variable in pine/makefile.lnx), relinks
bin/pine, runs the suite, then prints a per-file gcov summary. This
only instruments pine/*.c -- pico and c-client are separate makefiles
and stay uninstrumented. The rebuilt bin/pine is left instrumented
afterward (larger, slower); run ./build lnx again for a clean binary
before taking PDP-11 size measurements.
"""

import glob
import importlib
import os
import subprocess
import sys
import traceback
from concurrent.futures import ThreadPoolExecutor, as_completed

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(TEST_DIR)
PINE_DIR = os.path.join(REPO_ROOT, "pine")

sys.path.insert(0, TEST_DIR)
sys.path.insert(0, os.path.join(TEST_DIR, "cases"))


def discover(filters):
    names = []
    for path in sorted(glob.glob(os.path.join(TEST_DIR, "cases", "test_*.py"))):
        name = os.path.splitext(os.path.basename(path))[0]
        if not filters or any(f in name for f in filters):
            names.append(name)
    return names


def _rm_glob(pattern):
    for path in glob.glob(os.path.join(PINE_DIR, pattern)):
        os.remove(path)


def build_with_coverage():
    print("rebuilding pine/*.o with --coverage instrumentation...", flush=True)
    _rm_glob("*.o")
    _rm_glob("*.gcno")
    _rm_glob("*.gcda")
    _rm_glob("*.gcov")

    result = subprocess.run(
        ["make", "-f", "makefile.lnx", "PROFILE=--coverage"],
        cwd=PINE_DIR,
    )
    if result.returncode != 0:
        print("coverage build failed")
        return False

    pine_bin = os.path.join(REPO_ROOT, "bin", "pine")
    if os.path.exists(pine_bin):
        os.remove(pine_bin)
    os.link(os.path.join(PINE_DIR, "pine"), pine_bin)
    return True


def report_coverage():
    gcda_files = sorted(glob.glob(os.path.join(PINE_DIR, "*.gcda")))
    if not gcda_files:
        print("no .gcda files produced -- did any test actually run pine to completion?")
        return

    rows = []
    total_executed = 0
    total_lines = 0
    for gcda in gcda_files:
        source = os.path.splitext(os.path.basename(gcda))[0] + ".c"
        result = subprocess.run(
            ["gcov", "-o", PINE_DIR, source],
            cwd=PINE_DIR, capture_output=True, text=True,
        )
        pct, executed, lines = None, None, None
        for line in result.stdout.splitlines():
            if line.startswith("Lines executed:"):
                # "Lines executed:33.72% of 940"
                pct_str, _, lines_str = line[len("Lines executed:"):].partition(" of ")
                pct = float(pct_str.rstrip("%"))
                lines = int(lines_str)
                executed = round(pct / 100.0 * lines)
                break
        if pct is None:
            continue
        rows.append((source, pct, executed, lines))
        total_executed += executed
        total_lines += lines

    _rm_glob("*.gcov")

    rows.sort(key=lambda r: r[1])
    print()
    print("%-16s %8s %12s" % ("file", "coverage", "lines"))
    for source, pct, executed, lines in rows:
        print("%-16s %7.1f%% %6d/%-6d" % (source, pct, executed, lines))

    if total_lines:
        print()
        print("TOTAL: %.1f%% (%d/%d lines)" %
              (100.0 * total_executed / total_lines, total_executed, total_lines))

    print()
    print("bin/pine is now instrumented (larger/slower) -- run ./build lnx "
          "again before taking PDP-11 size measurements.")


def _run_one(name):
    """Run a single test module by name. Returns (name, ok, output),
    where output is "" on success or the formatted traceback on
    failure -- captured as a string rather than printed directly, so
    concurrent callers (see main()'s thread pool) can't interleave
    partial output from different tests.
    """
    try:
        module = importlib.import_module(name)
        module.run()
    except Exception:
        return name, False, traceback.format_exc()
    return name, True, ""


def _report(name, ok, output):
    print("%-40s%s" % (name, "ok" if ok else "FAIL"))
    if not ok:
        sys.stdout.write(output)


def main():
    args = sys.argv[1:]
    coverage = "--coverage" in args
    filters = [a for a in args if a != "--coverage"]

    if coverage:
        if not build_with_coverage():
            return 1

    names = discover(filters)
    if not names:
        print("no matching smoketests found")
        return 1

    failures = []
    if coverage:
        # Run serially: gcov's .gcda counter files accumulate per
        # source file across every bin/pine invocation in a run, and
        # concurrent processes writing them isn't worth the risk in
        # this maintenance/debug-only mode.
        for name in names:
            name, ok, output = _run_one(name)
            _report(name, ok, output)
            if not ok:
                failures.append(name)
    else:
        with ThreadPoolExecutor(max_workers=min(len(names), os.cpu_count() or 4)) as pool:
            futures = [pool.submit(_run_one, name) for name in names]
            for future in as_completed(futures):
                name, ok, output = future.result()
                _report(name, ok, output)
                if not ok:
                    failures.append(name)

    print()
    print("%d/%d passed" % (len(names) - len(failures), len(names)))

    if coverage:
        report_coverage()

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
