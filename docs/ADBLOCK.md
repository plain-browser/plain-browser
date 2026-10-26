# Adblock (adblock-rust) integration

Status 2026-10-23: builds on Chromium 156.0.8078.47 and blocks requests end to end (`scripts/test-adblock.ps1`).
Bug found by that test and fixed: with the filter pipe disconnected, deferred requests hung forever; the throttle now skips
disconnected filters and fails open after 3 s.

## Architecture
- `src/plainbrowser/adblock/rs` – Rust bridge (cxx) over `adblock` 0.13.3. Default lists
  (EasyList, EasyPrivacy, EasyList Polish, and our `plainbrowser-removeparam.txt` with `$removeparam` rules)
  are compiled in via `include_str!`. `cargo test` runs standalone (CI: `rust.yml`); GN ignores Cargo.toml/build.rs.
- `browser/` – `AdBlockService` (process-wide engine on its own sequence),
  `AdblockFilterImpl` (mojo receivers on that sequence), `ExposeInterfacesToRenderer`
  (registry hook, kill switch `--disable-features=PlainBrowserAdblock`),
  `TrackingParamsNavigationThrottle` (strips `utm_*`, `fbclid`, `gclid`, ... from frame navigations:
  defers the navigation, asks the engine for `rewritten_url`, and when it differs cancels the navigation and
  re-issues it with the clean URL, so the address bar and history never see the tracking parameters; kill switch
  `--disable-features=PlainBrowserTrackingParams`; POST navigations are left alone).
- `renderer/` – `AdblockURLLoaderThrottle` (defers each http(s) subresource, asks the
  browser, cancels with `ERR_BLOCKED_BY_CLIENT`), `AdblockThrottleFactory` (owned by
  chrome's `URLLoaderThrottleProviderImpl`, cloned per worker thread like SafeBrowsing),
  `CosmeticFilterAgent` (per frame, inserts hide-selector CSS as a user stylesheet).
- `common/adblock.mojom` – `AdblockFilter { Clone, ShouldBlockRequest, GetCosmeticCss }`.

Wiring patches: `patches/00-build/0001` (GN deps, shared with the other features) and `patches/30-adblock/0002..0005`,
about 35 added lines in Chromium files. `scripts/check-patches.py` dry-runs them against any upstream tag.

## Why GN and not a cargo staticlib
Chromium links its own Rust std (`build/rust/std`) and routes Rust allocation to
PartitionAlloc (`build/rust/allocator`); cxx's C++ side is compiled with Chromium's clang +
libc++. A cargo staticlib would bring a second std/allocator and an MSVC-STL cxx.

## Vendoring the crates (done, scripted)
1. `scripts/apply-plainbrowser.ps1` applies `patches/series`, including `30-adblock/0000-crate-config.patch`
   (adblock in `chromium_crates_io/Cargo.toml`, `gnrt_config.toml` entries, LICENSE patches for seahash and flatbuffers).
2. `scripts/vendor-adblock-crates.ps1` runs `gnrt vendor`, strips Cargo.lock checksums, runs `gnrt gen`, then
   `scripts/post-gnrt-fixups.ps1`.
Failures met on the way (all handled in those scripts or the patch): killed `gnrt vendor` leaves `.cargo/config.toml`
deleted; `gnrt vendor` deletes ucw's extracted `windows-rs-0.52.0` and `gnrt gen` overwrites ucw's patch to the
`windows_*_msvc` BUILD.gn files; nine crates need `allow_unsafe`; `thiserror` v1/v2 differ in build-script outputs;
flatbuffers' build.rs turns on nightly-only features; cxx `Result` needs C++ exceptions, so the bridge uses `bool`.
Security note: adblock parses untrusted filter lists in the browser process; moving it to a utility process is a later step.

## Not done yet (alpha scope)
- `$redirect` resources (treated as block), scriptlets, generic class/id hiding.
- Per-site toggle, toolbar counter, `chrome://adblock` page, list updates (lists are a
  build-time snapshot: refresh with the release).
- Navigation (`$document`) blocking; requests started from the browser process.
- `$removeparam` on subresources: the renderer throttle decides asynchronously and a deferred request's URL may
  only be edited synchronously (`URLLoaderThrottle::WillStartRequest`), so only navigations are cleaned today.
- Tracking-parameter stripping verified 2026-10-26 with `scripts/test-privacy.ps1` (headless, local server): the
  original URL never reaches the server, the re-issued navigation carries only the remaining parameters, and both
  kill switches leave the URL untouched.
