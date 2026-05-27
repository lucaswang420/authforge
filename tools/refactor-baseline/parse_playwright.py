#!/usr/bin/env python3
"""Parse Playwright e2e outputs into a deterministic baseline text file.

Two input formats are supported:

  --list    `playwright test --list` console output. Every chromium row is
            recorded as `LISTED`. Lets us snapshot baseline in environments
            where the backend can't run.

  --json    `playwright test --reporter=json` output. Each spec becomes one
            record per (project, status). Status is normalized to PASSED /
            FAILED / SKIPPED / TIMEDOUT / FLAKY.

Output format (one record per line, sorted lexicographically by id):

    <ID>\t<STATUS>

where ID is `<project> > <relative-spec-path>:<line>:<col> > <title-chain>`.
The leading project + spec path + line:col makes records position-stable so
later phases can compute set inclusion against the P0 baseline regardless of
test reordering.

Usage:
    parse_playwright.py --list  <list-output.txt>  --out <out.txt>
    parse_playwright.py --json  <reporter-json>    --out <out.txt>
    parse_playwright.py --selftest

Spec references:
    _Design: §2.8 P0, §12.3, Property 8_
    _Requirements: 14.1, 18.1, 18.2_
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

# ----------------------------------------------------------------------------
# `playwright test --list` parser
# ----------------------------------------------------------------------------

# Examples (Playwright 1.4x, U+203A right-pointing single guillemet):
#   "  [chromium] > auth.spec.ts:9:3 > Authentication > redirects to login"
#
# We accept both U+203A and the ASCII '>' fallback for resilience under
# code-page rewriting on Windows consoles.
_LIST_LINE_RE = re.compile(
    r"^\s*\[(?P<project>[^\]]+)\]\s*[\u203a>]\s*"
    r"(?P<spec>\S+\.spec\.[a-zA-Z]+):(?P<line>\d+):(?P<col>\d+)\s*"
    r"[\u203a>]\s*(?P<title>.+?)\s*$"
)


def parse_list(text: str) -> List[Tuple[str, str]]:
    out: List[Tuple[str, str]] = []
    for raw in text.splitlines():
        m = _LIST_LINE_RE.match(raw)
        if not m:
            continue
        project = m.group("project").strip()
        spec = m.group("spec").strip()
        line = m.group("line")
        col = m.group("col")
        # Normalize U+203A inside the title to ASCII '>' so the baseline file
        # is pure ASCII and survives Windows code-page round trips.
        title = m.group("title").strip().replace("\u203a", ">")
        rid = f"{project} > {spec}:{line}:{col} > {title}"
        out.append((rid, "LISTED"))
    return out


# ----------------------------------------------------------------------------
# `playwright test --reporter=json` parser
# ----------------------------------------------------------------------------


def _walk_specs(node: dict, path: List[str]) -> Iterable[Tuple[List[str], dict]]:
    for spec in node.get("specs", []) or []:
        yield path, spec
    for child in node.get("suites", []) or []:
        child_title = child.get("title") or child.get("file") or ""
        yield from _walk_specs(child, path + [child_title])


def parse_json(text: str) -> List[Tuple[str, str]]:
    data = json.loads(text)
    out: List[Tuple[str, str]] = []
    for top in data.get("suites", []) or []:
        top_title = top.get("title") or top.get("file") or ""
        for path, spec in _walk_specs(top, [top_title]):
            spec_file = spec.get("file") or top.get("file") or ""
            line = spec.get("line", 0)
            col = spec.get("column", 0)
            spec_title = spec.get("title") or ""
            # path is the suite chain leading to this spec; drop the file
            # title at index 0 so we mirror the --list output, which only
            # includes the describe-suite chain after the file.
            suite_chain = " > ".join([p for p in path[1:] if p])
            display_title = f"{suite_chain} > {spec_title}" if suite_chain else spec_title
            for tcase in spec.get("tests", []) or []:
                project = tcase.get("projectName") or "default"
                results = tcase.get("results") or []
                if not results:
                    # `playwright test --list --reporter=json` emits specs
                    # without results; treat them as LISTED so the parser
                    # still produces a deterministic baseline.
                    status = "LISTED"
                else:
                    last = results[-1]
                    status_kw = (last.get("status") or "unknown").lower()
                    status = {
                        "passed": "PASSED",
                        "failed": "FAILED",
                        "timedout": "TIMEDOUT",
                        "skipped": "SKIPPED",
                        "interrupted": "FAILED",
                        "expected": "PASSED",
                        "flaky": "FLAKY",
                    }.get(status_kw, "UNKNOWN")
                rid = f"{project} > {spec_file}:{line}:{col} > {display_title}"
                out.append((rid, status))
    return out


# ----------------------------------------------------------------------------
# Render
# ----------------------------------------------------------------------------


def render(records: List[Tuple[str, str]]) -> str:
    if not records:
        raise SystemExit("[parse_playwright] error: no records parsed")
    # Dedupe (test --list can repeat rows when a spec uses data-driven loops;
    # keep first occurrence). Sort using a tuple key that puts line/col as
    # integers so `auth:9:3` precedes `auth:14:3` (string sort would invert
    # them).
    seen: Dict[str, str] = {}
    for rid, status in records:
        if rid not in seen:
            seen[rid] = status

    _SORT_RE = re.compile(
        r"^(?P<project>[^>]+)>\s*(?P<spec>\S+):(?P<line>\d+):(?P<col>\d+)\s*>\s*(?P<title>.*)$"
    )

    def sort_key(rid: str):
        m = _SORT_RE.match(rid)
        if not m:
            return (rid, 0, 0, "")
        return (
            m.group("project").strip(),
            m.group("spec").strip(),
            int(m.group("line")),
            int(m.group("col")),
            m.group("title").strip(),
        )

    lines = [f"{rid}\t{seen[rid]}" for rid in sorted(seen.keys(), key=sort_key)]
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------
# Self-test
# ----------------------------------------------------------------------------

_FIXTURE_DIR = Path(__file__).resolve().parent / "fixtures"


def _selftest() -> int:
    list_path = _FIXTURE_DIR / "playwright-list.sample.txt"
    json_path = _FIXTURE_DIR / "playwright-json.sample.json"
    if not list_path.exists() or not json_path.exists():
        print(
            f"[parse_playwright][err] missing fixtures under {_FIXTURE_DIR}",
            file=sys.stderr,
        )
        return 2

    list_records = parse_list(list_path.read_text(encoding="utf-8"))
    if len(list_records) != 5:
        print(
            f"[parse_playwright][err] list mode: expected 5 rows, got {len(list_records)}",
            file=sys.stderr,
        )
        return 1
    list_rendered = render(list_records)
    list_lines = list_rendered.splitlines()
    if not all(line.endswith("\tLISTED") for line in list_lines):
        print(
            "[parse_playwright][err] list mode: not every row ends with LISTED",
            file=sys.stderr,
        )
        return 1
    expected_first_list = (
        "chromium > auth.spec.ts:9:3 > Authentication > redirects to login when not authenticated\tLISTED"
    )
    if list_lines[0] != expected_first_list:
        print(
            f"[parse_playwright][err] list first line {list_lines[0]!r} != {expected_first_list!r}",
            file=sys.stderr,
        )
        return 1

    json_records = parse_json(json_path.read_text(encoding="utf-8"))
    if len(json_records) != 5:
        print(
            f"[parse_playwright][err] json mode: expected 5 records, got {len(json_records)}",
            file=sys.stderr,
        )
        return 1
    json_rendered = render(json_records)
    json_lines = json_rendered.splitlines()
    passed = [ln for ln in json_lines if ln.endswith("\tPASSED")]
    skipped = [ln for ln in json_lines if ln.endswith("\tSKIPPED")]
    if len(passed) != 4 or len(skipped) != 1:
        print(
            f"[parse_playwright][err] json mode: expected 4 PASSED + 1 SKIPPED, got "
            f"{len(passed)} PASSED + {len(skipped)} SKIPPED",
            file=sys.stderr,
        )
        return 1
    expected_skipped = (
        "chromium > navigation.spec.ts:10:3 > Navigation > clicking logo returns home\tSKIPPED"
    )
    if skipped[0] != expected_skipped:
        print(
            f"[parse_playwright][err] json mode skipped row mismatch: {skipped[0]!r}",
            file=sys.stderr,
        )
        return 1

    print("[parse_playwright] selftest OK (list 5 rows; json 4 PASSED + 1 SKIPPED).")
    return 0


# ----------------------------------------------------------------------------
# CLI
# ----------------------------------------------------------------------------


def _main() -> int:
    p = argparse.ArgumentParser(
        description="Parse Playwright outputs into a deterministic baseline file"
    )
    src = p.add_mutually_exclusive_group()
    src.add_argument("--list", dest="list_path", help="path to `playwright test --list` output")
    src.add_argument("--json", dest="json_path", help="path to `playwright test --reporter=json` output")
    p.add_argument("--out", help="path to write parsed baseline")
    p.add_argument(
        "--print", dest="print_only", action="store_true",
        help="print parsed baseline to stdout instead of writing --out",
    )
    p.add_argument("--selftest", action="store_true")
    args = p.parse_args()

    if args.selftest:
        return _selftest()

    if args.list_path:
        text = Path(args.list_path).read_text(encoding="utf-8-sig", errors="replace")
        records = parse_list(text)
    elif args.json_path:
        text = Path(args.json_path).read_text(encoding="utf-8-sig")
        records = parse_json(text)
    else:
        p.error("--list or --json is required (or use --selftest)")

    rendered = render(records)
    if args.print_only or not args.out:
        sys.stdout.write(rendered)
        return 0

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(rendered, encoding="utf-8", newline="\n")
    statuses = {st for _, st in records}
    print(
        f"[parse_playwright] wrote {len(records)} records to {out_path} "
        f"(statuses={sorted(statuses)})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(_main())
