<# Headless test of tracking-parameter stripping and fingerprint protection (Windows build VM).
   Serves one page from a local HTTP server; chrome resolves site.test and mbank.pl to 127.0.0.1
   (--host-resolver-rules). The page reports location.href and navigator.hardwareConcurrency back to the
   server, so the server log is the result: it shows which URL was actually requested (tracking parameters
   are stripped before the request leaves the browser, so the original URL never reaches the server) and the
   values the page saw. Each run is killed after 8 s; --dump-dom cannot be used because the headless command
   handler gives up when the first navigation is cancelled and re-issued.
   Expected:
     params      GET /fp.html?id=7            hwc=2|4|8 (bucketed)
     compat      GET /fp.html?id=7            hwc=<real>  (mbank.pl is on the compatibility list)
     params-off  GET /fp.html?utm_source=..   (kill switch PlainBrowserTrackingParams)
     fp-off      GET /fp.html?id=7            hwc=<real>  (kill switch PlainBrowserFingerprintProtection)
   Usage: powershell -File scripts\test-privacy.ps1 [-Exe <chrome.exe>] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe")
$ErrorActionPreference = "Continue"
$dir = "C:\dw-test-privacy"
Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$dir\www" | Out-Null
@'
<html><head><title>fp</title></head><body><script>
var c=document.createElement('canvas');c.width=64;c.height=32;var x=c.getContext('2d');x.fillStyle='#f60';x.fillRect(5,5,40,20);x.fillText('Plain',6,20);
var gl=document.createElement('canvas').getContext('webgl');var ren=gl?gl.getParameter(gl.getExtension('WEBGL_debug_renderer_info').UNMASKED_RENDERER_WEBGL):'nogl';
fetch('/loaded?hwc='+navigator.hardwareConcurrency+'&webgl='+encodeURIComponent(ren)+'&native='+(HTMLCanvasElement.prototype.toDataURL.toString().indexOf('native code')>0)+'&href='+encodeURIComponent(location.href));
</script></body></html>
'@ | Set-Content "$dir\www\fp.html"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
$srv = Start-Process $py -ArgumentList "-u -m http.server 8011 --bind 127.0.0.1" -WorkingDirectory "$dir\www" -PassThru -WindowStyle Hidden -RedirectStandardError "$dir\srv.log"
Start-Sleep 2
$cases = @(
  @{ n = "params";     u = "http://site.test:8011/fp.html?utm_source=nl&fbclid=abc&id=7"; x = @() },
  @{ n = "compat";     u = "http://mbank.pl:8011/fp.html?utm_source=nl&id=7";            x = @() },
  @{ n = "params-off"; u = "http://site.test:8011/fp.html?utm_source=nl&id=7";           x = @("--disable-features=PlainBrowserTrackingParams") },
  @{ n = "fp-off";     u = "http://site.test:8011/fp.html?id=7";                         x = @("--disable-features=PlainBrowserFingerprintProtection") }
)
foreach ($c in $cases) {
  Remove-Item "$dir\prof" -Recurse -Force -ErrorAction SilentlyContinue
  $cl = @("--headless=new","--disable-gpu","--no-first-run","--user-data-dir=$dir\prof",'"--host-resolver-rules=MAP site.test 127.0.0.1, MAP mbank.pl 127.0.0.1"') + $c.x + @("`"$($c.u)`"")
  $p = Start-Process $Exe -ArgumentList $cl -PassThru -WindowStyle Hidden -RedirectStandardError "$dir\err-$($c.n).txt"
  Start-Sleep 8
  Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
}
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
Start-Sleep 1
$i = 0
Get-Content "$dir\srv.log" | Where-Object { $_ -match '"GET /(fp|loaded)' } | ForEach-Object {
  $line = $_ -replace '^127\.0\.0\.1 - - \[[^\]]+\] ', ''
  if ($line -match 'GET /fp\.html') { $name = $cases[[math]::Min($i, $cases.Count - 1)].n; $i++; "== $name" }
  "   " + [uri]::UnescapeDataString($line)
}
"DONE"
