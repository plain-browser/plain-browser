#!/usr/bin/env python3
"""The release pipeline, one pass (.github/workflows/pipeline.yml runs it every hour).

State lives in GitHub itself (tags, workflow runs, the draft release), so every pass starts from the facts and a
pass that dies half way is finished by the next one. In order:

  1. upstream   A newer ungoogled-chromium-windows tag than the one pinned in versions.toml: dry-run our patches on
                it; when they all apply, commit the bump to main (signed, through the GitHub API), tag it vX.Y.Z-alpha.1
                and start build-windows on the tag. When a patch does not apply a human is needed: an issue says which.
  2. build      The newest release tag has no build: start one. A build that failed: look at the failed jobs' logs;
                a runner or network problem is rerun (jobs that already finished are kept), a source problem
                (compile error, patch that does not apply) goes to an issue, because rerunning cannot fix it.
  3. release    A finished build: sign-release on the draft, then store-submit with commit=true (certification), then
                publish the release as a pre-release. store-submit that finds an earlier submission still in
                certification waits (it says DEFERRED); other failures are retried a few times, then an issue.

Stops when the repository variable PIPELINE_ENABLED is "false". DRY_RUN=1 prints the actions instead of doing them.
Needs: GH_TOKEN with contents, actions and issues write; run from a checkout of main.
"""
import base64, json, os, re, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
REPO = os.environ.get("GITHUB_REPOSITORY", "plain-browser/plain-browser")
UPSTREAM = "ungoogled-software/ungoogled-chromium-windows"
DRY = os.environ.get("DRY_RUN") == "1"
BUILD, SIGN, STORE = "build-windows.yml", "sign-release.yml", "store-submit.yml"
MAX_BUILD_RERUNS_TRANSIENT, MAX_BUILD_RERUNS_UNKNOWN, MAX_STORE_FAILURES = 4, 2, 3

# What a failed build log says about the failure. Source problems first: a rerun cannot fix them.
CODE_PATTERNS = [
    r"FAILED: obj/", r"ninja: build stopped", r"\berror: use of undeclared", r"\berror: [^\n]*\.(cc|h|mm|rs|cpp):",
    r"Hunk #\d+ FAILED", r"patch(\.exe)?: \*{4}", r"can't find file to patch", r"Reversed \(or previously applied\)",
    r"Build failed!", r"error\[E\d+\]", r"LNK\d{4}", r"FileNotFoundError", r"SyntaxError", r"ParserError",
]
TRANSIENT_PATTERNS = [
    r"The runner has received a shutdown signal", r"lost communication with the server", r"curl: \(\d+\)",
    r"ECONNRESET", r"ETIMEDOUT", r"socket hang up", r"HttpRequestException", r"Unable to download",
    r"Service Unavailable", r"HTTP 50\d", r"rate limit", r"schannel: ", r"The operation was canceled",
    r"Failed to (download|upload|create) (the )?artifact", r"TLS connection", r"Connection reset",
    r"RequestError", r"Internal Server Error", r"timed out",
]


def sh(*cmd, input=None, check=True):
    r = subprocess.run(cmd, input=input, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and r.returncode != 0:
        raise RuntimeError(f"{' '.join(cmd[:4])} failed ({r.returncode}): {r.stderr.strip()[:500]}")
    return r


def api(path, *args, method="GET"):
    r = sh("gh", "api", "-X", method, path, *args)
    return json.loads(r.stdout) if r.stdout.strip() else None


def log(msg):
    print(msg, flush=True)


def act(description, fn):
    """Run a state-changing action (or only say so under DRY_RUN)."""
    if DRY:
        log(f"[dry-run] would: {description}")
        return None
    log(f"doing: {description}")
    return fn()


# ---- versions --------------------------------------------------------------------------------------------------

def ucw_key(tag):
    return tuple(int(x) for x in re.findall(r"\d+", tag))


def release_key(tag):
    m = re.match(r"^v(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$", tag)
    return tuple(int(x) for x in m.groups()) if m else None


def pinned_ucw_tag():
    text = (ROOT / "versions.toml").read_text(encoding="utf-8")
    return re.search(r'^ungoogled_chromium_windows\s*=\s*"([^"]+)"', text, re.M).group(1)


def latest_upstream_tag():
    tags = [t["name"] for t in api(f"repos/{UPSTREAM}/tags?per_page=30")]
    tags = [t for t in tags if re.match(r"^\d+\.\d+\.\d+\.\d+-\d+(\.\d+)?$", t)]
    return max(tags, key=ucw_key)


def newest_release_tag():
    tags = [t["name"] for t in api(f"repos/{REPO}/tags?per_page=50")]
    keyed = [(release_key(t), t) for t in tags if release_key(t)]
    return max(keyed)[1] if keyed else None


# ---- classification (pure) ---------------------------------------------------------------------------------------

def classify_log(text):
    """'code' (a rerun cannot help), 'transient' (runner or network) or 'unknown'."""
    if any(re.search(p, text) for p in CODE_PATTERNS):
        return "code"
    if any(re.search(p, text) for p in TRANSIENT_PATTERNS):
        return "transient"
    return "unknown"


def excerpt(text, n=40):
    lines = [l for l in re.sub(r"\x1b\[[0-9;]*m", "", text).splitlines()
             if re.search(r"error|FAILED|Hunk|exception|exited with|failed", l, re.I)]
    cleaned = [re.sub(r"^\d{4}-\d\d-\d\dT[\d:.]+Z ", "", l)[:300] for l in lines[-n:]]
    return "\n".join(cleaned)


# ---- issues ----------------------------------------------------------------------------------------------------

def find_issue(title):
    r = sh("gh", "issue", "list", "--repo", REPO, "--state", "all", "--search", f'"{title}" in:title',
           "--json", "number,title,state,labels", "--limit", "20")
    for i in json.loads(r.stdout):
        if i["title"] == title:
            return i
    return None


def ensure_issue(title, body, labels, comment_marker=None):
    """Create the issue, or comment on the open one (once per marker). Returns the issue number."""
    existing = find_issue(title)
    if existing and existing["state"] == "OPEN":
        if comment_marker:
            comments = sh("gh", "issue", "view", str(existing["number"]), "--repo", REPO, "--json", "comments",
                          "--jq", "[.comments[].body]|join(\"\\n\")").stdout
            if comment_marker not in comments:
                act(f"comment on issue #{existing['number']}", lambda: sh(
                    "gh", "issue", "comment", str(existing["number"]), "--repo", REPO, "--body", body + f"\n\n<!-- {comment_marker} -->"))
        return existing["number"]
    if existing:  # closed: leave it closed, the work is done (or was decided against)
        return existing["number"]
    for label in labels:
        sh("gh", "label", "create", label, "--repo", REPO, "--force", "--color", "B60205" if label == "needs-human" else "0E8A16", check=False)

    def create():
        args = ["gh", "issue", "create", "--repo", REPO, "--title", title, "--body", body + (f"\n\n<!-- {comment_marker} -->" if comment_marker else "")]
        for label in labels:
            args += ["--label", label]
        return sh(*args).stdout.strip()
    return act(f"create issue '{title}'", create)


def close_issue(title, comment):
    existing = find_issue(title)
    if existing and existing["state"] == "OPEN":
        act(f"close issue #{existing['number']}", lambda: sh(
            "gh", "issue", "close", str(existing["number"]), "--repo", REPO, "--comment", comment))


# ---- runs ----------------------------------------------------------------------------------------------------

def runs_of(workflow, branch=None, title_prefix=None):
    q = f"repos/{REPO}/actions/workflows/{workflow}/runs?per_page=50" + (f"&branch={branch}" if branch else "")
    runs = api(q)["workflow_runs"]
    if title_prefix:  # the exact run-name: "store-submit <tag> (draft)" runs (diagnose, commit=false) must not count
        runs = [r for r in runs if (r.get("display_title") or "") == title_prefix]
    return runs  # newest first


def failed_jobs_text(run_id):
    """The logs of the failed jobs of a run, concatenated, plus the names of the jobs."""
    jobs = api(f"repos/{REPO}/actions/runs/{run_id}/jobs?per_page=100")["jobs"]
    failed = [j for j in jobs if j["conclusion"] in ("failure", "cancelled", "timed_out")]
    text = ""
    for j in failed:
        r = sh("gh", "api", f"repos/{REPO}/actions/jobs/{j['id']}/logs", check=False)
        text += "\n" + r.stdout
        for s in j["steps"]:
            if s["conclusion"] == "cancelled":
                text += "\nThe operation was canceled"
        if j["conclusion"] == "cancelled" and not r.stdout:
            text += "\nThe operation was canceled"
    return text, [j["name"] for j in failed]


def annotation_text(run_id):
    jobs = api(f"repos/{REPO}/actions/runs/{run_id}/jobs?per_page=100")["jobs"]
    out = ""
    for j in jobs:
        if j["conclusion"] == "failure":
            r = sh("gh", "api", f"repos/{REPO}/check-runs/{j['id']}/annotations", "--jq", ".[].message", check=False)
            out += r.stdout
    return out


def dispatch(workflow, ref, **inputs):
    args = ["gh", "workflow", "run", workflow, "--repo", REPO, "--ref", ref]
    for k, v in inputs.items():
        args += ["-f", f"{k}={v}"]
    act(f"dispatch {workflow} on {ref} {inputs}", lambda: sh(*args))


# ---- 1. upstream -----------------------------------------------------------------------------------------------

def signed_commit(branch, message, files):
    """One commit on `branch` with the given files, made through the API so GitHub signs it (main requires signed commits)."""
    head = api(f"repos/{REPO}/git/ref/heads/{branch}")["object"]["sha"]
    adds = [{"path": p, "contents": base64.b64encode((ROOT / p).read_bytes()).decode()} for p in files]
    body = {"query": "mutation($input: CreateCommitOnBranchInput!){createCommitOnBranch(input:$input){commit{oid}}}",
            "variables": {"input": {"branch": {"repositoryNameWithOwner": REPO, "branchName": branch},
                                    "message": {"headline": message}, "expectedHeadOid": head,
                                    "fileChanges": {"additions": adds}}}}
    r = sh("gh", "api", "graphql", "--input", "-", input=json.dumps(body))
    data = json.loads(r.stdout)
    if data.get("errors"):
        raise RuntimeError("createCommitOnBranch: " + json.dumps(data["errors"])[:500])
    return data["data"]["createCommitOnBranch"]["commit"]["oid"]


def step_upstream():
    pinned, latest = pinned_ucw_tag(), latest_upstream_tag()
    log(f"upstream: pinned {pinned}, latest {latest}")
    if ucw_key(latest) <= ucw_key(pinned):
        return
    title = f"Upstream update: ungoogled-chromium-windows {latest}"
    # Dry-run of our patches on the new tag (a few seconds, files fetched from googlesource).
    report = Path(tempfile.mkdtemp()) / "report.md"
    rc = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/check-patches.py"), "--tag", latest, "--report", str(report)],
                        env=dict(os.environ, GITHUB_TOKEN=os.environ.get("GH_TOKEN", ""))).returncode
    body = f"Pinned: `{pinned}`. Latest upstream tag: `{latest}`.\n\n" + (report.read_text() if report.exists() else "")
    sec = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/upstream-security-check.py"), "--from", pinned.split("-")[0],
                          "--to", latest.split("-")[0], "--report", str(report.with_name("security.md"))], capture_output=True)
    if sec.returncode == 0 and report.with_name("security.md").exists():
        body += "\n" + report.with_name("security.md").read_text()
    if rc == 2:
        log("check-patches: setup error (network?), next pass")
        return
    if rc == 1:
        ensure_issue(title, body + "\n\n**A patch does not apply: a human has to fix patches/ before the build starts "
                     "(the pipeline retries every hour and starts the build by itself once the dry-run passes).**",
                     ["needs-human"], comment_marker=f"patches-fail-{latest}")
        return
    # Patches apply: bump, commit, tag, build.
    if DRY:
        log(f"[dry-run] patches apply on {latest}: would bump, commit, tag and build")
        return
    version = sh(sys.executable, "-I", str(ROOT / "scripts/ci/bump-release.py"), "--ucw-tag", latest).stdout.strip()
    changed = sh("git", "-C", str(ROOT), "diff", "--name-only").stdout.split()
    tag = f"v{version}"
    ensure_issue(title, body + f"\n\nAutomated: release `{tag}` is built, signed and submitted to the Store by scripts/ci/pipeline.py.",
                 ["auto-release"])
    oid = act(f"commit {changed} to main", lambda: signed_commit("main", f"upstream: ungoogled-chromium-windows {latest}, version {version} (automated)", changed))
    act(f"tag {tag} at {oid}", lambda: sh("gh", "api", f"repos/{REPO}/git/refs", "-f", f"ref=refs/tags/{tag}", "-f", f"sha={oid}"))
    dispatch(BUILD, tag, profile="alpha")


# ---- 2. build and 3. release ------------------------------------------------------------------------------------

def step_release(tag):
    log(f"release pipeline for {tag}")
    runs = runs_of(BUILD, branch=tag)
    if not runs:
        dispatch(BUILD, tag, profile="alpha")
        return
    last = runs[0]
    if last["status"] != "completed":
        log(f"build run {last['id']} is {last['status']}")
        return
    if last["conclusion"] != "success":
        return handle_failed_build(tag, last)
    return handle_release(tag)


def handle_failed_build(tag, run):
    text, jobs = failed_jobs_text(run["id"])
    kind = classify_log(text)
    attempt = run.get("run_attempt", 1)
    log(f"build run {run['id']} {run['conclusion']} in {jobs}: {kind}, attempt {attempt}")
    limit = {"transient": MAX_BUILD_RERUNS_TRANSIENT, "unknown": MAX_BUILD_RERUNS_UNKNOWN}.get(kind, 0)
    if attempt < limit:
        def rerun():
            r = sh("gh", "run", "rerun", str(run["id"]), "--repo", REPO, "--failed", check=False)
            if r.returncode != 0:  # e.g. the stage trees expired: nothing to resume from, build again
                log("rerun refused (" + r.stderr.strip()[:200] + "): starting a fresh build")
                sh("gh", "workflow", "run", BUILD, "--repo", REPO, "--ref", tag, "-f", "profile=alpha")
        act(f"rerun failed jobs of run {run['id']} ({kind}, attempt {attempt})", rerun)
        return
    ensure_issue(f"Build failed: {tag}",
                 f"build-windows run {run['html_url']} failed in `{', '.join(jobs)}` ({kind}, attempt {attempt}).\n\n"
                 f"```\n{excerpt(text)}\n```\n\nFix the cause on main and the build can be resumed from the last stage tree "
                 f"(see docs/CI.md, `resume_run`); a rerun of the run does not help for a source problem.",
                 ["needs-human"], comment_marker=f"build-failed-{run['id']}")


def handle_release(tag):
    releases = api(f"repos/{REPO}/releases?per_page=30")
    rel = next((r for r in releases if r["tag_name"] == tag), None)
    if rel is None:
        ensure_issue(f"Release missing: {tag}", f"build-windows succeeded for {tag} but no release exists (the release job needs a tag ref).", ["needs-human"])
        return
    if not rel["draft"]:
        log(f"{tag} is published: done")
        return
    names = [a["name"] for a in rel["assets"]]
    if not any(n.endswith(".bundle") for n in names):
        signs = runs_of(SIGN, title_prefix=f"sign-release {tag}")
        if any(r["status"] != "completed" or r["conclusion"] == "success" for r in signs):
            log("sign-release is running or done, waiting for the assets")
            return
        if len(signs) >= MAX_STORE_FAILURES:
            ensure_issue(f"Signing failed: {tag}", f"sign-release failed {len(signs)} times for {tag}: {signs[0]['html_url']}", ["needs-human"])
            return
        dispatch(SIGN, "main", tag=tag)
        return
    subs = runs_of(STORE, title_prefix=f"store-submit {tag}")
    if any(r["status"] != "completed" for r in subs):
        log("store-submit is running")
        return
    if any(r["conclusion"] == "success" for r in subs):
        act(f"publish the release {tag} as a pre-release", lambda: sh("gh", "release", "edit", tag, "--repo", REPO, "--draft=false", "--prerelease"))
        close_issue(f"Upstream update: ungoogled-chromium-windows {pinned_ucw_tag()}", f"{tag} built, signed and submitted to Microsoft Store certification.")
        return
    failed = [r for r in subs if r["conclusion"] == "failure"]
    if failed and "DEFERRED" in annotation_text(failed[0]["id"]):
        log("store-submit deferred: an earlier submission is still in certification")
        failed = [r for r in failed if "DEFERRED" not in annotation_text(r["id"])]
    if len(failed) >= MAX_STORE_FAILURES:
        ensure_issue(f"Store submission failed: {tag}", f"store-submit failed {len(failed)} times for {tag}; last run {failed[0]['html_url']}\n\n"
                     f"```\n{excerpt(annotation_text(failed[0]['id']))}\n```", ["needs-human"], comment_marker=f"store-failed-{failed[0]['id']}")
        return
    dispatch(STORE, "main", tag=tag, commit="true")


def selftest():
    """Prove the one thing a dry run cannot: that a commit made through the API with this token is signed (main requires
    signed commits). Commits to a scratch branch and deletes it; fails when GitHub did not sign the commit."""
    branch = "pipeline-selftest"
    main_sha = api(f"repos/{REPO}/git/ref/heads/main")["object"]["sha"]
    sh("gh", "api", "-X", "DELETE", f"repos/{REPO}/git/refs/heads/{branch}", check=False)
    sh("gh", "api", f"repos/{REPO}/git/refs", "-f", f"ref=refs/heads/{branch}", "-f", f"sha={main_sha}")
    try:
        scratch = ROOT / ".pipeline-selftest"
        scratch.write_text("selftest\n")
        oid = signed_commit(branch, "pipeline selftest", [".pipeline-selftest"])
        v = api(f"repos/{REPO}/git/commits/{oid}")["verification"]
        log(f"commit {oid}: verified={v['verified']} reason={v['reason']}")
        if not v["verified"]:
            raise RuntimeError("the API commit is not signed: main would refuse it (" + v["reason"] + ")")
    finally:
        sh("gh", "api", "-X", "DELETE", f"repos/{REPO}/git/refs/heads/{branch}", check=False)


def release_pass():
    tag = newest_release_tag()
    if tag:
        step_release(tag)


def main():
    if sys.argv[1:] == ["selftest"]:
        selftest()
        return
    r = sh("gh", "api", f"repos/{REPO}/actions/variables/PIPELINE_ENABLED", "--jq", ".value", check=False)
    if r.returncode == 0 and r.stdout.strip().lower() == "false":
        log("PIPELINE_ENABLED=false: nothing to do")
        return
    errors = []
    for name, fn in (("upstream", step_upstream), ("release", release_pass)):
        try:
            fn()
        except Exception as e:  # one failing step must not hide the others; the next pass tries again
            log(f"::error::{name}: {e}")
            errors.append(name)
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
