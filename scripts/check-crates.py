#!/usr/bin/env python3
"""Pin the Rust crates that the adblock engine adds to Chromium's vendored crate set.

`gnrt vendor` (scripts/vendor-adblock-crates.ps1) resolves adblock-rust's dependencies and downloads the crates the tree
does not have yet from crates.io, at the versions it picks on the day of the build, into
third_party/rust/chromium_crates_io/vendor/<name>-v<major>. It does not leave checksums in Cargo.lock, so this script
pins what ends up on disk: for every directory that vendoring added, the SHA-256 over the whole tree (relative path
and SHA-256 of each file, sorted) together with the crate version, compared with the reviewed list in
scripts/adblock-crates.lock.

  - an added crate that is not in the list, or one whose version or tree hash differs: exit 1, nothing is built from it;
  - no list yet (or an empty one): the lines to commit are printed and the build goes on (first run only);
  - a listed crate that was not added this time: only a note.

usage: check-crates.py --vendor <vendor dir> --before <file: directory names before vendoring> --pins scripts/adblock-crates.lock
Reviewing the list: scripts/adblock-crates.lock says how (compare each tree with the .crate file of that version on
crates.io; only the files that our patches add may differ).
"""
import argparse, hashlib, re, sys
from pathlib import Path

SHA256 = re.compile(r"[0-9a-f]{64}")


def tree_hash(directory):
    """SHA-256 over '<relative path>\\0<sha256 of the file>\\n' for every file, sorted by path."""
    lines = []
    # Sorted by the tuple of path components, compared as plain strings: sorting the Path objects themselves ignores
    # case on Windows (and so orders README.md and a.rs differently than on Linux), which gave a different hash on the
    # runner than the one the list was reviewed with.
    for path in sorted((p for p in directory.rglob("*") if p.is_file()), key=lambda p: p.relative_to(directory).parts):
        rel = path.relative_to(directory).as_posix()
        lines.append(f"{rel}\0{hashlib.sha256(path.read_bytes()).hexdigest()}\n")
    return hashlib.sha256("".join(lines).encode("utf-8")).hexdigest()


def crate_version(directory):
    """The version in the crate's own Cargo.toml ([package] section), or None."""
    manifest = directory / "Cargo.toml"
    if not manifest.is_file():
        return None
    text = manifest.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'(?ms)^\[package\]\s*$(.*?)(?=^\[|\Z)', text)
    v = re.search(r'(?m)^version\s*=\s*"([^"]+)"', m.group(1)) if m else None
    return v.group(1) if v else None


def added(vendor, before_names):
    """{directory name: (version, tree hash)} for the directories of `vendor` that were not there before."""
    out = {}
    for d in sorted(p for p in vendor.iterdir() if p.is_dir() and p.name not in before_names):
        out[d.name] = (crate_version(d) or "?", tree_hash(d))
    return out


def parse_pins(text):
    pins = {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 3 or not SHA256.fullmatch(parts[2]):
            raise ValueError(f"pins line {number}: expected '<directory> <version> <sha256 of the tree>', got {line!r}")
        pins[parts[0]] = (parts[1], parts[2])
    return pins


def check(new, pins_text):
    """new: {directory: (version, tree hash)}. Returns (exit code, lines to print)."""
    pins = parse_pins(pins_text)
    listing = [f"{name} {version} {digest}" for name, (version, digest) in sorted(new.items())]
    if not pins:
        return 0, [f"::warning::scripts/adblock-crates.lock is empty: {len(new)} added crates are not checked. Review and commit:"] + listing
    out, bad = [], 0
    for name, (version, digest) in sorted(new.items()):
        want = pins.get(name)
        if want is None:
            out.append(f"::error::crate {name} {version} is not in scripts/adblock-crates.lock")
            bad += 1
        elif want != (version, digest):
            out.append(f"::error::crate {name}: {version} {digest} differs from the pinned {want[0]} {want[1]}")
            bad += 1
    for name in sorted(set(pins) - set(new)):
        out.append(f"note: pinned crate {name} was not added this time")
    if bad:
        out.append(f"{bad} added crate(s) do not match the pinned list. Review them (see scripts/adblock-crates.lock), then update it. Lines for the crates as they came in:")
        out += listing
        return 1, out
    out.append(f"adblock crates: {len(new)} added crates match scripts/adblock-crates.lock")
    return 0, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--vendor", required=True)
    ap.add_argument("--before", required=True, help="text file with the directory names of the vendor directory before vendoring")
    ap.add_argument("--pins", required=True)
    args = ap.parse_args()
    before = set(Path(args.before).read_text(encoding="utf-8").split())
    pins = Path(args.pins)
    code, lines = check(added(Path(args.vendor), before), pins.read_text(encoding="utf-8") if pins.exists() else "")
    print("\n".join(lines))
    return code


if __name__ == "__main__":
    sys.exit(main())
