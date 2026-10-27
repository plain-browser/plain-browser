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
artifact of that name from any earlier run. If one exists, the job stops after that lookup and `build-1` downloads
it from the earlier run (`download-artifact` with `run-id`, which is why `prepare` and `build-1` have
`actions: read`). Otherwise the tree is built and uploaded with a 14-day retention; `cleanup` leaves `tree-0-*`
alone. A change to any of the hashed inputs, or to the profile, gives a new name and a fresh tree. Saves about an
hour per run (the Chromium clone alone is 30 minutes) and makes a failed stage cheap to retry.

The hand-over zips use 7-Zip's `-mx=1` rather than upstream's `-mx=3`: a few percent larger, much faster to pack.

### Triggers and inputs

- `workflow_dispatch` with `profile` = `alpha` (default) or `official`.
- `push` of a tag `v*`: alpha profile, and the `release` job creates a **draft** release. `sign-release.yml` then
  takes the assets from that release, recomputes `SHA256SUMS.txt` and adds the cosign bundles; it expects nothing
  but the files on the release, so a draft is enough. Review the draft and the Store submission before publishing.
- Never `pull_request`: the build fetches and executes code from the network for hours, and a tag run writes a release.

`concurrency` serialises runs per ref without cancelling: a second push while a 30-hour run is in progress would
otherwise throw it away.

### Permissions and pins

Workflow-level `permissions: contents: read`; `release` adds `contents: write`, `cleanup` adds `actions: write`.
Every action is pinned to a commit SHA with its version in a comment (`actions/checkout` v7.0.1, `setup-python`
v7.1.0, `upload-artifact` v7.0.2, `download-artifact` v8.0.2); Dependabot (`.github/dependabot.yml`) proposes bumps.
The release is created with the preinstalled `gh` CLI and `GITHUB_TOKEN`, not a third-party action.

### Profiles

- **alpha** (default): `is_official_build=false`, `chrome_pgo_phase=0`, `use_thin_lto=false`,
  `enable_precompiled_headers=false`, `generate_about_credits=true`, appended to upstream's `flags.windows.gn` the same
  way `scripts/build.ps1` does. No PGO profile use, no LTO: links fit in 16 GB and `-j 4` works. This is what the
  VM builds and what the Store receives today.
- **official**: upstream's flags unchanged (`is_official_build=true`, `chrome_pgo_phase=2`, ThinLTO), `-j 2` like
  upstream. Upstream needs up to 16 stages of 3.5 h for it; `build-windows.yml` has 10, so expect `complete` to fail
  unless stages are added (copy a `build-N` block, bump the numbers, extend the `needs` lists). The PGO profile
  (`chrome_pgo_phase=2`) is downloaded by `clone.py -p win64` in `prepare` either way; the cost is in the LTO link and
  the longer compile, not the download. Keep alpha until there is a reason to pay for official.

### Expected time and cost

| | alpha | official |
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

## Microsoft Store submission

`.github/workflows/store-submit.yml` is separate from the build on purpose: the build runs code fetched from the
network for a day and a half and must not see the Partner Center credentials. The chain is

1. tag `v*` -> `build-windows.yml` -> draft release with the MSIX;
2. `sign-release.yml` (manual) -> `SHA256SUMS.txt` and cosign bundles on the release;
3. publish the release (by hand; a release created with `GITHUB_TOKEN` starts nothing) or run `store-submit` for a tag;
4. the job waits for a reviewer of the `store` environment, downloads the MSIX, checks the cosign bundles and the
   checksum, checks that `AppxManifest.xml` carries `MSIX_IDENTITY_NAME` (a dev package is refused), and runs
   `msstore publish dist -i dist -id <STORE_PRODUCT_ID> --packageRolloutPercentage <n>` (Microsoft Store Developer CLI,
   installed with `microsoft/setup-msstore-cli`, pinned to a commit).

The manual run has `commit=false`, which passes `--noCommit`: the submission stays a draft in Partner Center and you
send it from there. Rollout is the `rollout` input, else the variable `STORE_ROLLOUT_PERCENT`, else 10 %; raise it in
Partner Center after the update has reached a clean machine. Certification takes hours to days and the job does not
wait for it; the result is in Partner Center.

Not verified (the workflow has not run): the exact behaviour of `msstore publish <dir> -i <dir>` for a bare `.msix`
without a project, and whether a rollout percentage is accepted while the product has no earlier published package
(if not, drop `STORE_ROLLOUT_PERCENT` to 100 for the first automated submission). Option names were read from the
CLI source (`MSStore.CLI/Commands/PublishCommand.cs`).

**Package version.** `scripts/ci/package.ps1` turns `0.1.0-alpha.1` into `0.1.0.0`, so `alpha.2` would give the same
`0.1.0.0` and the Store refuses a version that is not higher than the last submitted one. Before the second Store
submission the mapping needs the pre-release number (for example `0.1.<n>.0` or a bump of the patch part); that is a
decision about the version scheme and is not made here.

## Still manual

- Set the three `MSIX_*` repository variables (Settings, Secrets and variables, Actions, Variables) before a Store build.
- Push the tag (`git tag -s v0.1.0-alpha.2 && git push origin v0.1.0-alpha.2`), wait about a day and a half.
- Review the draft release, run `sign-release.yml` on the tag, publish the release.
- Publish the release: `store-submit.yml` then waits for a reviewer of the `store` environment and sends the MSIX to
  certification (next section). Without the Partner Center setup, upload the MSIX by hand instead.
- Smoke test the build (`scripts/test-adblock.ps1`, `test-privacy.ps1`, `test-safebrowsing.ps1`) still runs on a
  Windows machine by hand; the workflow builds and packages but does not run the browser.
- Bumping the upstream tag: `versions.toml`, the patch dry-run (`patches-check.yml`) and, if the Windows SDK pin in
  `build/vs_toolchain.py` changes, the SDK version and installer URL in `scripts/ci/install-tools.ps1` (take both from
  upstream's `.github/actions/prepare/action.yml` at the new tag).

## Not verified yet

The workflow has not been run. Points that only a run can confirm: that `clone.py` and the downloads fit the first
job together with gnrt; that `build.py`'s console interrupt (Ctrl+Break to ninja after 3.5 h) works under the `pwsh`
step as it does under upstream's node action; the real size of the tree zip and the upload and download times; and the
number of stages an alpha build needs on these runners. If `complete` reports that 10 stages were not enough, add
stages.
