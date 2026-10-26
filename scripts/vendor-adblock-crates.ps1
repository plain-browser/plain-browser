<# Vendor the adblock-rust crates into the ungoogled-chromium-windows tree and generate their GN targets.
   Prerequisite: scripts\apply-plainbrowser.ps1 has applied patches\series (including
   30-adblock\0000-crate-config.patch). Needs network (cargo downloads ~18 crates).
   Order matters and each step below fixed a failure seen on Chromium 156.0.8078.47:
   1. .cargo\config.toml must exist (it makes cargo read crates from vendor\).
   2. gnrt vendor, with DEPOT_TOOLS_WIN_TOOLCHAIN=0 and the VS environment.
      It must not be killed mid-run: it removes .cargo\config.toml while it works.
   3. Strip "checksum" lines from Cargo.lock (Chromium's lock has none; vendored sources have no checksums).
   4. gnrt gen, then post-gnrt-fixups.ps1 (restores windows-rs and its BUILD.gn patch).
   Usage: powershell -File scripts\vendor-adblock-crates.ps1 [-Src C:\dw-build\ucw\build\src] [-Ucw C:\dw-build\ucw] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Ucw = "C:\dw-build\ucw")
$ErrorActionPreference = "Stop"
$crates = "$Src\third_party\rust\chromium_crates_io"
if (-not (Select-String -Path "$crates\Cargo.toml" -Pattern "dependencies\.adblock" -Quiet)) { throw "Run scripts\apply-plainbrowser.ps1 first (crate config patch missing)." }

# 1. cargo source replacement (standard `cargo vendor` config)
New-Item -ItemType Directory -Force "$crates\.cargo" | Out-Null
if (-not (Test-Path "$crates\.cargo\config.toml")) {
  [IO.File]::WriteAllText("$crates\.cargo\config.toml", "[source.crates-io]`nreplace-with = `"vendored-sources`"`n`n[source.vendored-sources]`ndirectory = `"vendor`"`n")
  Write-Host "created .cargo\config.toml"
}

# environment
$env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] } }
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
Set-Location $Src

# crates whose LICENSE comes from a patch must be downloaded fresh so the patch is applied
foreach ($d in "seahash-v4","flatbuffers-v25") { $p = "$crates\vendor\$d"; if ((Test-Path $p) -and -not (Test-Path "$p\LICENSE")) { Remove-Item $p -Recurse -Force } }

# 2. vendor
$eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
& $py tools\crates\run_gnrt.py vendor 2>&1 | ForEach-Object { "$_" }
$vendorrc = $LASTEXITCODE
$ErrorActionPreference = $eap
if ($vendorrc) { throw "gnrt vendor failed ($vendorrc)" }

# 3. no checksums in Chromium's Cargo.lock
$lock = [IO.File]::ReadAllText("$crates\Cargo.lock")
[IO.File]::WriteAllText("$crates\Cargo.lock", [regex]::Replace($lock, "(?m)^checksum = .*\r?\n", ""))

# 4. gen and fixups
# cargo reports progress on stderr; Windows PowerShell 5.1 raises a terminating error for that under "Stop".
$eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
& $py tools\crates\run_gnrt.py gen 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch "gen\.rs:324|Error formatting|Please format" }
$genrc = $LASTEXITCODE
$ErrorActionPreference = $eap
if ($genrc) { throw "gnrt gen failed ($genrc)" }
& "$PSScriptRoot\post-gnrt-fixups.ps1" -Src $Src -Ucw $Ucw
Write-Host "Done. Now regenerate the build files: out\Default\gn.exe gen out\Default, then build."
