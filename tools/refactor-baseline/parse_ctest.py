#!/usr/bin/env python3
"""Parse ctest -N (list) and ctest --output-on-failure (run) outputs into a
deterministic baseline text file.

Output format (one record per line, sorted by test number):

    <NN>\t<TEST_NAME>\t<STATUS>

where STATUS is one of:
    LISTED   - test exists in `ctest -N` listing only
    PASSED   - `Test #NN: NAME ... Passed`
    FAILED   - `Test #NN: NAME ... Failed`
    SKIPPED  - `Test #NN: NAME ... Skipped`

The deterministic format lets later phases (and `tools/check-ctest-coverage.sh`)
diff against the P0 baseline byte-for-byte regardless of timing jitter or
locale (we strip durations and progress prefixes).

Usage:
    parse_ctest.py --listing <ctest-N.txt> --run <ctest-run.txt> --out <out.txt>
    parse_ctest.py --selftest                         # run built-in fixtures
    parse_ctest.py --print --listing <ctest-N.txt>    # print parsed listing only

Spec references:
    _Design: §2.8 P0, §12.1, Property 3_
    _Requirements: 14.1, 17.5_
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict

# ----------------------------------------------------------------------------
# Parser
# ----------------------------------------------------------------------------

# Matches lines like "  Test #12: SomeTestName" emitted by `ctest -N`.
_LISTING_RE = re.compile(r"^\s*Test\s+#(\d+):\s+(\S.*?)\s*$")

# Matches lines like "12/45 Test #12: SomeTestName .... Passed   0.31 sec"
# (or Failed / Skipped). The test name may contain dots, so we tolerate the
# leader-dots before the status keyword.
_RUN_RE = re.compile(
    r"^\s*\d+/\d+\s+Test\s+#(\d+):\s+(\S.*?)\s+\.+\s+"
    r"(Passed|Failed|Skipped|Timeout|\*\*\*Failed)\b",
    re.IGNORECASE,
)


def parse_listing(text: str) -> Dict[int, str]:
    """Parse `ctest -N` output into {number -> name}."""
    out: Dict[int, str] = {}
    for raw in text.splitlines():
        m = _LISTING_RE.match(raw)
        if not m:
            continue
        num = int(m.group(1))
        name = m.group(2).strip()
        out[num] = name
    return out


def parse_run(text: str) -> Dict[int, str]:
    """Parse `ctest --output-on-failure` output into {number -> status}."""
    out: Dict[int, str] = {}
    for raw in text.splitlines():
        m = _RUN_RE.match(raw)
        if not m:
            continue
        num = int(m.group(1))
        kw = m.group(3).lower()
        if kw == "passed":
            status = "PASSED"
        elif kw in ("failed", "***failed", "timeout"):
            status = "FAILED"
        elif kw == "skipped":
            status = "SKIPPED"
        else:
            status = "UNKNOWN"
        out[num] = status
    return out


def merge(listing: Dict[int, str], run: Dict[int, str]) -> str:
    """Merge listing + run into the deterministic textual baseline."""
    if not listing:
        raise SystemExit("[parse_ctest] error: empty test listing")
    lines = []
    for num in sorted(listing.keys()):
        name = listing[num]
        status = run.get(num, "LISTED")
        lines.append(f"{num:04d}\t{name}\t{status}")
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------
# Self-test
# ----------------------------------------------------------------------------

_SELFTEST_FIXTURE_DIR = Path(__file__).resolve().parent / "fixtures"


def _selftest() -> int:
    listing_path = _SELFTEST_FIXTURE_DIR / "ctest-N.sample.txt"
    run_path = _SELFTEST_FIXTURE_DIR / "ctest-pass.sample.txt"
    if not listing_path.exists() or not run_path.exists():
        print(
            "[parse_ctest][err] missing fixtures under {}".format(
                _SELFTEST_FIXTURE_DIR
            ),
            file=sys.stderr,
        )
        return 2
    listing = parse_listing(listing_path.read_text(encoding="utf-8"))
    run = parse_run(run_path.read_text(encoding="utf-8"))
    if len(listing) != 10:
        print(
            f"[parse_ctest][err] expected 10 listed tests, got {len(listing)}",
            file=sys.stderr,
        )
        return 1
    if len(run) != 10:
        print(
            f"[parse_ctest][err] expected 10 run records, got {len(run)}",
            file=sys.stderr,
        )
        return 1
    if any(status != "PASSED" for status in run.values()):
        print(
            "[parse_ctest][err] expected every fixture run record PASSED",
            file=sys.stderr,
        )
        return 1
    if listing[1] != "AuthServiceGetUserInfoTest":
        print(
            f"[parse_ctest][err] listing[1] mismatch: {listing[1]!r}",
            file=sys.stderr,
        )
        return 1
    rendered = merge(listing, run)
    expected_first = "0001\tAuthServiceGetUserInfoTest\tPASSED"
    expected_last = "0010\tValidatorClientIdTest\tPASSED"
    first_line = rendered.splitlines()[0]
    last_line = rendered.splitlines()[-1]
    if first_line != expected_first:
        print(
            f"[parse_ctest][err] first line {first_line!r} != {expected_first!r}",
            file=sys.stderr,
        )
        return 1
    if last_line != expected_last:
        print(
            f"[parse_ctest][err] last line {last_line!r} != {expected_last!r}",
            file=sys.stderr,
        )
        return 1
    print("[parse_ctest] selftest OK (10 tests, all PASSED).")
    return 0


# ----------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------


def _main() -> int:
    p = argparse.ArgumentParser(
        description="Parse ctest -N + ctest run outputs into a baseline file"
    )
    p.add_argument("--listing", help="path to `ctest -N` output")
    p.add_argument("--run", help="path to `ctest --output-on-failure` output")
    p.add_argument("--out", help="path to write parsed baseline")
    p.add_argument(
        "--print",
        dest="print_only",
        action="store_true",
        help="print parsed baseline to stdout instead of writing --out",
    )
    p.add_argument(
        "--selftest", action="store_true", help="run built-in fixture self-test"
    )
    args = p.parse_args()

    if args.selftest:
        return _selftest()

    if not args.listing:
        p.error("--listing is required (or use --selftest)")
    listing_text = Path(args.listing).read_text(encoding="utf-8")
    run_text = (
        Path(args.run).read_text(encoding="utf-8") if args.run else ""
    )
    listing = parse_listing(listing_text)
    run = parse_run(run_text) if run_text else {}
    rendered = merge(listing, run)

    if args.print_only or not args.out:
        sys.stdout.write(rendered)
        return 0

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(rendered, encoding="utf-8", newline="\n")
    print(
        f"[parse_ctest] wrote {len(listing)} records to {out_path} "
        f"(run records: {len(run)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(_main())
