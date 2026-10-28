# Adblock (adblock-rust) integration

Status 2026-10-23: builds on Chromium 156.0.8078.47 and blocks requests end to end (`scripts/test-adblock.ps1`).
Bug found by that test and fixed: with the filter pipe disconnected, deferred requests hung forever; the throttle now skips
disconnected filters and fails open after 3 s.

## Architecture
- `src/plainbrowser/adblock/rs` – Rust bridge (cxx) over `adblock` 0.13.3. Default lists
  (EasyList, EasyPrivacy, EasyList Polish, and our `plainbrowser-removeparam.txt` with `$removeparam` rules)
  are compiled in via `include_str!`. `cargo test` runs standalone (CI: `rust.yml`); GN ignores Cargo.toml/build.rs.
- `engine/` – the engine in its own **utility process** (since 2026-10-28): `AdBlockEngine` owns the adblock-rust
  engine on its own sequence, `AdBlockEngineService` serves `common/adblock_engine.mojom` (`AdblockEngine`,
  `[ServiceSandbox=kService]`, the most restrictive utility sandbox, like Chromium's Storage Service; Task Manager:
  "Plain Browser ad blocker"). Registered in `chrome/utility/services.cc` (`patches/30-adblock/0008`). Filter lists,
  bundled or downloaded, are parsed only there.
- `browser/` – `AdBlockService` (the browser's handle on that process: starts it with
  `content::ServiceProcessHost` on first use, `engine_launcher.cc`; every query is async and fails open, i.e.
  "nothing to block", until the engine is built or when the process goes away; a process that went away is started
  again on the next query, with the downloaded lists dropped since they may have crashed it, at most
  `kMaxEngineStarts` = 4 starts per browser session),
  `AdblockFilterImpl` (mojo receivers for renderers on the service's sequence), `ExposeInterfacesToRenderer`
  (registry hook, kill switch `--disable-features=PlainBrowserAdblock`),
  `TrackingParamsNavigationThrottle` (strips `utm_*`, `fbclid`, `gclid`, ... from frame navigations:
  defers the navigation, asks the engine for `rewritten_url`, and when it differs cancels the navigation and
  re-issues it with the clean URL, so the address bar and history never see the tracking parameters; kill switch
  `--disable-features=PlainBrowserTrackingParams`; POST navigations are left alone).
- `renderer/` – `AdblockURLLoaderThrottle` (defers each http(s) subresource, asks the
  browser, cancels with `ERR_BLOCKED_BY_CLIENT`), `AdblockThrottleFactory` (owned by
  chrome's `URLLoaderThrottleProviderImpl`, cloned per worker thread like SafeBrowsing),
  `CosmeticFilterAgent` (per frame, inserts hide-selector CSS as a user stylesheet, and generic class/id rules
  for the names found in the document; see "Generic element hiding").
- `common/adblock.mojom` – `AdblockFilter { Clone, ShouldBlockRequest, GetCosmeticCss, GetGenericCosmeticCss }`;
  all queries carry the page's top-level origin (per-site exceptions) and `ShouldBlockRequest` the requesting
  frame's token (per-tab blocked-request counter).
- `browser/filter_list_updater.*`, `browser/filter_lists.*` – daily list updates from `lists.plain-browser.app`
  (`infra/filter-lists`); see "Filter list updates".
- `//plainbrowser/shields` – per-site exception list, toolbar button and bubble; see "UI and per-site exceptions".

Wiring patches: `patches/00-build/0001` (GN deps, shared with the other features), `patches/30-adblock/0002..0008`
and, for the UI and the exception list, `patches/60-ui/0005..0009`; about 60 added lines in Chromium files. `scripts/check-patches.py` dry-runs them against any upstream tag.

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
Security note: the filter lists (downloaded ones come from the network) are parsed in the adblock utility process
(`engine/`, `kService` sandbox), not in the browser process; a crash there costs ad blocking until the next query
restarts it, nothing else.

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
  with the `shield` vector icon (shield with "!" when the site is excepted; no own icon file yet). Tooltip, icon
  and **badge** follow the active tab: the number of requests blocked on the page is drawn in the icon's
  bottom-right corner (`ui::kColorBadgeBackground`/`Foreground`, "99+" above 99, nothing at 0 or on an excepted
  site) and kept current while the page loads; the same count is the button's accessible description. Click opens
  **the bubble** (`shields/ui/shields_bubble_view.*`, a `views::BubbleDialogDelegate` with its own contents
  view; `BubbleDialogDelegateView` is deprecated in 156 and its constructor private): the site, a toggle
  "Protections on for this site", a status line with the live count (or that protections are off) and a
  "Manage sites" link to the settings section below. Flipping the toggle edits the pref and reloads the tab.
- **Settings section** (`patches/60-ui/0009`): "Sites with protections off" on `chrome://settings/privacy`,
  under Site settings. A new Polymer element (`privacy_page/plainbrowser_shields_exceptions.*`, added by the
  patch) lists the pref with a remove button per site and an Add dialog; it edits the pref through
  `settingsPrivate` (the pref is in `prefs_util.cc`'s allowlist as a list). The dialog accepts a site, a host or
  a URL and stores the host; `ShieldsService` rewrites every entry as its eTLD+1 (`NormalizeExceptions`, posted
  after each change), and all readers go through `SiteForEntry()`, so `https://www.example.com/a` and
  `example.com` mean the same thing.
- **Strings and translations**: every string is in `shields/resources/plainbrowser_shields_strings.grdp`, a part
  of `chrome/app/generated_resources.grd` (`patches/60-ui/0008`), with Polish in
  `plainbrowser_shields_strings_pl.xtb`; see `shields/resources/README.md` for ids and adding a language. The
  bubble title "Plain Browser" stays a literal (product name).
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
  `WebContents` and increments a `WebContentsUserData` counter, reset on `PrimaryPageChanged`. Every change
  runs the counter's callbacks (`AddCountChangedCallback`), which the button and an open bubble subscribe to.
- Test: `scripts/test-shields.ps1` (headless): a seeded profile with `["example.com"]`; the page served as
  example.com loads the four ad hosts, keeps `utm_source` and sees the real `hardwareConcurrency`; the same page as
  example.org has them blocked, the parameter stripped and the core count bucketed.
- Badge, "Manage sites" link and the settings section (rendering) verified 2026-10-27 in a normal window
  (`scripts/test-shields-ui.ps1`: badge pixels with 4 blocked requests, none on an excepted site); the Polish
  strings (including the "few" plural), add (a URL is stored as its site), remove and the pref on disk by
  `scripts/test-shields-settings.ps1` (scheduled task `PBShieldsSettings` on the build VM), also passing.
- Not done: a global on/off switch in the UI (the kill switches are still command-line flags), languages other
  than English and Polish.

## Generic element hiding
Status 2026-10-27: done, verified headless (`scripts/test-cosmetic-lists.ps1`, part 1).

adblock-rust keeps the simple generic rules (`##.ad-box`, `###top-ad`, and complex ones keyed by a class or id like
`##.promo > .sponsored`) out of the per-site answer: they apply only to class names and ids the page actually has,
so the page must be asked (`Engine::hidden_class_id_selectors`). EasyList has about 13,000 of them.

- `GetCosmeticCss` answers with the per-site stylesheet and `generic_hide`: true under a `$generichide` exception,
  on an excepted site, or before the engine is ready. Then the renderer asks for nothing more.
- Otherwise `CosmeticFilterAgent` collects `[class],[id]` from the document at DOMContentLoaded and at load;
  after that the isolated-world script's MutationObserver (see "Procedural filters") reports every class name or
  id that appears, at any time, through `__plainbrowserCosmeticReport`, a function the agent installs in that
  world. Only names not sent before for the document go out (at most 2000 per call, 50,000 per document; the
  browser refuses calls with more than 10,000). `GetGenericCosmeticCss` returns `display: none !important` for the
  matching selectors minus the document's `#@#` exceptions; the agent inserts it as another user stylesheet. A new
  document resets everything (late replies are dropped).
- Every selector gets its own CSS rule. Blink drops a whole rule when one selector of its list is invalid, and the
  lists contain selectors Blink does not know; with one rule for a whole site, a single such selector used to switch
  off all hiding on that site.

## $redirect resources and scriptlets
Status 2026-10-27: done with our own resources (MPL-2.0, `src/plainbrowser/adblock/resources`), verified headless
(`scripts/test-cosmetic-lists.ps1`, part 1) and with Node tests (`resources/scriptlets_test.js`).

- **Resources** (`rs/src/resources.rs`): every engine gets `Engine::use_resources` with our files under the names
  and aliases the lists use. Redirects: `noop.js`/`noopjs`, `noop.txt`/`nooptext`, `noop.json`/`noopjson`,
  `noop.html`/`noopframe`, `noop.css`, `1x1.gif`, `2x2.png`, and stand-ins that keep pages working without
  loading anything: `googletagmanager_gtm.js` (runs `eventCallback`s), `google-analytics_analytics.js` (`ga()`
  runs `hitCallback`), `googlesyndication_adsbygoogle.js`, `chartbeat.js`, `fingerprint2.js`. Scriptlets
  (function style: adblock-rust calls the function a file starts with): `set-constant` (`set`),
  `abort-on-property-read`/`-write` (`aopr`/`aopw`), `abort-current-script` (`acs`, `acis`),
  `abort-on-stack-trace` (`aost`), `remove-node-text` (`rmnt`), `no-fetch-if`, `no-xhr-if`,
  `addEventListener-defuser` (`aeld`), `no-setTimeout-if` (`nostif`), `no-setInterval-if` (`nosiif`),
  `set-local-storage-item`, `set-session-storage-item`, `set-cookie`, `cookie-remover`, `json-prune`,
  `remove-attr` (`ra`), `remove-class` (`rc`), `window.open-defuser` (`nowoif`); shared helpers in `*.fn.js`. A
  rule naming a scriptlet we do not have (EasyPrivacy's one `json-edit`) is skipped.
- **Scriptlets in the page**: when the main world's script context is created, `CosmeticFilterAgent` makes a sync
  `GetScriptlets` call and runs the answer at once, wrapped in a function so our helpers are not page globals.
  Sync on purpose: they must run before the page's first script (Brave does the same).
- **Redirects**: the URL loader throttle decides asynchronously and can no longer change a URL, so redirects use
  `ChromeContentRendererClient::WillSendRequest` (`patches/30-adblock/0007`, `renderer/request_redirector.*`), the
  last renderer hook that may replace a subresource URL. The renderer fetches the hosts that `$redirect` rules name
  once per process (sync `GetRedirectHosts`, refreshed after a minute, so list updates arrive); for a request to one of them it asks
  `GetRedirect` (sync) and replaces the URL with the `data:` URL of our resource, which Blink loads itself. The
  replacement counts as a blocked request. Everything else goes on to the throttle as before.
- Limit: that hook does not know the request's destination, so the engine is asked with a type guessed from the
  file extension (`.js` script, images, `.css`, `.json`, `.html`, media; no extension: script, then image, then
  xmlhttprequest). A `$redirect` rule restricted to another type than the guess falls back to a plain block.

## Procedural filters
Status 2026-10-27: done, verified headless (`scripts/test-cosmetic-lists.ps1`, part 1).

adblock-rust is built without its `css-validation` feature (it would need the `selectors`/`cssparser` crates
vendored), so it hands procedural rules over as plain selectors. `cosmetic_resources` splits them out:
selectors with a non-CSS operator go to `procedural_selectors`, `:-abp-has()` becomes CSS `:has()` and
`:-abp-contains()` `:has-text()`; action rules (`:remove()`, `:style()`, `:remove-attr()`, `:remove-class()`)
come as adblock-rust's JSON (`procedural_actions`). The agent injects `renderer/cosmetic_script.js` into an
isolated world (`kCosmeticWorldId`) with both lists; the script parses the operators (`:has-text`, `:upward`,
`:xpath`, `:matches-css(-before/-after)`, `:min-text-length`, `:matches-attr`, `:matches-path`, and `:has`/`:not`
around them), applies the actions (no action: `display: none !important` inline) at once, at DOMContentLoaded, at
load and after DOM changes (batched), and runs the class/id observer described above. EasyList has about 290 such
`#?#` rules, EasyPrivacy a few `:style()` rules.

## Filter list updates
Status 2026-10-27: browser side and Worker done, verified with a local server (`scripts/test-cosmetic-lists.ps1`,
parts 2-4) and the Worker's unit tests; `lists.plain-browser.app` is deployed separately (`infra/filter-lists`).

- **Server**: `infra/filter-lists`, a Cloudflare Worker. A cron fetches EasyList, EasyPrivacy and EasyList Polish
  from their publishers every 6 hours, publishes a list only when it passes sanity checks (size, `[Adblock Plus]`
  header, at least 1000 rules, not shrunk by more than half) and serves `/v1/manifest.json` (SHA-256 and size per
  list) and `/v1/lists/<name>.txt`. No logs. Browsers never contact the publishers.
- **Browser**: `FilterListUpdater` (UI thread), started from `ChromeBrowserMainParts::PostBrowserStart`
  (`patches/30-adblock/0006`). At start-up it loads the lists stored in `<user data>\PlainBrowser Filter Lists`
  (each checked against the stored manifest) into a new engine. Two minutes after start-up and then every 24 h
  (Local State `plainbrowser.adblock.list_updates.last_check`, so restarts do not check more often; 1 h after a
  failure) it fetches the manifest and `manifest.json.sig`, checks the Ed25519 signature over the manifest's exact
  bytes against `filter_lists::kListSigningKeys` (BoringSSL `ED25519_verify`; two slots for key rotation; while
  both are empty every manifest is refused), downloads the lists whose hash changed, verifies size, SHA-256 and
  the header, writes them atomically and loads the store again. Requests: no cookies, cache bypassed,
  `X-Client: PlainBrowser/<major.minor.patch>`, a network traffic annotation `plainbrowser_filter_lists`.
- **Engine swap**: `AdBlockService::UseLists()` sends the lists to the engine process, where
  `AdBlockEngine::UseLists()` builds the new engine on the thread pool (about a second for
  EasyList) while the old one keeps answering, then swaps it in on the engine sequence; a newer call wins over an
  older build still running. Lists that were not downloaded stay the compiled-in ones, and
  `plainbrowser-removeparam.txt` is never downloaded. The compiled-in lists remain the fallback for a fresh
  profile, a failed download or a damaged store.
- **Switch**: Local State `plainbrowser.adblock.list_updates.enabled` (default on), "Update filter lists daily"
  in the "Sites with protections off" section of `chrome://settings/privacy` (`patches/60-ui/0009`). Off: no
  requests, the store is deleted, the engine goes back to the compiled-in lists.
- **Testing**: `--plainbrowser-filter-lists-url=<base>` points the updater at another server (http allowed) and
  makes the first check run 3 s after start-up; `--plainbrowser-filter-lists-key=<base64>` adds one accepted key,
  only together with the URL switch.
- Not done: a "last updated" line in Settings, delta updates.

## Debouncing (tracking redirectors)
A main-frame link through a known tracking redirector goes straight to its target: `l.facebook.com/l.php?u=<target>`,
`google.<tld>/url?q=<target>`, `www.youtube.com/redirect?q=`, Outlook Safe Links, Bing `ck/a` (base64), LinkedIn,
Reddit, VK, Steam, Slack and a few affiliate networks. The redirector is never contacted, so it does not learn about
the click.
- Rules: `browser/debounce_rules.cc`, our own list (MPL-2.0; not copied from Brave's). A rule is a host (subdomains
  match), a path (prefix when it ends in `/`) and the query parameters holding the target, optionally base64url after
  a fixed prefix. Only redirectors whose target is in the URL qualify; `t.co` or `bit.ly`, which decide server side,
  cannot be skipped. The list ships with the browser (no list updates).
- `TrackingParamsNavigationThrottle` applies it before the `$removeparam` check: at the start of a main-frame GET
  navigation or at a redirect into a redirector, it cancels the navigation and re-issues it with the target, which
  then goes through the throttle again (nested redirectors unwrap one step at a time; each step is shorter, so it
  ends). Only valid `http(s)` targets are used.
- Not applied when the redirector's site or the target's site has protections off (per-site toggle), in subframes,
  or for POST. Kill switch `--disable-features=PlainBrowserDebounce`.
- Tests: `debounce_rules_unittest.cc` (plainbrowser_unittests), `scripts/test-privacy.ps1` cases `debounce` and
  `debounce-off` (headless: the local server never sees `/l.php` unless the kill switch is on). Verified 2026-10-28.

## Not done yet (alpha scope)
- Scriptlets beyond our set (uBO's resources are GPLv3; we write our own), `$redirect` for request types the
  file extension does not reveal, procedural operators `:others()`, `:watch-attr()`, `:matches-prop()`.
- `chrome://adblock` page.
- Navigation (`$document`) blocking; requests started from the browser process.
- `$removeparam` on subresources: the renderer throttle decides asynchronously and a deferred request's URL may
  only be edited synchronously (`URLLoaderThrottle::WillStartRequest`), so only navigations are cleaned today.
- Tracking-parameter stripping verified 2026-10-26 with `scripts/test-privacy.ps1` (headless, local server): the
  original URL never reaches the server, the re-issued navigation carries only the remaining parameters, and both
  kill switches leave the URL untouched.
