<# End-to-end Manifest V2 test for a Plain Browser package (run on the Windows build VM).
   Loads the real uBlock Origin (MV2, GPLv3, gorhill/uBlock GitHub release asset uBlock0_<ver>.chromium.zip) as an
   unpacked extension with --load-extension, with the built-in blocker turned off (--disable-features=PlainBrowserAdblock)
   (the 08:25 build of the 0.1.0-alpha.1 package still names the feature DraftwayAdblock, see docs/MV2.md). Eight hostnames
   are served by a local HTTP server (--host-resolver-rules maps them to 127.0.0.1, so neither the internet nor DNS
   filters matter): four EasyList/EasyPrivacy ad hosts, two hosts that are only on Peter Lowe's list (a uBO default list
   that the built-in blocker does not ship, so only the extension can block them) and two controls. The test page itself
   is served over HTTP (http://site.test:8021/t.html) so uBO treats the tab like a normal site.
   Over the remote-debugging port the test
     (a) finds uBO's background page (chrome-extension://<id>/background.html) in /json/list and asks it, via CDP
         Runtime.evaluate, for chrome.management.getSelf() (enabled, installType) and the manifest_version, and waits
         until uBO reports readyToFilter;
     (b) opens chrome://extensions and reads each extensions-item's data (state, disableReasons,
         isAffectedByMV2Deprecation) and the presence of the MV2 deprecation panel;
     (c) opens the test page, reads the result from the tab title, and asks uBO's background page for the tab's
         blocked/allowed request counters (webRequest.onBeforeRequest blocking decisions made by the extension).
   Expected WITH_UBO: manifest_version 2, enabled, state ENABLED, no MV2 panel/warning, ad_* and pgl_* error (never
   reach the server), ctl_* allowed, uBO counters blocked=6. Expected NO_EXT (control run): all eight allowed.
   Usage: powershell -File scripts\test-mv2.ps1 [-Zip <package.zip>] [-UboVersion 1.75.0] [-UboZip <local zip>] #>
param([string]$Zip = "C:\dw-build\ucw\build\PlainBrowser_0.1.0-alpha.1_x64.zip",
      [string]$UboVersion = "1.75.0",
      [string]$UboZip = "")
$ErrorActionPreference = "Continue"
$dir = "C:\dw-test-mv2"
$http = 8021
Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dir\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$dir\srv","$dir\ubo" | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($Zip, "$dir\pkg")
$exe = Get-ChildItem "$dir\pkg" -Recurse -Filter chrome.exe | Select-Object -First 1
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
"package: $($exe.FullName) ($((Get-Item $exe.FullName).VersionInfo.FileVersion))"

# --- uBlock Origin ---
if (-not $UboZip) {
  $UboZip = "$dir\uBlock0_$UboVersion.chromium.zip"
  curl.exe -sL -o $UboZip "https://github.com/gorhill/uBlock/releases/download/$UboVersion/uBlock0_$UboVersion.chromium.zip"
}
[IO.Compression.ZipFile]::ExtractToDirectory($UboZip, "$dir\ubo")
$ext = (Get-ChildItem "$dir\ubo" -Recurse -Filter manifest.json | Select-Object -First 1).DirectoryName
$man = Get-Content "$ext\manifest.json" -Raw | ConvertFrom-Json
"uBlock Origin $($man.version): manifest_version $($man.manifest_version), permissions: $($man.permissions -join ' ')"
"   license: " + (Get-Content "$ext\LICENSE.txt" -First 1).Trim() + ", zip sha256 " + (Get-FileHash $UboZip -Algorithm SHA256).Hash.ToLower()

# --- test page and server content ---
Set-Content "$dir\srv\index.html" "ok"
@'
<html><head><title>pending</title></head><body><script>
async function t(u){ const c = new AbortController(); const id = setTimeout(() => c.abort(), 6000); const t0 = performance.now();
  try { await fetch(u,{mode:"no-cors",signal:c.signal}); clearTimeout(id); return "allowed(" + Math.round(performance.now()-t0) + "ms)"; }
  catch(e) { clearTimeout(id); return e.name === "AbortError" ? "TIMEOUT" : "error"; } }
(async () => {
  const tests = {
    ad_adnxs: "http://ib.adnxs.com:__PORT__/ttj",
    ad_criteo: "http://static.criteo.net:__PORT__/js/ld/publishertag.js",
    ad_scorecard: "http://sb.scorecardresearch.com:__PORT__/beacon.js",
    ad_pubmatic: "http://ads.pubmatic.com:__PORT__/AdServer/js/showad.js",
    pgl_101com: "http://101com.com:__PORT__/index.html",
    pgl_4info: "http://4info.com:__PORT__/index.html",
    ctl_example_com: "http://example.com:__PORT__/index.html",
    ctl_example_org: "http://example.org:__PORT__/index.html"
  };
  const out = [];
  for (const k in tests) out.push(k + "=" + await t(tests[k]));
  document.title = "RESULT " + out.join(" ");
})();
</script></body></html>
'@ -replace "__PORT__", $http | Set-Content "$dir\srv\t.html" -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP 101com.com 127.0.0.1, MAP 4info.com 127.0.0.1, MAP example.com 127.0.0.1, MAP example.org 127.0.0.1, MAP site.test 127.0.0.1"

# --- minimal CDP client (one WebSocket per call) ---
function Cdp($wsUrl, $method, $params) {
  $sock = New-Object System.Net.WebSockets.ClientWebSocket
  $ct = [System.Threading.CancellationToken]::None
  $sock.ConnectAsync([uri]$wsUrl, $ct).Wait()
  $msg = @{ id = 1; method = $method; params = $params } | ConvertTo-Json -Depth 5 -Compress
  $bytes = [Text.Encoding]::UTF8.GetBytes($msg)
  $sock.SendAsync((New-Object ArraySegment[byte] -ArgumentList @(,$bytes)), [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).Wait()
  $buf = New-Object byte[] 262144
  $answer = $null
  for ($n = 0; $n -lt 50 -and -not $answer; $n++) {
    $ms = New-Object IO.MemoryStream
    do {
      $r = $sock.ReceiveAsync((New-Object ArraySegment[byte] -ArgumentList @(,$buf)), $ct).Result
      $ms.Write($buf, 0, $r.Count)
    } while (-not $r.EndOfMessage)
    $o = [Text.Encoding]::UTF8.GetString($ms.ToArray()) | ConvertFrom-Json
    if ($o.id -eq 1) { $answer = $o }
  }
  $sock.Abort(); $sock.Dispose()
  return $answer
}
function Ev($wsUrl, $expr) {
  try {
    $o = Cdp $wsUrl "Runtime.evaluate" @{ expression = $expr; returnByValue = $true; awaitPromise = $true }
    if ($o.result.exceptionDetails) { return "EXCEPTION: " + $o.result.exceptionDetails.exception.description }
    return $o.result.result.value
  } catch { return "CDP ERROR: $($_.Exception.Message)" }
}
function Targets($port) { try { return @((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | ForEach-Object { $_ }) } catch { return @() } }  # PS 5.1 returns a JSON array as one object: unroll it
function OpenTab($port, $url) { return Invoke-RestMethod -Method Put "http://127.0.0.1:$port/json/new?$url" -TimeoutSec 5 }

$extensionsJs = @'
(() => {
  const mgr = document.querySelector('extensions-manager');
  if (!mgr) return 'no extensions-manager element';
  const list = mgr.shadowRoot.querySelector('extensions-item-list');
  const items = [...list.shadowRoot.querySelectorAll('extensions-item')].map(it => {
    const d = it.data || {}, sr = it.shadowRoot, tg = sr.querySelector('#enableToggle');
    return { name: d.name, version: d.version, manifestVersion: d.manifestVersion, state: d.state, disableReasons: d.disableReasons,
      isAffectedByMV2Deprecation: d.isAffectedByMV2Deprecation, mustRemainInstalled: d.mustRemainInstalled,
      enableToggle: !!tg, enableToggleChecked: tg ? tg.checked : null,
      mv2Keys: Object.keys(d).filter(k => /mv2|manifest|deprecat/i.test(k)).map(k => k + '=' + JSON.stringify(d[k])),
      text: sr.textContent.replace(/\s+/g, ' ').trim().slice(0, 400) };
  });
  const panel = list.shadowRoot.querySelector('extensions-mv2-deprecation-panel');
  const listText = list.shadowRoot.textContent.replace(/\s+/g, ' ').trim();
  return JSON.stringify({ items, mv2DeprecationPanel: !!panel, panelText: panel ? panel.shadowRoot.textContent.replace(/\s+/g, ' ').trim() : '',
    listMentionsUnsupported: /no longer supported|unsupported|turned off|deprecat/i.test(listText) }, null, 1);
})()
'@

function Run($tag, $port, $withExt) {
  $log = "$dir\srv_$tag.log"
  $srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","$http","--bind","127.0.0.1","--directory","$dir\srv") -PassThru -WindowStyle Hidden -RedirectStandardError $log
  Start-Sleep 2
  $a = @("--headless=new","--disable-gpu","--no-first-run","--window-size=1280,900","--user-data-dir=$dir\p_$tag","--remote-debugging-port=$port",
         "--disable-features=PlainBrowserAdblock",("--host-resolver-rules=" + [char]34 + $rules + [char]34))
  if ($withExt) { $a += "--load-extension=$ext" }
  $a += "about:blank"
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $a -PassThru -RedirectStandardError "$dir\e_$tag.txt"
  $ver = $null
  for ($i = 0; $i -lt 30 -and -not $ver; $i++) { Start-Sleep 1; try { $ver = Invoke-RestMethod "http://127.0.0.1:$port/json/version" -TimeoutSec 3 } catch { } }
  "[$tag] browser: $($ver.Browser)  flags: $(($a | Where-Object { $_ -like '--*feature*' -or $_ -like '--load-ext*' -or $_ -like '--headless*' }) -join ' ')"
  if ($withExt) {
    $bg = $null
    for ($i = 0; $i -lt 30 -and -not $bg; $i++) { Start-Sleep 1; $bg = Targets $port | Where-Object { $_.url -like "chrome-extension://*/background.html" } | Select-Object -First 1 }
    if ($bg) {
      "[$tag] (a) background page target: type=$($bg.type) url=$($bg.url)"
      $ready = $false
      for ($i = 0; $i -lt 60 -and -not $ready; $i++) { Start-Sleep 1; $v = Ev $bg.webSocketDebuggerUrl 'self["\u00b5Block"] !== undefined && self["\u00b5Block"].readyToFilter === true'; if ($v -eq $true) { $ready = $true } }
      "[$tag] (a) uBO readyToFilter: $ready (after ~$i s)"
      $info = Ev $bg.webSocketDebuggerUrl 'new Promise(r => chrome.management.getSelf(i => r(JSON.stringify({ id: i.id, name: i.name, version: i.version, enabled: i.enabled, installType: i.installType, type: i.type, manifest_version: chrome.runtime.getManifest().manifest_version, webRequestBlocking: typeof chrome.webRequest.onBeforeRequest.addListener === "function" }))))'
      "[$tag] (a) management.getSelf: $info"
    } else {
      "[$tag] (a) FAIL: no background page in /json/list; targets:"
      Targets $port | ForEach-Object { "      $($_.type) $($_.url)" }
    }
    "[$tag] other extension targets: " + ((Targets $port | Where-Object { $_.url -like "chrome-extension://*" -and $_.url -notlike "*/background.html" } | ForEach-Object { "$($_.type) $($_.url)" }) -join " | ")
    $et = OpenTab $port "chrome://extensions"
    $r = "(no answer)"
    for ($i = 0; $i -lt 15; $i++) { Start-Sleep 2; $r = Ev $et.webSocketDebuggerUrl $extensionsJs; if ("$r" -like "{*" -and "$r" -match '"name"') { break } }
    "[$tag] (b) chrome://extensions: $r"
    try { Invoke-RestMethod "http://127.0.0.1:$port/json/close/$($et.id)" -TimeoutSec 3 | Out-Null } catch { }
  }
  $pg = OpenTab $port "http://site.test:$http/t.html"
  $title = "(no answer)"
  Start-Sleep 1
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 2
    $t = (Targets $port | Where-Object { $_.id -eq $pg.id } | Select-Object -First 1).title
    if ($t) { $title = $t }
    if ($t -like "RESULT*") { break }
  }
  "[$tag] (c) $title"
  if ($withExt -and $bg) {
    "[$tag] (c) uBO per-tab counters: " + (Ev $bg.webSocketDebuggerUrl '(() => { const b = self["\u00b5Block"]; const out = []; for (const [id, ps] of b.pageStores) { if (!ps || !ps.rawURL || !/^https?:/.test(ps.rawURL)) continue; out.push("tab " + id + " " + ps.rawURL + " blocked=" + (ps.counts && ps.counts.blocked ? ps.counts.blocked.any : "?") + " allowed=" + (ps.counts && ps.counts.allowed ? ps.counts.allowed.any : "?")); } return out.join(" | ") || "(no page stores)"; })()')
  }
  Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dir\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
  "[$tag] (c) server received: " + ((Get-Content $log -ErrorAction SilentlyContinue | Select-String "GET" | ForEach-Object { ($_.Line -split '"')[1] }) -join " | ")
  $err = Get-Content "$dir\e_$tag.txt" -ErrorAction SilentlyContinue | Select-String -Pattern "extension|manifest" -CaseSensitive:$false | Select-Object -First 5
  if ($err) { "[$tag] stderr mentions extension/manifest:"; $err | ForEach-Object { "      $_" } }
}
Run "WITH_UBO" 9361 $true
Run "NO_EXT" 9362 $false
"DONE"
