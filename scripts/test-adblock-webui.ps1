<# chrome://adblock and the global switch in a normal window, English UI (run on the Windows build VM, in the
   interactive desktop session: a scheduled task with -LogonType Interactive, not SSH; a headless screenshot over
   SSH comes out blank for every page, chrome://version too). Checks patches/60-ui/0009 and 0011:
     1. a test page with four ad hosts, then chrome://adblock: title "Ad blocker", blocking On, at least 4 requests
        blocked, the engine running in its own process, the three lists, the update state, "Check now";
     2. chrome://settings/privacy: "Block ads and trackers" (on), "Lists not checked yet" or "Lists last checked",
        the "Ad blocker status" row;
     3. the switch turned off in Settings: chrome://adblock shows blocking Off, and on quit Local State holds
        plainbrowser.adblock.enabled = false.
   Screenshots to <Out>\shots, log to <Out>\webui.log, last line PASS or FAIL.
   Usage: powershell -File scripts\test-adblock-webui.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\webui] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\webui")
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
$log = "$Out\webui.log"; $fails = 0
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
function ClickCenter($el) { $r = $el.Current.BoundingRectangle; [Mouse]::Click([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)) }
function Texts($root) { (OfType $root ([System.Windows.Automation.ControlType]::Text) | ForEach-Object { $_.Current.Name }) -join " | " }
function WaitFor($what, $seconds, $test) { for ($i = 0; $i -lt $seconds * 2; $i++) { $r = & $test; if ($r) { return $r }; Start-Sleep -Milliseconds 500 }; Log "timeout waiting for $what"; $null }
function Tabs { try { Invoke-RestMethod "http://127.0.0.1:$port/json/list" -TimeoutSec 3 } catch { @() } }
# Opens a URL in the current tab through the address bar.
function Go($url) {
  [System.Windows.Forms.SendKeys]::SendWait("^l"); Start-Sleep -Milliseconds 400
  [System.Windows.Forms.SendKeys]::SendWait($url); Start-Sleep -Milliseconds 300
  [System.Windows.Forms.SendKeys]::SendWait("{ENTER}"); Start-Sleep 2
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
$port = 9382
$a = @("--no-first-run","--no-default-browser-check","--disable-gpu","--window-position=0,0","--window-size=1000,900",
       "--lang=en-US","--user-data-dir=$Out\profile","--remote-debugging-port=$port",
       ("--host-resolver-rules=" + [char]34 + $rules + [char]34), "http://example.com:8000/t.html")
$p = Start-Process -FilePath $Exe -ArgumentList $a -PassThru
$win = WaitFor "browser window" 30 { ByPid $p.Id | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window -and $_.Current.Name -ne "Plain Browser" } | Select-Object -First 1 }
Check ($null -ne $win) "browser window appeared"
WaitFor "page result" 30 { Tabs | Where-Object { $_.title -like "RESULT*" } } | Out-Null
# --- 1. chrome://adblock ---------------------------------------------------------------------------------------------
Go "chrome://adblock"
Check ($null -ne (WaitFor "status tab" 10 { Tabs | Where-Object { $_.url -like "chrome://adblock*" -and $_.title -eq "Ad blocker" } })) "chrome://adblock opens, title 'Ad blocker'"
$row = WaitFor "blocking row" 10 { NamedLike $win "Ad and tracker blocking" | Select-Object -First 1 }
Check ($null -ne $row) "row 'Ad and tracker blocking'"
Start-Sleep 3   # one refresh (every 2 s) after the page's first status
$texts = Texts $win; Log "page texts: $texts"
Check ($texts -like "*Ad and tracker blocking | On*") "blocking On"
$blocked = [regex]::Match($texts, "Requests blocked since the browser started \| ([0-9,]+)")
Check ($blocked.Success -and [int]($blocked.Groups[1].Value -replace ",", "") -ge 4) ("at least 4 requests blocked (" + $blocked.Groups[1].Value + ")")
Check ($texts -like "*Running in its own sandboxed process*") "engine running in its own process"
Check ($texts -like "*Process starts this session: 1*") "one engine process start"
foreach ($l in "easylist", "easyprivacy", "easylistpolish", "plainbrowser-removeparam") { Check ($texts -like "*| $l |*" -or $texts -like "*| $l") "list $l shown" }
Check ($texts -like "*Built into this version*" -or $texts -like "*Updated copy from lists.plain-browser.app*") "list sources shown"
Check ($texts -like "*Lists not checked yet*" -or $texts -like "*Lists last checked:*" -or $texts -like "*Daily updates are off*") "update state shown"
Check ($null -ne (OfType $win ([System.Windows.Automation.ControlType]::Button) | Where-Object { $_.Current.Name -eq "Check now" -or $_.Current.Name -like "Checking*" } | Select-Object -First 1)) "button 'Check now'"
Shot "01-adblock-page"
# --- 2. Settings: switch, last check, status link ------------------------------------------------------------------
Go "chrome://settings/privacy"
$switch = WaitFor "global switch" 10 { NamedLike $win "Block ads and trackers*" | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Button -or $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::CheckBox } | Select-Object -First 1 }
Check ($null -ne $switch) "switch 'Block ads and trackers'"
Check (@(NamedLike $win "Lists not checked yet").Count + @(NamedLike $win "Lists last checked:*").Count -gt 0) "'Lists not checked yet' or 'Lists last checked'"
Check (@(NamedLike $win "Ad blocker status*").Count -gt 0) "row 'Ad blocker status'"
Shot "02-settings"
# --- 3. switch off, the page follows, Local State ------------------------------------------------------------------
if ($switch) { $switch.SetFocus(); Start-Sleep -Milliseconds 300; [System.Windows.Forms.SendKeys]::SendWait(" "); Start-Sleep 1 }
Shot "03-settings-off"
Go "chrome://adblock"
$off = WaitFor "blocking Off" 10 { (Texts $win) -like "*Ad and tracker blocking | Off*" }
Check ($off) "chrome://adblock shows blocking Off"
Shot "04-adblock-off"
Check (-not $p.HasExited) "browser alive"
# Local State is written by ImportantFileWriter about 10 s after a change; the kill below does not flush it.
Start-Sleep 12
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 3; KillMine; Start-Sleep 1
Stop-Process -Id $srv.Id -Force -ErrorAction SilentlyContinue
$ls = Get-Content "$Out\profile\Local State" -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
$enabled = $null; try { $enabled = $ls.plainbrowser.adblock.enabled } catch {}
Log ("Local State plainbrowser.adblock.enabled = " + $enabled)
Check ($enabled -eq $false) "Local State holds plainbrowser.adblock.enabled = false"
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
