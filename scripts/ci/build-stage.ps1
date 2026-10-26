<# One build stage of the GitHub Actions build: a single `build.py --ci` pass on the prepared tree.
   With --ci build.py skips every finished setup step, writes args.gn and runs gn gen and bindgen on the first pass,
   and then runs ninja for at most 3.5 hours. It exits 124 when that time slice ends (the tree is then handed to
   the next job through an artifact) and 0 when chrome, chromedriver and mini_installer are built (it then also runs
   upstream's package.py; our packaging follows in scripts\ci\package.ps1). Same mechanism and exit code as
   ungoogled-chromium-windows' own .github/actions/stage.
   Writes finished=true|false to GITHUB_OUTPUT.
   Usage: pwsh -File scripts\ci\build-stage.ps1 [-Work C:\pb] [-Profile alpha|official] [-Jobs N] #>
param(
  [string]$Work = "C:\pb",
  [ValidateSet("alpha", "official")][string]$Profile = "alpha",
  [int]$Jobs = 0
)
. "$PSScriptRoot\common.ps1"
$ucw = Join-Path $Work "ucw"
if (-not (Test-Path "$ucw\build\src\BUILD.gn")) { throw "no prepared tree in $ucw (run prepare-tree.ps1 or unpack-tree.ps1 first)" }
if ($Jobs -le 0) {
  # Standard runners have 4 cores and 16 GB of RAM. The alpha profile links without LTO and fits 4 parallel jobs;
  # official (ThinLTO + PGO) needs the headroom upstream leaves with -j 2.
  $Jobs = if ($Profile -eq "official") { 2 } else { 4 }
}
Write-DiskFree
Run python -m pip install --quiet "httplib2==0.22.0" "clang==21.1.7"

Push-Location $ucw
try {
  Write-Step "python build.py --ci -j $Jobs"
  & python build.py --ci -j $Jobs
  $rc = $LASTEXITCODE
} finally {
  Pop-Location
}
Write-Step "build.py exited with $rc"
switch ($rc) {
  0 {
    Write-GitHubOutput finished true
    Get-ChildItem "$ucw\build\src\out\Default" -Filter "*.exe" | Where-Object { $_.Name -in "chrome.exe", "mini_installer.exe", "chromedriver.exe" } |
      ForEach-Object { Write-Host "  $($_.Name)  $([math]::Round($_.Length / 1MB)) MB" }
  }
  124 {
    Write-Host "ninja time slice ended; the next stage resumes from the saved tree"
    Write-GitHubOutput finished false
  }
  default { throw "build.py failed (exit $rc)" }
}
Write-DiskFree
