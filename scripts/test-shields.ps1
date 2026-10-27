<# Per-site exceptions ("shields") end-to-end test for a Plain Browser build (run on the Windows build VM).
   Seeds a profile whose Preferences contain the exception list ["example.com"] (pref plainbrowser.shields.exceptions,
   what the toolbar bubble writes), serves the same test page as example.com and as example.org from a local HTTP
   server (chrome --host-resolver-rules maps the hosts to 127.0.0.1) and loads it with a tracking parameter:
     http://<host>:8000/t.html?utm_source=test&keep=1
   The page fetches four known ad/tracker hosts (EasyList rules) and one control, and reports in its title the fetch
   results, its own location.search and navigator.hardwareConcurrency. The result is read over the remote-debugging port.
   Expected EXCEPTED (example.com): ad_* allowed (reach the server), search keeps utm_source, hc = the machine's real
   core count (no fingerprint noise). Expected PROTECTED (example.org): ad_* error, search is "?keep=1" (utm_source
   stripped by the navigation throttle), hc in {2,4,8}. ctl_* allowed in both.
   Usage: powershell -File scripts\test-shields.ps1 [-Exe <chrome.exe>]   (default: the tree's out\Default\chrome.exe) #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe")
$ErrorActionPreference = "Continue"
$root = "C:\dw-test-shields\headless"
# Kill only the chrome processes started by this script (the exe is shared with other work on the VM).
function KillMine { Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$root*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
KillMine
Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$root\srv" | Out-Null
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
Set-Content "$root\srv\index.html" "ok"
@'
<html><head><title>pending</title></head><body><script>
async function t(u){ const c = new AbortController(); const id = setTimeout(() => c.abort(), 6000);
  try { await fetch(u,{mode:"no-cors",signal:c.signal}); clearTimeout(id); return "allowed"; }
  catch(e) { clearTimeout(id); return e.name === "AbortError" ? "TIMEOUT" : "error"; } }
(async () => {
  const tests = {
    ad_adnxs: "http://ib.adnxs.com:8000/ttj",
    ad_criteo: "http://static.criteo.net:8000/js/ld/publishertag.js",
    ad_scorecard: "http://sb.scorecardresearch.com:8000/beacon.js",
    ad_pubmatic: "http://ads.pubmatic.com:8000/AdServer/js/showad.js",
    ctl_index: "/index.html"
  };
  const out = [];
  for (const k in tests) out.push(k + "=" + await t(tests[k]));
  out.push("search=" + location.search);
  out.push("hc=" + navigator.hardwareConcurrency);
  document.title = "RESULT " + out.join(" ");
})();
</script></body></html>
'@ | Set-Content "$root\srv\t.html" -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP example.com 127.0.0.1, MAP example.org 127.0.0.1"
# One profile for both runs, with example.com excepted. Chrome reads an existing Preferences file as-is.
$ud = "$root\profile"
New-Item -ItemType Directory "$ud\Default" | Out-Null
'{"plainbrowser":{"shields":{"exceptions":["example.com"]}}}' | Set-Content "$ud\Default\Preferences" -Encoding ascii
"real hardwareConcurrency: $env:NUMBER_OF_PROCESSORS"
function Run($host_, $tag, $port) {
  $log = "$root\srv_$tag.log"
  $srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","$root\srv") -PassThru -WindowStyle Hidden -RedirectStandardError $log
  Start-Sleep 2
  $a = @("--headless=new","--disable-gpu","--user-data-dir=$ud","--remote-debugging-port=$port",("--host-resolver-rules=" + [char]34 + $rules + [char]34), "http://$host_`:8000/t.html?utm_source=test&keep=1")
  $p = Start-Process -FilePath $Exe -ArgumentList $a -PassThru -RedirectStandardError "$root\e_$tag.txt"
  $title = "(no answer)"
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 2
    try { $j = Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3; $t = ($j | Where-Object { $_.type -eq "page" } | Select-Object -First 1).title; $title = $t; if ($t -like "RESULT*") { break } } catch { }
  }
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  KillMine
  Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
  "[$tag $host_] $title"
  "   server received: " + ((Get-Content $log -ErrorAction SilentlyContinue | Select-String "GET" | ForEach-Object { ($_.Line -split '"')[1] }) -join " | ")
}
Run "example.com" "EXCEPTED" 9371
Run "example.org" "PROTECTED" 9372
"DONE"
