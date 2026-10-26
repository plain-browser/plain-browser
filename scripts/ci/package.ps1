<# Packages a finished build the way the VM flow does: scripts\package-plainbrowser.py (portable zip and
   mini_installer, named PlainBrowser_<version>_x64*), then scripts\package-msix.py from that zip, then SHA256SUMS.txt
   in the sha256sum format sign-release.yml and SECURITY.md use. Everything lands in <Dist>.
   No Authenticode signing: Plain Browser ships through the Microsoft Store, which signs the MSIX itself. The MSIX
   gets the Store identity when the repository variables MSIX_IDENTITY_NAME, MSIX_PUBLISHER and MSIX_PUBLISHER_DISPLAY
   are set (Partner Center, Product identity); otherwise it is a dev package (identity PlainBrowser.Dev) that only
   installs on a machine you control. Both are unsigned.
   Usage: pwsh -File scripts\ci\package.ps1 [-Work C:\pb] [-Dist C:\pb\dist] #>
param([string]$Work = "C:\pb", [string]$Dist = "")
. "$PSScriptRoot\common.ps1"
$repo = Get-RepoRoot
$ucw = Join-Path $Work "ucw"
if (-not $Dist) { $Dist = Join-Path $Work "dist" }
$version = Get-ProductVersion
if (-not (Test-Path "$ucw\build\src\out\Default\mini_installer.exe")) { throw "mini_installer.exe not built; nothing to package" }
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory $Dist | Out-Null

Write-Step "Portable zip and installer (package-plainbrowser.py)"
Push-Location $ucw
try { Run python -I "$repo\scripts\package-plainbrowser.py" } finally { Pop-Location }
$zip = "$ucw\build\PlainBrowser_${version}_x64.zip"
$installer = "$ucw\build\PlainBrowser_${version}_x64_installer.exe"
foreach ($f in $zip, $installer) { if (-not (Test-Path $f)) { throw "expected output missing: $f" }; Copy-Item $f $Dist }

Write-Step "MSIX (package-msix.py)"
# The package version has four numeric parts and ends in 0 for the Store: 0.1.0-alpha.1 -> 0.1.0.0
$msixVersion = [regex]::Match($version, '^\d+\.\d+\.\d+').Value + ".0"
$msixArgs = @("--zip", $zip, "--out", $Dist, "--version", $msixVersion)
$identity = @(@($env:MSIX_IDENTITY_NAME, $env:MSIX_PUBLISHER, $env:MSIX_PUBLISHER_DISPLAY) | Where-Object { $_ })
if ($identity.Count -eq 3) {
  $msixArgs += @("--identity-name", $env:MSIX_IDENTITY_NAME, "--publisher", $env:MSIX_PUBLISHER, "--publisher-display", $env:MSIX_PUBLISHER_DISPLAY)
  Write-Host "Store identity: $env:MSIX_IDENTITY_NAME"
} elseif ($identity.Count -gt 0) {
  throw "set all three of MSIX_IDENTITY_NAME, MSIX_PUBLISHER, MSIX_PUBLISHER_DISPLAY, or none (dev package)"
} else {
  Write-Host "no Store identity configured: building the dev package (PlainBrowser.Dev)"
}
Run python -I "$repo\scripts\package-msix.py" @msixArgs
Remove-Item "$Dist\msix-stage" -Recurse -Force -ErrorAction SilentlyContinue

Write-Step "SHA256SUMS.txt"
$files = Get-ChildItem $Dist -File | Sort-Object Name
$sums = $files | ForEach-Object { "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower())  $($_.Name)" }
[IO.File]::WriteAllText("$Dist\SHA256SUMS.txt", ($sums -join "`n") + "`n")
Get-Content "$Dist\SHA256SUMS.txt"
$files | ForEach-Object { Write-Host "  $($_.Name)  $([math]::Round($_.Length / 1MB)) MB" }
