<# Full Plain Browser build: ungoogled-chromium-windows at the tag pinned in versions.toml plus our layer
   (patches/, patches/upstream-revert.list, src/plainbrowser, adblock crates, branding), signed and packaged.
   Self-elevates (the upstream build must run as administrator in a VS developer environment).
   Profiles:
     alpha    (default) is_official_build=false, no PGO/LTO, no PCH (PCH breaks with clang modules). Fits 32 GB RAM. Not for releases.
     official upstream flags (ThinLTO + PGO). Needs ~64 GB RAM and many hours.
   Usage: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Profile alpha|official] [-Work C:\dw-build] [-Jobs 12]
   Log:   <Work>\build.log #>
param(
  [ValidateSet("alpha","official")][string]$Profile = "alpha",
  [string]$Work = "C:\dw-build",
  [int]$Jobs = 0
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  $a = @("-NoProfile","-ExecutionPolicy","Bypass","-File","`"$PSCommandPath`"","-Profile",$Profile,"-Work","`"$Work`"","-Jobs",$Jobs)
  Start-Process powershell -Verb RunAs -ArgumentList $a
  Write-Host "Elevated build started (accept UAC). Log: $Work\build.log"; exit 0
}

New-Item -ItemType Directory -Force $Work | Out-Null
Start-Transcript -Path "$Work\build.log" -Append | Out-Null
try {
  $ver = (Select-String -Path "$root\versions.toml" -Pattern 'ungoogled_chromium_windows\s*=\s*"([^"]+)"').Matches[0].Groups[1].Value
  if ($ver -like "TODO*") { throw "Set ungoogled_chromium_windows tag in versions.toml first." }
  Write-Host "Tag: $ver  Profile: $Profile"
  $free = [math]::Round((Get-PSDrive ($Work.Substring(0,1))).Free/1GB); Write-Host "Free disk: $free GB"
  # Measured 2026-10-26: 38 GB sources + ~50 GB out/Default + 2 GB download cache for one tree.
  if ($free -lt 120) { throw "Need >= 120 GB free on the build drive (have $free)." }

  # Python: real python.exe + python3.exe copy first on PATH (upstream calls python3; avoids the Store alias).
  $pyExe = Get-ChildItem "$env:ProgramFiles\Python3*\python.exe","$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
  if (-not $pyExe) { throw "Python 3.11+ not found. Run scripts\install-tools.ps1." }
  $pyDir = $pyExe.DirectoryName
  if (-not (Test-Path "$pyDir\python3.exe")) { Copy-Item $pyExe.FullName "$pyDir\python3.exe" }
  $env:Path = "$pyDir;$pyDir\Scripts;$env:Path;$env:ProgramFiles\7-Zip;$env:ProgramFiles\Git\cmd"

  # Visual Studio developer environment (vcvars64).
  $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
  if (-not $vsPath) { throw "Visual Studio Build Tools not found." }
  cmd /c "`"$vsPath\VC\Auxiliary\Build\vcvars64.bat`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] } }
  $env:vs2022_install = $vsPath

  & python3 -m pip install --quiet "clang==21.1.7"
  if ($LASTEXITCODE) { throw "pip install clang failed" }

  $ucw = "$Work\ucw"
  if (-not (Test-Path $ucw)) { git clone --recurse-submodules https://github.com/ungoogled-software/ungoogled-chromium-windows.git $ucw }
  Push-Location $ucw
  git fetch --tags; git checkout --recurse-submodules $ver
  if ($LASTEXITCODE) { throw "git checkout $ver failed" }

  # Local flag overrides (work copy only; marker keeps this idempotent).
  $flags = "$ucw\flags.windows.gn"
  $content = (Get-Content $flags -Raw) -replace '(?s)\r?\n# PLAINBROWSER-OVERRIDE-BEGIN.*?# PLAINBROWSER-OVERRIDE-END\r?\n',''
  if ($Profile -eq "alpha") {
    $content += "`n# PLAINBROWSER-OVERRIDE-BEGIN`nis_official_build=false`nchrome_pgo_phase=0`nuse_thin_lto=false`nenable_precompiled_headers=false`ngenerate_about_credits=true`n# PLAINBROWSER-OVERRIDE-END`n"
  }
  Set-Content $flags -Value $content -NoNewline

  # Plain Browser layer, part 1: upstream series minus patches/upstream-revert.list, pruning.list without the
  # files we keep, safe_browsing_mode=1, our patches appended to the Windows series (all in the ucw work copy).
  & python3 -I "$root\scripts\prepare.py" $ucw "$ucw\build\src"
  if ($LASTEXITCODE) { throw "prepare.py failed" }

  # gn bootstrap looks for ninja via %NINJA% or PATH; use the copy shipped in the Chromium tree.
  $env:NINJA = "$ucw\build\src\third_party\ninja\ninja.exe"
  # --ci makes build.py resumable: finished steps (clone, patches, gn, bindgen) are skipped on re-run.
  # Its ninja step stops after 3.5 h with exit code 124; we simply resume. On success it runs package.py.
  $b = @("build.py","--ci"); if ($Jobs -gt 0) { $b += @("-j",$Jobs) }
  function Invoke-BuildPy {
    for ($pass = 1; $pass -le 30; $pass++) {
      Write-Host "Running: python3 $($b -join ' ')  pass $pass  ($(Get-Date))"
      & python3 @b
      $script:rc = $LASTEXITCODE
      if ($script:rc -ne 124) { return }
      Write-Host "ninja time slice ended, resuming"
    }
  }
  $src = "$ucw\build\src"
  $layered = Test-Path "$src\plainbrowser\BUILD.gn"
  # Phase 1 (fresh tree only): sources, pruning, patches (ours included), domain substitution, Rust toolchain,
  # args.gn, gn bootstrap. gn gen then fails because //plainbrowser and the vendored adblock crates are not
  # in the tree yet; that is the expected stop.
  if (-not $layered) {
    Invoke-BuildPy
    if ($rc -ne 0) {
      if (-not (Test-Path "$src\out\Default\gn.exe")) { throw "build.py failed before gn bootstrap (exit $rc); see the log above" }
      Write-Host "build.py stopped at gn gen (expected on a fresh tree)"
    }
  }
  # Phase 2 (idempotent): //plainbrowser, branding strings, adblock crates, gn gen.
  & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\scripts\apply-plainbrowser.ps1" -Src $src -Repo $root -Ucw $ucw
  if ($LASTEXITCODE) { throw "apply-plainbrowser.ps1 failed" }
  & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\scripts\vendor-adblock-crates.ps1" -Src $src -Ucw $ucw
  if ($LASTEXITCODE) { throw "vendor-adblock-crates.ps1 failed" }
  $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"
  Push-Location $src
  & "$src\out\Default\gn.exe" gen out\Default --fail-on-unused-args
  $gnrc = $LASTEXITCODE
  Pop-Location
  if ($gnrc) { throw "gn gen failed ($gnrc)" }
  # Phase 3: bindgen, ninja (chrome, chromedriver, mini_installer), upstream package.py.
  Invoke-BuildPy
  if ($rc) { throw "build.py failed (exit $rc). Re-run the script to resume incrementally." }
  # Authenticode: inner binaries must be signed before packaging, the installer after. No-op without a certificate.
  & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\scripts\sign-windows.ps1" -Out "$ucw\build\src\out\Default" -Dist "$ucw\build"
  if ($LASTEXITCODE) { throw "sign-windows.ps1 failed (inner binaries)" }
  if ($env:PLAINBROWSER_SIGN_THUMBPRINT -or $env:PLAINBROWSER_SIGN_PFX) {
    # The installer embeds chrome.7z, packed from the out dir: repack it so it carries the signed binaries.
    & $env:NINJA -C "$ucw\build\src\out\Default" mini_installer
    if ($LASTEXITCODE) { throw "ninja mini_installer (repack after signing) failed" }
  }
  & python3 "$root\scripts\package-plainbrowser.py"
  if ($LASTEXITCODE) { throw "package-plainbrowser.py failed" }
  & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\scripts\sign-windows.ps1" -Out "$ucw\build\src\out\Default" -Dist "$ucw\build"
  if ($LASTEXITCODE) { throw "sign-windows.ps1 failed (installer)" }
  Pop-Location
  Write-Host "BUILD OK $(Get-Date). Artifacts:"; Get-ChildItem $ucw\build -Filter "PlainBrowser_*" -File | ForEach-Object { Write-Host "  $($_.FullName)  $([math]::Round($_.Length/1MB)) MB" }
} catch {
  Write-Host "BUILD FAILED: $($_.Exception.Message)"
} finally {
  Write-Host "END"
  Stop-Transcript | Out-Null
}
