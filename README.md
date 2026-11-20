# Plain Browser

**A Chromium-based desktop browser for Windows 11 with no Google services, built-in ad blocking and Manifest V2 extension support.**

Plain Browser is a fork of [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium) that adds a native content blocker powered by Brave's [adblock-rust](https://github.com/brave/adblock-rust) engine and keeps Manifest V2 extensions working, the way Brave does, after Google removed them from Chrome.

## Goals

- **Free of Google.** No Google account, sync or telemetry calls, and the browser never contacts Google directly: everything ungoogled-chromium strips stays stripped. The one Google data source, Safe Browsing's public lists, is fetched through our own proxy (`infra/safebrowsing-proxy`) that removes everything that identifies you, and can be turned off; the other services (filter lists, extension repository) run on our own infrastructure.
- **Ad and tracker blocking built in.** adblock-rust runs inside the browser's network stack instead of in an extension, so blocking works without webRequest APIs and does not depend on the extension platform. Default lists: EasyList, EasyPrivacy and EasyList Polish.
- **Manifest V2 stays alive.** Chrome 139 and later disabled MV2 and the Web Store dropped it. Plain Browser keeps MV2 extensions and blocking `webRequest` working so tools like uBlock Origin keep their full power.
- **Small, rebase-friendly patch set.** Our code lives in one directory (`src/plainbrowser`) and a short list of patches on top of ungoogled-chromium, so we can follow every Chromium release.

## How it is built

```
Chromium (pinned tag)
  └─ ungoogled-chromium
       └─ ungoogled-chromium-windows   (build scripts for Windows)
            └─ Plain Browser           (this repository: patches + src/plainbrowser)
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and [docs/ADBLOCK.md](docs/ADBLOCK.md) for the blocker integration.

## Licenses

- Original code (`src/plainbrowser`, scripts, docs): **MPL-2.0**, see [LICENSE](LICENSE).
- Patches to Chromium and ungoogled-chromium (`patches/`): **BSD-3-Clause**, see [patches/LICENSE](patches/LICENSE).
- Upstream components keep their own licenses: Chromium and ungoogled-chromium (BSD-3-Clause), adblock-rust (MPL-2.0), bundled filter lists (GPLv3 or CC BY-SA 3.0, with attribution).

Details and third-party notices: [NOTICE.md](NOTICE.md). The name "Plain Browser" and any logo are not covered by these licenses.

## Status

**Beta. Builds are in the Microsoft Store; the installer and zip from the releases are unsigned test builds (`0.2.5-beta.2`).**

| Part | State |
|---|---|
| Build of ungoogled-chromium 157 (Chromium 157.0.8103.78) plus our layer | Done. Releases are built by the GitHub Actions pipeline in chained stages (`build-windows.yml`, `docs/CI.md`) and submitted to the Store; `scripts/build.ps1` builds the same on a Windows VM (about 7 h) |
| adblock-rust bridge crate (Rust + cxx, bundled lists) | Done, unit tests pass (`rust.yml`) |
| Browser/renderer adblock code (service, mojo filter, request throttle with redirect check, subframe navigation throttle, cosmetic agent) | **Blocks** end to end: `scripts/test-adblock.ps1` blocks four ad hosts, passes controls, kill switch `--disable-features=PlainBrowserAdblock` lets all through |
| Vendoring the adblock crates into Chromium's GN build (gnrt) | Done, scripted (`scripts/vendor-adblock-crates.ps1`) |
| Manifest V2 | Kept enabled (ungoogled patch) plus our cleanup of the deprecation UI; real-extension test: see `docs/MV2.md` |
| Tracking-parameter stripping on navigations (`utm_*`, `fbclid`, `gclid`, ...) | Done: navigation throttle + `$removeparam` list, verified headless (`scripts/test-privacy.ps1`) |
| Skipping tracking redirectors (`l.facebook.com/l.php?u=`, `google.com/url?q=`, Outlook Safe Links, ...) | Done: own rule list in the same throttle, verified headless (`scripts/test-privacy.ps1`) |
| Fingerprint protection (canvas, audio, WebGL strings, navigator) with a compatibility list of excepted sites (for example online banking) | Done as a first slice (page-world script, no Blink patches), verified headless; see `docs/FINGERPRINT.md` |
| Secure defaults: HTTPS-First Mode (balanced) on, secure DNS in automatic mode (DoH only with the resolver already in use) | HTTPS-First balanced is Chromium 157's own default (our patch was removed in 0.2.0); secure DNS automatic mode comes from reverting two ungoogled hunks (`patches/upstream-revert.list`). No test of its own for either (`docs/ROADMAP.md`) |
| Safe Browsing through our proxy (`sb.plain-browser.app`, `X-Client: PlainBrowser/<version>`) | Compiled in (ungoogled's removal patches dropped via `patches/upstream-revert.list`), verified headless: the list update goes only to our proxy. Standard protection only (the Enhanced option is hidden in Settings); reporting stays off through ungoogled's patches. The headless test only checks the list update |
| Branding (name, icons, install mode `%LOCALAPPDATA%\PlainBrowser`) | Done; portable zip, installer and MSIX built (`scripts/package-*.py`) |
| Distribution and updates | **Microsoft Store only (MSIX)**: the Store signs the package and delivers updates, so there is no own updater or Authenticode certificate. Listing text in `docs/STORE_LISTING.md` |
| Website (`website/`, plain-browser.app) | Static Next.js site with the privacy and security policies, EN and PL |
| Adblock UI: toolbar button with a blocked-count badge, bubble with a per-site toggle shared by adblock, parameter stripping and fingerprint protection, the list of excepted sites in Settings > Privacy and security, a global "Block ads and trackers" switch with the date of the last list check, and the `chrome://adblock` status page; English and Polish | Done (`scripts/test-shields.ps1`, `test-shields-ui.ps1`, `test-shields-settings.ps1`, `test-adblock-webui.ps1`; `docs/ADBLOCK.md`, "UI and per-site exceptions") |
| Generic cosmetic filtering (class/id rules for the names a page has) | Done (`scripts/test-cosmetic-lists.ps1`; `docs/ADBLOCK.md`, "Generic element hiding") |
| Filter list updates between releases: daily from our mirror `lists.plain-browser.app` (`infra/filter-lists`), Ed25519-signed manifest, hashes per list, switch in Settings | Done: browser side tested against a local server, Worker deployed (manifest with `sequence` and `expires`, six lists; `infra/filter-lists/scripts/verify-live.mjs` checks the live one; `infra/filter-lists/README.md`) |
| `$redirect` stand-ins, scriptlets, procedural filters (own resources, MPL-2.0) | Done (`scripts/test-cosmetic-lists.ps1`; `docs/ADBLOCK.md`) |
| Optional filter lists (EasyList Cookie List on by default, Fanboy's Annoyance List, uBlock filters), own filters and a request log on `chrome://adblock` | Done, shipped since 0.2.2-beta.2; unit and Rust tests, no end-to-end test yet (`docs/ADBLOCK.md`, "Optional lists, custom filters and the request log") |
| Curated extension repository: `chrome://recommended-extensions` (uBlock Origin, AdGuard, NoScript from `ext.plain-browser.app`, switch from a Chrome Web Store copy with its settings) | Done, shipped since 0.2.2-beta.2; repository live at `ext.plain-browser.app` since 2026-11-10, `infra/extensions` tested; no test of the browser side (`docs/MV2.md`) |
| Global Privacy Control (`Sec-GPC: 1`, `navigator.globalPrivacyControl`) on by default, toggle in Settings | Done, shipped since 0.2.2-beta.2 (`patches/90-defaults/0001`); no test checks the header yet |
| Default browser from the Microsoft Store package: status in Settings and "Make default" opening the package's Default apps page | Done, shipped since 0.2.2-beta.2 (`patches/60-ui/0013`, `src/plainbrowser/win`); no test yet |

Tests: `scripts/test-*.ps1` run headless on the build VM; C++ unit tests in `docs/TESTING.md`.
Store installation and the beta's features were checked by hand by the maintainer on a clean machine (2026-11-19). Not yet covered by automated tests: the UI on real pages beyond the test pages, and optional lists, GPC, the extension catalogue and "Make default" (no end-to-end test).

## Roadmap

Privacy features and the project-security plan are in [docs/ROADMAP.md](docs/ROADMAP.md); vulnerability reporting in [SECURITY.md](SECURITY.md).

1. End-to-end tests for the features listed above.
2. Ad-block request log and exceptions per profile; Blink-level fingerprint noise.

Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Changes: [CHANGELOG.md](CHANGELOG.md). Manual GitHub and Store setup
steps: [docs/GITHUB_SETUP.md](docs/GITHUB_SETUP.md) (Polish).
