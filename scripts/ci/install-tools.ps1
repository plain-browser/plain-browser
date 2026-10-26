<# Tools the GitHub-hosted windows-2022 runner lacks for a Plain Browser build.
   The image already ships Visual Studio 2022 Enterprise with the C++ workload, Git, 7-Zip, Python 3.12 (via
   actions/setup-python), the GitHub CLI and Windows SDKs 10.0.22621/26100 (MakeAppx for the MSIX). It lacks the
   Windows SDK that Chromium 156 pins (build/vs_toolchain.py: SDK_VERSION 10.0.28000.0) and the two Python modules
   build.py imports. scripts\install-tools.ps1 is the equivalent for a private VM (winget); this one is non-interactive.
   Usage: pwsh -File scripts\ci\install-tools.ps1 [-Work C:\pb] #>
param([string]$Work = "C:\pb")
. "$PSScriptRoot\common.ps1"
New-Item -ItemType Directory -Force $Work | Out-Null
Write-DiskFree

# 1. Windows SDK 10.0.28000.0: same installer URL and feature set as ungoogled-chromium-windows' own CI
#    (.github/actions/prepare/action.yml at the pinned tag).
Write-Step "Windows SDK 10.0.28000.0"
$kits = "${env:ProgramFiles(x86)}\Windows Kits\10"
if (Test-Path "$kits\Include\10.0.28000.0\um") {
  Write-Host "already installed"
} else {
  $installer = Join-Path $Work "winsdksetup.exe"
  Invoke-WebRequest -Uri "https://download.microsoft.com/download/38ab8f6d-3676-4860-ae84-3361308b9d7f/KIT_BUNDLE_WINDOWSSDK_MEDIACREATION/winsdksetup.exe" -OutFile $installer
  $features = "/features OptionId.DesktopCPPx64 OptionId.DesktopCPPx86 OptionId.DesktopCPPArm64 OptionId.WindowsDesktopDebuggers OptionId.UWPCPP OptionId.SigningTools /q /norestart /ceip off"
  $p = Start-Process -FilePath $installer -ArgumentList $features -PassThru -Wait
  if ($p.ExitCode -ne 0) { throw "winsdksetup.exe failed with exit code $($p.ExitCode)" }
  Remove-Item $installer -Force
}
foreach ($need in "Include\10.0.28000.0\um", "Debuggers\x64\dbghelp.dll") {
  if (-not (Test-Path "$kits\$need")) { throw "Windows SDK 10.0.28000.0 incomplete: missing $need" }
}

# 2. Python modules build.py needs (httplib2 for the downloader, clang bindings for V8's metagen).
Write-Step "Python modules"
Run python -m pip install --quiet "httplib2==0.22.0" "clang==21.1.7"

# 3. The repository scripts find Python under %ProgramFiles%\Python3* or %LOCALAPPDATA%\Programs\Python
#    (where the installer puts it on the VM). actions/setup-python installs into C:\hostedtoolcache instead, so
#    make the interpreter visible under the expected name with a junction; also provide python3.exe, which the
#    upstream build scripts call by that name.
Write-Step "Python aliases"
$pyExe = (Get-Command python -CommandType Application).Source
$pyDir = Split-Path $pyExe
if (-not (Test-Path "$pyDir\python3.exe")) { Copy-Item $pyExe "$pyDir\python3.exe" }
if (-not (Get-ChildItem "$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue)) {
  $m = [regex]::Match((& python -c "import sys; print('%d%d' % sys.version_info[:2])"), '\d+')
  New-Item -ItemType Junction -Path "$env:ProgramFiles\Python$($m.Value)" -Target $pyDir | Out-Null
  Write-Host "junction $env:ProgramFiles\Python$($m.Value) -> $pyDir"
}

# 4. Chromium paths exceed 260 characters; the image lifts the limit for the OS, Git needs its own switch.
Run git config --system core.longpaths true

Write-Step "Verification"
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
Write-Host "VS: $(& $vswhere -latest -products * -property installationPath)"
Write-Host "SDKs: $((Get-ChildItem "$kits\Include" -Directory | Select-Object -ExpandProperty Name) -join ', ')"
Run git --version
Run python --version
Run python3 --version
Run 7z i | Out-Null
$makeappx = Get-ChildItem "$kits\bin\10.*\x64\makeappx.exe" | Select-Object -Last 1
if (-not $makeappx) { throw "makeappx.exe not found (needed by scripts\package-msix.py)" }
Write-Host "MakeAppx: $($makeappx.FullName)"
Write-DiskFree
