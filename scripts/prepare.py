#!/usr/bin/env python3
"""Copy the Plain Browser layer into an ungoogled-chromium-windows checkout.

usage: prepare.py <ungoogled-chromium-windows dir> <chromium src dir>
Appends our patches to the ungoogled series, drops the upstream patches listed in patches/upstream-revert.list
from both upstream series files, and copies //plainbrowser into src.
Fails loudly: never silently skips a missing target.
"""
import shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from patchfilter import filter_patch  # noqa: E402

def revert_list():
    out = []
    for line in (ROOT / "patches" / "upstream-revert.list").read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            if line.startswith("hunks:"):
                _, repo, rest = line.split(":", 2)
                rel, regex = rest.split(":", 1)
                out.append(("hunks", (repo, rel, regex)))
            else:
                repo, rel = line.split(":", 1)
                out.append((repo, rel))
    return out


def drop_from_series(series: Path, drop: set) -> int:
    lines = series.read_text().splitlines()
    kept = [l for l in lines if l.strip() not in drop]
    missing = drop - {l.strip() for l in lines}
    if missing:
        print(f"error: not in {series}: {sorted(missing)} (upstream renamed it? update patches/upstream-revert.list)", file=sys.stderr)
        return -1
    series.write_text("\n".join(kept) + "\n")
    return len(lines) - len(kept)


def main(ucw: Path, src: Path) -> int:
    if not (ucw / "build.py").exists():
        print(f"error: {ucw} is not ungoogled-chromium-windows", file=sys.stderr); return 1
    if not src.is_dir():
        print(f"error: chromium src not found: {src}", file=sys.stderr); return 1
    dst = src / "plainbrowser"
    if dst.exists(): shutil.rmtree(dst)
    shutil.copytree(ROOT / "src" / "plainbrowser", dst, ignore=shutil.ignore_patterns("target", "*.lock.bak"))
    series_lines = [l.strip() for l in (ROOT / "patches" / "series").read_text().splitlines()
                    if l.strip() and not l.startswith("#")]
    out = ucw / "patches" / "plainbrowser"
    if out.exists(): shutil.rmtree(out)
    for rel in series_lines:
        p = ROOT / "patches" / rel
        if not p.exists():
            print(f"error: patch in series missing: {rel}", file=sys.stderr); return 1
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, out / rel)
    reverts = revert_list()
    prunes = {rel for r, rel in reverts if r == "prune"}
    if prunes:
        pruning = ucw / "ungoogled-chromium" / "pruning.list"
        n = drop_from_series(pruning, prunes)
        if n < 0:
            return 1
        print(f"ok: kept {n} files out of pruning.list")
    for kind, spec in reverts:
        if kind != "hunks":
            continue
        repo, rel, regex = spec
        pf = (ucw / "ungoogled-chromium" / "patches" if repo == "uc" else ucw / "patches") / rel
        if not pf.exists():
            print(f"error: upstream patch not found: {pf}", file=sys.stderr); return 1
        text, matched = filter_patch(pf.read_text(encoding="utf-8", errors="surrogateescape"), regex, keep=False)
        if not matched:
            print(f"error: no hunk of {rel} matches {regex!r} (upstream changed? update patches/upstream-revert.list)", file=sys.stderr); return 1
        pf.write_text(text, encoding="utf-8", errors="surrogateescape", newline="")
        print(f"ok: dropped {matched} hunks from {rel}")
    for repo, series_path in (("uc", ucw / "ungoogled-chromium" / "patches" / "series"), ("ucw", ucw / "patches" / "series")):
        drop = {rel for r, rel in reverts if r == repo}
        if drop:
            n = drop_from_series(series_path, drop)
            if n < 0:
                return 1
            print(f"ok: dropped {n} upstream patches from {series_path.relative_to(ucw)}")
    flags = ucw / "ungoogled-chromium" / "flags.gn"
    flags.write_text(flags.read_text().replace("safe_browsing_mode=0", "safe_browsing_mode=1"))
    series = ucw / "patches" / "series"
    existing = series.read_text().splitlines() if series.exists() else []
    existing = [l for l in existing if not l.startswith("plainbrowser/")]
    series.write_text("\n".join(existing + [f"plainbrowser/{r}" for r in series_lines]) + "\n")
    print(f"ok: copied //plainbrowser, {len(series_lines)} patches")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 3: print(__doc__); sys.exit(2)
    sys.exit(main(Path(sys.argv[1]), Path(sys.argv[2])))
