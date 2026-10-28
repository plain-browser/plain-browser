#!/usr/bin/env python3
"""Which upstream Chromium commits between two versions touch files our patches change.

A Chromium patch release is mostly security fixes cherry-picked to the release branch. When one of them lands in a
file that one of our patches also edits, the fix may conflict with our change (the patch no longer applies) or, worse,
apply cleanly next to it while our change undoes or bypasses it. This lists, for a version bump, every upstream
commit that touches a file in patches/series, with the patch that edits the file, and marks commits whose message
looks security-related. A human reads the marked ones against our patch before the bump ships.

usage: upstream-security-check.py --from 156.0.8078.47 --to 156.0.8078.73 [--report FILE]
The versions are Chromium versions (an ungoogled-chromium-windows tag without its -N.N suffix also works). Uses the
GitHub mirror chromium/chromium (googlesource's log pages need a login); set GITHUB_TOKEN to avoid the anonymous
rate limit (one request per upstream commit, a few hundred per patch release).
Not covered: files touched only by the ungoogled patches we revert (patches/upstream-revert.list) and our own
code under src/plainbrowser, which upstream does not change. Exit code 0 (the report is the result), non-zero on
a setup error (network, unknown version).
"""
import argparse, json, os, re, sys, time, urllib.error, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MIRROR = "chromium/chromium"
# Words in a commit message that suggest a security fix. Chromium hides the bug of a security fix until it ships,
# so the message (and "Merge to M<n>" cherry-picks) is all there is; false positives are fine, the list is short.
SECURITY_WORDS = re.compile(
    r"security|cve-\d|use[- ]after[- ]free|\buaf\b|overflow|out[- ]of[- ]bounds|\boob\b|type confusion|"
    r"sandbox|heap|crash|sanitiz|bounds check|integer|race|lifetime|dangling|raw_ptr", re.I)


def chromium_version(v):
    m = re.match(r"^(\d+\.\d+\.\d+\.\d+)", v)
    if not m:
        raise SystemExit("not a Chromium version: %s" % v)
    return m.group(1)


def api(path):
    req = urllib.request.Request("https://api.github.com/" + path,
                                 headers={"User-Agent": "plain-browser-security-check",
                                          "Accept": "application/vnd.github+json"})
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    for attempt in range(6):
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code in (403, 429, 500, 502, 503, 504) and attempt < 5:
                time.sleep(15 * (attempt + 1))
                continue
            raise SystemExit("GitHub API %s: HTTP %d" % (path, e.code))


def patched_files():
    """{path: [patch, ...]} for every file a patch in patches/series edits."""
    files = {}
    for line in (ROOT / "patches" / "series").read_text(encoding="utf-8").splitlines():
        name = line.strip()
        if not name or name.startswith("#"):
            continue
        for l in (ROOT / "patches" / name).read_text(encoding="utf-8").splitlines():
            m = re.match(r"^\+\+\+ b/(\S+)", l)
            if m:
                files.setdefault(m.group(1), []).append(name)
    return files


def upstream_commits(base, head):
    """Commits reachable from head and not from base, oldest first, with their files."""
    commits, page = [], 1
    while True:
        data = api("repos/%s/compare/%s...%s?per_page=100&page=%d" % (MIRROR, base, head, page))
        batch = data.get("commits", [])
        commits += batch
        if len(commits) >= data.get("total_commits", 0) or not batch:
            break
        page += 1
    out = []
    for c in commits:
        detail = api("repos/%s/commits/%s" % (MIRROR, c["sha"]))
        out.append({"sha": c["sha"], "message": c["commit"]["message"],
                    "files": [f["filename"] for f in detail.get("files", [])]})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--from", dest="base", required=True)
    ap.add_argument("--to", dest="head", required=True)
    ap.add_argument("--report")
    a = ap.parse_args()
    base, head = chromium_version(a.base), chromium_version(a.head)
    ours = patched_files()
    commits = upstream_commits(base, head)
    hits = {}  # path -> [commit]
    for c in commits:
        for f in c["files"]:
            if f in ours:
                hits.setdefault(f, []).append(c)
    flagged = sum(1 for cs in hits.values() for c in cs if SECURITY_WORDS.search(c["message"]))
    lines = ["### Upstream commits in files our patches edit (%s → %s)" % (base, head), "",
             "%d upstream commits, %d files edited by our patches; %d of those files changed upstream, %d commit(s) "
             "marked as possibly security-related." % (len(commits), len(ours), len(hits), flagged), ""]
    if not hits:
        lines.append("None of the files our patches edit changed upstream: nothing to review.")
    for path in sorted(hits):
        lines.append("- `%s` (ours: %s)" % (path, ", ".join("`%s`" % p for p in ours[path])))
        for c in hits[path]:
            subject = c["message"].splitlines()[0]
            mark = " **[review: security?]**" if SECURITY_WORDS.search(c["message"]) else ""
            lines.append("  - [`%s`](https://github.com/%s/commit/%s) %s%s" % (c["sha"][:10], MIRROR, c["sha"],
                                                                              subject.replace("|", "\\|"), mark))
    lines += ["", "Review each marked commit against our patch for that file: does the fix still hold with our change "
              "applied (same function, same check)? A clean apply is not enough."]
    text = "\n".join(lines) + "\n"
    if a.report:
        Path(a.report).write_text(text, encoding="utf-8")
    sys.stdout.write(text)


if __name__ == "__main__":
    main()
