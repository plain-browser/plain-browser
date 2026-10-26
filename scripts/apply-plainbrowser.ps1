<# Apply the Plain Browser layer to an already-built ungoogled-chromium-windows tree (build VM).
   Reverses the upstream patches in patches\upstream-revert.list (patch -R, last applied first), copies
   src\plainbrowser to <src>\plainbrowser and applies patches\series with the tree's patch.exe.
   Idempotent: a patch that is already applied (reverse dry-run succeeds) is skipped. A patch whose files are
   edited further after it is applied (cargo rewrites Cargo.toml after 30-adblock/0000) declares in its header
   "Applied-Marker: <file>:<substring>"; it is skipped when that file contains the substring.
   Usage: powershell -File scripts\apply-plainbrowser.ps1 [-Src C:\dw-build\ucw\build\src] [-Repo C:\dw-build\repo] [-Ucw C:\dw-build\ucw] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Repo = (Split-Path -Parent $PSScriptRoot), [string]$Ucw = "C:\dw-build\ucw")
$ErrorActionPreference = "Stop"
$patch = Join-Path $Src "third_party\git\usr\bin\patch.exe"
if (-not (Test-Path $patch)) { throw "patch.exe not found at $patch" }
if (-not (Test-Path "$Repo\src\plainbrowser")) { throw "Plain Browser sources not found under $Repo\src\plainbrowser" }

$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $py) { throw "Python 3 not found (needed for patchfilter.py and apply-branding.py)" }
# 0. upstream patches we do not want, reversed in reverse order of application (ucw after uc)
$reverts = Get-Content "$Repo\patches\upstream-revert.list" | Where-Object { $_.Trim() -and -not $_.StartsWith("#") }
$chromiumVersion = (Get-Content "$Ucw\ungoogled-chromium\chromium_version.txt" -Raw).Trim()
foreach ($line in $reverts) {
  $repoName, $rel = $line.Split(":", 2)
  if ($repoName -eq "hunks") {
    # hunks:uc:<patch>:<regex> -> reverse only the matching hunks (filtered with scripts/patchfilter.py)
    $r, $rest = $rel.Split(":", 2); $prel, $regex = $rest.Split(":", 2)
    $pf = if ($r -eq "uc") { Join-Path "$Ucw\ungoogled-chromium\patches" $prel } else { Join-Path "$Ucw\patches" $prel }
    $filtered = Join-Path $env:TEMP ("pb-hunks-" + ($prel -replace '[\\/]', '_'))
    & $py -I "$Repo\scripts\patchfilter.py" $pf $regex --keep -o $filtered
    if ($LASTEXITCODE) { throw "patchfilter found no hunk of $prel matching $regex" }
    $file = $filtered; $rel = "$prel (hunks: $regex)"
  } elseif ($repoName -eq "prune") {
    $dest = Join-Path $Src $rel
    if (Test-Path $dest) { Write-Host "[skip] $rel (present)"; continue }
    New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
    if (Test-Path "$Src\.git") { git -C $Src checkout -- $rel; if ($LASTEXITCODE) { throw "git checkout failed for $rel" } }
    else {
      $b64 = Invoke-WebRequest -UseBasicParsing "https://chromium.googlesource.com/chromium/src/+/refs/tags/$chromiumVersion/$($rel)?format=TEXT"
      [IO.File]::WriteAllBytes($dest, [Convert]::FromBase64String($b64.Content))
    }
    Write-Host "[ok]   restored pruned $rel"; continue
  }
  if ($repoName -ne "hunks") { $file = if ($repoName -eq "uc") { Join-Path "$Ucw\ungoogled-chromium\patches" $rel } else { Join-Path "$Ucw\patches" $rel } }
  if (-not (Test-Path $file)) { throw "upstream patch to revert not found: $file (renamed upstream? update patches/upstream-revert.list)" }
  # -R --forward reverses the hunks that are applied and skips ("Unreversed patch detected") the ones that are
  # not; patch writes a .rej for every skipped hunk, so rejects alone do not mean failure. Afterwards the patch
  # must apply forward cleanly (dry run): that is the proof the tree is back to the state before it.
  $before = Get-Date
  $out = & $patch -d $Src -p1 -R --forward --no-backup-if-mismatch -i $file 2>&1
  $reversed = @($out | Where-Object { $_ -match "^patching file" }).Count
  Get-ChildItem $Src -Recurse -Filter *.rej -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $before } | Remove-Item -Force
  if ($reversed -eq 0) { Write-Host "[skip] revert $rel (not applied)"; continue }
  & $patch -d $Src -p1 --dry-run --silent -i $file *> $null
  if ($LASTEXITCODE -ne 0) {
    # Expected when the patch depended on another patch reverted just before it (its context is gone with it).
    Write-Host "[warn] $rel: $reversed file(s) reversed, forward dry-run does not match (context from an earlier reverted patch); check git diff on its files"
  } else { Write-Host "[ok]   reverted $rel" }
}
# 1. //plainbrowser sources (mirror; rust target dirs excluded)
robocopy "$Repo\src\plainbrowser" "$Src\plainbrowser" /MIR /XD target vendor /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
Write-Host "copied //plainbrowser"

# 2. patches in series order
$series = Get-Content "$Repo\patches\series" | Where-Object { $_.Trim() -and -not $_.StartsWith("#") }
foreach ($rel in $series) {
  $file = Join-Path "$Repo\patches" $rel
  if (-not (Test-Path $file)) { throw "missing patch $rel" }
  $marker = Select-String -Path $file -Pattern '^Applied-Marker: ([^:]+):(.+)$' | Select-Object -First 1
  if ($marker) {
    $mfile = Join-Path $Src $marker.Matches[0].Groups[1].Value; $mtext = $marker.Matches[0].Groups[2].Value
    if ((Test-Path $mfile) -and ((Get-Content $mfile -Raw).Contains($mtext))) { Write-Host "[skip] $rel (marker present)"; continue }
  }
  & $patch -d $Src -p1 -R --dry-run --silent -i $file *> $null
  if ($LASTEXITCODE -eq 0) { Write-Host "[skip] $rel (already applied)"; continue }
  & $patch -d $Src -p1 --forward --no-backup-if-mismatch -i $file
  if ($LASTEXITCODE -ne 0) { throw "patch failed: $rel" }
  Write-Host "[ok]   $rel"
}
# 3. GN args: Safe Browsing compiled in (ungoogled's flags.gn sets safe_browsing_mode=0; the patches that delete the
#    code were reversed in step 0). Takes effect at the next gn gen.
$argsGn = "$Src\out\Default\args.gn"
if (Test-Path $argsGn) {
  $t = Get-Content $argsGn -Raw
  if ($t -match 'safe_browsing_mode\s*=\s*0') { ($t -replace 'safe_browsing_mode\s*=\s*0', 'safe_browsing_mode=1') | Set-Content $argsGn -NoNewline; Write-Host "args.gn: safe_browsing_mode=1" }
}
# 4. UI strings and their translations (grit ids move with the text; see apply-branding.py)
& $py -I "$Repo\scripts\apply-branding.py" $Src
if ($LASTEXITCODE) { throw "apply-branding.py failed" }
Write-Host "Plain Browser layer applied. Next: out\Default\gn.exe gen out\Default, then ninja (see docs/ADBLOCK.md)."
