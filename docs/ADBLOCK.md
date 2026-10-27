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
- `common/adblock.mojom` – `AdblockFilter { Clone, ShouldBlockRequest, GetCosmeticCss }`; both queries carry the
  page's top-level origin (per-site exceptions) and `ShouldBlockRequest` the requesting frame's token (per-tab
  blocked-request counter).
- `//plainbrowser/shields` – per-site exception list, toolbar button and bubble; see "UI and per-site exceptions".

Wiring patches: `patches/00-build/0001` (GN deps, shared with the other features), `patches/30-adblock/0002..0005`
and, for the UI and the exception list, `patches/60-ui/0005..0007`; about 60 added lines in Chromium files. `scripts/check-patches.py` dry-runs them against any upstream tag.

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

## UI and per-site exceptions (//plainbrowser/shields)
Status 2026-10-27: first UI slice, built against Chromium 156.0.8078.47 and verified headless
(`scripts/test-shields.ps1`) and in a normal window (`scripts/test-shields-ui.ps1`: UI Automation drives the
button and the bubble, checks the pushed exception list in the running renderer and takes screenshots; see
[TESTING.md](TESTING.md)).

- **One exception list for all protections.** Profile pref `plainbrowser.shields.exceptions`: a list of sites
  (eTLD+1, lower case, e.g. `example.com`). A site on the list gets no ad blocking (network and cosmetic), no
  tracking-parameter stripping and no fingerprint noise. Registered in `chrome::RegisterProfilePrefs`
  (`patches/60-ui/0005`), helpers in `shields/browser/shields_prefs.h`. The site of a request is the eTLD+1 of the
  page's top-level frame (`shields/common/site_util.h`), so a third-party frame on an excepted page is excepted too,
  and an excepted site embedded as a frame elsewhere is not.
- **Toolbar button** (`shields/ui/shields_button.*`, `patches/60-ui/0006`): a `ToolbarButton` right of the omnibox
  with the `shield` vector icon (shield with "!" when the site is excepted; no own icon file yet). Tooltip and icon
  follow the active tab. Click opens **the bubble** (`shields/ui/shields_bubble_view.*`, a
  `views::BubbleDialogDelegate` with its own contents view; `BubbleDialogDelegateView` is deprecated in 156 and its
  constructor private): the site, a toggle "Protections on for this site" and a status line with the
  number of requests blocked on the page (or that protections are off). Flipping the toggle edits the pref and
  reloads the tab. Strings are English literals for now (no .grd entries, so nothing for ungoogled's string tooling
  to carry).
- **How the three features read the list.**
  - Browser-side adblock filter: `AdblockFilter.ShouldBlockRequest` and `GetCosmeticCss` now carry the page's
    top-level origin (the renderer throttle reads it from the frame token, `patches/30-adblock/0003`) and the filter
    answers "allow" / empty CSS when `ShieldsService::IsExcepted(site)` says so. `ShieldsService`
    (`shields/browser/shields_service.*`) keeps a lock-protected, process-wide union of the lists of all loaded
    profiles (one profile in practice), refreshed by a `PrefChangeRegistrar` attached to each `BrowserContext` as
    user data (no KeyedService factory, no extra Chromium hook).
  - `TrackingParamsNavigationThrottle`: `PROCEED` when the destination site (main frame) or the embedding page
    (subframe) is excepted.
  - Fingerprint protection: the renderer decides synchronously when a script context is created, so the browser
    pushes the list into every renderer over `shields.mojom.ShieldsConfig` (bound at process creation in
    `ExposeInterfacesToRenderer`, `patches/30-adblock/0002`; renderer side registered in
    `ChromeContentRendererClient::ExposeInterfacesToBrowser`, `patches/60-ui/0007`) and re-sends it on every change.
    New renderers get the list at creation (`--plainbrowser-shields-exceptions`) as the initial value the first
    push replaces. Details in
    [FINGERPRINT.md](FINGERPRINT.md).
- **Blocked-request counter** (`shields/browser/blocked_requests_counter.*`): the renderer throttle sends its
  `LocalFrameToken`; on a block the filter posts one task to the UI thread, which resolves (process id, token) to the
  `WebContents` and increments a `WebContentsUserData` counter, reset on `PrimaryPageChanged`. The bubble shows the
  value when it opens (no live badge on the button yet).
- Test: `scripts/test-shields.ps1` (headless): a seeded profile with `["example.com"]`; the page served as
  example.com loads the four ad hosts, keeps `utm_source` and sees the real `hardwareConcurrency`; the same page as
  example.org has them blocked, the parameter stripped and the core count bucketed.
- Not done: a settings page (`chrome://settings` or `chrome://adblock`) listing the exceptions, a badge with the
  count on the button, translations, a global on/off switch in the UI (the kill switches are still command-line flags).

## Not done yet (alpha scope)
- `$redirect` resources (treated as block), scriptlets, generic class/id hiding.
- Settings page for the exception list, count badge on the toolbar button, `chrome://adblock` page, list updates
  (lists are a build-time snapshot: refresh with the release). The per-site toggle itself is done (above).
- Navigation (`$document`) blocking; requests started from the browser process.
- `$removeparam` on subresources: the renderer throttle decides asynchronously and a deferred request's URL may
  only be edited synchronously (`URLLoaderThrottle::WillStartRequest`), so only navigations are cleaned today.
- Tracking-parameter stripping verified 2026-10-26 with `scripts/test-privacy.ps1` (headless, local server): the
  original URL never reaches the server, the re-issued navigation carries only the remaining parameters, and both
  kill switches leave the URL untouched.
