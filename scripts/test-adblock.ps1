<# End-to-end adblock test for a Plain Browser package (run on the Windows build VM).
   Serves six hostnames from a local HTTP server (chrome --host-resolver-rules maps them to 127.0.0.1, so
   DNS filters such as Pi-hole cannot interfere) and loads a page that fetches one URL from each:
   four known ad/tracker hosts (EasyList rules) and two controls. The result is read from the tab title
   over the remote-debugging port. Run once with the filter on and once with --disable-features=DraftwayAdblock.
   Expected ON: ad_* error, never reach the server; ctl_* allowed. Expected OFF: all six allowed, none hang.
   Hosts are deliberately NOT on Chrome's HSTS preload list (google/wikipedia domains are: they would fail
   with or without a blocker).
   Usage: powershell -File scripts\test-adblock.ps1 [-Zip <package.zip>] #>
param([string]$Zip = "C:\dw-build\ucw\build\PlainBrowser_0.1.0-alpha.1_x64.zip")
$ErrorActionPreference = "Continue"
Get-Process chrome,python -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\dw-test*" -or $_.Name -eq "python" } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item C:\dw-test3 -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory C:\dw-test3\srv | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($Zip, "C:\dw-test3")
$exe = Get-ChildItem C:\dw-test3 -Recurse -Filter chrome.exe | Select-Object -First 1
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
Set-Content C:\dw-test3\srv\index.html "ok"
@'
<html><head><title>pending</title></head><body><script>
async function t(u){ const c = new AbortController(); const id = setTimeout(() => c.abort(), 6000); const t0 = performance.now();
  try { await fetch(u,{mode:"no-cors",signal:c.signal}); clearTimeout(id); return "allowed(" + Math.round(performance.now()-t0) + "ms)"; }
  catch(e) { clearTimeout(id); return e.name === "AbortError" ? "TIMEOUT" : "error"; } }
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
  document.title = "RESULT " + out.join(" ");
})();
</script></body></html>
'@ | Set-Content C:\dw-test3\t.html -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP example.com 127.0.0.1, MAP example.org 127.0.0.1"
function Run($extra, $tag, $port) {
  $log = "C:\dw-test3\srv_$tag.log"
  $srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","C:\dw-test3\srv") -PassThru -WindowStyle Hidden -RedirectStandardError $log
  Start-Sleep 2
  $ud = "C:\dw-test3\p_$tag"
  $a = @("--headless=new","--disable-gpu","--user-data-dir=$ud","--remote-debugging-port=$port",("--host-resolver-rules=" + [char]34 + $rules + [char]34)) + $extra + @("file:///C:/dw-test3/t.html")
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $a -PassThru -RedirectStandardError "C:\dw-test3\e_$tag.txt"
  $title = "(no answer)"
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 2
    try { $j = Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3; $t = ($j | Where-Object { $_.type -eq "page" } | Select-Object -First 1).title; $title = $t; if ($t -like "RESULT*") { break } } catch { }
  }
  Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\dw-test3\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
  "[$tag] $title"
  "   server received: " + ((Get-Content $log -ErrorAction SilentlyContinue | Select-String "GET" | ForEach-Object { ($_.Line -split '"')[1] }) -join " | ")
}
Run @() "ON" 9351
Run @("--disable-features=DraftwayAdblock") "OFF_killswitch" 9352
"DONE"
