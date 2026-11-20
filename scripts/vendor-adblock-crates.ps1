<# Vendor the adblock-rust crates into the ungoogled-chromium-windows tree and generate their GN targets.
   Prerequisite: scripts\apply-plainbrowser.ps1 has applied patches\series (including
   30-adblock\0000-crate-config.patch). Needs network (cargo downloads ~18 crates).
   Order matters and each step below fixed a failure seen on Chromium 156.0.8078.47:
   1. .cargo\config.toml must exist (it makes cargo read crates from vendor\).
   2. gnrt vendor, with DEPOT_TOOLS_WIN_TOOLCHAIN=0 and the VS environment.
      It must not be killed mid-run: it removes .cargo\config.toml while it works.
      Crates whose crates.io pin Chromium relaxes only in its vendor\ copy are taken from vendor\ while it resolves
      (Chromium 158: harfrust); config.toml and their Cargo.lock entries are restored afterwards.
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
# The crate directories the tree has before vendoring, to tell afterwards which ones gnrt added (scripts\check-crates.py).
$vendorDir = "$crates\vendor"
$dirsBefore = Join-Path ([IO.Path]::GetTempPath()) "vendor-dirs.before"
if (Test-Path $vendorDir) { Get-ChildItem $vendorDir -Directory | ForEach-Object { $_.Name } | Set-Content $dirsBefore } else { Set-Content $dirsBefore "" }
# gnrt vendor resolves the graph against crates.io first. Chromium relaxes some dependency pins of crates.io manifests
# with a patch in chromium_crates_io\patches that only its copy in vendor\ carries; against the index the pin conflicts
# (Chromium 158: harfrust 0.14.0 pins icu_properties =2.2.0, Chromium uses 2.4.0, see
# patches\harfrust-v0_14\0001-Relax-icu_properties-version-pin.patch). While gnrt vendor runs, cargo takes these
# crates from vendor\ through [patch.crates-io]; afterwards config.toml is restored and their Cargo.lock entries get
# their registry source back, so gnrt gen sees them as the crates.io crates they are (gnrt gen leaves path crates out). Drop an entry once
# upstream no longer patches the pin.
$resolveFromVendor = [ordered]@{ "harfrust" = "harfrust-v0_14" }
$cargoConfig = "$crates\.cargo\config.toml"
$cargoConfigBefore = [IO.File]::ReadAllText($cargoConfig)
$patched = @($resolveFromVendor.Keys | Where-Object { Test-Path "$crates\vendor\$($resolveFromVendor[$_])\Cargo.toml" })
if ($patched.Count) {
  $lines = $patched | ForEach-Object { "$_ = { path = `"vendor/$($resolveFromVendor[$_])`" }" }
  [IO.File]::WriteAllText($cargoConfig, $cargoConfigBefore + "`n# vendor-adblock-crates.ps1, removed after gnrt vendor`n[patch.crates-io]`n" + ($lines -join "`n") + "`n")
  Write-Host "resolving from vendor\ during gnrt vendor: $($patched -join ', ')"
}
$eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
try {
  & $py tools\crates\run_gnrt.py vendor 2>&1 | ForEach-Object { "$_" }
  $vendorrc = $LASTEXITCODE
} finally {
  $ErrorActionPreference = $eap
  if ($patched.Count) {
    [IO.File]::WriteAllText($cargoConfig, $cargoConfigBefore)
    $lock = [IO.File]::ReadAllText("$crates\Cargo.lock")
    foreach ($name in $patched) {
      $lock = [regex]::Replace($lock, '(?m)^(name = "' + [regex]::Escape($name) + '"\r?\nversion = "[^"]+"\r?\n)(?!source = )', '${1}source = "registry+https://github.com/rust-lang/crates.io-index"' + "`n")
    }
    [IO.File]::WriteAllText("$crates\Cargo.lock", $lock)
  }
}
if ($vendorrc) { throw "gnrt vendor failed ($vendorrc)" }

# The crates added now come from crates.io at whatever version gnrt picked today: what is on disk must be the reviewed
# trees (scripts\adblock-crates.lock).
& $py -I "$PSScriptRoot\check-crates.py" --vendor $vendorDir --before $dirsBefore --pins "$PSScriptRoot\adblock-crates.lock" 2>&1 | ForEach-Object { "$_" }
if ($LASTEXITCODE) { throw "the vendored adblock crates do not match scripts\adblock-crates.lock (see above)" }
Remove-Item $dirsBefore -Force -ErrorAction SilentlyContinue

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
