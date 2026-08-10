#!/usr/bin/env python3
"""
fix_17_7.py - mechanically fixes MISRA rule 17.7 ("return value of a
non-void function must be used") by prefixing the exact flagged
statements with (void), using the file:line locations straight out of
a cppcheck MISRA report. Only touches lines matching the safe pattern
"<whitespace><identifier>(...);" - anything else is left alone and
printed to stderr for manual review, so nothing gets silently
mis-edited.

Usage:
    python3 fix_17_7.py misra_report_raw.txt
Run it from inside the msmsm/ project folder (the report's file
paths are relative, e.g. "auth.c:30").
"""
import re
import sys
from collections import defaultdict

CALL_START = re.compile(r'^(\s*)([A-Za-z_][A-Za-z0-9_]*)\s*\(')

def main():
    if len(sys.argv) != 2:
        print("usage: fix_17_7.py <misra_report.txt>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1]) as f:
        report = f.read()

    locations = defaultdict(set)
    for m in re.finditer(r'^([A-Za-z0-9_./-]+\.c):(\d+):\d+:.*\[misra-c2012-17\.7\]', report, re.MULTILINE):
        locations[m.group(1)].add(int(m.group(2)))

    total_fixed = 0
    total_skipped = 0

    for filename, lines in locations.items():
        try:
            with open(filename) as f:
                src = f.readlines()
        except FileNotFoundError:
            print(f"SKIP (file not found): {filename}", file=sys.stderr)
            continue

        changed = False
        for lineno in sorted(lines):
            idx = lineno - 1
            if idx < 0 or idx >= len(src):
                continue
            line = src[idx]
            m = CALL_START.match(line)
            if not m:
                print(f"MANUAL REVIEW NEEDED: {filename}:{lineno}: {line.rstrip()}", file=sys.stderr)
                total_skipped += 1
                continue
            indent, ident = m.group(1), m.group(2)
            if ident in ("if", "while", "for", "switch", "return", "sizeof"):
                print(f"MANUAL REVIEW NEEDED: {filename}:{lineno}: {line.rstrip()}", file=sys.stderr)
                total_skipped += 1
                continue
            if line.lstrip().startswith("(void)"):
                continue  # already fixed
            src[idx] = f"{indent}(void){line[len(indent):]}"
            changed = True
            total_fixed += 1

        if changed:
            with open(filename, "w") as f:
                f.writelines(src)

    print(f"\nFixed {total_fixed} call sites automatically.")
    print(f"{total_skipped} sites need manual review (printed above) - "
          f"not touched, to avoid guessing wrong.")

if __name__ == "__main__":
    main()