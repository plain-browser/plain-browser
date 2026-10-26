<# Restores the build tree saved by pack-tree.ps1 into <Work> (creates <Work>\ucw) and deletes the zip.
   Usage: pwsh -File scripts\ci\unpack-tree.ps1 [-Work C:\pb] [-Zip C:\pb\tree.zip] #>
param([string]$Work = "C:\pb", [string]$Zip = "")
. "$PSScriptRoot\common.ps1"
if (-not $Zip) { $Zip = Join-Path $Work "tree.zip" }
if (-not (Test-Path $Zip)) { throw "tree archive not found: $Zip (artifact of the previous stage missing?)" }
Write-Host "tree.zip: $([math]::Round((Get-Item $Zip).Length / 1GB, 1)) GB"
Write-Step "7z x $Zip"
Run 7z x $Zip "-o$Work" "-y" | Select-Object -Last 5
Remove-Item $Zip -Force
if (-not (Test-Path "$Work\ucw\build\src\BUILD.gn")) { throw "unpacked tree is incomplete: $Work\ucw\build\src\BUILD.gn missing" }
Write-DiskFree
