# Testing

Three layers, from fast to slow:

| Layer | What | Where | Runs in |
|---|---|---|---|
| Rust | adblock-rust bridge (`cargo test`) | `src/plainbrowser/adblock/rs` | CI (`rust.yml`), any machine with cargo |
| Node | `fingerprint_script.js` (noise, determinism, native-looking wrappers) | `src/plainbrowser/fingerprint/renderer/fingerprint_script_test.js` | any machine with Node |
| Node | Workers: filter list mirror (`npm test` in `infra/filter-lists`: publish checks, manifest, serving, refusals) and Safe Browsing proxy (`infra/safebrowsing-proxy`) | `infra/*/test` | any machine with Node 22.18+ |
| **gtest** | **our C++: `plainbrowser_unittests`** (this page) | `*_unittest.cc` next to the code | the Chromium tree on the build VM |
| headless | the whole browser: `scripts/test-adblock.ps1`, `test-privacy.ps1`, `test-safebrowsing.ps1`, `test-shields.ps1`, `test-cosmetic-lists.ps1` (generic hiding; list updates from a local server, restart, switch off) | `scripts/` | the build VM, against `out\Default\chrome.exe` |
| UI | toolbar button and bubble in a normal window, driven by UI Automation: `scripts/test-shields-ui.ps1` (opens, toggles, closes; checks the pushed exception list in the running renderer, the blocked count on the button and the "Manage sites" link; English UI via `--lang`; screenshots); the Polish strings and the exception list in Settings: `scripts/test-shields-settings.ps1` (plural form of the count, add a URL and see its site, remove, the pref on disk); the Safe Browsing settings: `scripts/test-safebrowsing-settings.ps1` (the toggle under You and Google, no Enhanced protection, the pref on disk) | `scripts/` | the build VM's interactive desktop session (a scheduled task with `-LogonType Interactive`, not SSH) |

## plainbrowser_unittests

GN target `//plainbrowser:plainbrowser_unittests` (`src/plainbrowser/BUILD.gn`). It is a plain `test()` with
`//mojo/core/test:run_all_unittests` as the main (it initialises mojo for every test) and one `:unit_tests`
source_set per feature. It needs **no `//content`**: the production code is split so that the testable parts
link alone (see "Layout" below), which keeps the binary small and the link to a few minutes instead of linking
all of `content/browser` and Blink into a second chrome-sized executable.

### What is tested

`adblock` (`src/plainbrowser/adblock/**/*_unittest.cc`):

- `AdBlockService` (`browser/ad_block_service_unittest.cc`): engine built from the bundled lists on the ThreadPool,
  like production; a known EasyList host is blocked and a control is not; an invalid source URL falls back to the
  request URL; non-http(s) URLs are never blocked, rewritten or styled; `RewrittenUrl` strips `utm_*`, `gclid`,
  `fbclid` and keeps the other parameters in order, is empty when nothing was stripped and for requests that are
  blocked outright; cosmetic CSS carries the generic rules everywhere and the site-specific ones (wykop.pl) only on their site; `GetInstance()` returns the
  test instance.
- cxx bridge (`browser/adblock_ffi_unittest.cc`): block/exception/`$third-party` semantics with a tiny rule set,
  `rust::String`/`rust::Str` and `rust::Vec<rust::String>` crossing the bridge (generic class/id CSS), unparsable
  URLs are not blocked.
- `AdBlockService` generic hiding and list swaps (`browser/ad_block_service_unittest.cc`): EasyList's generic
  `##.a-sponsor` and `###ad-top` match only names the page has, `#@#` exceptions apply; `UseLists` replaces only
  the named list (EasyList Polish and the parameter list stay bundled), `UseLists({})` goes back to the bundled
  engine, and of two calls the later one wins.
- filter list store (`browser/filter_lists_unittest.cc`): manifest parsing (unknown lists, bad hashes, zero or
  fractional sizes dropped; non-format-1 rejected), serialisation round trip, `ListMatches` (size, SHA-256,
  `[Adblock Plus]` header, BOM), URLs, `ChangedLists`, store write/read with a damaged file left out, partial
  update, delete.
- Navigation throttle decision logic (`browser/tracking_params_rules_unittest.cc`): which navigations are
  eligible (http(s), not POST), which need the engine (has a query), source URL from the initiator origin, when an
  answer leads to cancel-and-reissue (changed URL, navigation still at the URL asked about); the
  `PlainBrowserTrackingParams` kill-switch name and default.
- `AdblockURLLoaderThrottle` (`renderer/adblock_url_loader_throttle_unittest.cc`) against a fake mojo
  `AdblockFilter` (`mojo::ReceiverSet` with a scripted implementation): block -> `ERR_BLOCKED_BY_CLIENT`, allow ->
  resume, non-http not deferred, initiator passed as source URL, top-level URL and frame token passed through
  (none for workers), fail-open after 3 s when the browser never answers (mock time), fail-open at once when the
  pipe closes, `DetachFromCurrentSequence` clones the pipe, destination -> adblock-rust request type mapping.

`fingerprint` (`src/plainbrowser/fingerprint/**/*_unittest.cc`):

- compatibility list (`renderer/fingerprint_exceptions_unittest.cc`): exact eTLD+1 match, case-insensitive, no
  subdomain/registry match, `--fingerprint-exceptions` parsing, user list added to the built-in list.
- features and switches (`common/features_unittest.cc`): kill-switch and switch names pinned.
- embedded script (`renderer/fingerprint_script_unittest.cc`): `embed_text.py` output carries both seed
  placeholders.

`shields` (`src/plainbrowser/shields/**/*_unittest.cc`):

- exception-list prefs (`browser/shields_prefs_unittest.cc`): `SiteForEntry` turns sites, hosts and URLs into
  the eTLD+1 and rejects non-web entries; `GetExceptions` normalises and drops duplicates; `SetSiteExcepted`
  removes every entry standing for a site; `NormalizeExceptions` rewrites typed entries and leaves a normal list
  untouched (returns false, writes nothing).

Not unit-tested, covered by the headless scripts instead: everything that needs `//content` or Blink, namely
`TrackingParamsNavigationThrottle` (`content::NavigationHandle`), `AdblockFilterImpl` and
`ExposeInterfacesToRenderer` (they resolve `RenderProcessHost` ids and consult the shields exception list),
`AdblockThrottleFactory`, `CosmeticFilterAgent`, `FingerprintAgent`, `AppendRendererCommandLineSwitches` (reads
profile prefs through `RenderProcessHost::FromID`) and the shields UI (button, badge, bubble, settings section:
the UI scripts above).

### Build and run on the build VM

The tree's copy `C:\dw-build\ucw\build\src\plainbrowser` is what gets built; sync it first.

```powershell
# on the VM, from C:\dw-build\ucw\build\src, with the VS environment set up as C:\dw-build\cycle.ps1 does
robocopy C:\dw-build\repo\src\plainbrowser plainbrowser /MIR      # after scripts/vm-sync.sh from the dev machine
out\Default\gn.exe gen out\Default --fail-on-unused-args         # only when a BUILD.gn changed
third_party\ninja\ninja.exe -C out\Default -j 8 plainbrowser_unittests
out\Default\plainbrowser_unittests.exe --gtest_brief=1
out\Default\plainbrowser_unittests.exe --gtest_filter='AdBlockService*' --single-process-tests
```

The first build compiles `//base/test`, gtest and the mojo test support (not part of the chrome build); after
that an edit to a test rebuilds in well under a minute plus the link. The `AdBlockService*` tests build the
engine from the full lists in `SetUp` (about a second each in a release build). Follow the VM sharing rules in the team notes (check for running `ninja`/`clang-cl`/`lld-link`,
`C:\dw-build\build.lock`).

### Layout: why the source_sets are split

- `//plainbrowser/adblock:engine` = `AdBlockService` and `tracking_params_rules` (pure decision helpers and the
  `PlainBrowserTrackingParams` feature), no `//content`; `:browser` adds the mojo filter impl, the renderer
  interface exposure (both need `//plainbrowser/shields:browser`, which needs `//content`) and the
  `content::NavigationThrottle`.
- `//plainbrowser/adblock:url_loader_throttle` = the renderer throttle alone (`blink/public/common` only);
  `:renderer` adds the factory and the cosmetic agent (full Blink).
- `//plainbrowser/fingerprint:exceptions` and `:script` are the Blink-free parts of `:renderer`.
- `//plainbrowser/shields:prefs` = the exception-list pref helpers (prefs, `//net` for the site), no `//content`;
  `:browser` adds `ShieldsService` and the blocked-request counter.

Chromium's `//plainbrowser:browser` and `//plainbrowser:renderer` groups are unchanged, so
`patches/00-build/0001-gn-deps.patch` is untouched.

### Adding a test

1. Put `foo_unittest.cc` next to `foo.cc`, in the same namespace, tests in an anonymous namespace. Keep each
   test to one behaviour; name it after the behaviour (`FailsOpenWhenTheBrowserNeverAnswers`).
2. Add it to the feature's `source_set("unit_tests")` in that directory's `BUILD.gn`. If the code under test
   lives in a source_set that depends on `//content` or `//third_party/blink/public:blink`, split the Blink-free
   part into its own source_set first (as above) instead of adding those deps to the test.
3. Need the engine? Use `AdBlockService::CreateForTesting()` with a `base::test::TaskEnvironment`, call
   `RunUntilIdle()` after creating it, and post queries to `service->task_runner()` (see
   `ad_block_service_unittest.cc`). Need mojo? The test main already initialises it; bind a fake
   implementation with `mojo::Receiver`/`ReceiverSet`. Need a feature state? `base::test::ScopedFeatureList`.
   A process switch? `base::test::ScopedCommandLine`.
4. `ninja -C out\Default plainbrowser_unittests && out\Default\plainbrowser_unittests.exe --gtest_brief=1`.
