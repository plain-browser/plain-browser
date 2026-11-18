#!/usr/bin/env python3
"""Pin the Rust crates that the adblock engine adds to Chromium's vendored crate set.

`gnrt vendor` (scripts/vendor-adblock-crates.ps1) resolves adblock-rust's dependencies against Chromium's Cargo.lock and
downloads whatever the tree does not have yet from crates.io, at the versions it picks on the day of the build.
Cargo records the SHA-256 of each downloaded crate in Cargo.lock; the vendor script then strips those lines. This
script runs in between: it takes the crates that are in the lock after vendoring but not before (the ones we added),
and compares name, version and checksum with the reviewed list in scripts/adblock-crates.lock.

  - an added crate that is not in the list, or one with another checksum: exit 1, nothing is built from it;
  - no list yet (or an empty one): the lines to commit are printed and the build goes on (first run only);
  - a listed crate that was not added this time: only a note (Chromium may have brought its own copy).

usage: check-crates.py --before Cargo.lock.before --after Cargo.lock --pins scripts/adblock-crates.lock
Reviewing a change of the list: compare the checksum with https://crates.io/api/v1/crates/<name>/<version> ("checksum")
and read the crate's diff on docs.rs or in its repository before committing it.
"""
import argparse, re, sys
from pathlib import Path

REGISTRY = "registry+https://github.com/rust-lang/crates.io-index"
SHA256 = re.compile(r"[0-9a-f]{64}")


def parse_lock(text):
    """[[package]] blocks -> {(name, version): (source, checksum)}; the values may be None."""
    packages = {}
    for block in re.split(r"(?m)^\[\[package\]\]\s*$", text)[1:]:
        fields = dict(re.findall(r'(?m)^(name|version|source|checksum) = "([^"]*)"\s*$', block))
        if "name" in fields and "version" in fields:
            packages[(fields["name"], fields["version"])] = (fields.get("source"), fields.get("checksum"))
    return packages


def added(before, after):
    """Registry crates with a checksum that the lock got during vendoring: {(name, version): checksum}."""
    return {
        key: checksum
        for key, (source, checksum) in after.items()
        if key not in before and source == REGISTRY and checksum
    }


def parse_pins(text):
    pins = {}
    for number, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 3 or not SHA256.fullmatch(parts[2]):
            raise ValueError(f"pins line {number}: expected '<name> <version> <sha256>', got {line!r}")
        pins[(parts[0], parts[1])] = parts[2]
    return pins


def check(before_text, after_text, pins_text):
    """Returns (exit code, lines to print)."""
    new = added(parse_lock(before_text), parse_lock(after_text))
    pins = parse_pins(pins_text)
    listing = [f"{name} {version} {checksum}" for (name, version), checksum in sorted(new.items())]
    if not pins:
        return 0, [f"::warning::scripts/adblock-crates.lock is empty: {len(new)} added crates are not checked. Review and commit:"] + listing
    out, bad = [], 0
    for (name, version), checksum in sorted(new.items()):
        want = pins.get((name, version))
        if want is None:
            out.append(f"::error::crate {name} {version} ({checksum}) is not in scripts/adblock-crates.lock")
            bad += 1
        elif want != checksum:
            out.append(f"::error::crate {name} {version}: checksum {checksum} differs from the pinned {want}")
            bad += 1
    for (name, version) in sorted(set(pins) - set(new)):
        out.append(f"note: pinned crate {name} {version} was not added this time")
    if bad:
        out.append(f"{bad} added crate(s) do not match the pinned list. Review them (see the header of scripts/check-crates.py), then update the list. Lines for the crates as they came in:")
        out += listing
        return 1, out
    out.append(f"adblock crates: {len(new)} added crates match scripts/adblock-crates.lock")
    return 0, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--before", required=True)
    ap.add_argument("--after", required=True)
    ap.add_argument("--pins", required=True)
    args = ap.parse_args()
    pins = Path(args.pins)
    code, lines = check(
        Path(args.before).read_text(encoding="utf-8"),
        Path(args.after).read_text(encoding="utf-8"),
        pins.read_text(encoding="utf-8") if pins.exists() else "",
    )
    print("\n".join(lines))
    return code


if __name__ == "__main__":
    sys.exit(main())
