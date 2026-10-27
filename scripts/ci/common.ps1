<# Helpers shared by the GitHub Actions build scripts in scripts\ci (dot-source this file).
   All scripts take -Work (default C:\pb): the ungoogled-chromium-windows checkout lives in <Work>\ucw and the
   Chromium tree in <Work>\ucw\build\src. The path must be the same on every runner of a run, because the
   build tree (args.gn, ninja logs) carries absolute paths and is handed from job to job as an artifact. #>
$ErrorActionPreference = "Stop"

function Get-RepoRoot { Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }

function Get-PinnedTag {
  $m = Select-String -Path (Join-Path (Get-RepoRoot) "versions.toml") -Pattern 'ungoogled_chromium_windows\s*=\s*"([^"]+)"'
  if (-not $m) { throw "versions.toml: ungoogled_chromium_windows not found" }
  $m.Matches[0].Groups[1].Value
}

function Get-ProductVersion {
  $text = Get-Content (Join-Path (Get-RepoRoot) "versions.toml") -Raw
  $m = [regex]::Match($text, '(?ms)^\[product\].*?^version\s*=\s*"([^"]+)"')
  if (-not $m.Success) { throw "versions.toml: [product] version not found" }
  $m.Groups[1].Value
}

function Write-Step([string]$Message) {
  Write-Host ""
  Write-Host "==> $Message  ($(Get-Date -Format 'yyyy-MM-dd HH:mm:ss'))"
}

# Runs a native command and throws when it exits non-zero (native failures are silent in PowerShell otherwise).
function Run([string]$Exe) {
  $argv = $args
  Write-Host "+ $Exe $($argv -join ' ')"
  & $Exe @argv
  if ($LASTEXITCODE -ne 0) { throw "$Exe exited with $LASTEXITCODE" }
}

# Like Run, for robocopy, whose exit codes below 8 mean success.
function RunRobocopy() {
  & robocopy @args /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $($args -join ' ')" }
}

# Imports the Visual Studio x64 developer environment (what build.py does with vcvars64 for every build command).
function Import-VsEnv {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: Visual Studio 2022 missing" }
  $vs = & $vswhere -latest -products * -prerelease -property installationPath
  if (-not $vs) { throw "Visual Studio installation not found" }
  cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] } }
  $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"
  $env:vs2022_install = $vs
}

function Write-GitHubOutput([string]$Name, [string]$Value) {
  if ($env:GITHUB_OUTPUT) { Add-Content -Path $env:GITHUB_OUTPUT -Value "$Name=$Value" }
  Write-Host "output: $Name=$Value"
}

function Write-DiskFree([string]$Drive = "C") {
  $free = [math]::Round((Get-PSDrive $Drive).Free / 1GB)
  Write-Host "free on ${Drive}: $free GB"
}

# Reports whether files the build is known to need are still in the Chromium tree, and how many files V8's test directory
# has (an earlier run lost v8/test/torque/test-torque.tq somewhere between clone and ninja). -Require throws when the
# file ninja asked for is missing, so a broken tree is neither packed nor reused from the cache.
function Write-Probe([string]$Src, [string]$Label, [switch]$Require) {
  $needed = "v8\test\torque\test-torque.tq"
  $missing = $false
  foreach ($rel in $needed, "v8\BUILD.gn", "base\BUILD.gn") {
    $ok = Test-Path (Join-Path $Src $rel)
    if (-not $ok -and $rel -eq $needed) { $missing = $true }
    Write-Host ("probe [{0}] {1}: {2}" -f $Label, $rel, $(if ($ok) { "present" } else { "MISSING" }))
  }
  foreach ($dir in "v8\test", "v8\test\torque") {
    $p = Join-Path $Src $dir
    $n = if (Test-Path $p) { @(Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue).Count } else { "no such directory" }
    Write-Host ("probe [{0}] files under {1}: {2}" -f $Label, $dir, $n)
  }
  if ($Require -and $missing) { throw "$needed is missing from the tree ($Label); see the probe lines above" }
}
