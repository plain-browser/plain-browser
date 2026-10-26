#!/usr/bin/env python3
"""Copy the Plain Browser layer into an ungoogled-chromium-windows checkout.

usage: prepare.py <ungoogled-chromium-windows dir> <chromium src dir>
Appends our patches to the ungoogled series and copies //plainbrowser into src.
Fails loudly: never silently skips a missing target.
"""
import shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

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
    series = ucw / "patches" / "series"
    existing = series.read_text().splitlines() if series.exists() else []
    existing = [l for l in existing if not l.startswith("plainbrowser/")]
    series.write_text("\n".join(existing + [f"plainbrowser/{r}" for r in series_lines]) + "\n")
    print(f"ok: copied //plainbrowser, {len(series_lines)} patches")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 3: print(__doc__); sys.exit(2)
    sys.exit(main(Path(sys.argv[1]), Path(sys.argv[2])))
