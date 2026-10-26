#!/usr/bin/env python3
"""Dry-run of patches/series against an ungoogled-chromium-windows tag, without a Chromium checkout.

Fetches only the files our patches touch (from chromium.googlesource.com at the Chromium version that the
ungoogled-chromium-windows tag pins), applies the hunks of the ungoogled-chromium and ungoogled-chromium-windows
patches that touch those same files (their other hunks are dropped on purpose), then applies our series in order
and reports which of our patches still apply. Takes a few seconds; runs on Linux with git's `patch`.

Not simulated: ungoogled's domain substitution (runs after all patches, so keep Google hostnames out of patch
hunks; see patches/series), binary pruning, and the two scripts that edit the tree outside patches/series
(scripts/apply-branding.py, scripts/post-gnrt-fixups.ps1).

usage: check-patches.py [--tag 156.0.8078.47-1.1] [--work DIR] [--report FILE]
Exit code 1 when any of our patches fails, 2 on a setup error (network, bad tag).
"""
import argparse, base64, json, os, re, shutil, subprocess, sys, tempfile, urllib.error, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UCW_REPO = "ungoogled-software/ungoogled-chromium-windows"
UC_REPO = "ungoogled-software/ungoogled-chromium"


def http_get(url, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent": "plain-browser-check-patches"})
    token = os.environ.get("GITHUB_TOKEN")
    if token and url.startswith("https://api.github.com/"):
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        raise
    return data if binary else data.decode("utf-8")


def pinned_tag():
    m = re.search(r'^ungoogled_chromium_windows\s*=\s*"([^"]+)"', (ROOT / "versions.toml").read_text(), re.M)
    if not m:
        raise SystemExit("versions.toml: ungoogled_chromium_windows not found")
    return m.group(1)


def series(text):
    return [l.strip() for l in text.splitlines() if l.strip() and not l.startswith("#")]


def touched_files(patch_text):
    """Paths (without the a/ b/ prefix) named by the file headers of a unified diff."""
    files = set()
    for line in patch_text.splitlines():
        m = re.match(r"^(?:\+\+\+|---) (?:[ab]/)?(\S+)", line)
        if m and m.group(1) != "/dev/null":
            files.add(m.group(1))
    return files


def filter_patch(patch_text, keep):
    """Keep only the per-file sections of a unified diff whose path is in `keep`."""
    lines = patch_text.splitlines(keepends=True)
    out, cur, cur_keep, i = [], [], False, 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("--- ") and i + 1 < len(lines) and lines[i + 1].startswith("+++ "):
            if cur_keep:
                out += cur
            cur = [line, lines[i + 1]]
            cur_keep = bool(touched_files(line + lines[i + 1]) & keep)
            i += 2
            continue
        if line.startswith("diff "):
            if cur_keep:
                out += cur
            cur, cur_keep = [], False
        else:
            cur.append(line)
        i += 1
    if cur_keep:
        out += cur
    return "".join(out)


def run_patch(src, patch_file, dry_run=False):
    cmd = ["patch", "-p1", "--forward", "--no-backup-if-mismatch", "-d", str(src), "-i", str(patch_file)]
    if dry_run:
        cmd.append("--dry-run")
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tag", default=None, help="ungoogled-chromium-windows tag (default: pinned in versions.toml)")
    ap.add_argument("--work", default=None, help="work directory (default: a temporary one, deleted afterwards)")
    ap.add_argument("--report", default=None, help="write the markdown report here as well as to stdout")
    a = ap.parse_args()
    tag = a.tag or pinned_tag()
    work = Path(a.work) if a.work else Path(tempfile.mkdtemp(prefix="check-patches-"))
    src = work / "src"
    if src.exists():
        shutil.rmtree(src)
    src.mkdir(parents=True)

    sub = http_get(f"https://api.github.com/repos/{UCW_REPO}/contents/ungoogled-chromium?ref={tag}")
    if sub is None:
        raise SystemExit(f"tag {tag} not found in {UCW_REPO}")
    uc_sha = json.loads(sub)["sha"]
    uc_raw = f"https://raw.githubusercontent.com/{UC_REPO}/{uc_sha}/"
    ucw_raw = f"https://raw.githubusercontent.com/{UCW_REPO}/{tag}/"
    chromium = http_get(uc_raw + "chromium_version.txt").strip()
    print(f"tag {tag}: ungoogled-chromium {uc_sha[:10]}, Chromium {chromium}", file=sys.stderr)

    ours = series((ROOT / "patches" / "series").read_text())
    our_files = set()
    for rel in ours:
        our_files |= touched_files((ROOT / "patches" / rel).read_text())

    missing = []
    for path in sorted(our_files):
        data = http_get(f"https://chromium.googlesource.com/chromium/src/+/refs/tags/{chromium}/{path}?format=TEXT")
        if data is None:
            missing.append(path)  # may be a file our patch creates; patch will tell
            continue
        dest = src / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(base64.b64decode(data))
    print(f"fetched {len(our_files) - len(missing)} of {len(our_files)} files", file=sys.stderr)

    upstream_notes = []
    for name, raw in (("ungoogled-chromium", uc_raw), ("ungoogled-chromium-windows", ucw_raw)):
        for rel in series(http_get(raw + "patches/series")):
            text = http_get(raw + "patches/" + rel)
            if text is None:
                upstream_notes.append(f"{name}/{rel}: not found"); continue
            if not (touched_files(text) & our_files):
                continue
            part = filter_patch(text, our_files)
            if not part.strip():
                continue
            pf = work / "upstream.patch"
            pf.write_text(part)
            rc, out = run_patch(src, pf)
            if rc != 0:
                upstream_notes.append(f"{name}/{rel}: hunks on our files did not apply (upstream problem or stale copy)\n```\n{out}\n```")
            else:
                upstream_notes.append(f"{name}/{rel}: applied (shares files with our patches)")

    rows, failed = [], []
    for rel in ours:
        rc, out = run_patch(src, ROOT / "patches" / rel)
        # Offsets are normal (upstream patches shift lines); fuzz means the context no longer matches.
        if rc == 0:
            note = " (fuzz: regenerate)" if "fuzz" in out else (" (offset)" if "offset" in out else "")
            rows.append((rel, "ok" + note, ""))
        else:
            failed.append(rel)
            rows.append((rel, "FAILED", out))

    lines = [f"## Patch check: {tag} (Chromium {chromium})", "",
             "| Patch | Result |", "|---|---|"]
    lines += [f"| `{rel}` | {status} |" for rel, status, _ in rows]
    for rel, status, out in rows:
        if out:
            lines += ["", f"<details><summary>{rel}</summary>", "", "```", out, "```", "</details>"]
    if missing:
        lines += ["", "Files not in upstream Chromium (created by our patches, or renamed upstream):",
                  *[f"- `{p}`" for p in missing]]
    if upstream_notes:
        lines += ["", "Upstream patches touching the same files:", *[f"- {n}" for n in upstream_notes]]
    lines += ["", "Not simulated: domain substitution, pruning, scripts/apply-branding.py, scripts/post-gnrt-fixups.ps1."]
    report = "\n".join(lines) + "\n"
    print(report)
    if a.report:
        Path(a.report).write_text(report)
    if not a.work:
        shutil.rmtree(work, ignore_errors=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
