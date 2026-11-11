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
# store-submit runs before this moment ended at CommitStarted and counted as done although Partner Center then
# answered CommitFailed; from here on the job waits for the verdict, so only later runs say anything.
STORE_VERDICT_SINCE = "2026-11-04T06:41:00Z"
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
    m = re.match(r"^v(\d+)\.(\d+)\.(\d+)-(?:alpha|beta)\.(\d+)$", tag)
    return tuple(int(x) for x in m.groups()) if m else None


def all_tags(repo):
    """{name: commit} of every tag: all pages, since the API promises no order and the newest need not be on the first."""
    out = sh("gh", "api", "--paginate", f"repos/{repo}/tags?per_page=100", "--jq", '.[] | .name + " " + .commit.sha').stdout
    return dict(line.split(" ", 1) for line in out.splitlines() if line.strip())


def pinned_ucw_tag():
    text = (ROOT / "versions.toml").read_text(encoding="utf-8")
    return re.search(r'^ungoogled_chromium_windows\s*=\s*"([^"]+)"', text, re.M).group(1)


def latest_upstream_tag():
    """(tag, commit) of the highest upstream tag; the commit is pinned next to the tag, since a tag can be moved."""
    tags = all_tags(UPSTREAM)
    tags = {n: sha for n, sha in tags.items() if re.match(r"^\d+\.\d+\.\d+\.\d+-\d+(\.\d+)?$", n)}
    latest = max(tags, key=ucw_key)
    return latest, tags[latest]


def release_tags():
    """Our release tags, newest first."""
    return [t for _, t in sorted(((release_key(t), t) for t in all_tags(REPO) if release_key(t)), reverse=True)]


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

def signed_commit(branch, message, files, head=None):
    """One commit on `branch` with the given files, made through the API so GitHub signs it (main requires signed commits).
    `head` is the commit the files were edited on (default: the checkout's HEAD): the API refuses the commit when the
    branch has moved since, instead of replacing someone's newer version of a file with ours. The next pass starts
    from a fresh checkout."""
    head = head or sh("git", "-C", str(ROOT), "rev-parse", "HEAD").stdout.strip()
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


def product_version():
    text = (ROOT / "versions.toml").read_text(encoding="utf-8")
    return re.search(r'(?ms)^\[product\].*?^version\s*=\s*"([^"]+)"', text).group(1)


def bump_message(ucw_tag, version):
    return f"upstream: ungoogled-chromium-windows {ucw_tag}, version {version} (automated)"


def finish_interrupted_bump():
    """A pass that died after the bump commit and before the tag leaves main at a version nobody builds: the upstream
    tag is pinned already, so step_upstream sees nothing new, and the release step only knows tags. Tag that commit
    now. Only the pipeline's own bump commit counts; a version raised by hand is not released from here."""
    version = product_version()
    tag = f"v{version}"
    if sh("gh", "api", f"repos/{REPO}/git/ref/tags/{tag}", check=False).returncode == 0:
        return False
    last = api(f"repos/{REPO}/commits?path=versions.toml&per_page=1")
    if not last or last[0]["commit"]["message"].splitlines()[0] != bump_message(pinned_ucw_tag(), version):
        return False
    oid = last[0]["sha"]
    log(f"bump commit {oid} for {tag} has no tag: finishing the interrupted pass")
    act(f"tag {tag} at {oid}", lambda: sh("gh", "api", f"repos/{REPO}/git/refs", "-f", f"ref=refs/tags/{tag}", "-f", f"sha={oid}"))
    dispatch(BUILD, tag, profile="alpha")
    return True


def step_upstream():
    if finish_interrupted_bump():
        return
    pinned, (latest, latest_commit) = pinned_ucw_tag(), latest_upstream_tag()
    log(f"upstream: pinned {pinned}, latest {latest}")
    if ucw_key(latest) <= ucw_key(pinned):
        return
    title = f"Upstream update: ungoogled-chromium-windows {latest}"
    # Dry-run of our patches on the new tag (a few seconds, files fetched from googlesource).
    report = Path(tempfile.mkdtemp()) / "report.md"
    try:
        rc = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/check-patches.py"), "--tag", latest, "--report", str(report)],
                            env=dict(os.environ, GITHUB_TOKEN=os.environ.get("GH_TOKEN", "")), timeout=600).returncode
    except subprocess.TimeoutExpired:
        log("check-patches took more than 10 minutes, next pass")
        return
    body = f"Pinned: `{pinned}`. Latest upstream tag: `{latest}`.\n\n" + (report.read_text() if report.exists() else "")
    # Every upstream commit between the two versions (a major bump is thousands): an extra for the issue, so it gets
    # a few minutes and is left out when slower (it once ran the whole 20-minute job into its timeout).
    try:
        sec = subprocess.run([sys.executable, "-I", str(ROOT / "scripts/upstream-security-check.py"), "--from", pinned.split("-")[0],
                              "--to", latest.split("-")[0], "--report", str(report.with_name("security.md"))], capture_output=True, timeout=240)
        if sec.returncode == 0 and report.with_name("security.md").exists():
            body += "\n" + report.with_name("security.md").read_text()
    except subprocess.TimeoutExpired:
        body += (f"\n(The upstream commit check was too slow for the pipeline: run `python3 scripts/upstream-security-check.py "
                 f"--from {pinned.split('-')[0]} --to {latest.split('-')[0]}` by hand.)")
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
    try:
        version = sh(sys.executable, "-I", str(ROOT / "scripts/ci/bump-release.py"), "--ucw-tag", latest, "--ucw-commit", latest_commit).stdout.strip()
    except RuntimeError as e:
        # Usually a file that no longer names the version where bump-release.py expects it: nothing a later pass fixes.
        ensure_issue(title, body + f"\n\n**The patches apply, but the version bump failed, so nothing was built:**\n\n```\n{e}\n```\n\n"
                     "Fix the file it names on main (or scripts/ci/bump-release.py); the pipeline tries again on every pass.",
                     ["needs-human"], comment_marker=f"bump-failed-{latest}")
        raise
    changed = sh("git", "-C", str(ROOT), "diff", "--name-only").stdout.split()
    tag = f"v{version}"
    ensure_issue(title, body + f"\n\nAutomated: release `{tag}` is built, signed and submitted to the Store by scripts/ci/pipeline.py.",
                 ["auto-release"])
    oid = act(f"commit {changed} to main", lambda: signed_commit("main", bump_message(latest, version), changed))
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
    if last["conclusion"] == "cancelled":
        # The whole run was cancelled: someone stopped it (a lost runner or a timeout fails the run instead).
        log(f"build run {last['id']} was cancelled: not rerun; start build-windows on {tag} by hand to go on")
        return
    if last["conclusion"] != "success":
        return handle_failed_build(tag, last)
    return handle_release(tag, last["updated_at"])


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


def handle_release(tag, built_at):
    """built_at: when the build that made the draft's packages finished. A build that is run again for the same tag
    replaces the packages and removes the old signatures (release job of build-windows.yml), so sign-release and
    store-submit runs from before that moment say nothing about what is in the draft now."""
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
        signs = [r for r in runs_of(SIGN, title_prefix=f"sign-release {tag}") if r["created_at"] >= built_at]
        if any(r["status"] != "completed" or r["conclusion"] == "success" for r in signs):
            log("sign-release is running or done, waiting for the assets")
            return
        if len(signs) >= MAX_STORE_FAILURES:
            ensure_issue(f"Signing failed: {tag}", f"sign-release failed {len(signs)} times for {tag}: {signs[0]['html_url']}", ["needs-human"])
            return
        dispatch(SIGN, "main", tag=tag)
        return
    subs = [r for r in runs_of(STORE, title_prefix=f"store-submit {tag}") if r["created_at"] >= max(STORE_VERDICT_SINCE, built_at)]
    if any(r["status"] != "completed" for r in subs):
        log("store-submit is running")
        return
    if any(r["conclusion"] == "success" for r in subs):
        act(f"publish the release {tag} as a pre-release", lambda: sh("gh", "release", "edit", tag, "--repo", REPO, "--draft=false", "--prerelease"))
        close_issue(f"Upstream update: ungoogled-chromium-windows {pinned_ucw_tag()}", f"{tag} built, signed and submitted to Microsoft Store certification.")
        return
    # Real failures, newest first; a DEFERRED run only waited for an earlier submission and is never one of them.
    # Stops reading annotations once the limit is reached.
    failed = []
    for r in subs:
        if r["conclusion"] != "failure" or len(failed) >= MAX_STORE_FAILURES:
            continue
        notes = annotation_text(r["id"])
        if "DEFERRED" in notes:
            if r is subs[0]:
                log("store-submit deferred: an earlier submission is still in certification")
            continue
        failed.append((r, notes))
    if len(failed) >= MAX_STORE_FAILURES:
        last, notes = failed[0]
        ensure_issue(f"Store submission failed: {tag}", f"store-submit failed {len(failed)} times for {tag}; last run {last['html_url']}\n\n"
                     f"```\n{excerpt(notes)}\n```", ["needs-human"], comment_marker=f"store-failed-{last['id']}")
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
        oid = signed_commit(branch, "pipeline selftest", [".pipeline-selftest"], head=main_sha)
        v = api(f"repos/{REPO}/git/commits/{oid}")["verification"]
        log(f"commit {oid}: verified={v['verified']} reason={v['reason']}")
        if not v["verified"]:
            raise RuntimeError("the API commit is not signed: main would refuse it (" + v["reason"] + ")")
    finally:
        sh("gh", "api", "-X", "DELETE", f"repos/{REPO}/git/refs/heads/{branch}", check=False)


def delete_superseded_draft(tag, newest):
    """Deletes the draft release of an older tag (the tag itself stays). Never a published release, and not a draft whose
    package went to Store certification or is on its way there: that submission is a fact the draft documents."""
    r = sh("gh", "release", "view", tag, "--repo", REPO, "--json", "isDraft", "--jq", ".isDraft", check=False)
    if r.returncode != 0 or r.stdout.strip() != "true":
        return
    if any(s["status"] != "completed" or s["conclusion"] == "success" for s in runs_of(STORE, title_prefix=f"store-submit {tag}")):
        log(f"draft release of {tag} kept: it has a Store submission")
        return
    act(f"delete the draft release of {tag}, superseded by {newest}",
        lambda: sh("gh", "release", "delete", tag, "--repo", REPO, "--yes"))


def retire_superseded(newest, older):
    """Only the newest release tag is taken further (a newer upstream version replaces the one before it, also in the
    Store queue). What an older tag still has under way is stopped, so it does not hold runners for a build nobody
    will release, its upstream issue is closed and its draft release is deleted (see delete_superseded_draft)."""
    for tag in older:
        running = False
        for run in runs_of(BUILD, branch=tag):
            if run["status"] != "completed":
                running = True
                act(f"cancel build run {run['id']} of {tag}, superseded by {newest}",
                    lambda run=run: sh("gh", "run", "cancel", str(run["id"]), "--repo", REPO, check=False))
        if not running:  # a run just cancelled may still be finishing its release job: the next pass deletes the draft
            delete_superseded_draft(tag, newest)
    current = f"Upstream update: ungoogled-chromium-windows {pinned_ucw_tag()}"
    r = sh("gh", "issue", "list", "--repo", REPO, "--state", "open", "--search", '"Upstream update" in:title', "--json", "number,title", "--limit", "50")
    for i in json.loads(r.stdout):
        if i["title"].startswith("Upstream update: ") and i["title"] != current:
            act(f"close issue #{i['number']} (superseded)", lambda i=i: sh(
                "gh", "issue", "close", str(i["number"]), "--repo", REPO, "--comment",
                f"Superseded by {newest}: this version is not taken further (its builds are cancelled and its draft release deleted)."))


def release_pass():
    tags = release_tags()
    if tags:
        retire_superseded(tags[0], tags[1:4])
        step_release(tags[0])


def main():
    if sys.argv[1:] == ["selftest"]:
        selftest()
        return
    r = sh("gh", "api", f"repos/{REPO}/actions/variables/PIPELINE_ENABLED", "--jq", ".value", check=False)
    if r.returncode == 0 and r.stdout.strip().lower() == "false":
        log("PIPELINE_ENABLED=false: nothing to do")
        return
    errors = []
    # The release of a build in flight first: a slow or failing upstream step must not hold it up.
    for name, fn in (("release", release_pass), ("upstream", step_upstream)):
        try:
            fn()
        except Exception as e:  # one failing step must not hide the others; the next pass tries again
            log(f"::error::{name}: {e}")
            errors.append(name)
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
