<# Apply the Plain Browser layer to an already-built ungoogled-chromium-windows tree (build VM).
   Copies src\plainbrowser to <src>\plainbrowser and applies patches\series with the tree's patch.exe.
   Idempotent: a patch that is already applied (reverse dry-run succeeds) is skipped.
   Usage: powershell -File scripts\apply-plainbrowser.ps1 [-Src C:\dw-build\ucw\build\src] [-Repo C:\dw-build\repo] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Repo = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = "Stop"
$patch = Join-Path $Src "third_party\git\usr\bin\patch.exe"
if (-not (Test-Path $patch)) { throw "patch.exe not found at $patch" }
if (-not (Test-Path "$Repo\src\plainbrowser")) { throw "Plain Browser sources not found under $Repo\src\plainbrowser" }

# 1. //plainbrowser sources (mirror; rust target dirs excluded)
robocopy "$Repo\src\plainbrowser" "$Src\plainbrowser" /MIR /XD target vendor /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
Write-Host "copied //plainbrowser"

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
# 3. UI strings and their translations (grit ids move with the text; see apply-branding.py)
$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $py) { throw "Python 3 not found (needed for apply-branding.py)" }
& $py -I "$Repo\scripts\apply-branding.py" $Src
if ($LASTEXITCODE) { throw "apply-branding.py failed" }
Write-Host "Plain Browser layer applied. Next: out\Default\gn.exe gen out\Default, then ninja (see docs/ADBLOCK.md)."
