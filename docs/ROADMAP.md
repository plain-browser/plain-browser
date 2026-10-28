# Roadmap: privacy features and project security

Status 2026-10-26. Effort figures are estimates from the work done so far, not measurements.

## Privacy features

| Feature | Plan | Effort | Status |
|---|---|---|---|
| Strip tracking parameters from links (`utm_*`, `fbclid`, ...) | `TrackingParamsNavigationThrottle`, `lists/plainbrowser-removeparam.txt`, `patches/30-adblock/0005` | done for navigations | **done**, verified headless on the build VM (`scripts/test-privacy.ps1`) |
| Bypass tracking redirects ("debouncing") | `browser/debounce_rules.cc`, in `TrackingParamsNavigationThrottle` | done | **done** 2026-10-28: own rule list (Google, Facebook, YouTube, Outlook Safe Links, Bing, ...), verified headless (`scripts/test-privacy.ps1`); [ADBLOCK.md](ADBLOCK.md), "Debouncing" |
| Safe Browsing (local hash database) through our proxy, no Google contact from the client | `patches/70-safebrowsing`, `patches/upstream-revert.list`, `infra/safebrowsing-proxy` | done | **built and verified end to end** 2026-10-26: the list update goes to `sb.plain-browser.app` with `X-Client` and gets HTTP 200 (public lists only: malware, phishing, unwanted software, download allowlist) |
| Fingerprint randomization with compatibility exceptions (for example online banking) | `src/plainbrowser/fingerprint`, `patches/80-fingerprint`, [FINGERPRINT.md](FINGERPRINT.md) | first slice done | **done**, verified headless on the build VM; JS logic unit-tested in Node |
| Secure defaults (HTTPS-First, secure DNS) | `patches/90-defaults/0001`, `patches/upstream-revert.list` (`doh-changes.patch`, 2 hunks) | small | **done** 2026-10-28: HTTPS-First balanced on by default (feature `HttpsFirstBalancedModeAutoEnable`, the behaviour checked with the flag on the current build: an HTTP-only site gets the warning); secure DNS automatic mode (not yet checked in a build). Already default through ungoogled: third-party cookies blocked (so bounce tracking mitigation runs), hyperlink auditing and preloading off. Not done: Global Privacy Control (Chromium 156 has it only as Google's location-targeted `UniversalOptOut` experiment) |
| Opt-in crash reports | `src/plainbrowser/crash`, `patches/95-crash/0001`, `patches/60-ui/0012`, `infra/crash-reports`, [CRASH_REPORTS.md](CRASH_REPORTS.md) | medium | **built** 2026-10-28: unit tests pass, Crashpad starts with our URL and the consent is written off by default; upload end to end, Worker deploy and symbols (PDB keeping, `symbol_level`) open |

### Tracking parameters and redirects
- adblock-rust already understands the uBlock-style `removeparam` filter option and returns a rewritten URL in its result
  (`BlockerResult.rewritten_url`), so the same engine and lists can drive this; no second rule format is needed.
- Subresources can be rewritten in the existing renderer throttle (it may change `request->url`).
- Main-frame navigations cannot: the renderer provider skips frame requests, and the address bar must show the cleaned
  URL. This needs a browser-side `NavigationThrottle` that cancels and re-issues the navigation, the way Brave's query
  filter works.
- Redirect bypass ("debouncing", e.g. `click.example/?url=https://target`) is a separate rule set: unwrap the target
  URL from known tracking redirectors. Done with our own rules (nothing copied from Brave's list).
- Per-site toggle: **done** 2026-10-26, shares the exception list (`plainbrowser.shields.exceptions`) with the
  adblock and fingerprint toggles; one toolbar bubble for all three ([ADBLOCK.md](ADBLOCK.md), "UI and per-site
  exceptions").

### Fingerprint randomization
- Approach used by Brave ("farbling"): a per-site, per-session seed (hash of eTLD+1 and a random session key) that
  slightly perturbs canvas readback, WebGL/WebGPU strings, audio output and a few navigator values, plus bucketing of
  hardware values. Noise must stay inside plausible ranges: arbitrary random values are themselves a fingerprint.
- The code lives in Blink and touches many files, so patches break often on rebase. Of everything in this document this
  is the most expensive to maintain against the "small, rebase-friendly patch set" goal.
- Compatibility exceptions (for example banks): one per-site list shared with the adblock and parameter-stripping
  toggles. The toolbar control exists (toggle in the bubble, 2026-10-26; a list pref rather than a content setting,
  so no `components/content_settings` patch); the exceptions are listed and edited on
  `chrome://settings/privacy` (2026-10-27, `patches/60-ui/0009`).
- Done as a first slice without Blink patches: a script injected into every page's main world (canvas, audio,
  WebGL strings, hardwareConcurrency, deviceMemory) with a per-site, per-session seed; built-in compatibility list of
  sites that need a stable fingerprint (online banking, payments) plus `--fingerprint-exceptions=a.pl,b.pl`. Details and limits in [FINGERPRINT.md](FINGERPRINT.md).
  The per-site toggle in the toolbar bubble and the settings page for the exception list are done. Next: Blink-level
  farbling for the surfaces where a page-world override is too easy to detect.

### Safe Browsing settings (done 2026-10-27, `patches/70-safebrowsing/0005`)
Settings > You and Google > Google services has an on/off toggle "Safe Browsing through the Plain Browser proxy"
(pref `safebrowsing.enabled`, the same one the Security page's radio group drives). `chrome://settings/security`
shows Standard (default) / No protection only: the Enhanced protection row is hidden (its real-time lookups are not
something our proxy supports and the policy promises standard protection only) and the descriptions say where
requests go, in English and Polish. Verified with `scripts/test-safebrowsing-settings.ps1`.

## Project security

| Item | Where | Status |
|---|---|---|
| Pin GitHub Actions by commit SHA, least-privilege tokens | `.github/workflows/*` | **done** (not yet pushed) |
| Dependabot for Actions and the adblock Cargo crates | `.github/dependabot.yml` | **done** (not yet pushed) |
| Upstream tracking: issue when a newer ungoogled-chromium-windows tag exists | `upstream-watch.yml` | **done**, logic dry-run on real data, not yet run on GitHub |
| Signed releases: SHA256SUMS plus keyless cosign bundles | `sign-release.yml`, `SECURITY.md` | written; the workflow now verifies every bundle before uploading, so the first run on a release is the test |
| Security policy | `SECURITY.md` | **done**; private vulnerability reporting enabled 2026-10-26 |
| Branch protection (no force-push, no deletion, linear history, signed commits) | `scripts/github-protect-main.sh` | **done** 2026-10-26: ruleset `protect-main` active (`scripts/github-protect-main.sh`), pushes verified against it |
| Secret scanning, push protection, Dependabot alerts | GitHub settings | **done** 2026-10-26, confirmed through the API: secret scanning, push protection, Dependabot alerts and security updates enabled |
| GPG/SSH signing of commits and tags | local git config (`gpg.format=ssh`, `commit.gpgsign=true`, key `~/.ssh/id_ed25519.pub`) | **done**: key registered as a signing key on GitHub, commits show as Verified |
| Windows code signing (Authenticode) | `scripts/sign-windows.ps1` | **dropped 2026-10-26**: Plain Browser ships only as an MSIX through the Microsoft Store, which signs the package itself. The script stays for a possible future direct-download channel |
| Updates | Microsoft Store | **covered by the Store** (automatic app updates, usually within a day of certification). No own updater while the Store is the only channel; see `docs/GITHUB_SETUP.md`, section 6 |
| Org-level 2FA, second owner, separate CI account | GitHub org `plain-browser` | organization exists; 2FA requirement, second owner and bot account still to set up (`docs/GITHUB_SETUP.md`, section 3) |
| Reproducible builds | build pipeline, `build-windows.yml` | long-term. Not realistic soon: our profile differs from upstream's and builds are not bit-identical. First goal is verifiable releases (above), then builds in GitHub Actions instead of the private VM (`build-windows.yml`, staged over several runners, written 2026-10-26, not yet run; `docs/CI.md`), then deterministic flags. |
| Patches apply on the next upstream tag | `scripts/check-patches.py`, `patches-check.yml`, `upstream-watch.yml` | **done**: dry-run without a Chromium checkout on every change to `patches/` and on every new upstream tag |
| Filter lists parsed outside the browser process | `src/plainbrowser/adblock/engine`, `patches/30-adblock/0008` | **done** 2026-10-28: the adblock engine runs in its own utility process (`kService` sandbox), fails open and restarts after a crash, dropping downloaded lists that may have caused it |
| CVE regression check for our patches | `scripts/upstream-security-check.py`, `upstream-watch.yml` | **done** 2026-10-28: for a version bump, every upstream commit (GitHub mirror `chromium/chromium`) in a file our patches edit, with security-looking ones marked; part of the upstream-watch issue. 156.0.8078.47 → .73: 2 of 43 files changed, both a headless fix |
| Few, small patches on top of ungoogled-chromium | repo layout | already the design; keep each patch under one category (`patches/series`) |

## Limits of what the signing items give
- A cosign bundle ties a file to this repository's workflow identity. It says nothing about the builder machine.
- Build attestations (SLSA provenance) only make sense once builds run in GitHub Actions; today they run on a private VM.
  `build-windows.yml` (`docs/CI.md`) is the first step: once a release is built there, the `release` job can add
  `actions/attest-build-provenance` for the packages, and sign-release.yml's note that the build is not provenance
  can go.
- Branch protection with required reviews blocks a single-maintainer workflow unless you add yourself as exempt or
  add a second reviewer; decide this before enabling it.
