<# Packs the build tree for the next stage: a zip of <Work>\ucw (checkout, patches, Chromium tree with out\Default)
   without build\download_cache, which is only needed during preparation. Same 7-Zip settings as upstream's stage
   action (-mx=3, -mtc=on keeps the NTFS timestamps ninja compares). The workflow uploads the zip as an artifact
   with compression level 0, since it is already compressed.
   Usage: pwsh -File scripts\ci\pack-tree.ps1 [-Work C:\pb] [-Out C:\pb\tree.zip] #>
param([string]$Work = "C:\pb", [string]$Out = "")
. "$PSScriptRoot\common.ps1"
if (-not $Out) { $Out = Join-Path $Work "tree.zip" }
if (Test-Path $Out) { Remove-Item $Out -Force }
Write-Step "7z a $Out"
Run 7z a "-tzip" $Out (Join-Path $Work "ucw") "-mx=3" "-mtc=on" "-xr!download_cache"
Write-Host "tree.zip: $([math]::Round((Get-Item $Out).Length / 1GB, 1)) GB"
Write-DiskFree
