# Plain Browser

**A Chromium-based desktop browser for Windows 11 with no Google services, built-in ad blocking and Manifest V2 extension support.**

Plain Browser is a fork of [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium) that adds a native content blocker powered by Brave's [adblock-rust](https://github.com/brave/adblock-rust) engine and keeps Manifest V2 extensions working, the way Brave does, after Google removed them from Chrome.

## Goals

- **Free of Google.** No Google account, sync, Safe Browsing or telemetry calls: everything ungoogled-chromium strips stays stripped, and our own services (updates, extension repository) run on our own infrastructure.
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

**Alpha. Test builds exist (`0.1.0-alpha.1`, unsigned); the Microsoft Store listing is being prepared.**

| Part | State |
|---|---|
| Build of ungoogled-chromium 156 plus our layer on the Windows build VM | Done (about 7 h, `scripts/build.ps1`). GitHub Actions build in chained stages written (`build-windows.yml`, `docs/CI.md`), first run pending |
| adblock-rust bridge crate (Rust + cxx, bundled lists) | Done, unit tests pass (`rust.yml`) |
| Browser/renderer adblock code (service, mojo filter, request throttle, cosmetic agent) | **Blocks** end to end: `scripts/test-adblock.ps1` blocks four ad hosts, passes controls, kill switch `--disable-features=PlainBrowserAdblock` lets all through |
| Vendoring the adblock crates into Chromium's GN build (gnrt) | Done, scripted (`scripts/vendor-adblock-crates.ps1`) |
| Manifest V2 | Kept enabled (ungoogled patch) plus our cleanup of the deprecation UI; real-extension test: see `docs/MV2.md` |
| Tracking-parameter stripping on navigations (`utm_*`, `fbclid`, `gclid`, ...) | Done: navigation throttle + `$removeparam` list, verified headless (`scripts/test-privacy.ps1`) |
| Fingerprint protection (canvas, audio, WebGL strings, navigator) with a compatibility list of excepted sites (for example online banking) | Done as a first slice (page-world script, no Blink patches), verified headless; see `docs/FINGERPRINT.md` |
| Safe Browsing through our proxy (`sb.plain-browser.app`, `X-Client: PlainBrowser/<version>`) | Compiled in (ungoogled's removal patches dropped via `patches/upstream-revert.list`), verified headless: the list update goes only to our proxy. Standard protection only; no reporting |
| Branding (name, icons, install mode `%LOCALAPPDATA%\PlainBrowser`) | Done; portable zip, installer and MSIX built (`scripts/package-*.py`) |
| Distribution and updates | **Microsoft Store only (MSIX)**: the Store signs the package and delivers updates, so there is no own updater or Authenticode certificate. Listing text in `docs/STORE_LISTING.md` |
| Website (`website/`, plain-browser.app) | Static Next.js site with the privacy and security policies, EN and PL |
| Adblock UI: toolbar button and bubble with a per-site toggle shared by adblock, parameter stripping and fingerprint protection | Done, verified headless (`scripts/test-shields.ps1`; `docs/ADBLOCK.md`, "UI and per-site exceptions") |
| Generic cosmetic filtering, list updates | Not started |
| Curated MV2 extension repository | Not started (`docs/ARCHITECTURE.md`, section 8.2) |

Tests: `scripts/test-*.ps1` run headless on the build VM; C++ unit tests in `docs/TESTING.md`.
Not yet tested: generic cosmetic hiding, the UI in a normal window on real pages, Store installation on a clean machine.

## Roadmap

Privacy features and the project-security plan are in [docs/ROADMAP.md](docs/ROADMAP.md); vulnerability reporting in [SECURITY.md](SECURITY.md).

1. Shields follow-ups: settings page for the exception list, blocked-count badge on the button, translations.
2. Microsoft Store submission of the MSIX; first public alpha through the Store.
3. Generic cosmetic filtering and filter-list updates independent of browser releases.
4. Curated MV2 extension repository (uBlock Origin and similar); tracking-redirect bypass; Blink-level fingerprint noise.

Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Changes: [CHANGELOG.md](CHANGELOG.md). Manual GitHub and Store setup
steps: [docs/GITHUB_SETUP.md](docs/GITHUB_SETUP.md) (Polish).
