<# Apply the Draftway layer to an already-built ungoogled-chromium-windows tree (build VM).
   Copies src\draftway to <src>\draftway and applies patches\series with the tree's patch.exe.
   Idempotent: a patch that is already applied (reverse dry-run succeeds) is skipped.
   Usage: powershell -File scripts\apply-draftway.ps1 [-Src C:\dw-build\ucw\build\src] [-Repo C:\dw-build\repo] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Repo = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$patch = Join-Path $Src "third_party\git\usr\bin\patch.exe"
if (-not (Test-Path $patch)) { throw "patch.exe not found at $patch" }
if (-not (Test-Path "$Repo\src\draftway")) { throw "Draftway sources not found under $Repo\src\draftway" }

# 1. //draftway sources (mirror; rust target dirs excluded)
robocopy "$Repo\src\draftway" "$Src\draftway" /MIR /XD target vendor /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
Write-Host "copied //draftway"

# 2. patches in series order
$series = Get-Content "$Repo\patches\series" | Where-Object { $_.Trim() -and -not $_.StartsWith("#") }
foreach ($rel in $series) {
  $file = Join-Path "$Repo\patches" $rel
  if (-not (Test-Path $file)) { throw "missing patch $rel" }
  & $patch -d $Src -p1 -R --dry-run --silent -i $file *> $null
  if ($LASTEXITCODE -eq 0) { Write-Host "[skip] $rel (already applied)"; continue }
  & $patch -d $Src -p1 --forward --no-backup-if-mismatch -i $file
  if ($LASTEXITCODE -ne 0) { throw "patch failed: $rel" }
  Write-Host "[ok]   $rel"
}
Write-Host "Draftway layer applied. Next: out\Default\gn.exe gen out\Default, then ninja (see docs/ADBLOCK.md)."
