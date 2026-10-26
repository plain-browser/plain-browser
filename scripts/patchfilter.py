#!/usr/bin/env python3
"""Split a unified diff by hunk: keep or drop the hunks whose changed lines match a regex.

usage: patchfilter.py <patch> <regex> --keep|--drop [-o OUT]
--keep writes only the hunks whose added/removed lines match the regex, --drop writes the others.
A file section is written only when at least one of its hunks is written. Used for
patches/upstream-revert.list "hunks:" entries: an upstream patch that mixes changes we want with changes we
do not (ungoogled's remove-unused-preferences-fields.patch deletes Safe Browsing prefs alongside others).
Exit code 3 when nothing matched (the upstream patch changed; check the regex).
"""
import argparse, re, sys


def split(text):
    """[(header_lines, [hunk_lines...]), ...]; preamble before the first file header is dropped."""
    lines = text.splitlines(keepends=True)
    files, cur, i = [], None, 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("--- ") and i + 1 < len(lines) and lines[i + 1].startswith("+++ "):
            cur = ([line, lines[i + 1]], [])
            files.append(cur)
            i += 2
            continue
        if cur is not None:
            if line.startswith("@@"):
                cur[1].append([line])
            elif cur[1] and not line.startswith("diff ") and not line.startswith("index "):
                cur[1][-1].append(line)
        i += 1
    return files


def filter_patch(text, regex, keep):
    pat = re.compile(regex)
    out, matched = [], 0
    for header, hunks in split(text):
        chosen = []
        for hunk in hunks:
            hit = any(pat.search(l) for l in hunk[1:] if l[:1] in "+-")
            matched += hit
            if hit == keep:
                chosen.append(hunk)
        if chosen:
            out += header
            for hunk in chosen:
                out += hunk
    return "".join(out), matched


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("patch")
    ap.add_argument("regex")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--keep", action="store_true")
    g.add_argument("--drop", action="store_true")
    ap.add_argument("-o", "--out", default=None, help="output file (default: stdout)")
    a = ap.parse_args()
    text = open(a.patch, encoding="utf-8", errors="surrogateescape").read()
    result, matched = filter_patch(text, a.regex, a.keep)
    if a.out:
        open(a.out, "w", encoding="utf-8", errors="surrogateescape", newline="").write(result)
    else:
        sys.stdout.write(result)
    print(f"{matched} hunks matched {a.regex!r}", file=sys.stderr)
    return 0 if matched else 3


if __name__ == "__main__":
    sys.exit(main())
