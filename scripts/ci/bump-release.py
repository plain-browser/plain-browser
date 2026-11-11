#!/usr/bin/env python3
"""Prepare the next automated release for a new ungoogled-chromium-windows tag (scripts/ci/pipeline.py runs this).

Pins the tag and the commit it points at in versions.toml, raises the product version by one patch level (0.1.5-alpha.1 -> 0.1.6-alpha.1, 0.2.0-beta.1 -> 0.2.1-beta.1: the
Store package version is major.minor.patch.0 and must grow with every submission), regenerates version.h and updates
the places that name the version (README, website, PRIVACY, CHANGELOG). Prints the new product version on stdout and
leaves the edited files in the working tree; the caller commits them.

usage: bump-release.py --ucw-tag 156.0.8078.79-1.1 --ucw-commit <40 hex> [--date 2026-11-04]
"""
import argparse, datetime, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TAG_RE = re.compile(r"^(\d+\.\d+\.\d+\.\d+)-\d+(\.\d+)?$")


def edit(path, fn, optional=False):
    p = ROOT / path
    old = p.read_text(encoding="utf-8")
    new = fn(old)
    if new == old:
        if optional:
            print(f"{path}: the version is not named there, left as it is", file=sys.stderr)
            return
        raise SystemExit(f"{path}: nothing to change (the expected text was not found)")
    p.write_text(new, encoding="utf-8")


def sub_once(pattern, repl, text, flags=0, what=""):
    new, n = re.subn(pattern, repl, text, count=1, flags=flags)
    if n != 1:
        raise SystemExit(f"pattern not found: {what or pattern}")
    return new


def next_version(full):
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)-(alpha|beta)\.\d+$", full)
    if not m:
        raise SystemExit("unexpected product version: " + full)
    return f"{m.group(1)}.{m.group(2)}.{int(m.group(3)) + 1}-{m.group(4)}.1"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ucw-tag", required=True)
    ap.add_argument("--ucw-commit", required=True, help="the commit the tag points at (prepare-tree.ps1 checks the clone against it)")
    ap.add_argument("--date", default=datetime.date.today().isoformat())
    args = ap.parse_args()
    m = TAG_RE.match(args.ucw_tag)
    if not m:
        raise SystemExit("unexpected ungoogled-chromium-windows tag: " + args.ucw_tag)
    chromium = m.group(1)
    if not re.fullmatch(r"[0-9a-f]{40}", args.ucw_commit):
        raise SystemExit("unexpected commit: " + args.ucw_commit)

    toml = (ROOT / "versions.toml").read_text(encoding="utf-8")
    old_tag = re.search(r'^ungoogled_chromium_windows\s*=\s*"([^"]+)"', toml, re.M).group(1)
    old_chromium = TAG_RE.match(old_tag).group(1)
    old_ver = re.search(r'(?ms)^\[product\].*?^version\s*=\s*"([^"]+)"', toml).group(1)
    new_ver = next_version(old_ver)
    pkg = new_ver.split("-")[0] + ".0"

    def versions_toml(t):
        t = sub_once(r'^(ungoogled_chromium_windows\s*=\s*)"[^"]+"[^\n]*',
                     rf'\g<1>"{args.ucw_tag}"   # Chromium {chromium}; checked {args.date}', t, re.M, "ucw tag")
        t = sub_once(r'^(ungoogled_chromium_windows_commit\s*=\s*)"[^"]+"', rf'\g<1>"{args.ucw_commit}"', t, re.M, "ucw commit")
        return sub_once(r'(?ms)(^\[product\].*?^version\s*=\s*)"[^"]+"', rf'\g<1>"{new_ver}"', t, 0, "product version")
    edit("versions.toml", versions_toml)
    subprocess.run([sys.executable, "-I", str(ROOT / "scripts" / "sync-version.py")], check=True, stdout=sys.stderr)

    edit("README.md", lambda t: t.replace(f"`{old_ver}`", f"`{new_ver}`", 1), optional=True)
    edit("website/lib/site.ts", lambda t: t.replace(f'"{old_ver}"', f'"{new_ver}"', 1))
    for doc in ("docs/PRIVACY.md", "docs/PRIVACY.pl.md"):
        # only the line that says which version the policy describes (line 6), not the "from version X" history
        def privacy(t, old_ver=old_ver, new_ver=new_ver):
            lines = t.split("\n")
            if old_ver not in lines[5]:
                raise SystemExit("PRIVACY: version not on line 6")
            lines[5] = lines[5].replace(old_ver, new_ver, 1)
            return "\n".join(lines)
        edit(doc, privacy)

    entry = (f"## [{new_ver}] - {args.date} [Chromium {chromium}, ungoogled-chromium-windows {args.ucw_tag}]\n\n"
             f"### Changed\n"
             f"- Chromium base updated from {old_chromium} to {chromium} (upstream update, built and submitted to the\n"
             f"  Microsoft Store by the automated release pipeline).\n"
             f"- Package version {pkg}.\n\n")
    edit("CHANGELOG.md", lambda t: sub_once(r"## \[Unreleased\]\n\n", lambda mm: mm.group(0) + entry, t, 0, "[Unreleased]"))
    print(new_ver)


if __name__ == "__main__":
    main()
