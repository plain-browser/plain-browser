# Changelog

All notable changes to Plain Browser. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow the product version in `versions.toml`; the Chromium base is given in brackets.

## [Unreleased]

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
