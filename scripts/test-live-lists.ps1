<# Filter list updates against the real mirror, lists.plain-browser.app (run on the Windows build VM once the Worker
   has published; infra/filter-lists). No test switches: the browser uses its compiled-in signing key and server.
   1. A fresh profile, headless, an empty page. The first check runs two minutes after start-up: within five
      minutes "<profile>\PlainBrowser Filter Lists" must hold manifest.json and the three lists.
   2. The stored lists are byte for byte what the mirror serves now: each file's SHA-256 equals the live manifest's
      entry for it (fetched here with the same X-Client header the browser sends).
   3. The engine built from them works: on a local test page, a known EasyList host is blocked and EasyList's
      generic ###ad-top hides #ad-top.
   Log lines "ok"/"FAIL", last line PASS or FAIL, to <Out>\live.log.
   Usage: powershell -File scripts\test-live-lists.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\live] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\live")
$ErrorActionPreference = "Continue"
$log = "$Out\live.log"; $fails = 0
trap { Add-Content $log ("{0:HH:mm:ss} ERROR line {1}: {2}" -f (Get-Date), $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message); continue }
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
function KillMine { Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
KillMine
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\srv", "$Out\profile" | Out-Null
Log "exe: $Exe"
$version = "0.1.2"
$server = "https://lists.plain-browser.app/v1/"
$store = "$Out\profile\PlainBrowser Filter Lists"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
@'
<html><head><title>pending</title></head><body><div id="ad-top">ad</div><script>
async function t(u){ try { await fetch(u, {mode: "no-cors"}); return "allowed"; } catch (e) { return "error"; } }
window.addEventListener("load", () => setTimeout(async () => {
  document.title = "RESULT adTop=" + getComputedStyle(document.getElementById("ad-top")).display +
    " adnxs=" + await t("http://ib.adnxs.com:8000/ttj");
}, 1500));
</script></body></html>
'@ | Set-Content "$Out\srv\c.html" -Encoding utf8
# --- the live manifest -------------------------------------------------------------------------------------------
$live = $null
try {
  $live = (Invoke-WebRequest -UseBasicParsing -Headers @{ "X-Client" = "PlainBrowser/$version" } ($server + "manifest.json")).Content | ConvertFrom-Json
} catch { Log "manifest fetch failed: $_" }
Check ($null -ne $live -and $live.format -eq 1) "the mirror serves a format-1 manifest"
$names = @("easylist", "easyprivacy", "easylistpolish")
foreach ($n in $names) { if ($live) { Log ("live $n " + $live.lists.$n.sha256 + " " + $live.lists.$n.size + " bytes, published " + $live.lists.$n.updated) } }
# --- 1. the browser downloads by itself ----------------------------------------------------------------------------
$port = 9385
$rules = "MAP example.org 127.0.0.1, MAP ib.adnxs.com 127.0.0.1"
$a = @("--headless=new","--disable-gpu","--no-first-run","--user-data-dir=$Out\profile","--remote-debugging-port=$port",
       ("--host-resolver-rules=" + [char]34 + $rules + [char]34), "about:blank")
$b = Start-Process -FilePath $Exe -ArgumentList $a -PassThru -RedirectStandardError "$Out\chrome-err.txt"
$started = Get-Date
$done = $false
for ($i = 0; $i -lt 300 -and -not $done; $i++) {
  Start-Sleep 1
  if (Test-Path "$store\manifest.json") {
    $done = (@($names | Where-Object { Test-Path "$store\$_.txt" })).Count -eq 3
  }
}
Log ("store complete after " + [int]((Get-Date) - $started).TotalSeconds + " s: $done")
Check $done "within five minutes the browser stored manifest.json and the three lists"
# --- 2. stored = served -------------------------------------------------------------------------------------------
foreach ($n in $names) {
  $file = "$store\$n.txt"
  $h = if (Test-Path $file) { (Get-FileHash $file -Algorithm SHA256).Hash.ToLower() } else { "(missing)" }
  Check ($live -and $h -eq $live.lists.$n.sha256) "stored $n matches the live manifest ($h)"
}
# --- 3. the engine built from them blocks and hides -----------------------------------------------------------------
$srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","$Out\srv") -PassThru -WindowStyle Hidden
Start-Sleep 5  # engine swap after the store was written
$tab = Invoke-RestMethod -Method Put ("http://127.0.0.1:$port/json/new?" + [uri]::EscapeDataString("http://example.org:8000/c.html")) -TimeoutSec 5
$title = "(no result)"
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 500
  $t = ((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.id -eq $tab.id }).title
  if ($t -like "RESULT*") { $title = $t; break }
}
Log "page: $title"
Check ($title -like "*adTop=none*") "downloaded EasyList: generic ###ad-top hides #ad-top"
Check ($title -like "*adnxs=error*") "downloaded EasyList: ib.adnxs.com is blocked"
Check (-not $b.HasExited) "browser alive"
Stop-Process -Id $b.Id -ErrorAction SilentlyContinue; Start-Sleep 3; KillMine
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
Get-Content "$Out\chrome-err.txt" -ErrorAction SilentlyContinue | Select-String "filter lists" | Select-Object -First 5 | ForEach-Object { Log ("chrome: " + $_.Line) }
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
