# Changelog

All notable changes to Plain Browser. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow the product version in `versions.toml`; the Chromium base is given in brackets.

## [Unreleased]

Written 2026-11-09 and checked with `scripts/check-patches.py` (all patches apply to 157.0.8103.38-1.1), the Rust
tests and the Node tests; not yet compiled or run in a browser.

### Fixed
- Ad blocking: a document loaded in an `<iframe>`/`<frame>` is now checked against the network rules (it is a
  navigation the browser process loads, which the renderer-side filter never saw), and every redirect target of a
  subresource is checked again (a request to an allowed URL that redirected to a blocked one went through).
  Written, not yet compiled or run in a browser.

### Added
- Global Privacy Control on by default: the `Sec-GPC: 1` header and `navigator.globalPrivacyControl`, with
  Chromium's own toggle under Settings > Privacy and security > Third-party cookies and site data
  (`90-defaults/0001`: features `UniversalOptOut`, `UniversalOptOutSettings`, Blink `GlobalPrivacyControlApi`, the
  pref `universal_optout.enabled` and forced eligibility so the toggle stays visible).
- Optional filter lists under "Block ads and trackers": EasyList Cookie List (cookie consent banners, on by
  default), Fanboy's Annoyance List and uBlock filters (off). Bundled, updated from the mirror only while on; the
  mirror serves them too (`infra/filter-lists`). uBlock filters (GPLv3) are listed in chrome://credits.
- chrome://adblock: "My filters" (own rules in Adblock Plus / uBlock Origin syntax, Local State
  `plainbrowser.adblock.custom_filters`) and a request log (blocked and replaced requests, removed tracking
  parameters, skipped redirectors), recorded only while the page is open and kept in memory only.
- chrome://recommended-extensions (`//plainbrowser/extensions`, registered by `20-hooks/0003`): uBlock Origin, AdGuard and NoScript
  from `ext.plain-browser.app` (new `infra/extensions`: CRX3 repacking with an `update_url`, one key
  per extension, Worker on R2, weekly workflow), installed with the normal permission dialog and the ID pinned,
  updated by Chromium's extension updater; a Chrome Web Store copy can be switched to ours with its settings; an
  offer to turn the built-in blocker off while uBlock Origin or AdGuard runs. Linked from Settings.
- Microsoft Store package: Settings shows whether Plain Browser is the default browser, and "Make default" opens
  its page in Windows' Default apps (`60-ui/0013`, `//plainbrowser/win`). Before, the package was always reported
  "not default" and the button opened a page for an unknown app.

### Added (branch `feat/websocket-and-worker-fingerprint`, built and tested on the build VM 2026-11-10)
- WebSockets go through the ad blocker: `$websocket` rules apply, decided in the browser process before a socket is opened, so a
  blocked tracker never receives the handshake (`ContentBrowserClient::WillInterceptWebSocket`/`CreateWebSocket`, chained with
  the extensions' webRequest proxy; the engine accepts ws/wss URLs). Shared and service workers are not covered.
- Fingerprint protection in dedicated, shared and service workers (`OffscreenCanvas`, `WorkerNavigator`, WebGL strings),
  seeded like the page of the same site.

### Changed
- Patches: every hook into a file all features share (`ChromeContentRendererClient`, `ChromeContentBrowserClient`,
  `ChromeBrowserMainParts`, `browser_prefs.cc`, `chrome_web_ui_configs.cc`) is now one call into the new
  `//plainbrowser/hooks`. `patches/20-hooks` (3 patches, about 40 lines) replaces 12 patches (`30-adblock/0002..0007`,
  `60-ui/0005`, `0007`, `0011`, `0014`, `80-fingerprint/0001..0002`); a new feature is added in
  `src/plainbrowser/hooks/*_hooks.cc` and needs no patch on those files. Same behaviour. An already-built tree needs
  a fresh `prepare` (the old patches are applied there).
- Release builds use the new `release` profile (`is_official_build=true`, still no PGO or LTO) instead of `alpha`;
  `build-windows` defaults to it (`docs/CI.md`). Not built yet: if it fails to compile only there, run the workflow
  with `profile` = `alpha` and fix the official-only error.
- A `.crx` from an arbitrary site no longer opens the install dialog (it was the default for a few days in this
  unreleased version, never shipped): extensions come from chrome://recommended-extensions, or the developer-mode
  drag and drop, or the chrome://flags entry "Handling of extension MIME type requests".
- Optional compile cache for CI (sccache on a Cloudflare R2 bucket, `docs/BUILD_CACHE.md`); off until
  `SCCACHE_BUCKET` is set.
- The engine process is configured with `UseConfig(EngineConfig)` (downloaded lists, optional lists, own filters)
  instead of `UseLists()`; the user's choices survive an engine restart, downloaded lists are still dropped after a
  crash.
- `filter_lists::ListMatches` and the mirror accept `! Title:` as a list header (uBlock Origin's lists have no
  `[Adblock Plus]` line).

## [0.2.1-beta.1] - 2026-11-10 [Chromium 157.0.8103.50, ungoogled-chromium-windows 157.0.8103.50-1.1]

### Changed
- Chromium base updated from 157.0.8103.38 to 157.0.8103.50 (upstream update, built and submitted to the
  Microsoft Store by the automated release pipeline).
- Package version 0.2.1.0.

## [0.2.0-beta.7] - 2026-11-09 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1] (never released: 0.2.1-beta.1 was built from the pipeline's upstream update before these changes were pushed; they ship with the next release)

### Changed
- Patches rewritten for cheaper Chromium bumps, same behaviour. Our `#include`s sit in one "Plain Browser: own
  includes" block right after each file's own header instead of between upstream includes (the list that changes
  every release). The adblock throttle is added by `AdblockThrottleProvider`, a wrapper around chrome's
  `URLLoaderThrottleProviderImpl`, so `30-adblock/0003` no longer edits that class. The Safe Browsing settings
  texts and their Polish translations moved from hunks in `settings_strings.grdp` and the generated
  `generated_resources_pl.xtb` to our own grdp/xtb (`//plainbrowser/safebrowsing/ui` sets them under Chromium's
  keys). The `[crate.*]` tables of adblock-rust's dependencies are one block at the end of `gnrt_config.toml`.
  `40-mv2/0001` and `60-ui/0004` are one-line hunks. The ShieldsConfig binding of a new renderer is in
  `60-ui/0007` with the rest of that pipe, not in the adblock patch. The settings section "Sites with
  protections off" is a Lit element (Chromium is moving Settings off Polymer); `60-ui/0006` includes the button
  header after the include guard and keeps the member at the end of the class.
- `scripts/check-patches.py`: a patch that applies with fuzz is a failure (the hunk may have landed in the wrong
  place), and `patches/upstream-revert.hunks` (written by `--regenerate`) pins which hunks the `hunks:` entries of
  `upstream-revert.list` drop, so a hunk split or move upstream is reported before a build.

### Added
- About page (`chrome://settings/help`): the Chromium and ungoogled-chromium versions the build is made from, and
  the ad blocking components (adblock-rust version, EasyList, EasyPrivacy, EasyList Polish), with a Polish
  translation (`60-ui/0012`). The versions come from `versions.toml` and the adblock crate pin through
  `scripts/sync-version.py`.

## [0.2.0-beta.6] - 2026-11-07 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1]

### Fixed
- Build: the settings lint (eslint) failed on two of our files: `!!pref.value` on a boolean in the shields exceptions
  component (`60-ui/0009`), and `passwordsLeakToggle` still listed in the `$` interface of `security_page.ts` after
  ungoogled removes that element (`70-safebrowsing/0005` removes the entry and its import). 0.2.0-beta.5 was tagged
  but its build stopped there (stage 5, after four stages of compiling) and was never released.
- Build: `build-windows.yml` can resume at stage 5 (`resume_stage`), and `scripts/ci/hotfix-tree.py` applies these two
  edits and this revision's `version.h` to the resumed tree, so beta.6 was built on top of beta.5's compiled stages 1-4.

## [0.2.0-beta.5] - 2026-11-06 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1]

### Fixed
- Build: the Safe Browsing hunks of ungoogled's `remove-unused-preferences-fields.patch` that we drop were matched case
  sensitively, so five hunks of `security_page.html`/`.ts` written `safeBrowsing...` still applied. They removed the
  `safeBrowsingEnhanced`, `safeBrowsingStandard` and `safeBrowsingRadioGroup` entries from the page's `$` type
  while the code (kept) still used them, and `tsc` failed in the settings build (TS2339). Chromium 157 made the
  difference visible. The same hunks also removed the "Turn off Safe Browsing?" confirmation dialog from the page
  (in 0.1.x too, see below), so choosing "No protection" had no confirmation. 0.2.0-beta.4 was tagged but its build
  stopped there and was never released.
- The Advanced Protection Program link is still removed from the Security page, now by
  `patches/70-safebrowsing/0005` (its removal in `remove-uneeded-ui.patch` no longer fits the restored dialog code).

## [0.2.0-beta.4] - 2026-11-06 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1]

### Fixed
- Build: `scripts/ci/prepare-tree.ps1` did not generate `build/util/MINIBRANCH` (the `minibranch` DEPS hook, which
  upstream's `build.py` runs right after `clone.py` since 157), so ninja stopped before compiling anything.
  0.2.0-beta.3 was tagged but its build stopped there and was never released.

## [0.2.0-beta.3] - 2026-11-06 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1]

### Fixed
- Build: the Rust toolchain step of `scripts/ci/prepare-tree.ps1` did not copy the `rustc-dev` package
  (`third_party/rust-toolchain-x64-dev`) into `rust-toolchain/lib` as upstream's `build.py` does since 157. Crubit's
  `cc_bindings_from_rs` needs the `rustc_*` crates and failed with E0463. 0.2.0-beta.2 was tagged but its build stopped
  there and was never released.

## [0.2.0-beta.2] - 2026-11-06 [Chromium 157.0.8103.38, ungoogled-chromium-windows 157.0.8103.38-1.1]

First beta. The product moves from alpha to beta; the build profile names (`alpha`, `official`) are about compiler flags and stay.

### Changed
- Chromium base updated from 156.0.8078.79 to 157.0.8103.38 (a new major version).
- Patches ported to Chromium 157: the settings pages are Lit templates and read prefs through `PrefService` now, so the
  "sites with protections off" section (`60-ui/0009`) observes its prefs by key instead of receiving a `prefs` object;
  the Safe Browsing settings hunks (`70-safebrowsing/0005`) follow the `pref-key` markup; context of the New Tab
  shortcut (`60-ui/0004`) and the Safe Browsing request patch (`70-safebrowsing/0001`) updated.
- Package version 0.2.0.0.

### Fixed
- Build: `//plainbrowser/shields:ui` depended on `//chrome/browser/ui`, which Chromium 157 split into many targets; it now
  depends on the three it needs (`chrome_pages`, `browser_window`, `toolbar_button`). 0.2.0-beta.1 was tagged but its
  build stopped in `gn gen` on this and was never released.

### Removed
- `90-defaults/0001-https-first-balanced`: Chromium 157 turns the auto-enable of HTTPS-First Mode's balanced setting
  on by default itself.

## [0.1.6-alpha.1] - 2026-11-04 [Chromium 156.0.8078.79, ungoogled-chromium-windows 156.0.8078.79-1.1]

### Changed
- Package version 0.1.6.0 (the Store package version grows with every release).

### Fixed
- Crash of the browser process when a visited URL matched the local phishing list (`NOTREACHED` in
  `GetThreatSeverity`: the list is synced as `SOCIAL_ENGINEERING_PUBLIC`, which upstream does not expect there).

## [0.1.5-alpha.1] - 2026-10-31 [Chromium 156.0.8078.79, ungoogled-chromium-windows 156.0.8078.79-1.1]

### Changed
- Chromium base updated from 156.0.8078.73 to 156.0.8078.79 (upstream security update).
- Package version 0.1.5.0.

### Fixed
- Build: `GetEnterpriseProfileId` stays defined (ungoogled's preference patch split its hunks in 156.0.8078.73 and
  only the call was kept back).

## [0.1.4-alpha.1] - 2026-10-30 [Chromium 156.0.8078.73, ungoogled-chromium-windows 156.0.8078.73-1.1]

### Changed
- Chromium base updated from 156.0.8078.47 to 156.0.8078.73 (upstream security update).
- Package version 0.1.4.0, the next Store package after 0.1.3.0.

## [0.1.3-alpha.1] - 2026-10-28 [Chromium 156.0.8078.47, ungoogled-chromium-windows 156.0.8078.47-1.1]

### Added
- Safe Browsing (local hash-prefix database, standard protection) compiled back in and routed through our proxy
  `sb.plain-browser.app`; only public lists are requested, with an `X-Client: PlainBrowser/<version>` header.
- Tracking-parameter stripping on navigations (`utm_*`, `fbclid`, `gclid`, ...) via a `$removeparam` list.
- Fingerprint protection (canvas, audio, WebGL strings, `hardwareConcurrency`, `deviceMemory`) with a built-in
  compatibility list for banking and payment sites and `--fingerprint-exceptions`.
- Per-site exceptions ("shields"): a toolbar button and bubble that turn ad blocking, tracking-parameter stripping
  and fingerprint protection off for the current site; one profile list (`plainbrowser.shields.exceptions`) read by
  all three.
- Shields UI follow-ups: a badge on the toolbar button with the number of requests blocked on the page (live
  while the page loads); "Sites with protections off" on Settings > Privacy and security (list, add, remove; a
  typed URL or host is stored as its site); a "Manage sites" link in the bubble; all shields strings translated
  into Polish (`src/plainbrowser/shields/resources`).
- `$redirect` rules serve our own stand-ins (noop scripts, a 1x1 GIF, tag manager / analytics / adsbygoogle
  stubs, ...) instead of a hard block; `##+js(...)` scriptlets run from our own implementations (set-constant,
  abort-current-script, no-fetch-if, json-prune and 15 more); procedural rules (`:has-text()`, `:upward()`,
  `:xpath()`, `:remove()`, `:style()`, ...) apply, also to content added later; generic class/id rules follow the
  DOM for as long as the page is open. All resources are ours, under MPL-2.0.
- The filter list manifest is signed (Ed25519); the browser accepts updates only with a valid signature.
- Generic element hiding: EasyList's class and id rules (`##.ad-box`, `###top-ad`, ...) now apply, matched against
  the class names and ids a page has, at load and on a few passes after it.
- Filter list updates between releases: once a day the browser fetches EasyList, EasyPrivacy and EasyList Polish
  from our mirror `lists.plain-browser.app` (Cloudflare Worker, `infra/filter-lists`, no logs), downloads only
  changed lists, checks their size and SHA-256 and swaps the engine in the background. "Update filter lists
  daily" in Settings > Privacy and security turns it off (the bundled lists are then used).
- Settings > You and Google > Google services: a toggle "Safe Browsing through the Plain Browser proxy", the one
  switch for the only Google service the browser still talks to (through our proxy).
- Project website (`website/`, plain-browser.app) with the privacy and security policies in English and Polish.
- Polish security policy (`docs/SECURITY.pl.md`).
- Debouncing: links through known tracking redirectors (`l.facebook.com/l.php?u=`, `google.<tld>/url?q=`,
  YouTube, Outlook Safe Links, Bing, LinkedIn, Reddit, VK, Steam, Slack, affiliate networks) go straight to the
  target, so the redirector never sees the click. Own rule list; off with the per-site toggle or
  `--disable-features=PlainBrowserDebounce`.
- Settings > Privacy and security: a "Block ads and trackers" switch for all sites (link cleaning stays on), and
  "Lists last checked: <date>" under the filter list update toggle.
- `chrome://adblock`: the state of the ad blocker (blocked requests since start-up, the engine process, the lists
  in use and where they come from, the last update check) with a "Check now" button; linked from Settings.

### Fixed
- Microsoft Store package: the taskbar button showed a blank page instead of the logo. The window pointed at the
  profile icon, which the package writes to its virtualized AppData where Explorer cannot see it; it now carries the
  package app's ID and the package's logo (`patches/60-ui/0010`). The package also gets a `resources.pri`, so the
  shell finds the 16-256 px unplated logos.
- Element hiding: one invalid selector in a site's rules (for example an Adblock Plus `#?#` rule) switched off all
  hide rules for that site, because they were one CSS rule; each selector now has its own.

### Changed
- Secure defaults: HTTPS-First Mode is on in its balanced setting (a public site that only answers over HTTP gets
  a warning first; local and intranet hosts do not), `patches/90-defaults/0001`; secure DNS is back in Chromium's
  automatic mode, so lookups go over DNS-over-HTTPS when the resolver already in use offers it (ungoogled turns
  this off; we drop two hunks of its `doh-changes.patch`, the removal of Google Public DNS stays). Both follow the
  user's choice in Settings > Security. Third-party cookies stay blocked (ungoogled's default), which also keeps
  Chromium's bounce tracking mitigation active.
- The ad blocker's engine runs in its own sandboxed utility process ("Plain Browser ad blocker" in Task Manager), so
  filter lists, which now come from the network, are never parsed in the browser process. If that process crashes,
  nothing is blocked until it is restarted on the next request, without the downloaded lists.
- Distribution decision: Plain Browser ships as an MSIX through the Microsoft Store only. The Store signs the package
  and delivers updates, so there is no own updater and no Authenticode certificate.
- Settings > Security: the Enhanced protection option is gone (its real-time lookups would send URLs; the proxy
  only serves the local-database updates) and the Standard / No protection descriptions say that requests go through
  the Plain Browser proxy, not to Google.

## [0.1.0-alpha.1] - 2026-10-26 [Chromium 156.0.8078.47, ungoogled-chromium-windows 156.0.8078.47-1.1]

First branded alpha. Test build, not for daily use.

### Added
- Plain Browser branding: product name, icons, install mode (`%LOCALAPPDATA%\PlainBrowser`), MSIX package.
- Built-in ad and tracker blocking with adblock-rust (EasyList, EasyPrivacy, EasyList Polish compiled in),
  kill switch `--disable-features=PlainBrowserAdblock`.
- Manifest V2 kept enabled (ungoogled-chromium patch) with our cleanup of the deprecation UI.
- First-run without the ungoogled-chromium page; no Web Store app or shortcut.

### Known limits
- Cosmetic filtering only for site-specific rules; no toolbar button or per-site toggle.
- Unsigned portable zip and installer: SmartScreen warns. Placeholder icon in the first build.
