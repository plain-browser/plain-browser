# Manifest V2 extensions: what was tested

Status 2026-10-26: **MV2 works end to end** in the packaged build `PlainBrowser_0.1.0-alpha.1_x64.zip`
(Chromium 156.0.8078.47). Tested with the real uBlock Origin, loaded unpacked, headless, with the
test `scripts/test-mv2.ps1`. Nothing here depends on the internet or on DNS: all test hosts are served by a
local HTTP server and mapped to 127.0.0.1 with `--host-resolver-rules`.

## Extension under test

| | |
|---|---|
| Extension | uBlock Origin (gorhill/uBlock), GitHub release asset `uBlock0_1.75.0.chromium.zip` |
| Version | 1.75.0 (`manifest_version: 2`, `minimum_chrome_version: 93.0`) |
| License | GPLv3 (`LICENSE.txt` in the zip) |
| sha256 of the zip | `393cf95709d1074d4022970e9014e434395c53a822387f1e25f43be97cf4b582` |
| Permissions | alarms, contextMenus, privacy, storage, tabs, unlimitedStorage, webNavigation, webRequest, **webRequestBlocking**, `<all_urls>` |
| How loaded | `--load-extension=<unpacked dir>` (install type `development`); extension id `oodlehidjagnanfjimjmapojmhlpinpm` (derived from the path, not the Web Store id) |

## What the test checks

The browser runs with `--headless=new`, `--disable-features=PlainBrowserAdblock` and `--remote-debugging-port`.
Over the DevTools protocol the test:

1. **(a) Extension loaded.** `/json/list` must contain a `background_page` target
   `chrome-extension://<id>/background.html` (uBO's MV2 background page; it exists only if the extension was
   installed, enabled and its background page started). In that page it waits for uBO's `readyToFilter` flag and
   calls `chrome.management.getSelf()` and `chrome.runtime.getManifest().manifest_version`.
2. **(b) No MV2 deprecation state.** Opens `chrome://extensions` and reads every `extensions-item`'s data from the
   WebUI (`state`, `disableReasons.unsupportedManifestVersion`, `isAffectedByMV2Deprecation`, the enable toggle) and
   whether the `extensions-mv2-deprecation-panel` is present.
3. **(c) Blocking works.** Opens `http://site.test:8021/t.html`, which fetches eight URLs: four EasyList/EasyPrivacy
   ad hosts (`ib.adnxs.com`, `static.criteo.net`, `sb.scorecardresearch.com`, `ads.pubmatic.com`), two hosts that
   are only on Peter Lowe's list (`101com.com`, `4info.com`; a uBO default list that the built-in blocker does not
   ship, so only the extension can block them) and two controls (`example.com`, `example.org`). The result is read
   from the tab title; the server log shows which requests arrived; uBO's background page reports the tab's
   blocked/allowed counters (its `webRequest.onBeforeRequest` blocking decisions).

The same page is loaded once more without the extension (`NO_EXT`) as the control run.

## Result (2026-10-26, package built 2026-10-26 08:25)

```
package: C:\dw-test-mv2\pkg\PlainBrowser_0.1.0-alpha.1_x64\chrome.exe (156.0.8078.47)
uBlock Origin 1.75.0: manifest_version 2, permissions: alarms contextMenus privacy storage tabs unlimitedStorage webNavigation webRequest webRequestBlocking <all_urls>
   license: GNU GENERAL PUBLIC LICENSE, zip sha256 393cf95709d1074d4022970e9014e434395c53a822387f1e25f43be97cf4b582
[WITH_UBO] browser: Chrome/156.0.8078.47  flags: --headless=new --disable-features=PlainBrowserAdblock --load-extension=C:\dw-test-mv2\ubo\uBlock0.chromium
[WITH_UBO] (a) background page target: type=background_page url=chrome-extension://oodlehidjagnanfjimjmapojmhlpinpm/background.html
[WITH_UBO] (a) uBO readyToFilter: True (after ~4 s)
[WITH_UBO] (a) management.getSelf: {"id":"oodlehidjagnanfjimjmapojmhlpinpm","name":"uBlock Origin","version":"1.75.0","enabled":true,"installType":"development","type":"extension","manifest_version":2,"webRequestBlocking":true}
[WITH_UBO] (b) chrome://extensions: uBlock Origin 1.75.0, state ENABLED, enable toggle present and checked,
           disableReasons all false (incl. unsupportedManifestVersion=false), isAffectedByMV2Deprecation=false,
           mv2DeprecationPanel=false, listMentionsUnsupported=false
[WITH_UBO] (c) RESULT ad_adnxs=error ad_criteo=error ad_scorecard=error ad_pubmatic=error pgl_101com=error pgl_4info=error ctl_example_com=allowed(14ms) ctl_example_org=allowed(11ms)
[WITH_UBO] (c) uBO per-tab counters: tab 2087652495 http://site.test:8021/t.html blocked=2 allowed=5
[WITH_UBO] (c) server received: GET /t.html HTTP/1.1 | GET /favicon.ico HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1
[NO_EXT] browser: Chrome/156.0.8078.47  flags: --headless=new --disable-features=PlainBrowserAdblock
[NO_EXT] (c) RESULT ad_adnxs=error ad_criteo=error ad_scorecard=error ad_pubmatic=error pgl_101com=allowed(9ms) pgl_4info=allowed(5ms) ctl_example_com=allowed(4ms) ctl_example_org=allowed(4ms)
[NO_EXT] (c) server received: GET /t.html HTTP/1.1 | GET /index.html HTTP/1.1 | GET /favicon.ico HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1
DONE
```

Reading of the result:

- The MV2 extension installs, is enabled, its background page runs, `manifest_version` is 2 and
  `chrome://extensions` shows no deprecation state of any kind (`unsupportedManifestVersion=false`,
  `isAffectedByMV2Deprecation=false`, no MV2 panel): `patches/40-mv2/0001` and ungoogled's
  `extensions-manifestv2.patch` do what they should.
- Blocking `webRequest` works: the two Peter Lowe hosts are blocked only when uBO is loaded (`error`, never reach
  the server, uBO counts `blocked=2`) and load in the control run (`allowed`, two extra `GET /index.html` on the
  server).
- The four EasyList hosts are blocked in **both** runs, so for them the test cannot tell uBO and the built-in
  blocker apart (see the next section). uBO's `blocked=2` instead of 6 is consistent with that: those four requests
  are cancelled by the renderer-side throttle before they reach the extension's `webRequest` listener.

## The four EasyList hosts and the adblock kill switch: a stale package, not a bug

In the package tested above (`PlainBrowser_0.1.0-alpha.1_x64.zip`, built 2026-10-26 08:25, `chrome.dll` dated
2026-10-19), `--disable-features=PlainBrowserAdblock` did not let the four EasyList hosts through (a CDP
`Network.loadingFailed` capture shows `net::ERR_BLOCKED_BY_CLIENT`, i.e. `AdblockURLLoaderThrottle`), and the
tracking-parameter stripping did not fire. Reason: that `chrome.dll` predates the codename rename; it contains the
string `DraftwayAdblock` (3 times) and no `PlainBrowserAdblock`, so the switch addressed a feature that does not exist in
that binary. The current build `out\Default\chrome.exe` (2026-10-26 11:39, `chrome.dll` with `PlainBrowserAdblock`,
`PlainBrowserTrackingParams`, `PlainBrowserFingerprintProtection`) behaves as documented. The control run repeated
against it, same page with `?utm_source=nl&id=7` appended:

```
[NO_EXT_outDefault] browser: Chrome/156.0.8078.47  flags: --headless=new --disable-features=PlainBrowserAdblock
[NO_EXT_outDefault] (c) RESULT ad_adnxs=allowed(11ms) ad_criteo=allowed(7ms) ad_scorecard=allowed(5ms) ad_pubmatic=allowed(3ms) pgl_101com=allowed(3ms) pgl_4info=allowed(3ms) ctl_example_com=allowed(3ms) ctl_example_org=allowed(3ms)
[NO_EXT_outDefault] (c) page url now: http://site.test:8021/t.html?utm_source=nl
[NO_EXT_outDefault] (c) server received: GET /t.html?utm_source=nl HTTP/1.1 | GET /ttj HTTP/1.1 | GET /js/ld/publishertag.js HTTP/1.1 | GET /favicon.ico HTTP/1.1 | GET /beacon.js HTTP/1.1 | GET /AdServer/js/showad.js HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1 | GET /index.html HTTP/1.1
```

All eight requests reach the server with the kill switch: the control is clean on the current build. (The `id=7`
parameter was removed from the URL while `utm_source` stayed; with `PlainBrowserAdblock` disabled the
tracking-parameter throttle is not registered, so that is not ours; the `/json/new?url` route of the DevTools HTTP
endpoint drops everything after the first `&` of the query. The kill-switch result is unaffected.)

The MV2 proof with uBO was made against the stale package. It still holds (MV2 code is not involved in the rename
and the Peter Lowe hosts separate uBO from the built-in blocker), but the next package should be re-tested with
`scripts/test-mv2.ps1`; there the `ad_*` hosts are expected to be blocked by uBO alone, with `blocked=6` in uBO's
counters and all eight `allowed` in `NO_EXT`.

## Limits of what was tested

- Headless only (`--headless=new`); the extension loaded fine there, so the normal-window fallback was not needed.
  Not covered: toolbar button/popup, the dashboard UI, cosmetic filtering, element picker, the "developer mode
  extensions" bubble.
- Unpacked load with `--load-extension`, not a CRX install or an update URL (curated repository: `docs/ARCHITECTURE.md` 8.2).
- One extension (uBO). Other MV2 APIs (pageAction, declarativeContent, ...) were not exercised.
- Fresh profile each run; persistence of uBO's settings/lists across restarts was not checked.

## How to run

On the build VM, with the packaged zip in place (default `C:\dw-build\ucw\build\PlainBrowser_0.1.0-alpha.1_x64.zip`):

```
powershell -File C:\dw-build\repo\scripts\test-mv2.ps1 [-Zip <package.zip>] [-UboVersion 1.75.0] [-UboZip <already downloaded zip>]
```

The script downloads `uBlock0_<ver>.chromium.zip` from GitHub (or uses `-UboZip`), works in `C:\dw-test-mv2`,
uses HTTP port 8021 and remote-debugging ports 9361/9362, and kills only the chrome processes it started from that
directory. If the VM's execution policy refuses `-File`, run it as
`powershell -NoProfile -Command "& ([ScriptBlock]::Create([IO.File]::ReadAllText('C:\dw-build\repo\scripts\test-mv2.ps1')))"`.
Expected on a current build: `[WITH_UBO]` with `manifest_version 2`, `state ENABLED`, `ad_*` and `pgl_*` `error`,
`ctl_*=allowed`, `blocked=6`; `[NO_EXT]` with all eight `allowed`.
