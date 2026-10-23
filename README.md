# Plain Browser

**A Chromium-based desktop browser for Windows 11 with no Google services, built-in ad blocking and Manifest V2 extension support.**

Plain Browser is a fork of [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium) that adds a native content blocker powered by Brave's [adblock-rust](https://github.com/brave/adblock-rust) engine and keeps Manifest V2 extensions working, the way Brave does, after Google removed them from Chrome.

## Goals

- **Free of Google.** No Google account, sync, Safe Browsing or telemetry calls: everything ungoogled-chromium strips stays stripped, and our own services (updates, extension repository) run on our own infrastructure.
- **Ad and tracker blocking built in.** adblock-rust runs inside the browser's network stack instead of in an extension, so blocking works without webRequest APIs and does not depend on the extension platform. Default lists: EasyList, EasyPrivacy and EasyList Polish.
- **Manifest V2 stays alive.** Chrome 139 and later disabled MV2 and the Web Store dropped it. Plain Browser keeps MV2 extensions and blocking `webRequest` working so tools like uBlock Origin keep their full power.
- **Small, rebase-friendly patch set.** Our code lives in one directory (`src/draftway`, internal codename) and a short list of patches on top of ungoogled-chromium, so we can follow every Chromium release.

## How it is built

```
Chromium (pinned tag)
  └─ ungoogled-chromium
       └─ ungoogled-chromium-windows   (build scripts for Windows)
            └─ Plain Browser           (this repository: patches + src/draftway)
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and [docs/ADBLOCK.md](docs/ADBLOCK.md) for the blocker integration.

## Licenses

Chromium and ungoogled-chromium are BSD-3-Clause; adblock-rust is MPL-2.0; bundled filter lists are GPLv3 or CC BY-SA 3.0 (see `src/draftway/adblock/lists/NOTICE.md`). The license of this repository's own code is not decided yet.

## Status

**Pre-alpha. There is no installable build yet.**

| Part | State |
|---|---|
| Stock ungoogled-chromium 156 build on our Windows builder | In progress (compiling) |
| adblock-rust bridge crate (Rust + cxx, bundled lists) | Done, 4 unit tests pass |
| Browser/renderer adblock code (service, mojo filter, request throttle, cosmetic filtering) | Written, patches dry-applied to Chromium 156 sources, **not compiled yet** |
| Vendoring the adblock crates into Chromium's GN build (gnrt) | Not done |
| MV2 | ungoogled-chromium already keeps MV2 enabled in 156; one small cleanup patch written, **untested** |
| Branding, installer, updater, extension repository | Not started |

## Roadmap

1. Finish the stock build, then apply our patches and vendor the adblock crates.
2. Compile, fix and test the blocker on real pages; toolbar toggle and per-site settings.
3. Branding, installer and update server; curated MV2 extension repository (uBlock Origin and similar).
4. Code signing and a public beta.
