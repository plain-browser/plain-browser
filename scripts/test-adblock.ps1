<# End-to-end adblock test for a Plain Browser package (run on the Windows build VM).
   Serves six hostnames from a local HTTP server (chrome --host-resolver-rules maps them to 127.0.0.1, so
   DNS filters such as Pi-hole cannot interfere) and loads a page that fetches one URL from each:
   four known ad/tracker hosts (EasyList rules) and two controls. The result is read from the tab title
   over the remote-debugging port. Run once with the filter on and once with --disable-features=PlainBrowserAdblock.
   Expected ON: ad_* error, never reach the server; ctl_* allowed. Expected OFF: all six allowed, none hang.
   WebSockets (patches/20-hooks/0002, adblock/browser/websocket_filter): ws_duckdns (EasyPrivacy
   ||duckdns.org^$third-party,websocket; a rule with a path such as ||neowin.net/ws/ does not match a URL with a port)
   and ws_control are opened too. The local server (scripts\test-http-ws-server.py) accepts a handshake. The browser
   decides before it opens a socket. Expected ON: ws_duckdns=error AND no `GET /ws/chat` in the server log, ws_control=open
   with `GET /ws`. Expected OFF_killswitch: both open, both in the log.
   Workers (fingerprint protection): the page is served over http (file:// pages are never protected) and also
   starts a blob: Worker; pg_* and wk_* are hardwareConcurrency and the pixel sum of an OffscreenCanvas/canvas fill.
   Expected ON: wk_hwc = pg_hwc in {2,4,8}, wk_cv = pg_cv (both noised, not 218880). OFF_fingerprint (feature
   PlainBrowserFingerprintProtection disabled): the machine's real core count and wk_cv = pg_cv = 218880.
   Usage: powershell -File scripts\test-adblock.ps1 [-Zip <package.zip> | -ExePath <path to chrome.exe, e.g. out\Default>] #>
param([string]$Zip = "C:\dw-build\ucw\build\PlainBrowser_0.1.0-alpha.1_x64.zip", [string]$ExePath = "")
$ErrorActionPreference = "Continue"
Get-Process chrome,python -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\dw-test*" -or $_.Path -like "C:\dw-build\ucw\build\src\out\*" -or $_.Name -eq "python" } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item C:\dw-test3 -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory C:\dw-test3\srv | Out-Null
if ($ExePath) {
  $exe = Get-Item $ExePath
} else {
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [IO.Compression.ZipFile]::ExtractToDirectory($Zip, "C:\dw-test3")
  $exe = Get-ChildItem C:\dw-test3 -Recurse -Filter chrome.exe | Select-Object -First 1
}
$exeDir = (Split-Path $exe.FullName) + "\*"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
Set-Content C:\dw-test3\srv\index.html "ok"
@'
<html><head><title>pending</title></head><body><script>
async function t(u){ const c = new AbortController(); const id = setTimeout(() => c.abort(), 6000); const t0 = performance.now();
  try { await fetch(u,{mode:"no-cors",signal:c.signal}); clearTimeout(id); return "allowed(" + Math.round(performance.now()-t0) + "ms)"; }
  catch(e) { clearTimeout(id); return e.name === "AbortError" ? "TIMEOUT" : "error"; } }
function w(u){ return new Promise(res => { let done = false; const fin = v => { if (!done) { done = true; res(v); } };
  try { const s = new WebSocket(u); s.onopen = () => { s.close(); fin("open"); }; s.onerror = () => fin("error"); s.onclose = () => fin("error"); setTimeout(() => fin("TIMEOUT"), 6000); }
  catch(e) { fin("exception"); } }); }
(async () => {
  const tests = {
    ad_adnxs: "http://ib.adnxs.com:8000/ttj",
    ad_criteo: "http://static.criteo.net:8000/js/ld/publishertag.js",
    ad_scorecard: "http://sb.scorecardresearch.com:8000/beacon.js",
    ad_pubmatic: "http://ads.pubmatic.com:8000/AdServer/js/showad.js",
    ctl_example_com: "http://example.com:8000/index.html",
    ctl_example_org: "http://example.org:8000/index.html"
  };
  const out = [];
  for (const k in tests) out.push(k + "=" + await t(tests[k]));
  const wkr = await new Promise(res => { try {
    const wsrc = "const c=new OffscreenCanvas(16,16);const x=c.getContext('2d');x.fillStyle='#c8c8c8';x.fillRect(0,0,16,16);const d=x.getImageData(0,0,16,16).data;let s=0;for(let i=0;i<d.length;i++)s+=d[i];postMessage({hwc:navigator.hardwareConcurrency,cv:s,origin:self.origin});";
    const w = new Worker(URL.createObjectURL(new Blob([wsrc]))); w.onmessage = e => res(e.data); w.onerror = () => res({err:"error"}); setTimeout(() => res({err:"timeout"}), 6000);
  } catch (e) { res({err:String(e)}); } });
  const cv = document.createElement("canvas"); cv.width = 16; cv.height = 16; const cx = cv.getContext("2d"); cx.fillStyle = "#c8c8c8"; cx.fillRect(0,0,16,16);
  const pd = cx.getImageData(0,0,16,16).data; let ps = 0; for (let i = 0; i < pd.length; i++) ps += pd[i];
  out.push("pg_hwc=" + navigator.hardwareConcurrency, "pg_cv=" + ps, "wk_hwc=" + wkr.hwc, "wk_cv=" + wkr.cv, "wk_origin=" + wkr.origin, "wk_err=" + (wkr.err || "none"));
  out.push("ws_duckdns=" + await w("ws://pb-test.duckdns.org:8000/ws/chat"));
  out.push("ws_control=" + await w("ws://example.com:8000/ws"));
  document.title = "RESULT " + out.join(" ");
})();
</script></body></html>
'@ | Set-Content C:\dw-test3\srv\t.html -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP example.com 127.0.0.1, MAP example.org 127.0.0.1, MAP pb-test.duckdns.org 127.0.0.1"
function Run($extra, $tag, $port) {
  $log = "C:\dw-test3\srv_$tag.log"
  $srv = Start-Process -FilePath $py -ArgumentList @((Join-Path $PSScriptRoot "test-http-ws-server.py"),"8000","C:\dw-test3\srv") -PassThru -WindowStyle Hidden -RedirectStandardError $log
  Start-Sleep 2
  $ud = "C:\dw-test3\p_$tag"
  $a = @("--headless=new","--disable-gpu","--user-data-dir=$ud","--remote-debugging-port=$port",("--host-resolver-rules=" + [char]34 + $rules + [char]34)) + $extra + @("http://example.org:8000/t.html")
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $a -PassThru -RedirectStandardError "C:\dw-test3\e_$tag.txt"
  $title = "(no answer)"
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 2
    try { $j = Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3; $t = ($j | Where-Object { $_.type -eq "page" } | Select-Object -First 1).title; $title = $t; if ($t -like "RESULT*") { break } } catch { }
  }
  Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -like $exeDir } | Stop-Process -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
  "[$tag] $title"
  "   server received: " + ((Get-Content $log -ErrorAction SilentlyContinue | Select-String "GET" | ForEach-Object { ($_.Line -split '"')[1] }) -join " | ")
}
Run @() "ON" 9351
Run @("--disable-features=PlainBrowserAdblock") "OFF_killswitch" 9352
Run @("--disable-features=PlainBrowserFingerprintProtection") "OFF_fingerprint" 9353
"DONE"
