<# Install the Windows toolchain needed to build Draftway/Chromium via winget.
   Self-elevates. Idempotent: already installed packages are skipped.
   Usage:  powershell -ExecutionPolicy Bypass -File scripts\install-tools.ps1 [-DefenderExclusion] [-BuildDir C:\dw-build]
   Log:    <BuildDir>\install-tools.log #>
param(
  [string]$BuildDir = "C:\dw-build",
  [switch]$DefenderExclusion   # opt-in: excludes BuildDir from Defender scanning (faster builds, weaker protection there)
)
$ErrorActionPreference = "Stop"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  $argList = @("-NoProfile","-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"","-BuildDir","`"$BuildDir`"")
  if ($DefenderExclusion) { $argList += "-DefenderExclusion" }
  Start-Process powershell -Verb RunAs -ArgumentList $argList
  Write-Host "Elevated copy started (accept the UAC prompt). Log: $BuildDir\install-tools.log"
  exit 0
}

New-Item -ItemType Directory -Force $BuildDir | Out-Null
Start-Transcript -Path "$BuildDir\install-tools.log" -Append | Out-Null
$free = [math]::Round((Get-PSDrive C).Free / 1GB)
Write-Host "C: free space: $free GB"
if ($free -lt 60) { Write-Warning "Under 60 GB free: tools fit (~25 GB) but the Chromium build will not. Free space before building." }

function Install-Pkg([string]$Id, [string]$Override = $null) {
  $listed = winget list -e --id $Id --source winget --accept-source-agreements 2>&1 | Out-String
  if ($listed -match [regex]::Escape($Id)) { Write-Host "[skip] $Id already installed"; return }
  Write-Host "[install] $Id"
  $args = @("install","-e","--id",$Id,"--source","winget","--silent","--accept-package-agreements","--accept-source-agreements","--disable-interactivity")
  if ($Override) { $args += @("--override",$Override) }
  & winget @args
  # -1978335189 = no applicable upgrade / already installed
  if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) { throw "winget failed for $Id (exit $LASTEXITCODE)" }
}

# Order matters: MSVC first (rustup's default host triple needs the linker).
$vsOverride = "--quiet --wait --norestart --nocache " +
  "--add Microsoft.VisualStudio.Workload.VCTools " +
  "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 " +
  "--add Microsoft.VisualStudio.Component.VC.ATLMFC " +
  "--add Microsoft.VisualStudio.Component.Windows11SDK.26100"
Install-Pkg "Microsoft.VisualStudio.2022.BuildTools" $vsOverride
# Chromium pins the Windows SDK (SDK_VERSION in build/vs_toolchain.py): 10.0.28000.0 for Chromium 156.
# The standalone SDK package also brings "Debugging Tools for Windows" (dbghelp.dll), which VS omits.
$sdk = "10.0.28000"
if (-not (Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\$sdk.0\um")) { Install-Pkg "Microsoft.WindowsSDK.$sdk" }
foreach ($need in "Include\$sdk.0\um","Debuggers\x64\dbghelp.dll") {
  if (-not (Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\$need")) { throw "Windows SDK $sdk incomplete: missing $need" }
}
Install-Pkg "Git.Git"
Install-Pkg "Python.Python.3.12"
Install-Pkg "7zip.7zip"
Install-Pkg "Rustlang.Rustup"

# Refresh PATH so freshly installed tools (git, python) resolve in this session.
$env:Path = [Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [Environment]::GetEnvironmentVariable("Path","User")

# Chromium paths exceed 260 chars.
Set-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name LongPathsEnabled -Value 1 -Type DWord
git config --system core.longpaths true 2>$null

if ($DefenderExclusion) {
  Add-MpPreference -ExclusionPath $BuildDir
  Write-Host "Defender exclusion added for $BuildDir"
}

# Verify.
$rustup = Join-Path $env:USERPROFILE ".cargo\bin\rustup.exe"
if (Test-Path $rustup) { & $rustup default stable-x86_64-pc-windows-msvc }
Write-Host "---- verification ----"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath
} else { Write-Warning "vswhere not found: Visual Studio Build Tools missing" }
Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
git --version; python --version; & "$env:ProgramFiles\7-Zip\7z.exe" | Select-Object -First 2
if (Test-Path "$env:USERPROFILE\.cargo\bin\cargo.exe") { & "$env:USERPROFILE\.cargo\bin\cargo.exe" --version }
Write-Host "DONE. Open a NEW 'Developer PowerShell for VS' so PATH changes apply."
Stop-Transcript | Out-Null
