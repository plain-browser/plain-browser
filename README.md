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

**Pre-alpha. There is no installable build yet.**

| Part | State |
|---|---|
| Stock ungoogled-chromium 156 build on our Windows builder | Done (6.7 h); unsigned test build in a draft release |
| adblock-rust bridge crate (Rust + cxx, bundled lists) | Done, 4 unit tests pass |
| Browser/renderer adblock code (service, mojo filter, request throttle, cosmetic agent) | Compiles, links and **blocks**: headless test (`scripts/test-adblock.ps1`) blocks four ad hosts, passes controls, and with `--disable-features=PlainBrowserAdblock` lets all through without hanging |
| Vendoring the adblock crates into Chromium's GN build (gnrt) | Done, scripted (`scripts/vendor-adblock-crates.ps1`) |
| MV2 | ungoogled-chromium already keeps MV2 enabled in 156; one small cleanup patch written and compiled, **no extension tested yet** |
| Tracking-parameter stripping on navigations (`utm_*`, `fbclid`, `gclid`, ...) | Done: navigation throttle + `$removeparam` list, verified headless (`scripts/test-privacy.ps1`) |
| Fingerprint protection (canvas, audio, WebGL strings, navigator) with a compatibility list of excepted sites (for example online banking) | Done as a first slice (page-world script, no Blink patches), verified headless; see `docs/FINGERPRINT.md` |
| Safe Browsing through our proxy (`sb.plain-browser.app`, `X-Client: PlainBrowser/<version>`) | Compiled in (ungoogled's removal patches dropped via `patches/upstream-revert.list`), verified headless: the list update goes only to our proxy. Standard protection only; no reporting |
| Branding, installer, updater, extension repository | Not started |

Verified: same six requests with the filter on and off. On: the four EasyList hosts never reach the server, controls load.
Off (kill switch): all six load. Not yet tested: cosmetic hiding (only site-specific rules exist), UI, MV2 extensions, installer.

## Roadmap

Privacy features and the project-security plan are in [docs/ROADMAP.md](docs/ROADMAP.md); vulnerability reporting in [SECURITY.md](SECURITY.md).

1. Finish the stock build, then apply our patches and vendor the adblock crates.
2. Compile, fix and test the blocker on real pages; toolbar toggle and per-site settings.
3. Branding, installer and update server; curated MV2 extension repository (uBlock Origin and similar).
4. Code signing and a public beta.
