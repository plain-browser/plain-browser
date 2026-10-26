<# Authenticode-sign the Plain Browser executables and installer (Windows build VM).
   Needs a code-signing certificate: either in the user's certificate store (-Thumbprint) or as a PFX file
   (-Pfx + PLAINBROWSER_SIGN_PFX_PASSWORD). Without a certificate the script does nothing and says so, so
   build.ps1 can always call it. Timestamping uses DigiCert's RFC 3161 server (any other works with -Timestamp).
   Signs, in this order: inner binaries in the out dir (chrome.exe, chrome_proxy.exe, setup.exe, notification_helper.exe,
   elevation_service.exe, *.dll in the version dir), then the packaged installer and MSIX if present.
   Re-signing is idempotent: already signed files are skipped unless -Force.
   Usage: powershell -File scripts\sign-windows.ps1 [-Out C:\dw-build\ucw\build\src\out\Default] [-Dist C:\dw-build\ucw\build]
                                                     [-Thumbprint <sha1> | -Pfx <path>] [-Force] #>
param(
  [string]$Out = "C:\dw-build\ucw\build\src\out\Default",
  [string]$Dist = "C:\dw-build\ucw\build",
  [string]$Thumbprint = $env:PLAINBROWSER_SIGN_THUMBPRINT,
  [string]$Pfx = $env:PLAINBROWSER_SIGN_PFX,
  [string]$Timestamp = "http://timestamp.digicert.com",
  [switch]$Force
)
$ErrorActionPreference = "Stop"
if (-not $Thumbprint -and -not $Pfx) {
  Write-Host "sign-windows: no certificate configured (PLAINBROWSER_SIGN_THUMBPRINT or PLAINBROWSER_SIGN_PFX); nothing signed."
  exit 0
}
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
  Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signtool) { throw "signtool.exe not found (install the Windows 10/11 SDK signing tools)" }
$auth = if ($Pfx) {
  if (-not $env:PLAINBROWSER_SIGN_PFX_PASSWORD) { throw "PLAINBROWSER_SIGN_PFX_PASSWORD not set" }
  @("/f", $Pfx, "/p", $env:PLAINBROWSER_SIGN_PFX_PASSWORD)
} else { @("/sha1", $Thumbprint) }

$targets = @()
foreach ($n in "chrome.exe","chrome_proxy.exe","setup.exe","notification_helper.exe","elevation_service.exe","chrome_pwa_launcher.exe") {
  $p = Join-Path $Out $n; if (Test-Path $p) { $targets += $p }
}
$verDir = Get-ChildItem $Out -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Select-Object -First 1
if ($verDir) { $targets += (Get-ChildItem $verDir.FullName -Filter *.dll | ForEach-Object FullName) }
$targets += (Get-ChildItem $Dist -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "PlainBrowser_*_installer.exe" -or $_.Name -like "PlainBrowser_*.msix" } | ForEach-Object FullName)

$signed = 0
foreach ($t in $targets) {
  if (-not $Force) {
    & $signtool.FullName verify /pa /q $t *> $null
    if ($LASTEXITCODE -eq 0) { Write-Host "[skip] already signed: $t"; continue }
  }
  & $signtool.FullName sign @auth /fd sha256 /tr $Timestamp /td sha256 /d "Plain Browser" $t
  if ($LASTEXITCODE -ne 0) { throw "signtool failed on $t" }
  $signed++
}
Write-Host "sign-windows: signed $signed of $($targets.Count) files."
