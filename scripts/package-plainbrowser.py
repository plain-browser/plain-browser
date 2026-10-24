#!/usr/bin/env python3
"""Package the built tree as Plain Browser (same logic as ungoogled-chromium-windows' package.py, our file names).

Run from the ungoogled-chromium-windows checkout root after build.py succeeded:
    python3 <repo>/scripts/package-plainbrowser.py
Writes build/PlainBrowser_<version>_<cpu>_installer.exe and build/PlainBrowser_<version>_<cpu>.zip; the zip's top-level
directory carries the same name. <version> is [product] version from versions.toml.
"""
import argparse, platform, re, shutil, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
UCW = Path.cwd()
sys.path.insert(0, str(UCW / "ungoogled-chromium" / "utils"))
import filescfg  # noqa: E402


def product_version():
    text = (REPO / "versions.toml").read_text(encoding="utf-8")
    m = re.search(r'(?ms)^\[product\].*?^version\s*=\s*"([^"]+)"', text)
    if not m:
        raise SystemExit("versions.toml: [product] version not found")
    return m.group(1)


def target_cpu(out):
    text = (out / "args.gn").read_text()
    for cpu in ("x64", "x86", "arm64"):
        if 'target_cpu="%s"' % cpu in text:
            return cpu
    raise SystemExit("target_cpu not found in args.gn")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cpu-arch", default=platform.architecture()[0], choices=("64bit", "32bit", "arm"))
    args = ap.parse_args()
    out = Path("build/src/out/Default")
    if not (out / "mini_installer.exe").exists():
        raise SystemExit("mini_installer.exe not built in %s (run from the ucw checkout root)" % out)
    stem = "PlainBrowser_%s_%s" % (product_version(), target_cpu(out))
    shutil.copyfile(out / "mini_installer.exe", "build/%s_installer.exe" % stem)
    timestamp = None
    try:
        timestamp = int(Path("build/src/build/util/LASTCHANGE.committime").read_text())
    except FileNotFoundError:
        pass
    excluded = {Path("mini_installer.exe"), Path("mini_installer_exe_version.rc"), Path("setup.exe"),
                Path("chrome.packed.7z")}
    files = filescfg.filescfg_generator(Path("build/src/chrome/tools/build/win/FILES.cfg"), out, args.cpu_arch,
                                        excluded)
    filescfg.create_archive(files, tuple(), out, Path("build/%s.zip" % stem), timestamp)
    print("wrote build/%s_installer.exe and build/%s.zip" % (stem, stem))


if __name__ == "__main__":
    main()
