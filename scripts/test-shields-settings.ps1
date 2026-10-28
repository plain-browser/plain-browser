<# Shields strings and settings test in a normal window, Polish UI (run on the Windows build VM, in the interactive
   desktop session: a scheduled task with -LogonType Interactive, not SSH). Checks patches/60-ui/0008..0009 and the
   badge count:
     1. the toolbar button and its bubble speak Polish (plainbrowser_shields_strings_pl.xtb), and the bubble counts the
        four blocked requests with the Polish "few" plural form ("zablokowano 4 zadania");
     2. "Zarzadzaj witrynami" in the bubble opens chrome://settings/privacy, which has the section "Witryny z wylaczona
        ochrona", empty at first;
     3. Add with "https://www.Example.com/login": the list shows "example.com" (ShieldsService rewrites the entry as
        the site), with a remove button named after it;
     4. remove: the list is empty again; add "bank.pl" and quit: Preferences holds exactly ["bank.pl"].
   Polish text is matched with ASCII wildcards (this file stays ASCII for Windows PowerShell 5.1).
   Screenshots to <Out>\shots, log to <Out>\settings.log, last line PASS or FAIL.
   Usage: powershell -File scripts\test-shields-settings.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\settings] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\settings")
$ErrorActionPreference = "Continue"
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Mouse {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
  public static void Click(int x, int y) { SetCursorPos(x, y); mouse_event(2, 0, 0, 0, UIntPtr.Zero); mouse_event(4, 0, 0, 0, UIntPtr.Zero); }
  public static void Wheel(int x, int y, int delta) { SetCursorPos(x, y); mouse_event(0x0800, 0, 0, unchecked((uint)delta), UIntPtr.Zero); }
}
"@
$log = "$Out\settings.log"; $fails = 0
trap { Add-Content $log ("{0:HH:mm:ss} ERROR line {1}: {2}" -f (Get-Date), $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message); continue }
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
function Shot($name) {
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $bmp.Save("$Out\shots\$name.png"); $g.Dispose(); $bmp.Dispose(); Log "shot $name"
}
function KillMine { Get-CimInstance Win32_Process -Filter "Name = 'chrome.exe'" | Where-Object { $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
$AE = [System.Windows.Automation.AutomationElement]; $TS = [System.Windows.Automation.TreeScope]
function ByPid($procId) { $AE::RootElement.FindAll($TS::Children, (New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $procId))) }
function NamedLike($root, $pattern) { @($root.FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) | Where-Object { $_.Current.Name -like $pattern } }
function OfType($root, $type) { @($root.FindAll($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, $type)))) }
# Scrolls the page until the element is well inside the screen first: a click on an element below the visible part
# lands on whatever is there (the taskbar, at the default window size). Chrome's UIA does not implement
# ScrollItemPattern for page content, so this turns the mouse wheel over the page.
function ClickCenter($el) {
  $area = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
  for ($i = 0; $i -lt 8; $i++) {
    $r = $el.Current.BoundingRectangle
    if (-not $r.IsEmpty -and ($r.Y + $r.Height) -lt ($area.Bottom - 40)) { break }
    [Mouse]::Wheel([int]($r.X + $r.Width / 2), [int]($area.Top + $area.Height / 2), -240); Start-Sleep -Milliseconds 400
  }
  $r = $el.Current.BoundingRectangle
  if ($r.IsEmpty -or ($r.Y + $r.Height / 2) -gt $area.Bottom) { Log ("not clicked, outside the screen: " + $el.Current.Name); return }
  [Mouse]::Click([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2))
}
# Presses a control through UI Automation (Invoke, Toggle or SelectionItem pattern), which does not depend on
# what the screen shows (a stuck system overlay, an undrawn console); falls back to a mouse click when the control
# has none of these patterns or the call fails (the shields button only declares ExpandCollapse, whose Expand
# returns E_FAIL).
function Activate($el) {
  foreach ($p in [System.Windows.Automation.InvokePattern]::Pattern, [System.Windows.Automation.TogglePattern]::Pattern,
                 [System.Windows.Automation.SelectionItemPattern]::Pattern) {
    $o = $null
    if ($el.TryGetCurrentPattern($p, [ref]$o)) {
      try {
        if ($o -is [System.Windows.Automation.InvokePattern]) { $o.Invoke() }
        elseif ($o -is [System.Windows.Automation.TogglePattern]) { $o.Toggle() }
        else { $o.Select() }
        return
      } catch { Log ("UIA call failed, clicking instead: " + $_.Exception.Message) }
    }
  }
  ClickCenter $el
}
function Texts($root) { (OfType $root ([System.Windows.Automation.ControlType]::Text) | ForEach-Object { $_.Current.Name }) -join " | " }
function WaitFor($what, $seconds, $test) { for ($i = 0; $i -lt $seconds * 2; $i++) { $r = & $test; if ($r) { return $r }; Start-Sleep -Milliseconds 500 }; Log "timeout waiting for $what"; $null }
# The bubble is a top-level window titled "Plain Browser", or (as in test-shields-ui.ps1) not listed as one: then
# find its toggle and climb to the bubble's root.
function Bubble($procId) {
  $top = ByPid $procId | Where-Object { $_.Current.Name -eq "Plain Browser" } | Select-Object -First 1
  if ($top) { return $top }
  $el = NamedLike $AE::RootElement "Ochrona w*czona na tej witrynie" | Select-Object -First 1
  if (-not $el) { return $null }
  $walker = [System.Windows.Automation.TreeWalker]::ControlViewWalker
  for ($i = 0; $i -lt 10 -and $el.Current.Name -ne "Plain Browser"; $i++) {
    $parent = $walker.GetParent($el)
    if (-not $parent -or $parent -eq $AE::RootElement -or $parent.Current.Name -like "* - Plain Browser") { break }
    $el = $parent
  }
  $el
}
# --- test page (four ad hosts, as in test-shields.ps1) and profile ------------------------------------------------
KillMine
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\srv", "$Out\shots", "$Out\profile" | Out-Null
Log "exe: $Exe"
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" | Select-Object -First 1).FullName
@'
<html><head><title>pending</title></head><body><script>
(async () => {
  for (const u of ["http://ib.adnxs.com:8000/ttj", "http://static.criteo.net:8000/js/ld/publishertag.js",
                   "http://sb.scorecardresearch.com:8000/beacon.js", "http://ads.pubmatic.com:8000/AdServer/js/showad.js"])
    try { await fetch(u, {mode: "no-cors"}); } catch (e) {}
  document.title = "RESULT done";
})();
</script></body></html>
'@ | Set-Content "$Out\srv\t.html" -Encoding utf8
$rules = "MAP ib.adnxs.com 127.0.0.1, MAP static.criteo.net 127.0.0.1, MAP sb.scorecardresearch.com 127.0.0.1, MAP ads.pubmatic.com 127.0.0.1, MAP example.com 127.0.0.1"
$srv = Start-Process -FilePath $py -ArgumentList @("-m","http.server","8000","--bind","127.0.0.1","--directory","$Out\srv") -PassThru -WindowStyle Hidden
Start-Sleep 2
$port = 9381
$a = @("--no-first-run","--no-default-browser-check","--disable-gpu","--window-position=0,0","--window-size=1000,800",
       "--lang=pl","--user-data-dir=$Out\profile","--remote-debugging-port=$port",
       ("--host-resolver-rules=" + [char]34 + $rules + [char]34), "http://example.com:8000/t.html")
$p = Start-Process -FilePath $Exe -ArgumentList $a -PassThru
$win = WaitFor "browser window" 30 { ByPid $p.Id | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and $_.Current.Name -ne "Plain Browser" } | Select-Object -First 1 }
Check ($null -ne $win) "browser window appeared"
WaitFor "page result" 30 { try { ((Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.title -like "RESULT*" }) } catch { $null } } | Out-Null
Start-Sleep 1
Shot "00-page-badge"
# --- 1. Polish toolbar button and bubble ----------------------------------------------------------------------------
$btn = WaitFor "toolbar button" 15 { OfType $win ([System.Windows.Automation.ControlType]::Button) | Where-Object { $_.Current.Name -like "Ochrona Plain Browser*" } | Select-Object -First 1 }
Check ($null -ne $btn) "toolbar button 'Ochrona Plain Browser' found"
if ($btn) { Log ("button: name='" + $btn.Current.Name + "' help='" + $btn.Current.HelpText + "'") }
Activate $btn; Start-Sleep 1
$bubble = WaitFor "bubble" 5 { Bubble $p.Id }
Check ($null -ne $bubble) "bubble opened"
Shot "01-bubble-pl"
if ($bubble) {
  $texts = Texts $bubble; Log "bubble texts: $texts"
  Check (@(NamedLike $bubble "Ochrona w*czona na tej witrynie").Count -gt 0) "toggle label in Polish"
  Check ($texts -like "*Na tej stronie zablokowano 4 *dania.*") "count in the Polish 'few' form (4 zadania)"
  $link = NamedLike $bubble "Zarz*dzaj witrynami" | Select-Object -First 1
  Check ($null -ne $link) "link 'Zarzadzaj witrynami'"
  # --- 2. the link opens the settings section ----------------------------------------------------------------------
  if ($link) { Activate $link }
}
$settings = WaitFor "settings tab" 10 { (Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3) | Where-Object { $_.url -like "chrome://settings/privacy*" } | Select-Object -First 1 }
Check ($null -ne $settings) "the link opened chrome://settings/privacy"
$title = WaitFor "section title" 10 { NamedLike $win "Witryny z wy*czon* ochron*" | Select-Object -First 1 }
Check ($null -ne $title) "section 'Witryny z wylaczona ochrona' on the privacy page"
Check (@(NamedLike $win "Nie dodano *adnych witryn*").Count -gt 0) "the list is empty"
Shot "02-settings-empty"
# --- 3. add a URL, see the site -----------------------------------------------------------------------------------
function AddSite($text) {
  $add = OfType $win ([System.Windows.Automation.ControlType]::Button) | Where-Object { $_.Current.Name.Trim() -eq "Dodaj" } | Select-Object -First 1   # not the bookmark star ("Dodaj ... do zakladek")
  Check ($null -ne $add) "button 'Dodaj'"
  # The first click right after the page opened can land before the page settled: retry once.
  $dialog = $null
  for ($try = 0; $try -lt 2 -and -not $dialog; $try++) {
    Activate $add
    $dialog = WaitFor "add dialog" 4 { NamedLike $win "Wy*cz ochron* dla witryny" | Select-Object -First 1 }
  }
  Check ($null -ne $dialog) "dialog 'Wylacz ochrone dla witryny'"
  Start-Sleep 1
  [System.Windows.Forms.SendKeys]::SendWait($text); Start-Sleep -Milliseconds 500
  [System.Windows.Forms.SendKeys]::SendWait("{ENTER}"); Start-Sleep 1
}
AddSite "https://www.Example.com/login"
$entry = WaitFor "normalized entry" 10 { OfType $win ([System.Windows.Automation.ControlType]::Text) | Where-Object { $_.Current.Name.Trim() -eq "example.com" } | Select-Object -First 1 }
Check ($null -ne $entry) "the list shows 'example.com' (entry rewritten as the site)"
Check (@(OfType $win ([System.Windows.Automation.ControlType]::Text) | Where-Object { $_.Current.Name -like "*www.*" -or $_.Current.Name -like "*login*" }).Count -eq 0) "no raw 'www.Example.com/login' entry left"
$remove = WaitFor "remove button" 5 { NamedLike $win "W*cz ponownie ochron* dla example.com" | Select-Object -First 1 }
Check ($null -ne $remove) "remove button 'Wlacz ponownie ochrone dla example.com'"
Shot "03-settings-added"
# --- 4. remove, add bank.pl, Preferences ---------------------------------------------------------------------------
if ($remove) { Activate $remove }
Check ($null -ne (WaitFor "empty list" 5 { NamedLike $win "Nie dodano *adnych witryn*" | Select-Object -First 1 })) "removed: the list is empty again"
AddSite "bank.pl"
Check ($null -ne (WaitFor "bank.pl" 10 { OfType $win ([System.Windows.Automation.ControlType]::Text) | Where-Object { $_.Current.Name.Trim() -eq "bank.pl" } | Select-Object -First 1 })) "the list shows 'bank.pl'"
Shot "04-settings-bank"
Check (-not $p.HasExited) "browser alive"
# Preferences is written by ImportantFileWriter about 10 s after a change; the kill below does not flush it.
Start-Sleep 12
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 3; KillMine; Start-Sleep 1
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
$prefs = Get-Content "$Out\profile\Default\Preferences" -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
$list = @(); try { $list = @($prefs.plainbrowser.shields.exceptions) } catch {}
Log ("Preferences plainbrowser.shields.exceptions = [" + ($list -join ", ") + "]")
Check ($list.Count -eq 1 -and $list[0] -eq "bank.pl") "Preferences holds exactly [bank.pl]"
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
