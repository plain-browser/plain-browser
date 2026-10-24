# Roadmap: privacy features and project security

Status 2026-10-24. Effort figures are estimates from the work done so far, not measurements.

## Privacy features

| Feature | Plan | Effort | Status |
|---|---|---|---|
| Strip tracking parameters from links (`utm_*`, `fbclid`, ...) and bypass tracking redirects | See below | days to 1-2 weeks | not started |
| Fingerprint randomization with per-site exceptions (banks) | See below | weeks, plus rebase cost on every Chromium update | design only, decision needed |

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
- Suggested order: parameter stripping first (cheap, high value), then the per-site exception UI, fingerprinting last
  and only for the few surfaces with the best value for the maintenance cost (canvas, audio, navigator).

## Project security

| Item | Where | Status |
|---|---|---|
| Pin GitHub Actions by commit SHA, least-privilege tokens | `.github/workflows/*` | **done** (not yet pushed) |
| Dependabot for Actions and the adblock Cargo crates | `.github/dependabot.yml` | **done** (not yet pushed) |
| Upstream tracking: issue when a newer ungoogled-chromium-windows tag exists | `upstream-watch.yml` | **done**, logic dry-run on real data, not yet run on GitHub |
| Signed releases: SHA256SUMS plus keyless cosign bundles | `sign-release.yml`, `SECURITY.md` | written, **untested** until pushed |
| Security policy | `SECURITY.md` | done; needs private vulnerability reporting enabled |
| Branch protection (PR, review, no force-push, require signed commits) | GitHub settings | **needs the repository owner**: the API token used here cannot read or change it |
| Secret scanning, push protection, Dependabot alerts | GitHub settings | to confirm in Settings, Code security |
| GPG/SSH signing of commits and tags | local git config and a key | needs your key; not set up |
| Windows code signing (Authenticode) | certificate or Azure Artifact Signing | open; eligibility for a Polish individual/company not confirmed |
| Org-level 2FA, separate CI accounts | GitHub org | the repo is under a personal account: create an organization first if more people join |
| Reproducible builds | build pipeline | long-term. Not realistic soon: our profile differs from upstream's and builds are not bit-identical. First goal is verifiable releases (above), then deterministic flags. |
| CVE regression check for our patches | CI | partial: patches must apply cleanly (checked on each bump). A real check needs a list of security-relevant files our patches touch, compared against upstream commits. |
| Few, small patches on top of ungoogled-chromium | repo layout | already the design; keep each patch under one category (`patches/series`) |

## Limits of what the signing items give
- A cosign bundle ties a file to this repository's workflow identity. It says nothing about the builder machine.
- Build attestations (SLSA provenance) only make sense once builds run in GitHub Actions; today they run on a private VM.
- Branch protection with required reviews blocks a single-maintainer workflow unless you add yourself as exempt or
  add a second reviewer; decide this before enabling it.
