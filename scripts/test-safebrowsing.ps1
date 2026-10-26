<# Checks that Safe Browsing is compiled in and talks only to our proxy (Windows build VM).
   Runs the browser headless for a while with a NetLog, then greps the NetLog for Safe Browsing requests:
   every one must go to sb.plain-browser.app with the X-Client header, and none to a Google host.
   The v4 update protocol schedules its first list update 1 to 5 minutes after startup, so the default is 330 s.
   Expected output: at least one "sb.plain-browser.app/v4/threatListUpdates:fetch" URL, "X-Client: PlainBrowser/<ver>",
   "response status of the proxy: HTTP/1.1 200" and "google hosts: 0". A 403 means Google refused one of the
   requested lists (see patches/70-safebrowsing/0003 and 0004).
   Usage: powershell -File scripts\test-safebrowsing.ps1 [-Exe <chrome.exe>] [-Seconds 45] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe", [int]$Seconds = 330)
$ErrorActionPreference = "Continue"
$dir = "C:\dw-test-sb"
Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $dir | Out-Null
$cl = @("--headless=new","--disable-gpu","--no-first-run","--user-data-dir=$dir\prof","--log-net-log=$dir\netlog.json","--net-log-capture-mode=Default","about:blank")
$p = Start-Process $Exe -ArgumentList $cl -PassThru -WindowStyle Hidden -RedirectStandardError "$dir\err.txt"
Start-Sleep $Seconds
Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep 2
$log = Get-Content "$dir\netlog.json" -Raw
$urls = [regex]::Matches($log, '"url":"(https?://[^"]*)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
"== Safe Browsing URLs:"; $urls | Where-Object { $_ -match "sb\.plain-browser\.app|safebrowsing" } | ForEach-Object { "   " + ($_ -replace '\$req=[^&]*', '$req=...') }
"== X-Client header seen: " + ([regex]::Matches($log, 'X-Client: PlainBrowser/[0-9.]+') | ForEach-Object { $_.Value } | Sort-Object -Unique) -join ", "
"== response status of the proxy: " + (([regex]::Matches($log, '"headers":\["(HTTP/[^"]*)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique) -join ", ")
"== google hosts: " + @($urls | Where-Object { $_ -match "google|gstatic|googleapis" }).Count
"== all hosts contacted:"; $urls | ForEach-Object { ([uri]$_).Host } | Sort-Object -Unique | ForEach-Object { "   $_" }
"DONE"
