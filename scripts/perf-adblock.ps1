<# Latency test for the adblock throttle (run on the Windows build VM).
   Loads a page that fires 300 fetches at 20 local hostnames right after browser start and reports per-request
   latency (first request, p50, p95, max) and total time, with the filter on and off. The first request shows the
   cost of the engine load; later ones the cost of a decision.
   Usage: powershell -File scripts\perf-adblock.ps1 [-Zip <package.zip>] #>
param([string]$Zip = "C:\dw-build\ucw\build\PlainBrowser_0.1.0-alpha.1_x64.zip")
$ErrorActionPreference = "Continue"
Get-Process chrome,python -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\dw-perf*" -or $_.Name -eq "python" } | Stop-Process -Force -ErrorAction SilentlyContinue
Remove-Item C:\dw-perf -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory C:\dw-perf\srv | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory($Zip, "C:\dw-perf")
$exe = Get-ChildItem C:\dw-perf -Recurse -Filter chrome.exe | Select-Object -First 1
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
Set-Content C:\dw-perf\srv\x "ok"
@'
<html><head><title>pending</title></head><body><script>
const hosts = Array.from({length: 20}, (_, i) => "h" + i + ".perf.test");
const paths = ["/x", "/x?a=1", "/static/app.js", "/img/photo.jpg", "/api/data.json", "/css/site.css"];
async function one(u) {
  const c = new AbortController(); const id = setTimeout(() => c.abort(), 15000); const t0 = performance.now();
  try { await fetch(u, {mode: "no-cors", signal: c.signal, cache: "no-store"}); clearTimeout(id); return performance.now() - t0; }
  catch (e) { clearTimeout(id); return -(performance.now() - t0); }
}
(async () => {
  const t0 = performance.now(); const urls = [];
  for (let i = 0; i < 300; i++) urls.push("http://" + hosts[i % 20] + ":8000" + paths[i % 6]);
  const first = await one(urls[0]);
  const rest = await Promise.all(urls.slice(1).map(one));
  const ok = rest.filter(v => v >= 0).sort((a, b) => a - b);
  const err = rest.length - ok.length;
  const q = p => ok.length ? Math.round(ok[Math.min(ok.length - 1, Math.floor(p * ok.length))]) : -1;
  document.title = "RESULT first=" + Math.round(Math.abs(first)) + "ms p50=" + q(0.5) + "ms p95=" + q(0.95) + "ms max=" + q(1) +
    "ms errors=" + err + " total=" + Math.round(performance.now() - t0) + "ms";
})();
</script></body></html>
'@ | Set-Content C:\dw-perf\t.html -Encoding utf8
$rules = (0..19 | ForEach-Object { "MAP h$_.perf.test 127.0.0.1" }) -join ", "
function Run($extra, $tag, $port) {
  $srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","C:\dw-perf\srv") -PassThru -WindowStyle Hidden -RedirectStandardError "C:\dw-perf\srv_$tag.log"
  Start-Sleep 2
  $a = @("--headless=new","--disable-gpu","--user-data-dir=C:\dw-perf\p_$tag","--remote-debugging-port=$port","--enable-logging=stderr","--v=1",("--host-resolver-rules=" + [char]34 + $rules + [char]34)) + $extra + @("file:///C:/dw-perf/t.html")
  $p = Start-Process -FilePath $exe.FullName -ArgumentList $a -PassThru -RedirectStandardError "C:\dw-perf\e_$tag.txt"
  $title = "(no answer)"
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 2
    try { $j = Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3; $t = ($j | Where-Object { $_.type -eq "page" } | Select-Object -First 1).title; $title = $t; if ($t -like "RESULT*") { break } } catch {}
  }
  Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\dw-perf\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
  Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep 1
  "[$tag] $title"
  Get-Content "C:\dw-perf\e_$tag.txt" -ErrorAction SilentlyContinue | Select-String "Draftway adblock engine ready" | ForEach-Object { "   " + $_.Line.Substring($_.Line.IndexOf("Draftway")) }
}
Run @() "ON" 9361
Run @() "ON_again" 9362
Run @("--disable-features=DraftwayAdblock") "OFF" 9363
"DONE"
