# Roadmap: privacy features and project security

Status 2026-10-26. Effort figures are estimates from the work done so far, not measurements.

## Privacy features

| Feature | Plan | Effort | Status |
|---|---|---|---|
| Strip tracking parameters from links (`utm_*`, `fbclid`, ...) | `TrackingParamsNavigationThrottle`, `lists/plainbrowser-removeparam.txt`, `patches/30-adblock/0005` | done for navigations | **done**, verified headless on the build VM (`scripts/test-privacy.ps1`) |
| Bypass tracking redirects ("debouncing") | See below | days | not started |
| Fingerprint randomization with per-site exceptions (banks) | `src/plainbrowser/fingerprint`, `patches/80-fingerprint`, [FINGERPRINT.md](FINGERPRINT.md) | first slice done | **done**, verified headless on the build VM; JS logic unit-tested in Node |

### Tracking parameters and redirects
- adblock-rust already understands the uBlock-style `removeparam` filter option and returns a rewritten URL in its result
  (`BlockerResult.rewritten_url`), so the same engine and lists can drive this; no second rule format is needed.
- Subresources can be rewritten in the existing renderer throttle (it may change `request->url`).
- Main-frame navigations cannot: the renderer provider skips frame requests, and the address bar must show the cleaned
  URL. This needs a browser-side `NavigationThrottle` that cancels and re-issues the navigation, the way Brave's query
  filter works.
- Redirect bypass ("debouncing", e.g. `click.example/?url=https://target`) is a separate rule set: unwrap the target
  URL from known tracking redirectors. Reusing Brave's debounce rule list would save work; **its license and data format
  must be checked before copying anything.**
- Per-site toggle: shares the exception list with the adblock toggle (below), so the UI work comes first.

### Fingerprint randomization
- Approach used by Brave ("farbling"): a per-site, per-session seed (hash of eTLD+1 and a random session key) that
  slightly perturbs canvas readback, WebGL/WebGPU strings, audio output and a few navigator values, plus bucketing of
  hardware values. Noise must stay inside plausible ranges: arbitrary random values are themselves a fingerprint.
- The code lives in Blink and touches many files, so patches break often on rebase. Of everything in this document this
  is the most expensive to maintain against the "small, rebase-friendly patch set" goal.
- Exceptions ("banks"): one per-site content setting, shared with the adblock and parameter-stripping toggles. Needs a
  settings page and a toolbar control that do not exist yet.
- Done as a first slice without Blink patches: a script injected into every page's main world (canvas, audio,
  WebGL strings, hardwareConcurrency, deviceMemory) with a per-site, per-session seed; built-in exception list of
  banks plus `--fingerprint-exceptions=a.pl,b.pl`. Details and limits in [FINGERPRINT.md](FINGERPRINT.md).
  Next: a settings page for the exception list, then Blink-level farbling for the surfaces where a page-world
  override is too easy to detect.

## Project security

| Item | Where | Status |
|---|---|---|
| Pin GitHub Actions by commit SHA, least-privilege tokens | `.github/workflows/*` | **done** (not yet pushed) |
| Dependabot for Actions and the adblock Cargo crates | `.github/dependabot.yml` | **done** (not yet pushed) |
| Upstream tracking: issue when a newer ungoogled-chromium-windows tag exists | `upstream-watch.yml` | **done**, logic dry-run on real data, not yet run on GitHub |
| Signed releases: SHA256SUMS plus keyless cosign bundles | `sign-release.yml`, `SECURITY.md` | written; the workflow now verifies every bundle before uploading, so the first run on a release is the test |
| Security policy | `SECURITY.md` | done; needs private vulnerability reporting enabled |
| Branch protection (no force-push, no deletion, linear history, signed commits) | `scripts/github-protect-main.sh` | **run it yourself**: the assistant's token cannot change repository rules |
| Secret scanning, push protection, Dependabot alerts | GitHub settings | to confirm in Settings, Code security |
| GPG/SSH signing of commits and tags | local git config (`gpg.format=ssh`, `commit.gpgsign=true`, key `~/.ssh/id_ed25519.pub`) | **configured in this checkout**; register the key as a signing key on GitHub (`scripts/github-protect-main.sh` does it) so commits show as Verified |
| Windows code signing (Authenticode) | `scripts/sign-windows.ps1`, called by `build.ps1` | script ready (signtool, SHA-256, RFC 3161 timestamp); **no certificate yet**, so it signs nothing. Eligibility for a Polish individual/company not confirmed |
| Org-level 2FA, separate CI accounts | GitHub org | the repo is under a personal account: create an organization first if more people join |
| Reproducible builds | build pipeline | long-term. Not realistic soon: our profile differs from upstream's and builds are not bit-identical. First goal is verifiable releases (above), then deterministic flags. |
| Patches apply on the next upstream tag | `scripts/check-patches.py`, `patches-check.yml`, `upstream-watch.yml` | **done**: dry-run without a Chromium checkout on every change to `patches/` and on every new upstream tag |
| CVE regression check for our patches | CI | needs a list of security-relevant files our patches touch, compared against upstream commits |
| Few, small patches on top of ungoogled-chromium | repo layout | already the design; keep each patch under one category (`patches/series`) |

## Limits of what the signing items give
- A cosign bundle ties a file to this repository's workflow identity. It says nothing about the builder machine.
- Build attestations (SLSA provenance) only make sense once builds run in GitHub Actions; today they run on a private VM.
- Branch protection with required reviews blocks a single-maintainer workflow unless you add yourself as exempt or
  add a second reviewer; decide this before enabling it.
