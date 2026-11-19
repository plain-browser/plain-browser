# Shared helpers for recording shots on DW-VIDEO (runs in the interactive session via the PBShot task).
$ErrorActionPreference = 'Continue'
$Log = 'C:\PlainBrowser\shots\shot.log'
function L($m) { "$(Get-Date -Format s) $m" | Out-File $Log -Append -Encoding utf8 }
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(int f, int x, int y, int d, int e);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, int f, int e);
  public struct POINT { public int X; public int Y; }
}
"@
$wsh = New-Object -ComObject WScript.Shell
$Exe = 'C:\PlainBrowser\PlainBrowser_0.1.3-alpha.1_x64\chrome.exe'
$Videos = 'C:\Users\builder\Videos'
function Obs($type, $data = '{}') { $r = & C:\PlainBrowser\obs-ws.ps1 $type $data; L "$type -> $(([string]$r).Substring(0, [Math]::Min(300, ([string]$r).Length)))"; return $r }
function Scene($name) { Obs 'SetCurrentProgramScene' ('{"sceneName":"' + $name + '"}') | Out-Null; Start-Sleep -Milliseconds 400 }
function RecStart { Obs 'StartRecord' | Out-Null; Start-Sleep 2 }
function RecStop($name) {
  Start-Sleep 1; $r = Obs 'StopRecord' | ConvertFrom-Json; Start-Sleep 2
  $src = $r.d.responseData.outputPath -replace '/', '\'; $dst = Join-Path $Videos "$name.mkv"
  Remove-Item $dst -ErrorAction SilentlyContinue
  for ($i = 0; $i -lt 30 -and (Test-Path $src); $i++) { try { Move-Item $src $dst -ErrorAction Stop } catch { Start-Sleep 1 } }
  if (Test-Path $dst) { L "saved $dst" } else { L "ERROR not saved $dst (left $src)" } }
function Browsers { Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } }
function BrowserClose {
  foreach ($b in Browsers) { $b.CloseMainWindow() | Out-Null }
  $i = 0; while ((Browsers) -and $i -lt 15) { Start-Sleep 1; $i++ }
  Start-Sleep 3; Get-Process chrome -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue; Start-Sleep 1 }
function BrowserStart([string[]]$extra = @(), [string]$url = '', [switch]$Fresh) {
  BrowserClose; if ($Fresh) { Remove-Item C:\PlainBrowser\profile-video -Recurse -Force -ErrorAction SilentlyContinue }
  $args2 = @('--no-first-run', '--start-maximized', '--hide-crash-restore-bubble', '--user-data-dir=C:\PlainBrowser\profile-video', '--lang=pl') + $extra; if ($url) { $args2 += $url }
  Start-Process $Exe -ArgumentList $args2; Start-Sleep 7; Focus }
function Front($h) { [W]::keybd_event(0x12, 0, 0, 0); [W]::keybd_event(0x12, 0, 2, 0); [W]::SetForegroundWindow($h) | Out-Null }
function ObsMin { $o = Get-Process obs64 -ErrorAction SilentlyContinue | Select-Object -First 1; if ($o -and $o.MainWindowHandle -ne 0) { [W]::ShowWindow($o.MainWindowHandle, 6) | Out-Null } }
function MinOthers { foreach ($n in 'WindowsTerminal','explorer') { Get-Process $n -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle } | ForEach-Object { [W]::ShowWindow($_.MainWindowHandle, 6) | Out-Null } } }
function AppStart($url) { BrowserClose; Start-Process $Exe -ArgumentList @('--no-first-run', '--start-maximized', '--hide-crash-restore-bubble', '--user-data-dir=C:\PlainBrowser\profile-video', '--lang=pl', "--app=$url"); Start-Sleep 6; Focus }
function Focus { ObsMin; MinOthers; $b = Browsers | Select-Object -First 1; if ($b) { [W]::ShowWindow($b.MainWindowHandle, 3) | Out-Null; Front $b.MainWindowHandle } }
function Buttons { $out = @(); foreach ($b in Browsers) { foreach ($top in $AE::RootElement.FindAll($TS::Children, (New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $b.Id)))) {
  $out += $top.FindAll($TS::Descendants, (New-Object System.Windows.Automation.PropertyCondition($AE::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button))) } }; $out }
function Keys($k) { $wsh.SendKeys($k) }
function TypeSlow($text, $ms = 95) { foreach ($ch in $text.ToCharArray()) { $c = [string]$ch; if ('+^%~(){}[]'.Contains($c)) { $c = '{' + $c + '}' }; $wsh.SendKeys($c); Start-Sleep -Milliseconds $ms } }
function Go($url, $ms = 95) { Focus; Start-Sleep -Milliseconds 300; Keys '^l'; Start-Sleep -Milliseconds 500; TypeSlow $url $ms; Start-Sleep -Milliseconds 500; Keys '{ENTER}' }
function MoveTo($x, $y, $ms = 700) { $p = New-Object W+POINT; [W]::GetCursorPos([ref]$p) | Out-Null; $n = [int]($ms / 15)
  for ($i = 1; $i -le $n; $i++) { $t = $i / $n; $e = $t * $t * (3 - 2 * $t); [W]::SetCursorPos([int]($p.X + ($x - $p.X) * $e), [int]($p.Y + ($y - $p.Y) * $e)) | Out-Null; Start-Sleep -Milliseconds 15 } }
function Click($x, $y) { MoveTo $x $y; Start-Sleep -Milliseconds 250; [W]::mouse_event(2, 0, 0, 0, 0); Start-Sleep -Milliseconds 60; [W]::mouse_event(4, 0, 0, 0, 0) }
$AE = [System.Windows.Automation.AutomationElement]; $TS = [System.Windows.Automation.TreeScope]
function FindLike($pattern, $timeout = 10) {
  for ($i = 0; $i -lt $timeout * 2; $i++) {
    foreach ($b in Get-Process chrome -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 }) {
      foreach ($top in $AE::RootElement.FindAll($TS::Children, (New-Object System.Windows.Automation.PropertyCondition($AE::ProcessIdProperty, $b.Id)))) {
        $hit = $top.FindAll($TS::Descendants, [System.Windows.Automation.Condition]::TrueCondition) | Where-Object { $_.Current.Name -like $pattern } | Select-Object -First 1
        if ($hit) { return $hit } } }
    Start-Sleep -Milliseconds 500 }
  L "not found: $pattern"; return $null }
function ClickEl($el) { if (-not $el) { return }; $r = $el.Current.BoundingRectangle; Click ([int]($r.X + $r.Width / 2)) ([int]($r.Y + $r.Height / 2)) }
function Term {
  Get-Process WindowsTerminal -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 1
  Start-Process wt.exe -ArgumentList '--maximized', '-d', 'C:\repo', 'powershell.exe', '-NoLogo', '-NoExit', '-ExecutionPolicy', 'Bypass', '-File', 'C:\PlainBrowser\shots\termprofile.ps1'
  Start-Sleep 5; $t = Get-Process WindowsTerminal -ErrorAction SilentlyContinue | Select-Object -First 1
  ObsMin; Front $t.MainWindowHandle; Start-Sleep 1
}
function Cmd($text, $wait = 3) { TypeSlow $text 55; Start-Sleep -Milliseconds 400; Keys '{ENTER}'; Start-Sleep $wait }
function Mouse-Park { MoveTo 1900 540 200 }
