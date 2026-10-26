#!/usr/bin/env python3
"""Dry-run of patches/series against an ungoogled-chromium-windows tag, without a Chromium checkout.

Fetches only the files our patches touch (from chromium.googlesource.com at the Chromium version that the
ungoogled-chromium-windows tag pins), applies the hunks of the ungoogled-chromium and ungoogled-chromium-windows
patches that touch those same files (their other hunks are dropped on purpose), then applies our series in order
and reports which of our patches still apply. Takes a few seconds; runs on Linux with git's `patch`.

Not simulated: ungoogled's domain substitution (runs after all patches, so keep Google hostnames out of patch
hunks; see patches/series), binary pruning, and the two scripts that edit the tree outside patches/series
(scripts/apply-branding.py, scripts/post-gnrt-fixups.ps1).

usage: check-patches.py [--tag TAG] [--work DIR] [--report FILE] [--regenerate]
                        [--extra-upstream uc:<patch> ...] [--export DIR]
--extra-upstream adds the files touched by that upstream patch (uc: or ucw:) to the set that is fetched and
replayed, e.g. the patches listed in upstream-revert.list, to see what their files look like without them.
--export writes every replayed file, with ungoogled's domain substitution applied where upstream applies it,
into DIR: the exact content an already-built tree should have for those files (copy it over the tree).
--regenerate rewrites every patch in patches/ that applied (with or without offset) so that its hunks and
context match the tag exactly (header kept, body regenerated with difflib, no timestamps). Review the diff
before committing. Exit code 1 when any of our patches fails, 2 on a setup error (network, bad tag).
"""
import argparse, base64, json, os, re, shutil, subprocess, sys, tempfile, urllib.error, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
UCW_REPO = "ungoogled-software/ungoogled-chromium-windows"
UC_REPO = "ungoogled-software/ungoogled-chromium"


def http_get(url, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent": "plain-browser-check-patches"})
    token = os.environ.get("GITHUB_TOKEN")
    if token and url.startswith("https://api.github.com/"):
        req.add_header("Authorization", "Bearer " + token)
    import time
    for attempt in range(6):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read()
            break
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            if e.code == 429 and attempt < 5:  # googlesource rate limit: back off and retry
                time.sleep(10 * (attempt + 1))
                continue
            raise
    return data if binary else data.decode("utf-8")


def fetch_chromium_file(chromium, path, cache_dir):
    """Base64 TEXT fetch from googlesource, cached on disk (None when the file does not exist upstream)."""
    cached = cache_dir / chromium / path
    missing = cache_dir / chromium / (path + ".missing")
    if cached.exists():
        return cached.read_bytes()
    if missing.exists():
        return None
    data = http_get(f"https://chromium.googlesource.com/chromium/src/+/refs/tags/{chromium}/{path}?format=TEXT")
    cached.parent.mkdir(parents=True, exist_ok=True)
    if data is None:
        missing.write_bytes(b"")
        return None
    raw = base64.b64decode(data)
    cached.write_bytes(raw)
    return raw


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


def regenerate(patch_file, src, before):
    """Rewrite patch_file's body as a fresh unified diff of the files it touched (header above '---' kept)."""
    import difflib
    text = patch_file.read_text()
    header = text.split("\n---\n", 1)[0] + "\n---\n" if "\n---\n" in text else ""
    body = []
    for path, old in before.items():
        f = src / path
        new = f.read_text(encoding="utf-8", errors="surrogateescape") if f.exists() else None
        if old == new:
            continue
        body += difflib.unified_diff((old or "").splitlines(keepends=True), (new or "").splitlines(keepends=True),
                                     fromfile="a/" + path if old is not None else "/dev/null",
                                     tofile="b/" + path if new is not None else "/dev/null", n=3)
    if body:
        patch_file.write_text(header + "".join(body), newline="\n")


def export_tree(src, files, out_dir, uc_raw):
    """Copy the replayed files to out_dir, applying ungoogled's domain substitution to the ones it lists."""
    regexes = []
    for line in filter(len, http_get(uc_raw + "domain_regex.list").splitlines()):
        pattern, replacement = line.split("#", 1)
        regexes.append((re.compile(pattern), replacement))
    listed = set(l.strip() for l in http_get(uc_raw + "domain_substitution.list").splitlines() if l.strip())
    n = substituted = 0
    for path in sorted(files):
        f = src / path
        if not f.exists():
            continue
        data = f.read_bytes()
        if path in listed:
            text = data.decode("utf-8", errors="surrogateescape")
            for rx, repl in regexes:
                text = rx.sub(repl, text)
            data = text.encode("utf-8", errors="surrogateescape")
            substituted += 1
        dest = out_dir / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        n += 1
    print(f"exported {n} files to {out_dir} ({substituted} domain-substituted)", file=sys.stderr)


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
    ap.add_argument("--regenerate", action="store_true", help="rewrite the bodies of our patches to match the tag")
    ap.add_argument("--extra-upstream", action="append", default=[], help="uc:<patch> or ucw:<patch> whose files to include")
    ap.add_argument("--export", default=None, help="write the replayed files (domain-substituted) into this directory")
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
    for spec in a.extra_upstream:
        repo, rel = spec.split(":", 1)
        text = http_get((uc_raw if repo == "uc" else ucw_raw) + "patches/" + rel)
        if text is None:
            raise SystemExit(f"--extra-upstream: {spec} not found")
        our_files |= touched_files(text)

    missing = []
    cache_dir = (Path(a.work) if a.work else work) / "cache"
    for path in sorted(our_files):
        raw = fetch_chromium_file(chromium, path, cache_dir)
        if raw is None:
            missing.append(path)  # may be a file our patch creates; patch will tell
            continue
        dest = src / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(raw)
    print(f"fetched {len(our_files) - len(missing)} of {len(our_files)} files", file=sys.stderr)

    reverted, hunk_drops = set(), {}
    for line in (ROOT / "patches" / "upstream-revert.list").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("prune:"):
            continue
        if line.startswith("hunks:"):
            _, repo, rest = line.split(":", 2)
            rel, regex = rest.split(":", 1)
            regex, _, exclude = regex.partition(":!")
            hunk_drops[(repo, rel)] = (regex, exclude or None)
        else:
            reverted.add(tuple(line.split(":", 1)))

    upstream_notes = []
    for name, raw, key in (("ungoogled-chromium", uc_raw, "uc"), ("ungoogled-chromium-windows", ucw_raw, "ucw")):
        upstream_series = series(http_get(raw + "patches/series"))
        for _, rel in reverted:
            if (key, rel) in reverted and rel not in upstream_series:
                upstream_notes.append(f"{name}/{rel}: listed in upstream-revert.list but NOT in the upstream series (renamed?)")
        for rel in upstream_series:
            if (key, rel) in reverted:
                upstream_notes.append(f"{name}/{rel}: skipped (patches/upstream-revert.list)")
                continue
            text = http_get(raw + "patches/" + rel)
            if text is None:
                upstream_notes.append(f"{name}/{rel}: not found"); continue
            if (key, rel) in hunk_drops:
                from patchfilter import filter_patch as drop_hunks
                regex, exclude = hunk_drops[(key, rel)]
                text, n = drop_hunks(text, regex, keep=False, exclude_path=exclude)
                upstream_notes.append(f"{name}/{rel}: {n} hunks dropped (patches/upstream-revert.list)")
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
        patch_file = ROOT / "patches" / rel
        before = {}
        if a.regenerate:
            for path in sorted(touched_files(patch_file.read_text())):
                f = src / path
                before[path] = f.read_text(encoding="utf-8", errors="surrogateescape") if f.exists() else None
        rc, out = run_patch(src, patch_file)
        if a.regenerate and rc == 0:
            regenerate(patch_file, src, before)
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
    if a.export:
        export_tree(src, our_files, Path(a.export), uc_raw)
    report = "\n".join(lines) + "\n"
    print(report)
    if a.report:
        Path(a.report).write_text(report)
    if not a.work:
        shutil.rmtree(work, ignore_errors=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
