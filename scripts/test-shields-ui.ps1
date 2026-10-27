<# Toolbar button and bubble ("shields") test in a normal window (run on the Windows build VM, in the interactive
   desktop session: a scheduled task with -LogonType Interactive, not an SSH session, or UI Automation sees no windows).
   Serves the same test page as scripts\test-shields.ps1 as example.com (four ad hosts, one control, location.search,
   hardwareConcurrency in the title), opens it in a fresh profile and drives the UI with UI Automation:
     1. click "Plain Browser protections": the bubble opens, the toggle is on, the status counts the blocked requests;
     2. flip the toggle: the pref changes while the renderer runs, the tab reloads, the title shows ad_* allowed, the
        tracking parameter kept and the real core count (the ShieldsConfig push, not the start-up switch);
     3. click the button again: the bubble closes; click once more: it reopens showing "off";
     4. flip it back on, Esc closes the bubble, the reloaded title shows the protections again;
     5. the button shows the count badge (blue pixels in the button) while protections are on and none while they are
        off, and "Manage sites" in the bubble opens chrome://settings/privacy.
   The UI language is forced with --lang (default en-US): the checks match English strings. The Polish strings are
   checked by scripts\test-shields-settings.ps1.
   The browser process must survive every open/close (the bubble owns its widget by hand). Screenshots of the whole
   screen go to <Out>\shots, the log to <Out>\bubble.log, the last line is PASS or FAIL.
   Usage: powershell -File scripts\test-shields-ui.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\ui] [-Lang en-US] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\ui",
      [string]$Lang = "en-US")
$ErrorActionPreference = "Continue"
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Mouse {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
  public static void Click(int x, int y) { SetCursorPos(x, y); mouse_event(2, 0, 0, 0, UIntPtr.Zero); mouse_event(4, 0, 0, 0, UIntPtr.Zero); }
}
"@
$log = "$Out\bubble.log"; $fails = 0
# Every error lands in the log with its line; the run continues so the later checks still report.
trap { Add-Content $log ("{0:HH:mm:ss} ERROR line {1}: {2}" -f (Get-Date), $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message); continue }
# Writes to the log only: nothing may leak into the pipeline, or WaitFor's callers get a string instead of $null.
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
function Shot($name) {
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $bmp.Save("$Out\shots\$name.png"); $g.Dispose(); $bmp.Dispose(); Log "shot $name"
}
# Bluish pixels inside the toolbar button (inset 4 px, past any focus ring): the count badge is the only blue on the
# otherwise grey icon (ui::kColorBadgeBackground), so this is about 60 with a badge and 0 without.
function BadgePixels($el) {
  $r = $el.Current.BoundingRectangle; $w = [int]$r.Width - 8; $h = [int]$r.Height - 8
  if ($w -le 0 -or $h -le 0) { return -1 }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen([int]$r.X + 4, [int]$r.Y + 4, 0, 0, $bmp.Size)
  $n = 0
  for ($x = 0; $x -lt $w; $x++) { for ($y = 0; $y -lt $h; $y++) { $c = $bmp.GetPixel($x, $y); if ($c.B -gt $c.R + 40 -and $c.B -gt 150) { $n++ } } }
  $g.Dispose(); $bmp.Dispose(); $n
}
function KillMine { Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
# --- UI Automation helpers -------------------------------------------------------------------------------------
$AE = [System.Windows.Automation.AutomationElement]; $TS = [System.Windows.Automation.TreeScope]
function ByPid($procId) { $AE::RootElement.FindAll($TS::Children, (New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $procId))) }
function Named($root, $name) { $root.FindFirst($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, $name))) }
function ClickCenter($el) { $r = $el.Current.BoundingRectangle; [Mouse]::Click([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)) }
function ToggleState($el) { try { $el.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState.ToString() } catch { "(no TogglePattern)" } }
function Texts($root) { ($root.FindAll($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, [System.Windows.Automation.ControlType]::Text))) | ForEach-Object { $_.Current.Name }) -join " | " }
# The bubble is its own top-level window of the browser process, titled "Plain Browser".
function Bubble($procId) {
  $top = ByPid $procId | Where-Object { $_.Current.Name -eq "Plain Browser" } | Select-Object -First 1
  if ($top) { return $top }
  # Not listed as a top-level window: find the toggle anywhere and climb to the bubble's root ("Plain Browser").
  $el = $AE::RootElement.FindFirst($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, "Protections on for this site")))
  if (-not $el) { return $null }
  $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
  for ($i = 0; $i -lt 10 -and $el.Current.Name -ne "Plain Browser"; $i++) {
    $parent = $walker.GetParent($el)
    if (-not $parent -or $parent -eq $AE::RootElement -or $parent.Current.Name -like "* - Plain Browser") { break }
    $el = $parent
  }
  $el
}
# The toggle shares its name with the label next to it: take the element that supports TogglePattern.
function Toggle($root) {
  $all = @($root.FindAll($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::NameProperty, "Protections on for this site"))))
  $t = $all | Where-Object { @($_.GetSupportedPatterns() | Where-Object { $_.ProgrammaticName -like "Toggle*" }).Count -gt 0 } | Select-Object -First 1
  if (-not $t) { $t = $all | Where-Object { $_.Current.ControlType -ne [System.Windows.Automation.ControlType]::Text } | Select-Object -First 1 }
  $t
}
function WaitFor($what, $seconds, $test) { for ($i = 0; $i -lt $seconds * 2; $i++) { $r = & $test; if ($r) { return $r }; Start-Sleep -Milliseconds 500 }; Log "timeout waiting for $what"; $null }
# /json/list escapes '&' in titles as '&amp;'.
function Title($port) { try { (((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.type -eq "page" } | Select-Object -First 1).title) -replace "&amp;", "&" } catch { "(no title)" } }
# The toggle reloads the tab, whose URL was already cleaned, so a reload cannot show the navigation throttle's live
# answer: open the tracking URL in a new tab (DevTools /json/new), read that tab's title, close the tab.
function TitleOfNewTab($port, $url) {
  $tab = Invoke-RestMethod -Method Put ("http://127.0.0.1:$port/json/new?" + [uri]::EscapeDataString($url)) -TimeoutSec 5
  $title = "(no title)"
  for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500
    $t = (((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.id -eq $tab.id }).title) -replace "&amp;", "&"
    if ($t -like "RESULT*") { $title = $t; break }
  }
  Invoke-RestMethod ("http://127.0.0.1:$port/json/close/" + $tab.id) -TimeoutSec 5 | Out-Null
  Start-Sleep 1
  $title
}
$trackingUrl = "http://example.com:8000/t.html?utm_source=test&keep=1"
# Renderer processes of the browser under test: the live ShieldsConfig path is only proven if the tab's renderer
# survives the toggle's reload (a new process would get the list from --fingerprint-exceptions at start-up instead).
function RendererPids($browserPid) { @(Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe' AND ParentProcessId = $browserPid" | Where-Object { $_.CommandLine -like "*--type=renderer*" } | ForEach-Object { $_.ProcessId }) }
# --- test page and profile ------------------------------------------------------------------------------------
KillMine
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\srv", "$Out\shots", "$Out\profile" | Out-Null
Log "exe: $Exe"
try { Log ("UIA root: '" + $AE::RootElement.Current.Name + "' children=" + $AE::RootElement.FindAll($TS::Children, [System.Windows.Automation.Condition]::TrueCondition).Count) } catch { Log "UIA init failed: $_" }
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
Set-Content "$Out\srv\index.html" "ok"
@'
<html><head><title>pending</title></head><body style="font:16px sans-serif"><p>Plain Browser shields test page.</p><script>
async function t(u){ const c = new AbortController(); const id = setTimeout(() => c.abort(), 6000);
  try { await fetch(u,{mode:"no-cors",signal:c.signal}); clearTimeout(id); return "allowed"; }
  catch(e) { clearTimeout(id); return e.name === "AbortError" ? "TIMEOUT" : "error"; } }
(async () => {
  const tests = { ad_adnxs: "http://ib.adnxs.com:8000/ttj", ad_criteo: "http://static.criteo.net:8000/js/ld/publishertag.js",
    ad_scorecard: "http://sb.scorecardresearch.com:8000/beacon.js", ad_pubmatic: "http://ads.pubmatic.com:8000/AdServer/js/showad.js",
    ctl_index: "/index.html" };
  const out = [];
  for (const k in tests) out.push(k + "=" + await t(tests[k]));
  out.push("search=" + location.search); out.push("hc=" + navigator.hardwareConcurrency);
  document.title = "RESULT " + out.join(" ");
})();
</script></body></html>
'@ | Set-Content "$Out\srv\t.html" -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP example.com 127.0.0.1"
$srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","$Out\srv") -PassThru -WindowStyle Hidden
Start-Sleep 2
$port = 9375
# One renderer for everything (no spare process either): the renderer-survival check and the new-tab checks then
# speak about the process that already runs, i.e. about the list pushed over ShieldsConfig, not a start-up snapshot.
$a = @("--no-first-run","--no-default-browser-check","--disable-gpu","--window-position=0,0","--window-size=1000,720",
       "--renderer-process-limit=1","--lang=$Lang",
       "--user-data-dir=$Out\profile","--remote-debugging-port=$port",("--host-resolver-rules=" + [char]34 + $rules + [char]34),
       "http://example.com:8000/t.html?utm_source=test&keep=1")
$p = Start-Process -FilePath $Exe -ArgumentList $a -PassThru
$win = WaitFor "browser window" 30 { ByPid $p.Id | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and $_.Current.Name -ne "Plain Browser" } | Select-Object -First 1 }
Check ($null -ne $win) "browser window appeared"
# Diagnostics: every top-level element of the browser process, and the chrome.exe processes under test.
ByPid $p.Id | ForEach-Object { Log ("  top-level: " + $_.Current.ControlType.ProgrammaticName + " '" + $_.Current.Name + "' class=" + $_.Current.ClassName) }
Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Log ("  chrome pid " + $_.ProcessId + " parent " + $_.ParentProcessId + " type=" + $(if ($_.CommandLine -match "--type=(\w+)") { $Matches[1] } else { "browser" })) }
Log ("  launched pid " + $p.Id + " exited=" + $p.HasExited)
$t0 = WaitFor "page result" 30 { $t = Title $port; if ($t -like "RESULT*") { $t } }
Log "title before: $t0"
Check ($t0 -like "*ad_adnxs=error*" -and $t0 -like "*search=?keep=1*") "protected page: ads blocked, utm_source stripped"
$r0 = RendererPids $p.Id; Log ("renderers before: " + ($r0 -join ","))
Shot "00-page"
# --- 1. open ------------------------------------------------------------------------------------------------------
# Prefix match: the accessible name may carry the tooltip's ": on for <site>" suffix.
function Buttons($root) { $root.FindAll($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button))) }
$btn = WaitFor "toolbar button" 15 { Buttons $win | Where-Object { $_.Current.Name -like "Plain Browser protections*" } | Select-Object -First 1 }
Check ($null -ne $btn) "toolbar button 'Plain Browser protections' found"
Log ("  buttons in window: " + ((Buttons $win | ForEach-Object { "'" + $_.Current.Name + "'" }) -join " "))
Log ("button: " + $btn.Current.ControlType.ProgrammaticName + " help='" + $btn.Current.HelpText + "'")
ClickCenter $btn; Start-Sleep 1
$bubble = WaitFor "bubble" 5 { Bubble $p.Id }
Check ($null -ne $bubble) "bubble opened on click"
ByPid $p.Id | ForEach-Object { Log ("  top-level now: " + $_.Current.ControlType.ProgrammaticName + " '" + $_.Current.Name + "'") }
Shot "01-open"
if ($bubble) {
  Log ("bubble texts: " + (Texts $bubble))
  $toggle = Toggle $bubble
  Check ($null -ne $toggle) "toggle found"
  Log ("toggle: " + $toggle.Current.ControlType.ProgrammaticName + " state=" + (ToggleState $toggle))
  Check ((ToggleState $toggle) -eq "On") "toggle is On for a protected site"
  Check ((Texts $bubble) -like "*4 requests blocked on this page.*") "status counts 4 blocked requests"
  $bp = BadgePixels $btn; Log "badge pixels with 4 blocked: $bp"
  Check ($bp -gt 20) "the button shows the count badge"
  # --- 2. turn off ---------------------------------------------------------------------------------------------
  ClickCenter $toggle; Start-Sleep 1
  Log ("after toggle, texts: " + (Texts $bubble) + " state=" + (ToggleState $toggle))
  Check ((Texts $bubble) -like "*Protections off for this site. The page is reloading.*") "status says off + reloading"
  $t1 = WaitFor "reloaded page (off)" 30 { $t = Title $port; if ($t -like "RESULT*" -and $t -ne $t0) { $t } }
  Log "title after off: $t1"
  Check ($t1 -like "*ad_adnxs=allowed*ad_pubmatic=allowed*") "off: ads allowed"
  Check ($t1 -like ("*hc=" + $env:NUMBER_OF_PROCESSORS + "*")) "off: real hardwareConcurrency (live ShieldsConfig push)"
  Log ("bubble still open after the toggle's reload: " + ($null -ne (Bubble $p.Id)))
  $tn = TitleOfNewTab $port $trackingUrl; Log "new tab while off: $tn"
  Check ($tn -like "*search=?utm_source=test&keep=1*") "off: a new navigation keeps utm_source (navigation throttle reads the live list)"
  Log ("bubble still open after the new tab: " + ($null -ne (Bubble $p.Id)))
  Start-Sleep 3
  $r1 = RendererPids $p.Id; Log ("renderers after: " + ($r1 -join ","))
  Check (($r0 | Where-Object { $r1 -notcontains $_ }).Count -eq 0 -and $r0.Count -gt 0) "renderer kept across the toggle: the list reached a running process"
  Shot "02-off"
}
Check (-not $p.HasExited) "browser alive after toggle"
# --- 3. close by a click on the button, reopen --------------------------------------------------------------------
# The bubble closes on deactivation, which the tab switch of the new-tab check (and maybe the reload) already caused:
# make sure one is open before checking that a click on the button closes it instead of reopening it.
if ($null -eq (Bubble $p.Id)) {
  Log "no bubble open (deactivated): opening one for the close-by-click check"
  ClickCenter $btn; Start-Sleep 1
  Check ($null -ne (WaitFor "bubble (for the close check)" 5 { Bubble $p.Id })) "bubble opened for the close check"
}
ClickCenter $btn; Start-Sleep 1
Check ($null -eq (Bubble $p.Id)) "a click on the button while the bubble is open closes it (and does not reopen it)"
Shot "03-closed"
Check (-not $p.HasExited) "browser alive after close"
ClickCenter $btn; Start-Sleep 1
$bubble = WaitFor "bubble (reopen)" 5 { Bubble $p.Id }
Check ($null -ne $bubble) "bubble reopened"
Shot "04-reopened"
if ($bubble) {
  $toggle = Toggle $bubble
  Log ("reopened: state=" + (ToggleState $toggle) + " texts: " + (Texts $bubble))
  Check ((ToggleState $toggle) -eq "Off") "toggle is Off for the excepted site"
  Check ((Texts $bubble) -like "*are off for this site.*") "status says protections are off"
  $bp = BadgePixels $btn; Log "badge pixels on the excepted site: $bp"
  Check ($bp -eq 0) "no badge while protections are off"
  # --- 4. turn back on, Esc ------------------------------------------------------------------------------------
  $tBefore = Title $port
  ClickCenter $toggle; Start-Sleep 1
  Check ((Texts $bubble) -like "*Protections on. The page is reloading.*") "status says on + reloading"
  [System.Windows.Forms.SendKeys]::SendWait("{ESC}"); Start-Sleep 1
  Check ($null -eq (Bubble $p.Id)) "Esc closed the bubble"
  $t2 = WaitFor "reloaded page (on)" 30 { $t = Title $port; if ($t -like "RESULT*" -and $t -ne $tBefore) { $t } }
  Log "title after on: $t2"
  Check ($t2 -like "*ad_adnxs=error*") "on again: ads blocked"
  $tn = TitleOfNewTab $port $trackingUrl; Log "new tab while on: $tn"
  Check ($tn -like "*search=?keep=1*") "on again: a new navigation has utm_source stripped"
  # The new tab lands in the process that was created while the site was excepted: the push must have replaced
  # that start-up list, or the site stays excepted there for good.
  Check ($tn -match "hc=(2|4|8)( |$)") "on again: a new document in the running renderer gets fingerprint noise"
  Shot "05-on-again"
}
# --- 5. Manage sites -----------------------------------------------------------------------------------------------
ClickCenter $btn; Start-Sleep 1
$bubble = WaitFor "bubble (manage)" 5 { Bubble $p.Id }
$link = if ($bubble) { Named $bubble "Manage sites" } else { $null }
Check ($null -ne $link) "bubble has the 'Manage sites' link"
if ($link) {
  ClickCenter $link
  $settings = WaitFor "settings tab" 10 { (Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.url -like "chrome://settings/privacy*" } | Select-Object -First 1 }
  Check ($null -ne $settings) "'Manage sites' opened chrome://settings/privacy"
  Check ($null -eq (Bubble $p.Id)) "the bubble closed"
  Start-Sleep 2; Shot "06-settings"
}
Check (-not $p.HasExited) "browser alive at the end"
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; KillMine
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
