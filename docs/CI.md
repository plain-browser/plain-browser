# Building Plain Browser in GitHub Actions

Status 2026-10-26: `.github/workflows/build-windows.yml` and `scripts/ci/*.ps1` are written and lint clean
(actionlint, PyYAML) but **have not run yet**. Until the first green run, releases are still built by hand on the
private VM (`scripts/build.ps1`, about 7 hours with the alpha profile on 12 vCPU). Everything below that is a time or
size figure is an estimate from that VM build and from ungoogled-chromium-windows' own CI, not a measurement.

## Why stages

A GitHub-hosted job may run for at most 6 hours and a standard `windows-2022` runner has 4 cores and 16 GB of RAM.
One job cannot build Chromium. ungoogled-chromium-windows solves this in its own repository with a chain of jobs
(`.github/workflows/reusable-build.yml`, 16 stages, and `.github/actions/stage`): `build.py --ci` runs ninja for
3.5 hours, then interrupts it and exits with code 124; the job zips `build/src` and uploads it as an artifact; the
next job downloads it and runs `build.py --ci` again, which skips every finished setup step and resumes ninja
(ninja only rebuilds what is missing). We reuse that mechanism unchanged at the pinned tag (`versions.toml`,
`156.0.8078.47-1.1`; `build.py` there has `--ci`, the 3.5 h timeout and exit code 124) and only add our layer around it.

## The jobs

| Job | Runner | What it does | Hands over |
|---|---|---|---|
| `prepare` (stage 0) | windows-2022 | `scripts/ci/install-tools.ps1`, then `scripts/ci/prepare-tree.ps1`: clones ungoogled-chromium-windows at the pinned tag, appends the profile flags to `flags.windows.gn`, clones Chromium (`clone.py`), runs `scripts/prepare.py`, downloads, pruning, unpacking, the ungoogled patches, the Windows patches with ours appended, domain substitution, `scripts/apply-branding.py`, the Rust toolchain copy, `scripts/vendor-adblock-crates.ps1` (gnrt vendor + gen + fixups). No GN yet. | artifact `tree-0` |
| `build-1` | windows-2022 | downloads `tree-0-*` (from this run or the earlier one it came from), `scripts/ci/build-stage.ps1`: `build.py --ci -j 4` writes `args.gn`, bootstraps GN, runs `gn gen` and bindgen, then ninja for 3.5 h | `tree-1`, or the packages |
| `build-2` ... `build-10` | windows-2022 | same, one ninja time slice each. A stage that follows a finished one is skipped without starting a runner (`if: needs.<prev>.outputs.finished != 'true'`) | `tree-N`, or the packages |
| the finishing stage | | `scripts/ci/package.ps1`: `scripts/package-plainbrowser.py` (portable zip + `mini_installer`), `scripts/package-msix.py`, `SHA256SUMS.txt` | artifact `plainbrowser-<profile>` (14 days) |
| `complete` | ubuntu | fails the run when no stage finished (same as upstream's job of that name) | |
| `release` | ubuntu | tag runs only: draft GitHub release `v*` with `PlainBrowser_*` and `SHA256SUMS.txt`; `contents: write` only here | |
| `cleanup` | ubuntu | deletes the `tree-1` ... `tree-10` artifacts of the run (`actions: write` only here); `tree-0-*` stays for reuse | |

Order of our layer is the one `docs/ARCHITECTURE.md` section 2 describes: `prepare.py` before the patches (it adds
`patches/series` to the upstream series, drops the upstream patches and pruned files in `patches/upstream-revert.list`
and copies `src/plainbrowser` into the tree), `apply-branding.py` after the patches, `vendor-adblock-crates.ps1`
after the Rust toolchain exists and before `gn gen`. `prepare-tree.ps1` runs the setup half of upstream's `build.py`
step by step with upstream's own utilities (`clone.py`, `downloads.py`, `prune_binaries.py`, `patches.py`,
`domain_substitution.py`, same arguments as `build.py`) because `build.py` runs setup and ninja in one go and our
steps have to sit in between. From stage 1 on, `build.py --ci` is in charge, exactly as upstream runs it.

### Hand-over between jobs

`scripts/ci/pack-tree.ps1` zips `C:\pb\ucw` (the ungoogled-chromium-windows checkout with `build/src` and
`out/Default` inside, without `build/download_cache`) with 7-Zip, `-mx=3 -mtc=on` like upstream (`-mtc` keeps the
NTFS timestamps ninja compares). The workflow uploads it with `compression-level: 0` as `tree-<stage>` with a 2-day
retention and the next stage downloads and extracts it to the same path. The path (`C:\pb`, `PB_WORK`) must be
identical on every runner, since `args.gn` and the ninja logs contain absolute paths. Each stage pays the upload and
the download once; upstream's tree zips are tens of GB, so count roughly half an hour per side on top of the 3.5 h
of ninja. Nothing uses the Actions cache: it is capped at 10 GB per repository, far below a Chromium tree.

### Reuse of the prepared tree

`prepare` names its artifact `tree-0-<profile>-<hash>`, where the hash covers everything `prepare-tree.ps1` reads
(`versions.toml`, `patches/`, `src/`, `branding/`, the `scripts/*.py` and `scripts/*.ps1` helpers, and the three CI
scripts it runs), and first asks the artifacts API (`GET /repos/{repo}/actions/artifacts?name=...`) for a non-expired
artifact of that name from an earlier `build-windows` run of this repository on `main` or a `v*` tag (the lookup
filters on the run's repository, branch, workflow file and event: a pull request from a fork runs its own workflow
file and could upload an artifact under the same name, which is computable from public files). If one exists, the job stops after that lookup and `build-1` downloads
it from the earlier run (`download-artifact` with `run-id`, which is why `prepare` and `build-1` have
`actions: read`). Otherwise the tree is built and uploaded with a 14-day retention; `cleanup` leaves `tree-0-*`
alone. A change to any of the hashed inputs, or to the profile, gives a new name and a fresh tree. Saves about an
hour per run (the Chromium clone alone is 30 minutes) and makes a failed stage cheap to retry.

The hand-over zips use 7-Zip's `-mx=1` rather than upstream's `-mx=3`: a few percent larger, much faster to pack.

### Triggers and inputs

- `workflow_dispatch` with `profile` = `release` (default), `alpha` or `official`.
- `push` of a tag `v*`: release profile, and the `release` job creates a **draft** release. `sign-release.yml` then
  takes the assets from that release, recomputes `SHA256SUMS.txt` and adds the cosign bundles; it expects nothing
  but the files on the release, so a draft is enough. Review the draft and the Store submission before publishing.
- `workflow_dispatch` with `resume_run` = a run id and `resume_stage` = 5, 6 or 7: skips `prepare` and the stages before
  it, restores `tree-4` (`tree-5`, `tree-6`) of that run (the per-stage trees live 2 days) and continues from that stage;
  stages 5 and 6 first run `scripts/ci/hotfix-tree.py` (it also copies this revision's `version.h`). For a stage that failed
  on a source problem fixed later in `patches/` (a ninja error, not a lost runner): fix, tag the commit with a tag
  that has `[skip ci]` in its commit message, and dispatch `--ref <tag>` with `resume_run`, so the `release` job
  (which needs a tag ref) runs. The hotfix script has to reproduce what the fixed patch would have done to the tree;
  remove it from the workflow once no tree needs it.
- Never `pull_request`: the build fetches and executes code from the network for hours, and a tag run writes a release.

`concurrency` serialises runs per ref without cancelling: a second push while a 30-hour run is in progress would
otherwise throw it away.

### Permissions and pins

Workflow-level `permissions: contents: read`; `release` adds `contents: write`, `cleanup` adds `actions: write`.
Every action is pinned to a commit SHA with its version in a comment (`actions/checkout` v7.0.1, `setup-python`
v7.1.0, `upload-artifact` v7.0.2, `download-artifact` v8.0.2); Dependabot (`.github/dependabot.yml`) proposes bumps.
The release is created with the preinstalled `gh` CLI and `GITHUB_TOKEN`, not a third-party action.

### Security checks on the repository itself

- `workflow-lint.yml`: zizmor on `.github/` (template injection, excessive permissions, unpinned actions, secrets
  outside an environment); fails on a finding. `uvx zizmor .github` runs it locally. Permissions are declared per job
  (`permissions: {}` at the top of the workflows that need write access).
- `codeql.yml`: CodeQL for `actions`, `python` and `javascript-typescript` (not C++/Rust or the Chromium tree).
- `dependency-review.yml`: a pull request that brings in a dependency with a high-severity advisory fails.
- `scorecard.yml`: OpenSSF Scorecard, weekly and on pushes to `main`.
- Dependabot (`.github/dependabot.yml`): GitHub Actions, cargo, pip (`scripts/ci/requirements.txt`, with hashes) and npm,
  weekly, a 7-day cooldown for new releases, grouped.
- Provenance: the `release` job attests the packages and `SHA256SUMS.txt` (`actions/attest-build-provenance`);
  `store-submit.yml` requires the attestation (or, for builds that predate it, the build run's artifact).
- Environments: `store` (Partner Center), `extensions` (extension signing keys, Cloudflare token) and `build` (compile
  cache credentials) accept deployments from `main` only (`build` also from `v*` tags); `website` has no rule.
  Because of that a build-windows run on another branch is refused: merge to `main` or tag to build.
  Status 2026-11-13: the extension signing keys (`EXT_KEY_*`) are secrets of `extensions` only. `CLOUDFLARE_API_TOKEN`
  and the `SCCACHE_*` credentials are still repository secrets (jobs fall back to them): move them as
  `docs/GITHUB_SETUP.md` section 2 says, then delete the repository copies.

### Profiles

- **release** (default, since 2026-11-10, not yet built): `is_official_build=true`, `chrome_pgo_phase=0`,
  `use_thin_lto=false`, `enable_precompiled_headers=false`, `generate_about_credits=true`, appended to upstream's
  `flags.windows.gn` the same way `scripts/build.ps1` does. Official-build code paths without LTO and PGO: stack
  variables are zero-initialised (`init_stack_vars_zero`), Rust panics abort at once, the release linker settings
  apply, and these are the settings Chromium's own stable channel is built and fuzzed with, minus the optimisation.
  CFG is on in every profile (`win_enable_cfg_guards = !is_debug`). No LTO: links fit in 16 GB and `-j 4` works.
  If the first run fails to compile only here, fall back to `alpha` with the `profile` input and find the
  official-only error.
- **alpha**: the same with `is_official_build=false`. This is what the Store received up to 0.2.0-beta.7 and what the
  VM builds for development; not for releases.
- **official**: upstream's flags unchanged (`is_official_build=true`, `chrome_pgo_phase=2`, ThinLTO), `-j 2` like
  upstream. Upstream needs up to 16 stages of 3.5 h for it; `build-windows.yml` has 10, so expect `complete` to fail
  unless stages are added (copy a `build-N` block, bump the numbers, extend the `needs` lists). The PGO profile
  (`chrome_pgo_phase=2`) is downloaded by `clone.py -p win64` in `prepare` either way; the cost is in the LTO link and
  the longer compile, not the download. Keep `release` until there is a reason to pay for official.

### Compile cache (optional)
With the repository variable `SCCACHE_BUCKET` set, the build stages compile through sccache with a Cloudflare R2 bucket as
the cache, shared by all stages and runs; setup and limits in [BUILD_CACHE.md](BUILD_CACHE.md), "Status 2026-11-10".
It is what should bring a security release (a few hundred changed files) from a full build to a fraction of it; the time
table below is for a cold cache.

### Expected time and cost

| | release, alpha | official |
|---|---|---|
| `prepare` | 1.5 to 2 h (Chromium clone and downloads dominate; gnrt builds for about 10 min) | same |
| ninja total | the VM needs about 5.5 h of ninja on 12 vCPU; on 4 slower cores expect 16 to 22 h, i.e. 5 to 7 slices | upstream: up to 16 slices |
| per stage | 3.5 h ninja + about 10 min setup (SDK install, unpack) + about 1 h pack, upload, download | same |
| wall clock | about 28 to 35 h | over 60 h, and more than 10 stages |
| runner minutes | about the same as wall clock: the chain is serial | |

For a **public repository** GitHub-hosted standard runners are free (minutes and the artifact storage that comes with
them; only larger runners are billed), which is what upstream relies on. For a private repository the plan quota
(2,000 min/month and 500 MB of artifacts on Free) does not cover a single run. The repository is public, so a run
costs nothing but time; the storage of the `tree-*` artifacts (several tens of GB per run while it lasts) is why
`cleanup` deletes them and their retention is 2 days.

### Limits that shape the design

- 6 h per job on GitHub-hosted runners (5 days on self-hosted ones, which is why the old placeholder workflow wanted
  a self-hosted runner). 35 days per workflow run; a 10-stage chain is far below.
- 4 cores, 16 GB RAM, `windows-2022` image; the spec sheet's "14 GB SSD" is nominal, upstream's full official build
  runs on the `C:` drive of these runners. `install-tools.ps1` prints the free space at start and end of each script.
- Artifacts: no documented per-artifact size limit; upstream uploads tens of GB per stage. Uploading the same artifact
  name twice in one run is not allowed, hence `tree-0`, `tree-1`, ... and one `plainbrowser-<profile>`.
- Actions cache: 10 GB per repository, so it is not used.
- The runner image lacks the Windows SDK Chromium 156 pins (`10.0.28000.0`); `install-tools.ps1` installs it with the
  same installer URL and feature set as upstream's prepare action at the pinned tag, in every job (fresh runner each
  time, 3 to 5 minutes). The image has VS 2022 Enterprise with MSVC 14.5x, Git, 7-Zip, Python 3.12, `gh`, and the
  SDKs 10.0.22621/26100 whose `MakeAppx.exe` `package-msix.py` uses.

### Things done differently from the VM, and why

- `scripts/build.ps1` is not used: it self-elevates with a UAC prompt, requires 200 GB free and loops over
  `build.py --ci` for up to 30 passes on one machine. `build-stage.ps1` does one pass and reports `finished`.
- `scripts/apply-plainbrowser.ps1` is not used: it is the flow for an already-built tree (reverses upstream patches
  with `patch -R`). A fresh tree takes the `prepare.py` flow, in which our patches go in with the upstream series,
  before domain substitution (see the hostname rule in `patches/series`).
- `scripts/vendor-adblock-crates.ps1` and the other repository scripts look for Python under `%ProgramFiles%\Python3*`
  or `%LOCALAPPDATA%\Programs\Python` (where the installer puts it on the VM); `actions/setup-python` installs under
  `C:\hostedtoolcache`. `install-tools.ps1` adds a junction `%ProgramFiles%\Python312` to that directory and a
  `python3.exe` copy, so the scripts run unchanged.
- `prepare.py` removes the files we need back from `ungoogled-chromium/pruning.list`, which `build.py` reads only with
  `--tarball`; the clone flow prunes with ungoogled-chromium-windows' own `pruning.list` (identical content at this
  tag). `prepare-tree.ps1` drops the same lines there too. The proper fix belongs in `prepare.py`.
- No Authenticode signing anywhere (project decision: Microsoft Store only, the Store signs the MSIX). `sign-windows.ps1`
  is not called. The MSIX gets the Store identity from the repository variables `MSIX_IDENTITY_NAME`,
  `MSIX_PUBLISHER`, `MSIX_PUBLISHER_DISPLAY` (Partner Center, Product identity); without them it is the dev package.

## Release pipeline (hourly, no human step)

`.github/workflows/pipeline.yml` runs `scripts/ci/pipeline.py` on a schedule (three times an hour; GitHub runs schedules on a best-effort basis and delayed them by hours here). All state is read from GitHub (tags, workflow
runs, the draft release), so a pass that dies half way is finished by the next one. Stop it with the repository
variable `PIPELINE_ENABLED=false`; `DRY_RUN=1 python3 scripts/ci/pipeline.py` prints what a pass would do.

1. **Upstream.** A newer `ungoogled-chromium-windows` tag than the one in `versions.toml`: dry-run our patches on it
   (`check-patches.py`). All apply: `scripts/ci/bump-release.py` pins the tag and the commit it points at
   (`ungoogled_chromium_windows_commit`; `prepare-tree.ps1` stops when the cloned tag is at another commit, so a tag
   moved after it was pinned is not built) and raises the product version by one patch
   level (`0.1.5-alpha.1` -> `0.1.6-alpha.1`; the Store package version is `major.minor.patch.0` and must grow with every
   submission), the files go to `main` as one commit made through the GitHub API (that commit is signed by GitHub, which
   the `protect-main` ruleset requires), the tag `vX.Y.Z-alpha.1` is created and `build-windows` is dispatched on it.
   A pass that dies between the commit and the tag is finished by the next one (the pipeline's own bump commit
   without its tag is tagged and built); the commit is refused when `main` moved after the pass checked it out.
   A bump that fails (a file no longer names the version where `bump-release.py` looks for it) is reported in the
   same issue with label `needs-human`.
   A patch that does not apply, applies with fuzz (context changed: `check-patches.py --regenerate`, then review),
   or an upstream patch whose dropped hunks no longer match `patches/upstream-revert.hunks`: issue
   `Upstream update: ...` with label `needs-human`; the build starts by itself in the first pass after the patches
   are fixed.
2. **Build.** The newest release tag has no build: dispatch one. A failed build: the logs of the failed jobs are
   classified. Runner and network problems (curl, shutdown signal, artifact transfer) are rerun with `gh run rerun --failed`
   (up to 4 times; finished stages are kept), unknown ones twice. Source problems (compile error, patch that does not
   apply) are not rerun: issue `Build failed: <tag>` with the log excerpt, label `needs-human`. After the fix the run
   is resumed from the last stage tree with `resume_run` / `resume_stage` (see Triggers and inputs). A run that was
   cancelled as a whole was stopped by a person and is left alone: start `build-windows` on the tag by hand to go on.
   Only the newest release tag is taken further: builds of the three tags before it that are still running are
   cancelled, their `Upstream update` issues closed and their draft releases deleted (the tags stay; a draft whose
   package went to Store certification, and any published release, is kept).
3. **Release.** Finished build: `sign-release` on the draft, then `store-submit` with `commit=true`, then the release is
   published as a pre-release and the upstream issue is closed. `store-submit` leaves an earlier submission that is
   still in certification alone (the Store keeps one pending submission) and fails with `DEFERRED`; the pipeline then
   waits an hour without counting a failure. Other failures are retried up to 3 times, then issue
   `Store submission failed: <tag>`.

What the pipeline cannot do is repair a source problem by itself: that needs a person (or an agent that can edit
`patches/`). It makes the problem visible within the hour and resumes the build afterwards.

`pipeline.yml` has a manual `selftest` input: it commits to a scratch branch through the API and checks that GitHub
signed the commit. Run it after changing the token permissions or the `protect-main` ruleset.

## Microsoft Store submission

`.github/workflows/store-submit.yml` is separate from the build on purpose: the build runs code fetched from the
network for a day and a half and must not see the Partner Center credentials. It is dispatched by the pipeline (or by
hand for any tag) and has two jobs:

1. `verify` (Ubuntu, no secrets): downloads the draft release, checks the cosign bundles (the identity is exactly
   `sign-release.yml@refs/heads/main`: a copy of the workflow on another branch is refused) and the checksum, checks that
   the tag's commit is on `main` and that the `.msix` is byte for byte the one a successful `build-windows` run of this
   repository made for that commit (its artifact lives 14 days; for an older tag build again), checks that `AppxManifest.xml` carries `MSIX_IDENTITY_NAME` (a dev package is refused) and hands
   only the verified `.msix` on as an artifact;
2. `submit` (Windows, environment `store`): `msstore reconfigure`, then `msstore publish <file.msix> -id <STORE_PRODUCT_ID>
   --packageRolloutPercentage <n>` (Microsoft Store Developer CLI, `microsoft/setup-msstore-cli` pinned to a commit).
   It runs on Windows because `msstore` keeps its credentials in the system keyring (libsecret on Linux, which the
   runner image lacks). `msstore` deletes the pending submission, creates a new one with the package and, with
   `commit=true`, commits it to certification. The first argument must be the package file: a directory is not
   recognised as a project.

The `store` environment has no required reviewer (the pipeline must run without a person); it only lets branch `main`
read the Partner Center secrets. `commit=false` passes `--noCommit` and leaves a draft in Partner Center. Rollout is the
`rollout` input, else the variable `STORE_ROLLOUT_PERCENT`, else **100**, which means no gradual rollout (the rollout is
switched off in the submission metadata). Partner Center accepts no new submission while a gradual rollout of the
previous one is in progress (`CommitFailed` / `InvalidState`, and the portal says "halt or finalize the rollout"), and
with no person in the loop nobody finalizes a 10 % rollout; a value below 100 therefore needs
`msstore submission rollout finalize` by hand before the next release. After the commit the job waits for the verdict
(`CommitStarted` becomes `PreProcessing`/`Certification`, or `CommitFailed`, which fails the job); certification itself
takes hours to days and is not waited for. The submission also gets certification notes for the restricted capability
`runFullTrust` (a new submission starts with empty notes).

Observed on the first real runs: `CommitFailed` ("active validation errors which cannot be exposed via API") for a
submission that the portal showed as valid. The cause was the gradual rollout of the previous submission still in
progress, not the content.


## Still manual

- The three `MSIX_*` repository variables and the Partner Center secrets (docs/GITHUB_SETUP.md section 5) are set once.
- Smoke test of a build: `scripts/test-adblock.ps1`, `test-privacy.ps1`, `test-safebrowsing.ps1` still run on a
  Windows machine by hand; the workflows build, package and submit but do not run the browser. The pipeline submits
  what builds, so a regression that only a run shows reaches certification; keep `STORE_ROLLOUT_PERCENT` low.
- Source problems in a build (compile error, patch that does not apply) need a person: the pipeline opens an issue
  within the hour and resumes the build after the fix.
- If the Windows SDK pin in `build/vs_toolchain.py` changes with a new upstream tag, the SDK version and installer URL
  in `scripts/ci/install-tools.ps1` (take both from upstream's `.github/actions/prepare/action.yml` at that tag).

## Not verified yet

The build, signing and Store submission have run end to end by hand. Not yet seen in a run: the pipeline's own
version-bump commit (run the `selftest` input first), a rerun of a failed stage by the pipeline, and a second
submission while the first one is in certification (`DEFERRED`).
