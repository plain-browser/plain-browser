# Roadmap: privacy features and project security

Status 2026-11-09. Effort figures are estimates from the work done so far, not measurements.

## Privacy features

| Feature | Plan | Effort | Status |
|---|---|---|---|
| Strip tracking parameters from links (`utm_*`, `fbclid`, ...) | `TrackingParamsNavigationThrottle`, `lists/plainbrowser-removeparam.txt`, `patches/20-hooks` | done for navigations | **done**, verified headless on the build VM (`scripts/test-privacy.ps1`) |
| Bypass tracking redirects ("debouncing") | `browser/debounce_rules.cc`, in `TrackingParamsNavigationThrottle` | done | **done** 2026-10-28: own rule list (Google, Facebook, YouTube, Outlook Safe Links, Bing, ...), verified headless (`scripts/test-privacy.ps1`); [ADBLOCK.md](ADBLOCK.md), "Debouncing" |
| Safe Browsing (local hash database) through our proxy, no Google contact from the client | `patches/70-safebrowsing`, `patches/upstream-revert.list`, `infra/safebrowsing-proxy` | done | **built and verified end to end** 2026-10-26: the list update goes to `sb.plain-browser.app` with `X-Client` and gets HTTP 200 (public lists only: malware, phishing, unwanted software, download allowlist) |
| Fingerprint randomization with compatibility exceptions (for example online banking) | `src/plainbrowser/fingerprint`, `patches/80-fingerprint`, [FINGERPRINT.md](FINGERPRINT.md) | first slice done | **done**, verified headless on the build VM; JS logic unit-tested in Node |
| Secure defaults (HTTPS-First, secure DNS) | `patches/90-defaults/0001`, `patches/upstream-revert.list` (`doh-changes.patch`, 2 hunks) | small | **done** 2026-10-28: HTTPS-First balanced on by default (feature `HttpsFirstBalancedModeAutoEnable`, the behaviour checked with the flag on the current build: an HTTP-only site gets the warning); secure DNS automatic mode (not yet checked in a build). Already default through ungoogled: third-party cookies blocked (so bounce tracking mitigation runs), hyperlink auditing and preloading off. Global Privacy Control: on by default since 2026-11-09 (`patches/90-defaults/0001`; Chromium 157 has the header, the API and a settings toggle behind three features). WebRTC: ungoogled already sets `disable_non_proxied_udp` (no local or public IP leaks over UDP outside a proxy) |
| Opt-in crash reports | was `src/plainbrowser/crash`, `patches/95-crash`, `infra/crash-reports` | medium | **dropped** 2026-10-28: Crashpad stays off as in ungoogled-chromium (no dumps, no uploads). A working implementation (own consent pref, Cloudflare Worker + R2, built and unit-tested) is in commit b72feb8, reverted |

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

### Request filtering: gaps and the plan (written 2026-11-10, not implemented)
Today every subresource request is decided in the renderer (`AdblockURLLoaderThrottle`, two IPC hops to the engine
process). Known gaps ([ADBLOCK.md](ADBLOCK.md), "Not done yet"): no navigation (`$document`) blocking, no requests from
the browser process, `$removeparam` only on navigations, `$redirect` with a request type guessed from the file
extension, and **no WebSockets** (they do not pass `URLLoaderThrottle`). The adblock lists have `$websocket` rules.
Checked against Chromium 157.0.8103.38:
- WebSockets: `ContentRendererClient::CreateWebSocketHandshakeThrottleProvider()` returns a
  `blink::WebSocketHandshakeThrottleProvider` (`Clone(task_runner)`, `CreateThrottle(local_frame_token, task_runner)`);
  chrome returns `WebSocketHandshakeThrottleProviderImpl` (Safe Browsing). A wrapper like `AdblockThrottleProvider`
  fits: one more hook function in `hooks/renderer_hooks.cc` and one hunk in `patches/20-hooks/0001`, the throttle
  asks the same `AdblockFilter.ShouldBlockRequest` with type `websocket`. Smallest step, closes the largest hole.
- `$document` and malware/phishing lists in the engine (the alternative to Safe Browsing, see above): a block decision
  in `TrackingParamsNavigationThrottle` (browser side, it already asks the engine), for main-frame navigations.
  `WillStartRequest` may return `BLOCK_REQUEST_AND_COLLAPSE`/`CANCEL` with an error page; needs an interstitial.
- The real fix for the request type and for browser-side requests is Brave's design, a browser-process proxy
  `URLLoaderFactory` (`ContentBrowserClient::WillCreateURLLoaderFactory`): every type, one IPC hop less, `$removeparam`
  on subresources. It replaces the renderer throttle and `WillSendRequest` redirect, so it is a rewrite of
  `adblock/renderer` and of 30-adblock/0003 behaviour, with the extensions' webRequest ordering to re-verify
  (uBO as MV2 extension must still see the requests it expects).
Status: the WebSocket wrapper is on branch `feat/websocket-and-worker-fingerprint` (2026-11-10; built and tested, see
[ADBLOCK.md](ADBLOCK.md), "WebSockets"); it stops the page from using a blocked socket but not the handshake request, so a
browser-side `WillCreateWebSocket` decision is the next step for real blocking. `$document` and the factory proxy are not started. Order
when the build VM is on: compile and test the branch, then `$document`, then the factory proxy.

### Fingerprint protection: workers and Blink (written 2026-11-10, not implemented)
The page-world script (`fingerprint_script.js`) is injected from `DidCreateScriptContext`, which workers never get:
`OffscreenCanvas` in a Dedicated Worker returns unmodified pixels and `WorkerNavigator.hardwareConcurrency` the real
core count, and the difference between page and worker is itself a signal. Checked against 157.0.8103.38:
`content::ContentRendererClient::DidInitializeWorkerContextOnWorkerThread(v8::Local<v8::Context>)` exists and
`ChromeContentRendererClient` does not override it, so the hook needs a new override (header and .cc hunk in
`patches/20-hooks/0001`). Open points: the worker has no top-level site, so the seed must come from the worker's own
origin (same-site workers match the page; cross-site ones differ), the script must tolerate missing page globals
(it is a Node-tested file, `fingerprint_script_test.js`, which can grow a worker harness first), and the session key is
already on the renderer's command line. The alternative with the lowest detectability is Blink-level noise at the
readback points (canvas, audio) as Brave does, which touches several Blink files and is the most expensive thing here
to rebase. The worker hook is on the same branch (see [FINGERPRINT.md](FINGERPRINT.md), "Workers"), built and tested;
Blink-level noise is not started. Worker coverage can be claimed once the branch is merged; the third-party-iframe worker case stays a known gap.

### Safe Browsing: terms of use (checked 2026-11-10)
Google's terms (developers.google.com/safe-browsing/terms, `v4/usage-limits`) allow the Safe Browsing API only for
"non-commercial use ... not for sale or revenue generating purposes", unless there is a separate agreement; the
commercial product is Web Risk (paid). Plain Browser is free and open source, so it fits today. Three consequences:
1. **Any revenue** (paid tier, ads, a search-engine deal, a commercial sponsor) means either Web Risk, a separate
   agreement, or dropping the proxy; decide before it happens. The alternative that needs no agreement is phishing
   and malware lists in the adblock engine (needs `$document` blocking, see "Request filtering" below).
2. **Notice**: the terms require, before a user starts using the service and in each warning, a notice that the
   reliability and accuracy cannot be guaranteed. Done in the Settings text (2026-11-10, EN and PL); the warning page
   itself is Chromium's, unchanged. Not yet checked against the wording the terms ask for by someone who owns the
   decision.
3. The proxy is a technical convenience for privacy, not a licence workaround; the terms do not mention proxies, so
   ask Google before relying on it for anything beyond the current free use.

## Project security

| Item | Where | Status |
|---|---|---|
| Pin GitHub Actions by commit SHA, least-privilege tokens | `.github/workflows/*` | **done** (not yet pushed) |
| Dependabot for Actions and the adblock Cargo crates | `.github/dependabot.yml` | **done** (not yet pushed) |
| Upstream tracking: issue when a newer ungoogled-chromium-windows tag exists | `pipeline.yml` | **done**, logic dry-run on real data, not yet run on GitHub |
| Signed releases: SHA256SUMS plus keyless cosign bundles | `sign-release.yml`, `SECURITY.md` | written; the workflow now verifies every bundle before uploading, so the first run on a release is the test |
| Security policy | `SECURITY.md` | **done**; private vulnerability reporting enabled 2026-10-26 |
| Branch protection (no force-push, no deletion, linear history, signed commits) | `scripts/github-protect-main.sh` | **done** 2026-10-26: ruleset `protect-main` active (`scripts/github-protect-main.sh`), pushes verified against it |
| Secret scanning, push protection, Dependabot alerts | GitHub settings | **done** 2026-10-26, confirmed through the API: secret scanning, push protection, Dependabot alerts and security updates enabled |
| GPG/SSH signing of commits and tags | local git config (`gpg.format=ssh`, `commit.gpgsign=true`, key `~/.ssh/id_ed25519.pub`) | **done**: key registered as a signing key on GitHub, commits show as Verified |
| Windows code signing (Authenticode) | `scripts/sign-windows.ps1` | **dropped 2026-10-26**: Plain Browser ships only as an MSIX through the Microsoft Store, which signs the package itself. The script stays for a possible future direct-download channel |
| Updates | Microsoft Store | **covered by the Store** (automatic app updates, usually within a day of certification). No own updater while the Store is the only channel; see `docs/GITHUB_SETUP.md`, section 6 |
| Org-level 2FA, second owner, separate CI account | GitHub org `plain-browser` | organization exists; 2FA requirement, second owner and bot account still to set up (`docs/GITHUB_SETUP.md`, section 3) |
| Reproducible builds | build pipeline, `build-windows.yml` | long-term. Not realistic soon: our profile differs from upstream's and builds are not bit-identical. First goal is verifiable releases (above), then builds in GitHub Actions instead of the private VM (`build-windows.yml`, staged over several runners, written 2026-10-26, not yet run; `docs/CI.md`), then deterministic flags. |
| Patches apply on the next upstream tag | `scripts/check-patches.py`, `patches-check.yml`, `pipeline.yml` | **done**: dry-run without a Chromium checkout on every change to `patches/` and on every new upstream tag |
| Filter lists parsed outside the browser process | `src/plainbrowser/adblock/engine`, `patches/30-adblock/0008` | **done** 2026-10-28: the adblock engine runs in its own utility process (`kService` sandbox), fails open and restarts after a crash, dropping downloaded lists that may have caused it |
| CVE regression check for our patches | `scripts/upstream-security-check.py`, `pipeline.yml` | **done** 2026-10-28: for a version bump, every upstream commit (GitHub mirror `chromium/chromium`) in a file our patches edit, with security-looking ones marked; part of the upstream-watch issue. 156.0.8078.47 → .73: 2 of 43 files changed, both a headless fix |
| Few, small patches on top of ungoogled-chromium | repo layout | already the design; keep each patch under one category (`patches/series`) |

## Limits of what the signing items give
- A cosign bundle ties a file to this repository's workflow identity. It says nothing about the builder machine.
- Build attestations (SLSA provenance) only make sense once builds run in GitHub Actions; today they run on a private VM.
  `build-windows.yml` (`docs/CI.md`) is the first step: once a release is built there, the `release` job can add
  `actions/attest-build-provenance` for the packages, and sign-release.yml's note that the build is not provenance
  can go.
- Branch protection with required reviews blocks a single-maintainer workflow unless you add yourself as exempt or
  add a second reviewer; decide this before enabling it.
