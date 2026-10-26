<# Apply the Plain Browser layer to an already-built ungoogled-chromium-windows tree (build VM).
   Reverses the upstream patches in patches\upstream-revert.list (patch -R, last applied first), copies
   src\plainbrowser to <src>\plainbrowser and applies patches\series with the tree's patch.exe.
   Idempotent: a patch that is already applied (reverse dry-run succeeds) is skipped. A patch whose files are
   edited further after it is applied (cargo rewrites Cargo.toml after 30-adblock/0000) declares in its header
   "Applied-Marker: <file>:<substring>"; it is skipped when that file contains the substring.
   -FromPrepare: the tree was built from a fresh clone by build.ps1, where prepare.py already put our series and the
   upstream-revert.list into build.py's patch run. Then the revert step (0) and the series (2) are skipped: those
   patches are applied, and the "already applied" check cannot tell (a later patch can change the same lines).
   Usage: powershell -File scripts\apply-plainbrowser.ps1 [-Src C:\dw-build\ucw\build\src] [-Repo C:\dw-build\repo] [-Ucw C:\dw-build\ucw] [-FromPrepare] #>
param([string]$Src = "C:\dw-build\ucw\build\src", [string]$Repo = (Split-Path -Parent $PSScriptRoot), [string]$Ucw = "C:\dw-build\ucw", [switch]$FromPrepare)
$ErrorActionPreference = "Stop"
$patch = Join-Path $Src "third_party\git\usr\bin\patch.exe"
if (-not (Test-Path $patch)) { throw "patch.exe not found at $patch" }
if (-not (Test-Path "$Repo\src\plainbrowser")) { throw "Plain Browser sources not found under $Repo\src\plainbrowser" }

$py = (Get-ChildItem "$env:LOCALAPPDATA\Programs\Python\Python3*\python.exe","$env:ProgramFiles\Python3*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $py) { throw "Python 3 not found (needed for patchfilter.py and apply-branding.py)" }
# 0. upstream patches we do not want, reversed in reverse order of application (ucw after uc)
$reverts = Get-Content "$Repo\patches\upstream-revert.list" | Where-Object { $_.Trim() -and -not $_.StartsWith("#") }
if ($FromPrepare) { $reverts = @(); Write-Host "[skip] upstream reverts and patch series (applied by build.py via prepare.py)" }
$chromiumVersion = (Get-Content "$Ucw\ungoogled-chromium\chromium_version.txt" -Raw).Trim()
foreach ($line in $reverts) {
  $repoName, $rel = $line.Split(":", 2)
  if ($repoName -eq "hunks") {
    # hunks:uc:<patch>:<regex> -> reverse only the matching hunks (filtered with scripts/patchfilter.py)
    $r, $rest = $rel.Split(":", 2); $prel, $regex = $rest.Split(":", 2)
    $exclude = $null; if ($regex -match '^(.*):!(.*)$') { $regex = $Matches[1]; $exclude = $Matches[2] }
    $pf = if ($r -eq "uc") { Join-Path "$Ucw\ungoogled-chromium\patches" $prel } else { Join-Path "$Ucw\patches" $prel }
    $filtered = Join-Path $env:TEMP ("pb-hunks-" + ($prel -replace '[\\/]', '_'))
    $fa = @($pf, $regex, "--keep", "-o", $filtered); if ($exclude) { $fa += @("--exclude-path", $exclude) }
    # Windows PowerShell 5.1 turns any stderr line of a native command into a terminating error under "Stop";
    # patchfilter reports its hunk count on stderr, so run it with "Continue" and judge by the exit code.
    $eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    & $py -I "$Repo\scripts\patchfilter.py" @fa 2>&1 | Out-Null
    $pfrc = $LASTEXITCODE
    $ErrorActionPreference = $eap
    # Exit 3 = no matching hunk: on a fresh tree prepare.py already cut them out of the patch file, nothing to reverse.
    if ($pfrc -eq 3) { Write-Host "[skip] revert hunks of $prel (already dropped by prepare.py)"; continue }
    if ($pfrc) { throw "patchfilter failed on $prel (exit $pfrc)" }
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
  $eap = $ErrorActionPreference; $ErrorActionPreference = "Continue"
  $out = & $patch -d $Src -p1 -R --forward --no-backup-if-mismatch -i $file 2>&1
  $ErrorActionPreference = $eap
  # Hunks in the patch vs hunks patch refused with "Unreversed patch detected": the difference was reversed.
  $hunks = @(Get-Content $file | Where-Object { $_ -match "^@@ " }).Count
  $unrev = @($out | Where-Object { $_ -match "Unreversed patch detected" }).Count
  $reversed = $hunks - $unrev
  Get-ChildItem $Src -Recurse -Filter *.rej -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $before } | Remove-Item -Force
  if ($reversed -le 0) { Write-Host "[skip] revert $rel (not applied)"; continue }
  & $patch -d $Src -p1 --dry-run --silent -i $file *> $null
  if ($LASTEXITCODE -ne 0) {
    # Expected when the patch depended on another patch reverted just before it (its context is gone with it).
    Write-Host "[warn] ${rel}: ${reversed} of ${hunks} hunks reversed, forward dry-run does not match (context from an earlier reverted patch); check git diff on its files"
  } else { Write-Host "[ok]   reverted $rel ($reversed hunks)" }
}
# 1. //plainbrowser sources (mirror; rust target dirs excluded)
robocopy "$Repo\src\plainbrowser" "$Src\plainbrowser" /MIR /XD target vendor /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
Write-Host "copied //plainbrowser"

# 2. patches in series order
$series = Get-Content "$Repo\patches\series" | Where-Object { $_.Trim() -and -not $_.StartsWith("#") }
if ($FromPrepare) { $series = @() }
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
