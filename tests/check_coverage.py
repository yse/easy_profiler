#!/usr/bin/env python3
"""Gate global LLVM coverage and, for pull requests, changed executable lines."""

import argparse
import json
import re
import sys
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("coverage_json", type=Path)
    parser.add_argument("--diff", type=Path)
    parser.add_argument("--lcov", type=Path)
    parser.add_argument("--min-lines", type=float, default=80.0)
    parser.add_argument("--min-branches", type=float, default=82.0)
    parser.add_argument("--min-changed-lines", type=float, default=80.0)
    return parser.parse_args()


def changed_lines(diff_path):
    if diff_path is None or not diff_path.exists():
        return {}

    changed = {}
    filename = None
    hunk = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")
    for line in diff_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("+++ b/"):
            filename = line[6:]
            continue
        match = hunk.match(line)
        if filename is None or match is None:
            continue
        start = int(match.group(1))
        count = int(match.group(2) or "1")
        if count:
            changed.setdefault(filename, set()).update(range(start, start + count))
    return changed


def normalized(filename):
    filename = filename.replace("\\", "/")
    marker = "easy_profiler_core/"
    position = filename.rfind(marker)
    return filename[position:] if position >= 0 else filename


def read_lcov(report_path):
    counts = {}
    filename = None
    for entry in report_path.read_text(encoding="utf-8").splitlines():
        if entry.startswith("SF:"):
            filename = normalized(entry[3:])
            counts.setdefault(filename, {})
        elif entry.startswith("DA:") and filename is not None:
            fields = entry[3:].split(",")
            line = int(fields[0])
            count = int(fields[1])
            old_count = counts[filename].get(line, 0)
            counts[filename][line] = max(old_count, count)
        elif entry == "end_of_record":
            filename = None
    return counts


def changed_line_coverage(line_counts, changed):
    coverable = 0
    covered = 0
    for filename, wanted in changed.items():
        counts = line_counts.get(filename, {})
        executable = wanted.intersection(counts)
        coverable += len(executable)
        covered += sum(counts[line] > 0 for line in executable)
    return covered, coverable


def main():
    args = parse_args()
    payload = json.loads(args.coverage_json.read_text(encoding="utf-8"))
    totals = payload["data"][0]["totals"]
    lines = float(totals["lines"]["percent"])
    branches = float(totals["branches"]["percent"])

    print(f"core coverage: lines {lines:.2f}%, branches {branches:.2f}%")
    failed = lines < args.min_lines or branches < args.min_branches

    changed = changed_lines(args.diff)
    if changed and (args.lcov is None or not args.lcov.exists()):
        print("coverage gate failed: --lcov is required for changed-line coverage", file=sys.stderr)
        failed = True
    elif changed:
        covered, coverable = changed_line_coverage(read_lcov(args.lcov), changed)
        if coverable:
            percent = 100.0 * covered / coverable
            print(f"changed executable lines: {percent:.2f}% ({covered}/{coverable})")
            failed = failed or percent < args.min_changed_lines
        else:
            print("changed executable lines: none found in the coverage map")
    else:
        print("changed executable lines: not applicable")

    if failed:
        print(
            f"coverage gate failed (minimum lines {args.min_lines:.2f}%, "
            f"branches {args.min_branches:.2f}%, changed lines "
            f"{args.min_changed_lines:.2f}%)",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
