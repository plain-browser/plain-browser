<#  The request log of chrome://adblock is kept per profile (AdBlockService::Log, ShieldsService::ProfileIdFor*).
    One browser process (with windows, not --headless: a headless browser takes only one target; started with
    --disable-gpu it also runs from a non-interactive session such as WMI/SSH) with the Default profile, a second
    profile ("P2") and a private window; chrome://adblock is open in each, then each one loads a page whose requests a different EasyList
    rule blocks. Checks, reading the page's #log over the debugging port:
      1. Default's page lists only Default's blocked request, Profile 2's only its own;
      2. the private window logs nothing (its page stays empty) and its request is in neither of the others;
      3. Clear on one page leaves the other profile's entries;
      4. a profile's entries are gone once its last chrome://adblock page closes (a new page starts empty).
    A second browser start with the same --user-data-dir hands the URL to the running process (new window of the
    named profile), which is how the profiles are opened. chrome:// addresses on the command line are not opened,
    so each window starts on a blank http page and is sent to chrome://adblock over the debugging port.
    Usage: powershell -File scripts\test-adblock-profiles.ps1 [-Zip <package.zip> | -ExePath <chrome.exe>] [-Out C:\dw-test-profiles]
    Log: <Out>\profiles.log, last line PASS or FAIL. #>
param([string]$Zip = "", [string]$ExePath = "", [string]$Out = "C:\dw-test-profiles")
$ErrorActionPreference = "Continue"
$log = "$Out\profiles.log"; $fails = 0
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe' OR Name = 'python.exe'" -ErrorAction SilentlyContinue |
  Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep 1
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\srv", "$Out\ud" | Out-Null
if ($ExePath) { $exe = (Get-Item $ExePath).FullName }
else {
  if (-not $Zip) { $Zip = (Get-ChildItem "C:\dw-build\ucw\build\PlainBrowser_*_x64.zip" | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName }
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [IO.Compression.ZipFile]::ExtractToDirectory($Zip, "$Out\pkg")
  $exe = (Get-ChildItem "$Out\pkg" -Recurse -Filter chrome.exe | Select-Object -First 1).FullName
}
Log "exe: $exe"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $py) { $py = (Get-Command python -ErrorAction SilentlyContinue).Source }
# Three pages, each fetching one URL that EasyList blocks, on a host and path of its own.
$pages = @{ "pa.html" = "http://ib.adnxs.com:8000/from-default.js"; "pb.html" = "http://static.criteo.net:8000/from-profile2.js";
            "pi.html" = "http://sb.scorecardresearch.com:8000/from-private.js" }
foreach ($k in $pages.Keys) {
  "<html><head><title>pending</title></head><body><script>(async()=>{try{await fetch('$($pages[$k])',{mode:'no-cors'})}catch(e){}document.title='RESULT done';})();</script></body></html>" |
    Set-Content "$Out\srv\$k" -Encoding utf8
}
"<html><head><title>blank</title></head><body></body></html>" | Set-Content "$Out\srv\blank.html" -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP example.com 127.0.0.1"
$srv = Start-Process -FilePath $py -ArgumentList @("-m", "http.server", "8000", "--bind", "127.0.0.1", "--directory", "$Out\srv") -PassThru -WindowStyle Hidden
Start-Sleep 2
$port = 9391; $ud = "$Out\ud"
# Windows PowerShell 5 hands the JSON array over as one object: unroll it with foreach before filtering.
function Tabs { try { $j = Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3; @(foreach ($x in $j) { if ($x.type -eq "page") { $x } }) } catch { @() } }
# Starts the browser (the first time) or hands a URL to the running one; returns the targets that appeared.
function Open($extra, $url) {
  $before = @(Tabs | ForEach-Object { $_.id })
  $a = @("--disable-gpu", "--no-first-run", "--user-data-dir=$ud", "--remote-debugging-port=$port",
         ("--host-resolver-rules=" + [char]34 + $rules + [char]34)) + $extra + @($url)
  $p = Start-Process -FilePath $exe -ArgumentList $a -PassThru -RedirectStandardError "$Out\chrome-$([guid]::NewGuid().ToString('N').Substring(0,6)).txt"
  $new = $null
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 500
    $new = @(Tabs | Where-Object { $before -notcontains $_.id -and $_.url -notlike "chrome://omnibox-popup*" } | Select-Object -First 1)
    if ($new.Count -gt 0 -and $new[0]) { break }
  }
  if ($new.Count -gt 0 -and $new[0]) { $new[0] } else { $null }
}
# One DevTools command on a tab over its debugging socket; returns the response's result object.
function Cdp($tab, $method, $params) {
  while ($tab -is [array]) { $tab = $tab[0] }   # a pipeline can hand over a one-element array
  $ws = New-Object System.Net.WebSockets.ClientWebSocket
  $ct = [Threading.CancellationToken]::None
  $ws.ConnectAsync([Uri]$tab.webSocketDebuggerUrl, $ct).Wait()
  $msg = @{ id = 1; method = $method; params = $params } | ConvertTo-Json -Compress -Depth 5
  $bytes = [Text.Encoding]::UTF8.GetBytes($msg)
  $ws.SendAsync([ArraySegment[byte]]$bytes, [Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).Wait()
  $buf = New-Object byte[] 65536; $sb = New-Object Text.StringBuilder
  do { $r = $ws.ReceiveAsync([ArraySegment[byte]]$buf, $ct).Result; [void]$sb.Append([Text.Encoding]::UTF8.GetString($buf, 0, $r.Count)) } until ($r.EndOfMessage)
  $ws.Dispose()
  ($sb.ToString() | ConvertFrom-Json).result
}
function Eval($tab, $expr) { (Cdp $tab "Runtime.evaluate" @{ expression = $expr; returnByValue = $true }).result.value }
# chrome:// URLs on the command line are not opened by a headless browser: open a blank page in the window, then go there.
function OpenInternal($extra, $url) {
  $tab = Open $extra "http://example.com:8000/blank.html"
  Log ("window opened: " + $(if ($tab) { "$($tab.id) $($tab.url)" } else { "none" }))
  if ($null -eq $tab) { return $null }
  [void](Cdp $tab "Page.navigate" @{ url = $url })
  for ($i = 0; $i -lt 20; $i++) { Start-Sleep -Milliseconds 500; $t = Tabs | Where-Object { $_.id -eq $tab.id }; if ($t -and $t.url -like "$url*") { return $t } }
  Log ("did not reach $url, tab is now: " + (Tabs | Where-Object { $_.id -eq $tab.id } | ForEach-Object { $_.url }))
  $null
}
function LogText($tab) { [string](Eval $tab "document.getElementById('log').innerText") }
function Title($id) { (Tabs | Where-Object { $_.id -eq $id } | Select-Object -First 1).title }
function WaitTitle($tab) { for ($i = 0; $i -lt 40; $i++) { if ((Title $tab.id) -like "RESULT*") { return $true }; Start-Sleep -Milliseconds 500 }; $false }

# Every start names its profile: without --profile-directory the browser opens the last profile used, not Default.
# --- the three windows, each with chrome://adblock open BEFORE its page runs (the log records only then) ----------------
$adbA = OpenInternal @("--profile-directory=Default") "chrome://adblock"
$adbB = OpenInternal @("--profile-directory=P2") "chrome://adblock"
$adbP = OpenInternal @("--incognito") "chrome://adblock"
Check ($null -ne $adbA -and $null -ne $adbB -and $null -ne $adbP) ("three chrome://adblock windows (Default, Profile 2, private): " + ($null -ne $adbA) + "/" + ($null -ne $adbB) + "/" + ($null -ne $adbP))
if ($null -eq $adbA -or $null -eq $adbB -or $null -eq $adbP) {
  Log ("targets: " + ((Tabs | ForEach-Object { $_.url }) -join ", "))
  Log "FAIL (setup)"; Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
  exit 1
}
Start-Sleep 3   # the pages register their viewers when they open
$pgA = Open @("--profile-directory=Default") "http://example.com:8000/pa.html"
$pgB = Open @("--profile-directory=P2") "http://example.com:8000/pb.html"
$pgP = Open @("--incognito") "http://example.com:8000/pi.html"
Check ($null -ne $pgA -and $null -ne $pgB -and $null -ne $pgP) "the three test pages opened"
foreach ($t in @($pgA, $pgB, $pgP)) { if ($t) { Check (WaitTitle $t) ("page " + $t.url + " ran") } }
Start-Sleep 5   # the log page refreshes every 2 s

$ta = LogText $adbA; $tb = LogText $adbB; $tp = LogText $adbP
Log "Default log: $($ta -replace '\s+', ' ')"
Log "Profile 2 log: $($tb -replace '\s+', ' ')"
Log "private log: $($tp -replace '\s+', ' ')"
Check ($ta -like "*from-default.js*") "Default's page shows Default's request"
Check ($ta -notlike "*from-profile2.js*" -and $ta -notlike "*from-private.js*") "Default's page shows nothing of Profile 2 or the private window"
Check ($tb -like "*from-profile2.js*") "Profile 2's page shows its request"
Check ($tb -notlike "*from-default.js*" -and $tb -notlike "*from-private.js*") "Profile 2's page shows nothing of Default or the private window"
Check ($tp -notlike "*from-*.js*") "the private window's page is empty (nothing logged for it)"

# --- Clear on one page leaves the other ---------------------------------------------------------------------------
[void](Eval $adbA "document.getElementById('clearLog').click()")
Start-Sleep 4
$ta = LogText $adbA; $tb = LogText $adbB
Check ($ta -notlike "*from-default.js*") "Clear empties Default's log"
Check ($tb -like "*from-profile2.js*") "Profile 2's log is untouched by it"

# --- a profile's entries go when its last page closes -------------------------------------------------------------
[void](Invoke-RestMethod "http://127.0.0.1:$port/json/close/$($adbB.id)" -ErrorAction SilentlyContinue)
Start-Sleep 2
$pgB2 = Open @("--profile-directory=P2") "http://example.com:8000/pb.html?again"
if ($pgB2) { [void](WaitTitle $pgB2) }   # requests made while no page of Profile 2 is open must not be kept
$adbB2 = OpenInternal @("--profile-directory=P2") "chrome://adblock"
Start-Sleep 5
$tb2 = if ($adbB2) { LogText $adbB2 } else { "(no page)" }
Log "Profile 2 log after reopening: $($tb2 -replace '\s+', ' ')"
Check ($null -ne $adbB2 -and $tb2 -notlike "*from-profile2.js*") "after its last page closed, Profile 2's earlier requests are gone"

Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
