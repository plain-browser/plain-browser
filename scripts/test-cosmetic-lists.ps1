<# Cosmetic filtering, $redirect, scriptlets and filter list updates, end to end, headless (run on the Windows build
   VM). Every page is served by a local server; --host-resolver-rules maps the sites the bundled lists name to it.
   1. With the bundled lists:
      - generic hiding: a page on example.org has <div id="ad-top"> and <div class="a-sponsor"> (EasyList ###ad-top,
        ##.a-sponsor), a control <div class="article-body"> and, 1.5 s after load, a script-added .a-sponsor: all
        but the control display:none;
      - observer: an #ad-top added 35 s after load is hidden too (the isolated-world MutationObserver);
      - scriptlets: on x.com, navigator.globalPrivacyControl reads false (EasyPrivacy +js(set, ...)); on
        ncbi.nlm.nih.gov, the page's own ncbi.sg.ping is replaced (+js(set, ncbi.sg, {}) and
        +js(set, ncbi.sg.ping, noopFunc), on an object the page creates after the scriptlets ran);
      - procedural: on cjleo.com, .alt-widget-title:has-text(advertisement) hides only the matching title; on
        bitdefender.com, body[style="opacity: 0;"]:style(opacity: 1 !important;) makes the body visible.
   2. Update: a local server plays lists.plain-browser.app (--plainbrowser-filter-lists-url, which also makes the
      first check run 3 s after start-up; --plainbrowser-filter-lists-key adds the test key below) with a signed
      manifest and an EasyList holding ||fresh-ads.test^, ##.plainbrowser-fresh-ad and
      ||redirect-test.example^$script,redirect=chartbeat.js (the bundled lists' redirect rules name paths, which the
      test server's :8000 URLs never match).
      2a. A wrong signature: the browser fetches manifest and signature, downloads nothing, stores nothing.
      2b. The right one: it fetches the list, stores it in "<profile>\PlainBrowser Filter Lists", and a page then
          has fresh-ads.test blocked and the div hidden; a script from redirect-test.example runs our chartbeat.js
          stand-in (window.pSUPERFLY exists) and never reaches the server.
   3. Restart with the same profile and no server: the stored list is used at once (fresh-ads.test still blocked).
   4. Updates off in Local State (plainbrowser.adblock.list_updates.enabled = false): the stored lists are deleted,
      no request reaches the server, fresh-ads.test is allowed again (bundled EasyList).
   Log lines "ok"/"FAIL", last line PASS or FAIL, to <Out>\lists.log.
   Usage: powershell -File scripts\test-cosmetic-lists.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\lists] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\lists")
$ErrorActionPreference = "Continue"
$log = "$Out\lists.log"; $fails = 0
trap { Add-Content $log ("{0:HH:mm:ss} ERROR line {1}: {2}" -f (Get-Date), $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message); continue }
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
function KillMine { Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
KillMine
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\srv", "$Out\lists\v1\lists", "$Out\profile" | Out-Null
Log "exe: $Exe"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
# --- pages ---------------------------------------------------------------------------------------------------------
@'
<html><head><title>pending</title></head><body>
<div id="ad-top">ad</div><div class="a-sponsor">ad</div><div class="article-body">text</div>
<div class="plainbrowser-fresh-ad">fresh</div>
<script>
const d = (el) => el ? getComputedStyle(el).display : "missing";
async function t(u){ try { await fetch(u, {mode: "no-cors"}); return "allowed"; } catch (e) { return "error"; } }
window.addEventListener("load", () => {
  setTimeout(() => { const e = document.createElement("div"); e.id = "late"; e.className = "a-sponsor"; e.textContent = "late"; document.body.appendChild(e); }, 1500);
  setTimeout(async () => {
    document.title = "RESULT idAdTop=" + d(document.getElementById("ad-top")) +
      " classSponsor=" + d(document.querySelector(".a-sponsor")) +
      " control=" + d(document.querySelector(".article-body")) +
      " late=" + d(document.getElementById("late")) +
      " freshDiv=" + d(document.querySelector(".plainbrowser-fresh-ad")) +
      " freshHost=" + await t("http://fresh-ads.test:8000/x.js");
  }, 6000);
});
</script></body></html>
'@ | Set-Content "$Out\srv\c.html" -Encoding utf8
Set-Content "$Out\srv\x.js" "1"
@'
<html><head><title>pending</title></head><body><script>
window.addEventListener("load", () => {
  setTimeout(() => { const e = document.createElement("div"); e.id = "ad-top"; e.textContent = "very late"; document.body.appendChild(e); }, 35000);
  setTimeout(() => { const e = document.getElementById("ad-top"); document.title = "RESULT veryLate=" + getComputedStyle(e).display; }, 37000);
});
</script></body></html>
'@ | Set-Content "$Out\srv\late.html" -Encoding utf8
@'
<html><head><title>pending</title><script src="http://redirect-test.example:8000/stub.js"></script></head><body><script>
document.title = "RESULT pSUPERFLY=" + typeof window.pSUPERFLY + " real=" + (window.realStub === 1);
</script></body></html>
'@ | Set-Content "$Out\srv\r.html" -Encoding utf8
Set-Content "$Out\srv\stub.js" "window.realStub = 1;"
@'
<html><head><title>pending</title><script>window.ncbi = { sg: { ping: function () { return "real"; } } };</script></head><body><script>
const ping = window.ncbi && window.ncbi.sg && window.ncbi.sg.ping;
document.title = "RESULT gpc=" + navigator.globalPrivacyControl + " ping=" + (typeof ping === "function" ? ping() : "none");
</script></body></html>
'@ | Set-Content "$Out\srv\s.html" -Encoding utf8
@'
<html><head><title>pending</title></head><body>
<div class="alt-widget-title" id="ad">advertisement</div><div class="alt-widget-title" id="news">News</div>
<script>
window.addEventListener("load", () => setTimeout(() => {
  document.title = "RESULT ad=" + getComputedStyle(document.getElementById("ad")).display + " news=" + getComputedStyle(document.getElementById("news")).display;
}, 1500));
</script></body></html>
'@ | Set-Content "$Out\srv\p.html" -Encoding utf8
@'
<html><head><title>pending</title></head><body style="opacity: 0;"><p>text</p>
<script>
window.addEventListener("load", () => setTimeout(() => { document.title = "RESULT opacity=" + getComputedStyle(document.body).opacity; }, 1500));
</script></body></html>
'@ | Set-Content "$Out\srv\b.html" -Encoding utf8
# --- the update server's content: an EasyList of our own, and its manifest -----------------------------------------
$listText = "[Adblock Plus 2.0]`n! Title: Plain Browser test list`n||fresh-ads.test^`n##.plainbrowser-fresh-ad`n||redirect-test.example^`$script,redirect=chartbeat.js`n"
[IO.File]::WriteAllBytes("$Out\lists\v1\lists\easylist.txt", [Text.Encoding]::UTF8.GetBytes($listText))
$hash = (Get-FileHash "$Out\lists\v1\lists\easylist.txt" -Algorithm SHA256).Hash.ToLower()
$size = (Get-Item "$Out\lists\v1\lists\easylist.txt").Length
# The manifest and its Ed25519 signature by a throwaway test key (the browser accepts the key only with
# --plainbrowser-filter-lists-url). Fixed text: changing the list above needs a new signature
# (infra/filter-lists/scripts/keygen.mjs and Node's crypto.sign).
$manifest = "{`"format`": 1, `"lists`": {`"easylist`": {`"sha256`": `"$hash`", `"size`": $size}}}"
$testKey = "j0mHrvmXO0MrbNgs2ETvb3ZqV76gQPIzXvpLFLmSN7Q="
$goodSig = "v/raJqGpqls7sfmgtD3jCvjM6rLoifOqET19lXyB54FrjaAcLcvxANXcWdK0ay53lc4hGkyVX/u3dv/zggTDDg=="
$badSig = "AAAAJqGpqls7sfmgtD3jCvjM6rLoifOqET19lXyB54FrjaAcLcvxANXcWdK0ay53lc4hGkyVX/u3dv/zggTDDg=="
Check ($hash -eq "69c9931bef8c6f24c08beb0752380749471fece15c4d6ac645fb8c993ae8e0c6") "test list has the hash the signature was made for"
$manifest | Set-Content "$Out\lists\v1\manifest.json" -Encoding ascii -NoNewline
$badSig | Set-Content "$Out\lists\v1\manifest.json.sig" -Encoding ascii -NoNewline
# --- helpers -------------------------------------------------------------------------------------------------------
$rules = "MAP example.org 127.0.0.1, MAP fresh-ads.test 127.0.0.1, MAP redirect-test.example 127.0.0.1, MAP x.com 127.0.0.1, MAP www.ncbi.nlm.nih.gov 127.0.0.1, MAP cjleo.com 127.0.0.1, MAP bitdefender.com 127.0.0.1"
$updateArgs = @("--plainbrowser-filter-lists-url=http://127.0.0.1:8001/v1/", "--plainbrowser-filter-lists-key=$testKey")
$port = 9383
$store = "$Out\profile\PlainBrowser Filter Lists"
$page = "http://example.org:8000/c.html"
function StartServer($dir, $p, $name) { Start-Process -FilePath $py -ArgumentList @("-m","http.server","$p","--bind","127.0.0.1","--directory",$dir) -PassThru -WindowStyle Hidden -RedirectStandardError "$Out\srv-$name.log" }
function StartBrowser([string[]]$extra) {
  $a = @("--headless=new","--disable-gpu","--no-first-run","--user-data-dir=$Out\profile","--remote-debugging-port=$port",
         ("--host-resolver-rules=" + [char]34 + $rules + [char]34)) + $extra + @("about:blank")
  $b = Start-Process -FilePath $Exe -ArgumentList $a -PassThru -RedirectStandardError "$Out\chrome-$(Get-Random).txt"
  for ($i = 0; $i -lt 30; $i++) { Start-Sleep 1; try { Invoke-RestMethod "http://127.0.0.1:$port/json/version" -TimeoutSec 2 | Out-Null; break } catch {} }
  $b
}
function StopBrowser($b) { Stop-Process -Id $b.Id -ErrorAction SilentlyContinue; Start-Sleep 3; KillMine; Start-Sleep 1 }
function PageResult([string]$url = $page, [int]$seconds = 20) {
  $tab = Invoke-RestMethod -Method Put ("http://127.0.0.1:$port/json/new?" + [uri]::EscapeDataString($url)) -TimeoutSec 5
  $title = "(no result)"
  for ($i = 0; $i -lt $seconds * 2; $i++) {
    Start-Sleep -Milliseconds 500
    $t = ((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.id -eq $tab.id }).title
    if ($t -like "RESULT*") { $title = $t; break }
  }
  Invoke-RestMethod ("http://127.0.0.1:$port/json/close/" + $tab.id) -TimeoutSec 5 | Out-Null
  $title
}
function Requests($name) { @(Get-Content "$Out\srv-$name.log" -ErrorAction SilentlyContinue | Select-String '"GET ([^ ]+)' | ForEach-Object { $_.Matches[0].Groups[1].Value }) }
$pages = StartServer "$Out\srv" 8000 "pages"
Start-Sleep 2
# --- 1. generic hiding with the bundled lists ---------------------------------------------------------------------
$b = StartBrowser @()
$r = PageResult; Log "bundled: $r"
Check ($r -like "*idAdTop=none*") "generic id rule (###ad-top) hides #ad-top"
Check ($r -like "*classSponsor=none*") "generic class rule (##.a-sponsor) hides .a-sponsor"
Check ($r -like "*control=block*") "control .article-body stays visible"
Check ($r -like "*late=none*") "an element added after load is hidden by a rescan"
Check ($r -like "*freshDiv=block*" -and $r -like "*freshHost=allowed*") "the test list's rules are not active yet"
$r = PageResult "http://example.org:8000/r.html"; Log "no redirect rule yet: $r"
Check ($r -like "*real=true*") "without the test list, redirect-test.example's script loads"
$r = PageResult "http://x.com:8000/s.html"; Log "scriptlet x.com: $r"
Check ($r -like "*gpc=false*") "scriptlet set: navigator.globalPrivacyControl is false on x.com"
$r = PageResult "http://www.ncbi.nlm.nih.gov:8000/s.html"; Log "scriptlet ncbi: $r"
# EasyPrivacy sets ncbi.sg to {} and ncbi.sg.ping to noopFunc: either way the page's own ping must be gone.
Check ($r -like "RESULT*" -and $r -notlike "*ping=real*") "scriptlets set (chain created later by the page): the page's ncbi.sg.ping is replaced"
$r = PageResult "http://example.org:8000/s.html"; Log "scriptlet elsewhere: $r"
Check ($r -like "*ping=real*") "no scriptlet on a site the rules do not name"
$r = PageResult "http://cjleo.com:8000/p.html"; Log "procedural: $r"
Check ($r -like "*ad=none*" -and $r -like "*news=block*") ":has-text(advertisement) hides only the matching element"
$r = PageResult "http://bitdefender.com:8000/b.html"; Log "style action: $r"
Check ($r -like "*opacity=1*") ":style(opacity: 1 !important;) applied"
$r = PageResult "http://example.org:8000/late.html" 45; Log "observer: $r"
Check ($r -like "*veryLate=none*") "an element added 35 s after load is hidden (MutationObserver)"
StopBrowser $b
# --- 2a. update with a wrong signature: nothing is downloaded -------------------------------------------------------
$lists = StartServer "$Out\lists" 8001 "lists-badsig"
Start-Sleep 2
$b = StartBrowser $updateArgs
Start-Sleep 8
Log ("server requests (bad signature): " + ((Requests "lists-badsig") -join " "))
Check ((Requests "lists-badsig") -contains "/v1/manifest.json.sig") "the browser asked for the signature"
Check (-not ((Requests "lists-badsig") -contains "/v1/lists/easylist.txt")) "bad signature: the list is not downloaded"
Check (-not (Test-Path "$store\manifest.json")) "bad signature: nothing stored"
StopBrowser $b
Stop-Process -Id $lists.Id -Force -ErrorAction SilentlyContinue
# --- 2b. update with the right signature ----------------------------------------------------------------------------
$goodSig | Set-Content "$Out\lists\v1\manifest.json.sig" -Encoding ascii -NoNewline
$lists = StartServer "$Out\lists" 8001 "lists"
Start-Sleep 2
$b = StartBrowser $updateArgs
$stored = $null
for ($i = 0; $i -lt 30 -and -not $stored; $i++) { Start-Sleep 1; if (Test-Path "$store\manifest.json") { $stored = Get-Content "$store\manifest.json" -Raw } }
Log ("server requests: " + ((Requests "lists") -join " "))
Check ((Requests "lists") -contains "/v1/manifest.json") "the browser asked for the manifest"
Check ((Requests "lists") -contains "/v1/lists/easylist.txt") "the browser downloaded the changed list"
Check ($stored -like "*$hash*") "the list is stored with its hash"
Check ((Get-Content "$store\easylist.txt" -Raw -ErrorAction SilentlyContinue) -eq $listText) "the stored list is the served one"
Start-Sleep 3  # engine rebuild on the thread pool
$r = PageResult; Log "updated: $r"
Check ($r -like "*freshHost=error*") "downloaded network rule blocks fresh-ads.test"
Check ($r -like "*freshDiv=none*") "downloaded generic rule hides .plainbrowser-fresh-ad"
$r = PageResult "http://example.org:8000/r.html"; Log "redirect: $r"
Check ($r -like "*pSUPERFLY=object*" -and $r -like "*real=false*") '$redirect=chartbeat.js: our stand-in ran instead of the script' 
StopBrowser $b
Stop-Process -Id $lists.Id -Force -ErrorAction SilentlyContinue
# --- 3. restart without the server: the stored list is used -------------------------------------------------------
$b = StartBrowser $updateArgs
Start-Sleep 4
$r = PageResult; Log "restarted: $r"
Check ($r -like "*freshHost=error*") "after a restart the stored list blocks fresh-ads.test"
StopBrowser $b
# --- 4. updates off -------------------------------------------------------------------------------------------------
& $py -I -c "import json,sys; p=sys.argv[1]; d=json.load(open(p,encoding='utf-8')); d.setdefault('plainbrowser',{}).setdefault('adblock',{}).setdefault('list_updates',{})['enabled']=False; json.dump(d,open(p,'w',encoding='utf-8'))" "$Out\profile\Local State"
$lists = StartServer "$Out\lists" 8001 "lists-off"
Start-Sleep 2
$b = StartBrowser $updateArgs
Start-Sleep 8
Check (-not (Test-Path $store)) "updates off: the stored lists are deleted"
Check ((Requests "lists-off").Count -eq 0) "updates off: no request to the list server"
$r = PageResult; Log "off: $r"
Check ($r -like "*freshHost=allowed*" -and $r -like "*idAdTop=none*") "updates off: bundled lists again (fresh-ads.test allowed, EasyList rules active)"
StopBrowser $b
Stop-Process -Id $lists.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $pages.Id -Force -ErrorAction SilentlyContinue
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
