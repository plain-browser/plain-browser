<# Safe Browsing settings test in a normal window (run on the Windows build VM, in the interactive desktop session:
   a scheduled task with -LogonType Interactive, not SSH). Checks patches/70-safebrowsing/0005-settings-ui:
     1. Settings > You and Google > Google services has the toggle "Safe Browsing through the Plain Browser proxy"
        (Polish UI: "Bezpieczne przeglądanie przez proxy Plain Browser"), on by default;
     2. flipping it off writes safebrowsing.enabled=false to the profile's Preferences;
     3. Settings > Privacy and security > Security shows no "Enhanced protection" radio, and "No protection" is
        selected after step 2.
   Screenshots to <Out>\shots, log to <Out>\sb-settings.log, last line PASS or FAIL.
   Usage: powershell -File scripts\test-safebrowsing-settings.ps1 [-Exe <chrome.exe>] [-Out C:\dw-test-shields\sb] #>
param([string]$Exe = "C:\dw-build\ucw\build\src\out\Default\chrome.exe",
      [string]$Out = "C:\dw-test-shields\sb")
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
$log = "$Out\sb-settings.log"; $fails = 0
trap { Add-Content $log ("{0:HH:mm:ss} ERROR line {1}: {2}" -f (Get-Date), $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message); continue }
function Log($m) { Add-Content $log ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Check($ok, $what) { if ($ok) { Log "ok   $what" } else { $script:fails++; Log "FAIL $what" } }
function Shot($name) {
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $bmp.Save("$Out\shots\$name.png"); $g.Dispose(); $bmp.Dispose(); Log "shot $name"
}
function KillMine { Get-CimInstance Win32_Process | Where-Object { $_.Name -eq "chrome.exe" -and $_.CommandLine -like "*$Out*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
$AE = [System.Windows.Automation.AutomationElement]; $TS = [System.Windows.Automation.TreeScope]
function ByPid($procId) { $AE::RootElement.FindAll($TS::Children, (New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $procId))) }
function NamedLike($root, $pattern) { @($root.FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) | Where-Object { $_.Current.Name -like $pattern } }
function ClickCenter($el) { $r = $el.Current.BoundingRectangle; [Mouse]::Click([int]($r.X + $r.Width / 2), [int]($r.Y + $r.Height / 2)) }
function ToggleState($el) { try { $el.GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Current.ToggleState.ToString() } catch { "(no TogglePattern)" } }
function Selected($el) { try { $el.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern).Current.IsSelected } catch { "(no SelectionItemPattern)" } }
function WaitFor($what, $seconds, $test) { for ($i = 0; $i -lt $seconds * 2; $i++) { $r = & $test; if ($r) { return $r }; Start-Sleep -Milliseconds 500 }; Log "timeout waiting for $what"; $null }
function OpenTab($port, $url) { $tab = Invoke-RestMethod -Method Put ("http://127.0.0.1:$port/json/new?" + [uri]::EscapeDataString($url)) -TimeoutSec 5; Start-Sleep 4; $tab }
function CloseTab($port, $tab) { try { Invoke-RestMethod ("http://127.0.0.1:$port/json/close/" + $tab.id) -TimeoutSec 5 | Out-Null } catch {}; Start-Sleep 1 }
# ---------------------------------------------------------------------------------------------------------------
KillMine
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory "$Out\shots", "$Out\profile" | Out-Null
Log "exe: $Exe"
$port = 9379
$p = Start-Process -FilePath $Exe -ArgumentList @("--no-first-run","--no-default-browser-check","--disable-gpu","--window-position=0,0","--window-size=1000,720","--user-data-dir=$Out\profile","--remote-debugging-port=$port") -PassThru
$win = WaitFor "browser window" 30 { ByPid $p.Id | Where-Object { $_.Current.ControlType -eq [System.Windows.Automation.ControlType]::Window } | Select-Object -First 1 }
Check ($null -ne $win) "browser window appeared"
# --- 1. the toggle on Google services ------------------------------------------------------------------------------
$tab = OpenTab $port "chrome://settings/googleServices"
$toggleName = "Bezpieczne przegl*danie przez proxy Plain Browser*"   # Polish UI on the VM; English fallback below
$toggle = WaitFor "proxy toggle" 10 { @(NamedLike $win $toggleName) + @(NamedLike $win "Safe Browsing through the Plain Browser proxy*") | Where-Object { (ToggleState $_) -ne "(no TogglePattern)" } | Select-Object -First 1 }
Check ($null -ne $toggle) "toggle 'Safe Browsing through the Plain Browser proxy' found on Google services"
if ($toggle) { Log ("toggle name: '" + $toggle.Current.Name + "' state=" + (ToggleState $toggle)); Check ((ToggleState $toggle) -eq "On") "toggle is On by default" }
Shot "01-google-services"
# --- 2. flip it off ------------------------------------------------------------------------------------------------
if ($toggle) {
  ClickCenter $toggle; Start-Sleep 1
  Check ((ToggleState $toggle) -eq "Off") "toggle is Off after the click"
  Shot "02-toggled-off"
}
CloseTab $port $tab
# --- 3. the Security page ------------------------------------------------------------------------------------------
$tab = OpenTab $port "chrome://settings/security"
$enhanced = @(NamedLike $win "Silniejsza ochrona*") + @(NamedLike $win "Enhanced protection*")
Check (@($enhanced).Count -eq 0) "no 'Enhanced protection' radio on the Security page"
$candidates = @(NamedLike $win "Brak ochrony*") + @(NamedLike $win "No protection*")
foreach ($c in $candidates) { Log ("  candidate: " + $c.Current.ControlType.ProgrammaticName + " '" + $c.Current.Name + "' patterns=" + (($c.GetSupportedPatterns() | ForEach-Object { $_.ProgrammaticName }) -join ",") + " selected=" + (Selected $c) + " toggle=" + (ToggleState $c)) }
# cr-radio-button exposes the radio itself as a RadioButton with SelectionItemPattern; take whichever element answers.
# String comparison on purpose: Selected() returns a bool, and `$true -ne "<string>"` coerces the string to $true.
$none = $candidates | Where-Object { "$(Selected $_)" -ne "(no SelectionItemPattern)" -or (ToggleState $_) -ne "(no TogglePattern)" } | Select-Object -First 1
Check ($null -ne $none) "'No protection' radio found"
# The radio group reflects the pref a moment after the page renders: wait for the selection, do not sample once.
$selectedNone = WaitFor "'No protection' selected" 8 { @(NamedLike $win "Brak ochrony*") + @(NamedLike $win "No protection*") | Where-Object { "$(Selected $_)" -eq "True" } | Select-Object -First 1 }
if ($selectedNone) { Log ("no-protection radio: '" + $selectedNone.Current.Name + "' selected=True") }
Check ($null -ne $selectedNone) "'No protection' is selected after the toggle"
Shot "03-security"
CloseTab $port $tab
# --- Preferences on disk --------------------------------------------------------------------------------------------
Stop-Process -Id $p.Id -ErrorAction SilentlyContinue; Start-Sleep 3; KillMine; Start-Sleep 1
$prefs = Get-Content "$Out\profile\Default\Preferences" -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
$enabled = $null; try { $enabled = $prefs.safebrowsing.enabled } catch {}
Log ("Preferences safebrowsing.enabled = " + $enabled + "  enhanced = " + $(try { $prefs.safebrowsing.enhanced } catch { "(absent)" }))
Check ($enabled -eq $false) "safebrowsing.enabled is false in Preferences"
Log $(if ($fails -eq 0) { "PASS" } else { "FAIL ($fails checks)" })
