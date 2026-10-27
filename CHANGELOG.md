# Changelog

All notable changes to Plain Browser. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow the product version in `versions.toml`; the Chromium base is given in brackets.

## [Unreleased]

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
- Settings > You and Google > Google services: a toggle "Safe Browsing through the Plain Browser proxy", the one
  switch for the only Google service the browser still talks to (through our proxy).
- Project website (`website/`, plain-browser.app) with the privacy and security policies in English and Polish.
- Polish security policy (`docs/SECURITY.pl.md`).

### Changed
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
